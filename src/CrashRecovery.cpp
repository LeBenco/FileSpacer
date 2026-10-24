#include "CrashRecovery.h"
#include "Settings.h"
#include "FolderStateStore.h"
#include "UIStrings.h"
#include <atlbase.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <strsafe.h>
#include <string>
#include <vector>

namespace filespacer { namespace recovery {
namespace {
struct CrashData {
    DWORD magic, process, thread;
    LONG busy;
    BOOL test;
    EXCEPTION_RECORD exception;
    CONTEXT context;
};
constexpr DWORD MAGIC = 0x46535231;
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
void message(UINT id, UINT icon = MB_ICONINFORMATION) {
    MessageBoxW(nullptr, getString(id), getString(IDS_APP_NAME), MB_OK | icon);
}

// No allocation, registry writes, UI or C++ stack unwinding in the faulting thread.
// The healthy helper writes the minidump while this thread waits in the filter.
LONG WINAPI unhandledException(EXCEPTION_POINTERS *info) {
    if (crash && helperProcess && InterlockedCompareExchange(&crash->busy, 1, 0) == 0) {
        crash->thread = GetCurrentThreadId();
        crash->exception = *info->ExceptionRecord;
        crash->exception.ExceptionRecord = nullptr;
        crash->context = *info->ContextRecord;
        SetEvent(crashEvent);
        HANDLE waits[] = { completedEvent, helperProcess };
        WaitForMultipleObjects(2, waits, FALSE, 30000);
        TerminateProcess(GetCurrentProcess(), info->ExceptionRecord->ExceptionCode);
    }
    return previousFilter ? previousFilter(info) : EXCEPTION_CONTINUE_SEARCH;
}

void writeFile(const std::wstring &path, const void *bytes, DWORD size) {
    HANDLE raw = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    CHandle file(raw == INVALID_HANDLE_VALUE ? nullptr : raw);
    if (!file) return;
    DWORD written = 0; const WORD bom = 0xFEFF;
    WriteFile(file, &bom, sizeof(bom), &written, nullptr);
    WriteFile(file, bytes, size, &written, nullptr);
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
    std::wstring report = L"FileSpacer crash diagnostics\r\n";
    wchar_t details[256];
    StringCchPrintfW(details, _countof(details), L"Code: 0x%08lX\r\nAddress: 0x%llX\r\nThread: %lu\r\n",
        data->exception.ExceptionCode, reinterpret_cast<DWORD64>(data->exception.ExceptionAddress),
        data->thread);
    report += details;
    const auto directory = dataDirectory();
    const wchar_t *kind = data->test ? L"test" : L"normal";
    if (!directory.empty()) {
        // The dump is the only evidence usable after the fact: the process hosts the
        // Shell and every installed extension, so the faulting module is unknown here.
        HMODULE library = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        auto dump = library ? reinterpret_cast<decltype(&MiniDumpWriteDump)>(
            GetProcAddress(library, "MiniDumpWriteDump")) : nullptr;
        if (dump && SUCCEEDED(StringCchPrintfW(details, _countof(details), L"\\recovery-%s-%lu.dmp",
                kind, data->process))) {
            HANDLE raw = CreateFileW((directory + details).c_str(), GENERIC_WRITE, 0,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            CHandle file(raw == INVALID_HANDLE_VALUE ? nullptr : raw);
            if (file) {
                MINIDUMP_EXCEPTION_INFORMATION exception = {data->thread, nullptr, FALSE};
                EXCEPTION_POINTERS pointers = {const_cast<EXCEPTION_RECORD *>(&data->exception),
                    const_cast<CONTEXT *>(&data->context)};
                exception.ExceptionPointers = &pointers;
                report += dump(parent, data->process, file, MiniDumpNormal, &exception, nullptr, nullptr)
                    ? L"Minidump: written.\r\n" : L"Minidump: failed.\r\n";
            }
        }
        if (library) FreeLibrary(library);
        if (SUCCEEDED(StringCchPrintfW(details, _countof(details), L"\\recovery-%s-%lu.txt",
                kind, data->process)))
            writeFile(directory + details, report.data(),
                static_cast<DWORD>(report.size() * sizeof(wchar_t)));
    }
    UnmapViewOfFile(data);
    SetEvent(completed);
    if (WaitForSingleObject(parent, 30000) != WAIT_OBJECT_0) return;
    message(IDS_RECOVERY_CRASHED, MB_ICONERROR);
}
} // namespace

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

bool hasSettingsAccess() { return !!settingsLock; }

bool start() {
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
    crash->magic = MAGIC; crash->process = GetCurrentProcessId();
#ifdef FILESPACER_DEBUG
    crash->test = settings::testMode;
#endif
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
