#include "CrashRecovery.h"
#include "Settings.h"
#include "FolderStateStore.h"
#include "UIStrings.h"
#include <atlbase.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <psapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "Psapi.lib")

namespace filespacer { namespace recovery {
namespace {
enum Operation : DWORD { NoLabel = 0, ReadDraw, GetView, GetLabelRect, GetBackground, FillBackground };
struct CrashData {
    DWORD magic, process, thread, operation;
    LONG busy;
    BOOL test, restarted;
    DWORD64 function, input;
    DWORD inputSize;
    EXCEPTION_RECORD exception;
    CONTEXT context;
    wchar_t command[32768];
};
constexpr DWORD MAGIC = 0x46535231;
bool fullNamesBlocked = false;
CrashData *crash = nullptr;
CHandle mapping, crashEvent, completedEvent, helperProcess, settingsLock;
LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;

std::wstring dataDirectory() {
    CComHeapPtr<wchar_t> path;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &path)))
        return {};
    std::wstring directory = std::wstring(path) + L"\\FileSpacer";
    const int status = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (status != ERROR_SUCCESS && status != ERROR_ALREADY_EXISTS && status != ERROR_FILE_EXISTS)
        return {};
    return directory;
}
HANDLE lockSettings(bool exclusive) {
    const auto directory = dataDirectory();
    if (directory.empty()) return nullptr;
#ifdef FILESPACER_DEBUG
    const wchar_t *name = settings::testMode ? L"\\settings-test.lock" : L"\\settings.lock";
#else
    const wchar_t *name = L"\\settings.lock";
#endif
    HANDLE file = CreateFileW((directory + name).c_str(), GENERIC_READ,
        exclusive ? 0 : FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return file == INVALID_HANDLE_VALUE ? nullptr : file;
}
const wchar_t *recoveryKey(bool test) {
    return test ? L"Software\\FileSpacer\\Test\\Recovery" : L"Software\\FileSpacer\\Recovery";
}
void message(UINT id, UINT icon = MB_ICONINFORMATION) {
    MessageBoxW(nullptr, getString(id), getString(IDS_APP_NAME), MB_OK | icon);
}

// No allocation, symbols, registry writes, UI or C++ stack unwinding in the faulting thread.
// The healthy helper reads the captured context while this thread waits in the filter.
LONG reportException(EXCEPTION_POINTERS *info, Operation operation,
        DWORD64 function, DWORD64 input = 0, DWORD inputSize = 0) {
    if (!crash || !helperProcess || InterlockedCompareExchange(&crash->busy, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;
    crash->thread = GetCurrentThreadId();
    crash->operation = operation;
    crash->function = function;
    crash->input = input;
    crash->inputSize = inputSize;
    crash->exception = *info->ExceptionRecord;
    crash->exception.ExceptionRecord = nullptr;
    crash->context = *info->ContextRecord;
    SetEvent(crashEvent);
    HANDLE waits[] = { completedEvent, helperProcess };
    WaitForMultipleObjects(2, waits, FALSE, 30000);
    TerminateProcess(GetCurrentProcess(), info->ExceptionRecord->ExceptionCode);
    return EXCEPTION_CONTINUE_SEARCH; // Only reached if termination failed.
}
LONG labelException(EXCEPTION_POINTERS *info, Operation operation,
        DWORD64 function, DWORD64 input = 0, DWORD inputSize = 0) {
    // Other exceptions follow their existing handlers; no global catch-all around painting.
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    return reportException(info, operation, function, input, inputSize);
}
LONG WINAPI unhandledException(EXCEPTION_POINTERS *info) {
    reportException(info, NoLabel, 0);
    return previousFilter ? previousFilter(info) : EXCEPTION_CONTINUE_SEARCH;
}

struct Diagnostics {
    HMODULE library = nullptr;
    decltype(&SymInitializeW) initialize = nullptr;
    decltype(&SymCleanup) cleanup = nullptr;
    decltype(&SymSetOptions) options = nullptr;
    decltype(&SymFunctionTableAccess64) table = nullptr;
    decltype(&SymGetModuleBase64) base = nullptr;
    decltype(&StackWalk64) walk = nullptr;
    decltype(&MiniDumpWriteDump) dump = nullptr;
    bool load() {
        library = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!library) return false;
#define LOAD_ENTRY(field, name) field = reinterpret_cast<decltype(field)>(GetProcAddress(library, name))
        LOAD_ENTRY(initialize, "SymInitializeW"); LOAD_ENTRY(cleanup, "SymCleanup");
        LOAD_ENTRY(options, "SymSetOptions"); LOAD_ENTRY(table, "SymFunctionTableAccess64");
        LOAD_ENTRY(base, "SymGetModuleBase64"); LOAD_ENTRY(walk, "StackWalk64");
        LOAD_ENTRY(dump, "MiniDumpWriteDump");
#undef LOAD_ENTRY
        return initialize && cleanup && options && table && base && walk && dump;
    }
    ~Diagnostics() { if (library) FreeLibrary(library); }
};
struct Range { DWORD64 begin = 0, end = 0; };
Range functionRange(Diagnostics &diag, HANDLE process, DWORD64 address) {
    const DWORD64 module = diag.base(process, address);
    auto entry = static_cast<PRUNTIME_FUNCTION>(diag.table(process, address));
    return module && entry ? Range{module + entry->BeginAddress, module + entry->EndAddress} : Range{};
}
bool inside(const Range &range, DWORD64 address) {
    return range.begin && address >= range.begin && address < range.end;
}
bool allowedNativeModule(HANDLE process, DWORD64 address, std::wstring &report) {
    MEMORY_BASIC_INFORMATION memory = {};
    wchar_t path[32768] = {};
    if (!VirtualQueryEx(process, reinterpret_cast<void *>(address), &memory, sizeof(memory))
            || memory.Type != MEM_IMAGE
            || !GetMappedFileNameW(process, memory.AllocationBase, path, _countof(path)))
        return false;
    report += path; report += L"\r\n";
    const wchar_t *name = wcsrchr(path, L'\\');
    if (!name) return false;
    // A module name alone is insufficient: require the Windows System32 or WinSxS path.
    wchar_t windows[32768] = {};
    if (!GetWindowsDirectoryW(windows, _countof(windows))) return false;
    const wchar_t *drive = wcschr(windows, L':');
    if (!drive) return false;
    const std::wstring system = std::wstring(drive + 1) + L"\\System32\\";
    const std::wstring sideBySide = std::wstring(drive + 1) + L"\\WinSxS\\";
    std::wstring lower = path, lowerSystem = system, lowerSide = sideBySide;
    CharLowerBuffW(&lower[0], static_cast<DWORD>(lower.size()));
    CharLowerBuffW(&lowerSystem[0], static_cast<DWORD>(lowerSystem.size()));
    CharLowerBuffW(&lowerSide[0], static_cast<DWORD>(lowerSide.size()));
    if (lower.find(lowerSystem) == std::wstring::npos && lower.find(lowerSide) == std::wstring::npos)
        return false;
    const wchar_t *allowed[] = {L"\\comctl32.dll", L"\\user32.dll", L"\\win32u.dll",
        L"\\gdi32.dll", L"\\gdi32full.dll", L"\\ntdll.dll", L"\\kernelbase.dll", L"\\kernel32.dll"};
    for (const auto candidate : allowed) if (lstrcmpiW(name, candidate) == 0) return true;
    return false;
}

bool attributeLabelCrash(Diagnostics &diag, HANDLE process, const CrashData &data,
        std::wstring &report) {
    const auto &error = data.exception;
    if (data.operation < ReadDraw || data.operation > FillBackground
            || error.ExceptionCode != EXCEPTION_ACCESS_VIOLATION
            || error.NumberParameters != 2 || error.ExceptionInformation[0] > 1)
        return false;
    const Range scope = functionRange(diag, process, data.function);
    if (!scope.begin) return false;
    const DWORD64 fault = reinterpret_cast<DWORD64>(error.ExceptionAddress);
    if (data.operation == ReadDraw) {
        // For notification reads require both the exact dedicated core and its input range.
        const DWORD64 bad = error.ExceptionInformation[1];
        return error.ExceptionInformation[0] == 0 && inside(scope, fault)
            && data.inputSize == sizeof(NMLVCUSTOMDRAW) && bad >= data.input
            && bad - data.input < data.inputSize;
    }
    // Direct faults in our cores, or a complete chain of approved native implementation
    // frames ending at that exact core. Foreign/shared application callbacks reject attribution.
    if (inside(scope, fault)) return true;
    if (!allowedNativeModule(process, fault, report)) return false;
    CHandle thread(OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, data.thread));
    if (!thread) return false;
    CONTEXT context = data.context;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip; frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp; frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    DWORD64 previousPC = context.Rip, previousSP = context.Rsp;
    for (int depth = 0; depth < 64; ++depth) {
        if (!diag.walk(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context,
                nullptr, diag.table, diag.base, nullptr) || !frame.AddrPC.Offset
                || frame.AddrStack.Offset < previousSP)
            return false;
        if (inside(scope, frame.AddrPC.Offset)) return true;
        if (frame.AddrPC.Offset == previousPC && frame.AddrStack.Offset == previousSP) {
            if (depth != 0) return false;
        } else if (!allowedNativeModule(process, frame.AddrPC.Offset, report)) {
            return false;
        }
        previousPC = frame.AddrPC.Offset; previousSP = frame.AddrStack.Offset;
    }
    return false;
}

bool repeatedCrash(bool test) {
    const wchar_t *mutexName = test ? L"Local\\FileSpacer.RecoveryCounter.Test"
        : L"Local\\FileSpacer.RecoveryCounter";
    CHandle mutex(CreateMutexW(nullptr, FALSE, mutexName));
    if (!mutex) return false;
    const DWORD wait = WaitForSingleObject(mutex, 5000);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return false;
    FILETIME time; GetSystemTimeAsFileTime(&time);
    ULARGE_INTEGER now = {}; now.LowPart = time.dwLowDateTime; now.HighPart = time.dwHighDateTime;
    ULONGLONG first = 0; DWORD count = 0, size = sizeof(first);
    const wchar_t *key = recoveryKey(test);
    RegGetValueW(HKEY_CURRENT_USER, key, L"WindowStart", RRF_RT_QWORD, nullptr, &first, &size);
    size = sizeof(count);
    RegGetValueW(HKEY_CURRENT_USER, key, L"CrashCount", RRF_RT_DWORD, nullptr, &count, &size);
    if (!first || now.QuadPart < first || now.QuadPart - first > 10ULL * 60 * 10000000) {
        first = now.QuadPart; count = 0;
    }
    count = count < 2 ? count + 1 : 2;
    const bool saved = RegSetKeyValueW(HKEY_CURRENT_USER, key, L"WindowStart", REG_QWORD,
        &first, sizeof(first)) == ERROR_SUCCESS
        && RegSetKeyValueW(HKEY_CURRENT_USER, key, L"CrashCount", REG_DWORD,
            &count, sizeof(count)) == ERROR_SUCCESS;
    ReleaseMutex(mutex);
    return saved && count >= 2;
}

void runHelper(HANDLE parent, HANDLE map, HANDLE event, HANDLE completed) {
    CHandle parentHandle(parent), mapHandle(map), eventHandle(event), completedHandle(completed);
    auto data = static_cast<CrashData *>(MapViewOfFile(map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(CrashData)));
    if (!data) return;
    const bool valid = data->magic == MAGIC && GetProcessId(parent) == data->process;
    HANDLE waits[] = {event, parent};
    const DWORD wait = valid ? WaitForMultipleObjects(2, waits, FALSE, INFINITE) : WAIT_FAILED;
    if (wait != WAIT_OBJECT_0 || !data->busy) { UnmapViewOfFile(data); return; }
#ifdef FILESPACER_DEBUG
    settings::testMode = !!data->test;
#endif
    const bool repeated = repeatedCrash(!!data->test);
    bool attributed = false;
    std::wstring report = L"FileSpacer crash diagnostics\r\n";
    wchar_t details[256];
    StringCchPrintfW(details, _countof(details),
        L"Code: 0x%08lX\r\nAddress: 0x%llX\r\nOperation: %lu\r\nThread: %lu\r\n",
        data->exception.ExceptionCode, reinterpret_cast<DWORD64>(data->exception.ExceptionAddress),
        data->operation, data->thread);
    report += details;
    Diagnostics diag;
    bool initialized = false;
    if (diag.load()) {
        diag.options(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS
            | SYMOPT_NO_PROMPTS | SYMOPT_IGNORE_NT_SYMPATH);
        initialized = !!diag.initialize(parent, L"", TRUE); // No symbol-server configuration.
    }
    if (initialized) {
        attributed = attributeLabelCrash(diag, parent, *data, report);
        const auto directory = dataDirectory();
        if (!directory.empty()) {
            StringCchPrintfW(details, _countof(details), L"\\recovery-%s-%lu.dmp",
                data->test ? L"test" : L"normal", data->process);
            HANDLE raw = CreateFileW((directory + details).c_str(), GENERIC_WRITE, 0,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            CHandle file(raw == INVALID_HANDLE_VALUE ? nullptr : raw);
            if (file) {
                MINIDUMP_EXCEPTION_INFORMATION exception = {data->thread, nullptr, FALSE};
                EXCEPTION_POINTERS pointers = {const_cast<EXCEPTION_RECORD *>(&data->exception),
                    const_cast<CONTEXT *>(&data->context)};
                exception.ExceptionPointers = &pointers;
                diag.dump(parent, data->process, file, MiniDumpNormal, &exception, nullptr, nullptr);
            }
        }
        diag.cleanup(parent);
    }
    report += attributed ? L"Attribution: experimental label operation (strong evidence, not root-cause proof).\r\n"
        : L"Attribution: inconclusive; option unchanged.\r\n";
    const auto directory = dataDirectory();
    if (!directory.empty()) {
        StringCchPrintfW(details, _countof(details), L"\\recovery-%s-%lu.txt",
            data->test ? L"test" : L"normal", data->process);
        HANDLE raw = CreateFileW((directory + details).c_str(), GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHandle file(raw == INVALID_HANDLE_VALUE ? nullptr : raw);
        if (file) {
            DWORD written = 0; const WORD bom = 0xFEFF;
            WriteFile(file, &bom, sizeof(bom), &written, nullptr);
            WriteFile(file, report.data(), static_cast<DWORD>(report.size() * sizeof(wchar_t)), &written, nullptr);
        }
    }
    const bool disabled = attributed && settings::disableFullNamesOnSelection() == ERROR_SUCCESS;
    const bool restart = disabled && !data->restarted;
    std::wstring command = data->command;
    UnmapViewOfFile(data);
    SetEvent(completed);
    if (WaitForSingleObject(parent, 30000) != WAIT_OBJECT_0) return;
    if (restart) {
        command += L" /recovery-restarted";
        STARTUPINFOW startup = {sizeof(startup)}; PROCESS_INFORMATION process = {};
        if (CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                &startup, &process)) {
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
            message(IDS_LABEL_CRASH_DISABLED, MB_ICONWARNING);
        } else message(IDS_RECOVERY_RESTART_FAILED, MB_ICONERROR);
    } else if (attributed && !disabled) message(IDS_RECOVERY_DISABLE_FAILED, MB_ICONERROR);
    if (repeated) message(IDS_RECOVERY_REPEATED, MB_ICONWARNING);
    else if (!attributed) message(IDS_RECOVERY_UNATTRIBUTED, MB_ICONERROR);
    else if (disabled && !restart) message(IDS_LABEL_DISABLED_NO_RESTART, MB_ICONWARNING);
}
} // namespace

// These functions contain only the operation's code and its local SEH filter.
// Their x64 unwind entries delimit the protected operation without PDB downloads.
__declspec(noinline) LabelDraw readLabelDraw(const NMLVCUSTOMDRAW *draw) {
    __try {
        return {draw->nmcd.dwDrawStage, draw->dwItemType, draw->nmcd.uItemState,
            draw->nmcd.dwItemSpec, draw->nmcd.hdc};
    }
    __except(labelException(GetExceptionInformation(), ReadDraw,
            reinterpret_cast<DWORD64>(&readLabelDraw), reinterpret_cast<DWORD64>(draw), sizeof(*draw))) {
        return {}; // reportException terminates; never resume an unstable process.
    }
}
__declspec(noinline) DWORD labelView(HWND control) {
    __try { return ListView_GetView(control); }
    __except(labelException(GetExceptionInformation(), GetView, reinterpret_cast<DWORD64>(&labelView))) { return 0; }
}
__declspec(noinline) BOOL labelRect(HWND control, int item, RECT *rect) {
    __try { return ListView_GetItemRect(control, item, rect, LVIR_LABEL); }
    __except(labelException(GetExceptionInformation(), GetLabelRect, reinterpret_cast<DWORD64>(&labelRect))) { return FALSE; }
}
__declspec(noinline) COLORREF labelBackground(HWND control) {
    __try { return ListView_GetBkColor(control); }
    __except(labelException(GetExceptionInformation(), GetBackground, reinterpret_cast<DWORD64>(&labelBackground))) { return CLR_NONE; }
}
__declspec(noinline) void fillLabel(HDC dc, const RECT *rect, COLORREF color) {
    __try {
        const int saved = SaveDC(dc);
        if (saved) {
            SetDCBrushColor(dc, color);
            FillRect(dc, rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            RestoreDC(dc, saved);
        }
    }
    __except(labelException(GetExceptionInformation(), FillBackground, reinterpret_cast<DWORD64>(&fillLabel))) {}
}

bool handleCommandLine(int argc, wchar_t **argv, int *exitCode) {
#ifdef FILESPACER_DEBUG
    for (int i = 1; i < argc; ++i) if (lstrcmpiW(argv[i], L"/test") == 0) settings::testMode = true;
#endif
    if (argc == 6 && lstrcmpiW(argv[1], L"/recovery-helper") == 0) {
        HANDLE handles[4] = {};
        for (int i = 0; i < 4; ++i) {
            wchar_t *end = nullptr;
            const auto value = wcstoull(argv[i + 2], &end, 16);
            if (!value || !end || *end) { *exitCode = 2; return true; }
            handles[i] = reinterpret_cast<HANDLE>(value);
        }
        runHelper(handles[0], handles[1], handles[2], handles[3]);
        *exitCode = 0; return true;
    }
    bool resetOptions = false, resetAll = false;
    for (int i = 1; i < argc; ++i) {
        resetOptions |= lstrcmpiW(argv[i], L"/reset-options") == 0;
        resetAll |= lstrcmpiW(argv[i], L"/reset-all") == 0;
    }
    if (!resetOptions && !resetAll) return false;
    for (int i = 1; i < argc; ++i) {
        if (lstrcmpiW(argv[i], L"/reset-options") == 0 || lstrcmpiW(argv[i], L"/reset-all") == 0) continue;
#ifdef FILESPACER_DEBUG
        if (lstrcmpiW(argv[i], L"/test") == 0) continue;
#endif
        message(IDS_RESET_CLI_ARGUMENTS, MB_ICONERROR); *exitCode = 2; return true;
    }
    if (resetAll && resetOptions) {
        message(IDS_RESET_CLI_ARGUMENTS, MB_ICONERROR); *exitCode = 2; return true;
    }
    CHandle exclusive(lockSettings(true));
    if (!exclusive) { message(IDS_RESET_CLI_BUSY, MB_ICONERROR); *exitCode = 3; return true; }
    bool openWindows = false;
    const BOOL enumerated = EnumWindows([](HWND window, LPARAM context) -> BOOL {
        wchar_t name[64] = {};
        if (GetClassNameW(window, name, _countof(name))
                && lstrcmpW(name, L"FileSpacer Item Window") == 0) {
            *reinterpret_cast<bool *>(context) = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&openWindows));
    // Older binaries do not hold the new file lock; reject their visible windows too.
    if (openWindows || !enumerated) {
        message(IDS_RESET_CLI_BUSY, MB_ICONERROR); *exitCode = 3; return true;
    }
    if (resetAll) {
        auto store = folderStateStore();
        if (!store || !store->clearAll()) {
            message(IDS_RESET_FOLDER_STATE_DELETE_ERROR, MB_ICONERROR); *exitCode = 4; return true;
        }
    }
    const LSTATUS status = settings::resetGlobalOptions();
    if (status != ERROR_SUCCESS) {
        auto error = getErrorMessage(status);
        auto text = formatString(IDS_RESET_CLI_FAILED, error.get());
        MessageBoxW(nullptr, text.get(), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
        *exitCode = 4; return true;
    }
    message(resetAll ? IDS_RESET_CLI_ALL_DONE : IDS_RESET_CLI_OPTIONS_DONE);
    *exitCode = 0; return true;
}

bool canUseFullNames() {
    return !fullNamesBlocked && crash && helperProcess
        && WaitForSingleObject(helperProcess, 0) == WAIT_TIMEOUT;
}
bool hasSettingsAccess() { return !!settingsLock; }
void blockFullNames() { fullNamesBlocked = true; }

bool start(bool restarted) {
    settingsLock.Attach(lockSettings(false));
    if (!settingsLock) return false;
    SECURITY_ATTRIBUTES security = {sizeof(security), nullptr, TRUE};
    mapping.Attach(CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE,
        0, sizeof(CrashData), nullptr));
    crashEvent.Attach(CreateEventW(&security, FALSE, FALSE, nullptr));
    completedEvent.Attach(CreateEventW(&security, FALSE, FALSE, nullptr));
    CHandle parent(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        TRUE, GetCurrentProcessId()));
    if (!mapping || !crashEvent || !completedEvent || !parent) return false;
    crash = static_cast<CrashData *>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(CrashData)));
    if (!crash) return false;
    *crash = {};
    crash->magic = MAGIC; crash->process = GetCurrentProcessId(); crash->restarted = restarted;
#ifdef FILESPACER_DEBUG
    crash->test = settings::testMode;
#endif
    if (FAILED(StringCchCopyW(crash->command, _countof(crash->command), GetCommandLineW()))) return false;
    wchar_t executable[32768] = {}, command[32768] = {};
    if (!GetModuleFileNameW(nullptr, executable, _countof(executable))) return false;
    if (FAILED(StringCchPrintfW(command, _countof(command), L"\"%s\" /recovery-helper %llX %llX %llX %llX",
            executable, reinterpret_cast<DWORD64>(static_cast<HANDLE>(parent)),
            reinterpret_cast<DWORD64>(static_cast<HANDLE>(mapping)),
            reinterpret_cast<DWORD64>(static_cast<HANDLE>(crashEvent)),
            reinterpret_cast<DWORD64>(static_cast<HANDLE>(completedEvent))))) return false;
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<BYTE> attributes(bytes);
    auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) return false;
    HANDLE handles[] = {parent, mapping, crashEvent, completedEvent};
    bool success = !!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        handles, sizeof(handles), nullptr, nullptr);
    STARTUPINFOEXW startup = {}; startup.StartupInfo.cb = sizeof(startup); startup.lpAttributeList = list;
    PROCESS_INFORMATION process = {};
    if (success) success = !!CreateProcessW(executable, command, nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr, &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(list);
    if (!success) return false;
    CloseHandle(process.hThread); helperProcess.Attach(process.hProcess);
    // These handles cannot leak into unrelated children launched later by the Shell.
    for (HANDLE handle : handles) SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
    previousFilter = SetUnhandledExceptionFilter(unhandledException);
    return true;
}
}} // namespace
