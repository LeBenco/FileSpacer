// Investigation only: real Shell restoration, exact identities and native Explorer.
#define FILESPACER_SELECTION_DISCOVERY_TEST
#define FILESPACER_SELECTION_PRODUCT_TEST
#define wmain selectionFixtureMain
#include "SelectionClearTests.cpp"
#undef wmain
#include <exdisp.h>
#include <shlguid.h>
#include "ShellViewIcons.h"
#include <dwmapi.h>

namespace {
void restoreExpose(HWND frame) {
    // A real caption click is setup, before capturing the empty baseline.
    // Background native helpers do not necessarily have foreground permission.
    need(SetWindowPos(frame, HWND_TOP, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != FALSE, "Cannot expose fixture caption");
    RECT bounds = {}; need(GetWindowRect(frame, &bounds) != FALSE, "Cannot locate fixture caption");
    const POINT caption = {(bounds.left + bounds.right) / 2,
        bounds.top + GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER)
            + GetSystemMetrics(SM_CYCAPTION) / 2};
    need(GetAncestor(WindowFromPoint(caption), GA_ROOT) == frame
        && SendMessageW(frame, WM_NCHITTEST, 0, MAKELPARAM(caption.x, caption.y)) == HTCAPTION,
        "Fixture caption is not an exposed activation target");
    need(SetCursorPos(caption.x, caption.y) != FALSE, "Cannot reach fixture caption");
    INPUT click[2] = {}; click[0].type = click[1].type = INPUT_MOUSE;
    click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    need(SendInput(2, click, sizeof(INPUT)) == 2, "Cannot click fixture caption");
    const ULONGLONG deadline = GetTickCount64() + 2000;
    while (GetForegroundWindow() != frame && GetTickCount64() < deadline) pump(1);
    need(GetForegroundWindow() == frame, "Caption click did not activate fixture");
}
HWND restoreKeyboardFocus(HWND frame) {
    GUITHREADINFO state = {sizeof(state)};
    need(GetGUIThreadInfo(GetWindowThreadProcessId(frame, nullptr), &state) != FALSE, "Cannot inspect keyboard focus");
    return state.hwndFocus;
}
HRESULT restoreExplorerAll(IShellView *shell, IFolderView2 *view, HWND frame, IShellFolderView *bulk) {
    if (bulk) return bulk->Select(SFVS_SELECT_ALLITEMS);
    need(SetForegroundWindow(frame) != FALSE || GetForegroundWindow() == frame, "Cannot activate Explorer fixture");
    need(SUCCEEDED(shell->UIActivate(SVUIA_ACTIVATE_FOCUS)), "Cannot focus Explorer view");
    INPUT input[4] = {};
    for (auto &entry : input) entry.type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_CONTROL; input[1].ki.wVk = 'A';
    input[2].ki.wVk = 'A'; input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3].ki.wVk = VK_CONTROL; input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    need(SendInput(4, input, sizeof(INPUT)) == 4, "Cannot send native Ctrl+A");
    const ULONGLONG deadline = GetTickCount64() + 10000;
    int selected = 0;
    do {
        if (SUCCEEDED(view->ItemCount(SVGIO_SELECTION, &selected)) && selected == fixtureCount) return S_OK;
        pump(1);
    } while (GetTickCount64() < deadline);
    return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
}
struct RestoreExplorer {
    HWND frame = nullptr;
    CComPtr<IShellView> shell;
    CComPtr<ViewEvents> events;
    ~RestoreExplorer() {
        if (events) events->detach();
        if (frame && IsWindow(frame)) PostMessageW(frame, WM_CLOSE, 0, 0);
    }
    void open(const wchar_t *path) {
        wchar_t system[MAX_PATH] = {};
        need(GetWindowsDirectoryW(system, _countof(system)) != 0, "Cannot locate Windows");
        const std::wstring executable = std::wstring(system) + L"\\explorer.exe";
        std::wstring command = L"\"" + executable + L"\" \"" + path + L"\"";
        STARTUPINFOW start = {sizeof(start)}; PROCESS_INFORMATION process = {};
        need(CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &start, &process) != FALSE, "Cannot launch Explorer fixture");
        std::unique_ptr<void, decltype(&CloseHandle)> processHandle(process.hProcess, &CloseHandle);
        std::unique_ptr<void, decltype(&CloseHandle)> threadHandle(process.hThread, &CloseHandle);
        CComPtr<IShellWindows> windows;
        need(SUCCEEDED(windows.CoCreateInstance(CLSID_ShellWindows)), "No Shell windows");
        const ULONGLONG deadline = GetTickCount64() + 30000;
        bool logged = false;
        while (!shell && GetTickCount64() < deadline) {
            long count = 0;
            windows->get_Count(&count);
            if (!logged && count) { std::printf("EXPLORER windows=%ld\n", count); std::fflush(stdout); }
            for (long i = 0; i < count && !shell; ++i) {
                CComPtr<IDispatch> dispatch;
                if (FAILED(windows->Item(CComVariant(i), &dispatch))) continue;
                CComQIPtr<IWebBrowserApp> browser(dispatch);
                CComQIPtr<IServiceProvider> services(browser);
                CComPtr<IShellBrowser> host;
                CComPtr<IShellView> candidate;
                CComPtr<IFolderView2> view;
                CComPtr<IPersistFolder2> folder;
                CComHeapPtr<ITEMIDLIST> folderId;
                CComHeapPtr<wchar_t> shown;
                SHANDLE_PTR window = 0;
                if (!browser || !services || FAILED(browser->get_HWND(&window))
                    || FAILED(services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&host)))
                    || FAILED(host->QueryActiveShellView(&candidate))
                    || FAILED(candidate->QueryInterface(IID_PPV_ARGS(&view)))
                    || FAILED(view->GetFolder(IID_PPV_ARGS(&folder)))
                    || FAILED(folder->GetCurFolder(&folderId))
                    || FAILED(SHGetNameFromIDList(folderId, SIGDN_FILESYSPATH, &shown))
                    ) continue;
                if (!logged) { std::printf("EXPLORER path=%ls\n", shown.m_pData); std::fflush(stdout); }
                if (_wcsicmp(shown, path) != 0) continue;
                frame = reinterpret_cast<HWND>(window);
                shell = candidate;
            }
            if (count) logged = true;
            if (!shell) pump(10);
        }
        need(shell != nullptr, "Explorer view did not become available");
        events.Attach(new ViewEvents);
        events->OnViewCreated(shell);
        need(SUCCEEDED(events->observed), "Cannot observe Explorer enumeration");
    }
};
struct RestoreHost : SelectionHost {
    void createProfile(const wchar_t *path, FOLDERVIEWMODE mode, FOLDERVIEWOPTIONS profile) {
        frame = CreateWindowW(L"FileSpacer Placement Prototype", L"Restore all investigation", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            50, 50, 850, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        need(frame != nullptr, "Cannot create restoration fixture");
        need(WTSRegisterSessionNotification(frame, NOTIFY_FOR_THIS_SESSION) != FALSE, "Cannot observe session");
        RECT bounds = {}; GetClientRect(frame, &bounds);
        const FOLDERSETTINGS settings = {static_cast<UINT>(mode), FWF_AUTOARRANGE | FWF_NOWEBVIEW | FWF_NOHEADERINALLVIEWS}; events.Attach(new ViewEvents);
        need(SUCCEEDED(host.initialize(frame, bounds, settings, nullptr, events, profile)), "Cannot initialize profile");
        CComPtr<IShellItem> folder;
        need(SUCCEEDED(SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&folder)))
            && SUCCEEDED(host.navigate(folder, false, true)), "Cannot navigate restoration fixture");
        const ULONGLONG deadline = GetTickCount64() + 30000;
        while ((!events->navigated || !events->sink || !events->sink->ready) && GetTickCount64() < deadline) pump(10);
        need(events->navigated && events->sink && events->sink->ready && SUCCEEDED(events->observed), "Profile enumeration did not finish");
        need(SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)), "Cannot activate profile");
    }
};
std::vector<uint8_t> restoreFreeFixture(const std::vector<PCUITEMID_CHILD> &ids) {
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(nullptr, 0));
    need(stream != nullptr, "Cannot encode free layout");
    for (size_t i = 0; i < ids.size(); ++i) {
        const POINT point = {30 + static_cast<LONG>(i % 50) * 300, 30 + static_cast<LONG>(i / 50) * 300};
        need(SUCCEEDED(IStream_WritePidl(stream, ids[i])) && SUCCEEDED(IStream_Write(stream, &point, sizeof(point))), "Cannot serialize free position");
    }
    STATSTG stat = {}; LARGE_INTEGER start = {};
    need(SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && SUCCEEDED(stream->Seek(start, STREAM_SEEK_SET, nullptr)), "Cannot inspect free layout");
    std::vector<uint8_t> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    need(SUCCEEDED(IStream_Read(stream, bytes.data(), static_cast<ULONG>(bytes.size()))), "Cannot read free layout");
    return bytes;
}
std::vector<uint8_t> restoreSnapshot(const std::vector<PCUITEMID_CHILD> &ids) {
    struct Header { uint32_t version, selected, flags; POINT offset; };
    static_assert(sizeof(Header) == 20, "Legacy version-1 selection header");
    const Header header = {1, static_cast<uint32_t>(ids.size()), 0, {}};
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(nullptr, 0));
    need(stream && SUCCEEDED(IStream_Write(stream, &header, sizeof(header))), "Cannot encode selection fixture");
    for (const auto id : ids) need(SUCCEEDED(IStream_WritePidl(stream, id)), "Cannot encode selected identity");
    STATSTG stat = {}; LARGE_INTEGER start = {};
    need(SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && SUCCEEDED(stream->Seek(start, STREAM_SEEK_SET, nullptr)), "Cannot inspect selection fixture");
    std::vector<uint8_t> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    need(SUCCEEDED(IStream_Read(stream, bytes.data(), static_cast<ULONG>(bytes.size()))), "Cannot read selection fixture");
    return bytes;
}
#ifndef RESTORE_BASELINE
void restoreEnumeration(IShellView *shell, ViewEvents *events);
void restoreSafety(IShellView *shell, IFolderView2 *view, ViewEvents *events, const wchar_t *fixture,
        const std::vector<PCUITEMID_CHILD> &ids, const std::vector<std::wstring> &wanted,
        const std::function<bool()> &current, filespacer::ShellSelectionClear &binding) {
    binding.bind(shell, current);
    need(SUCCEEDED(binding.clear(shell, current)), "Cannot clear safety setup");
    unsigned queries = 0;
    auto pending = restoreSnapshot(ids);
    const HRESULT cancelledResult = filespacer::restoreReopenSelection(shell, pending,
        [&] { return current() && ++queries < 64; }, &binding);
    int selectedCount = -1;
    need(cancelledResult == E_ABORT && pending.empty()
        && SUCCEEDED(view->ItemCount(SVGIO_SELECTION, &selectedCount)) && selectedCount == 0,
        "Cancellation changed the selection before identity proof completed");
    std::puts("SAFETY cancellation: pending consumed, exact empty selection retained");
    CComPtr<IShellItem> victim;
    need(SUCCEEDED(view->GetItem(static_cast<int>(ids.size()) - 1, IID_PPV_ARGS(&victim))), "Cannot identify final fixture item");
    const std::wstring original = itemPath(victim);
    auto expected = wanted;
    expected.erase(std::remove(expected.begin(), expected.end(), original), expected.end());
    auto duplicate = ids; duplicate.back() = duplicate.front();
    pending = restoreSnapshot(duplicate);
    const auto beforeDuplicate = binding.acceleratedSelectAllCount();
    need(SUCCEEDED(filespacer::restoreReopenSelection(shell, pending, current, &binding)) && pending.empty(), "Duplicate fallback failed");
    CComPtr<IShellItemArray> selected;
    need(SUCCEEDED(view->GetSelection(FALSE, &selected)) && arrayPaths(selected) == expected
        && binding.acceleratedSelectAllCount() == beforeDuplicate, "Duplicate identities incorrectly authorized Select All");
    std::puts("SAFETY duplicate identities: exact subset restored, no global selection");
    need(SUCCEEDED(binding.clear(shell, current)), "Cannot clear stale setup"); binding.reset();
    const std::wstring replacement = std::wstring(fixture) + L"\\replacement.txt";
    std::vector<char> content;
    {
        const HANDLE opened = CreateFileW(original.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        need(opened != INVALID_HANDLE_VALUE, "Cannot read owned fixture");
        std::unique_ptr<void, decltype(&CloseHandle)> file(opened, &CloseHandle);
        LARGE_INTEGER size = {}; DWORD read = 0;
        need(GetFileSizeEx(file.get(), &size) && size.QuadPart >= 0 && size.QuadPart < 1024, "Unexpected fixture size");
        content.resize(static_cast<size_t>(size.QuadPart));
        need(ReadFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &read, nullptr)
            && read == content.size(), "Cannot retain fixture content");
    }
    const auto create = [&](const std::wstring &path) {
        const HANDLE opened = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        need(opened != INVALID_HANDLE_VALUE, "Cannot create owned replacement fixture");
        std::unique_ptr<void, decltype(&CloseHandle)> file(opened, &CloseHandle);
        DWORD written = 0;
        need(WriteFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &written, nullptr)
            && written == content.size(), "Cannot restore fixture content");
    };
    create(replacement);
    need(DeleteFileW(original.c_str()) != FALSE, "Cannot remove owned stale fixture");
    restoreEnumeration(shell, events);
    need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW, &selectedCount))
        && selectedCount == static_cast<int>(ids.size()), "Replacement did not retain view count");
    binding.bind(shell, current);
    const auto beforeStale = binding.acceleratedSelectAllCount();
    pending = restoreSnapshot(ids);
    const HRESULT staleResult = filespacer::restoreReopenSelection(shell, pending, current, &binding);
    selected.Release();
    need(pending.empty() && SUCCEEDED(view->GetSelection(FALSE, &selected)) && arrayPaths(selected) == expected
        && binding.acceleratedSelectAllCount() == beforeStale, "Same-count replacement selected a new identity");
    std::printf("SAFETY same-count replacement: exact surviving identities, no global selection, result=%08lX\n",
        static_cast<unsigned long>(staleResult));
    binding.reset();
    need(DeleteFileW(replacement.c_str()) != FALSE, "Cannot remove owned replacement"); create(original);
    restoreEnumeration(shell, events);
}
#endif
void restoreEnumeration(IShellView *shell, ViewEvents *events) {
    events->sink->ready = false;
    need(SUCCEEDED(shell->Refresh()), "Cannot refresh test view");
    const ULONGLONG deadline = GetTickCount64() + 30000;
    while (!events->sink->ready && GetTickCount64() < deadline) pump(10);
    need(events->sink->ready, "Missing completed enumeration event");
}
void restoreMatrix(const wchar_t *output, const wchar_t *fixture, bool explorer, bool freeOnly, bool observe,
        bool retainSelection = false) {
    std::unique_ptr<presentation::PointerParking> parking;
    if (observe) { parking = std::make_unique<presentation::PointerParking>(); presentation::pointer = parking.get(); }
    struct ResetPointer { ~ResetPointer() { presentation::pointer = nullptr; } } resetPointer;
    RestoreExplorer native;
    if (explorer) native.open(fixture);
    struct Mode { const char *name; FOLDERVIEWMODE mode; int size; bool arranged; };
    const Mode modes[] = {{"extra-large", FVM_ICON, 256, true}, {"large", FVM_ICON, 96, true},
        {"medium", FVM_ICON, 48, true}, {"small", FVM_SMALLICON, 16, true},
        {"list", FVM_LIST, 16, true}, {"details", FVM_DETAILS, 16, true},
        {"tiles", FVM_TILE, 48, true}, {"content", FVM_CONTENT, 32, true}, {"free", FVM_ICON, 48, false}};
    FILE *raw = nullptr;
    need(_wfopen_s(&raw, (std::wstring(output) + L"\\restore-all.csv").c_str(), L"wb") == 0, "Cannot write results");
    std::unique_ptr<FILE, decltype(&std::fclose)> log(raw, &std::fclose);
    std::fputs("app,view,arm,repetition,restore_ms,first_array_ms,selected,private_all_calls,operation,profile\n", raw);
    for (const auto &mode : modes) {
        requireSession();
        if (freeOnly && mode.arranged) continue;
        if (explorer && !mode.arranged) continue;
        const bool classic = mode.mode != FVM_DETAILS && mode.mode != FVM_LIST;
        const bool privateEligible = !explorer && classic;
        RestoreHost owner;
        if (!explorer) owner.createProfile(fixture, mode.mode, classic
            ? static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING) : FVO_DEFAULT);
        const CComPtr<IShellView> shell = explorer ? native.shell : owner.events->shell;
        CComQIPtr<IFolderView2> view(shell);
        CComQIPtr<IShellFolderView> bulk(shell);
        need(view != nullptr, "No folder view");
        const HWND frame = explorer ? native.frame : owner.frame;
        const auto current = [&] { return !cancelled && !locked && IsWindow(frame) != FALSE
            && (explorer || owner.events->shell.p == shell.p); };
        need(SUCCEEDED(view->SetViewModeAndIconSize(mode.mode, mode.size)), "Cannot set test mode");
        if (!explorer) need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE,
            mode.arranged ? FWF_AUTOARRANGE : 0)), "Cannot set arrangement");
        restoreEnumeration(shell, explorer ? native.events.p : owner.events.p);
        UINT legacyMode = 0; FOLDERVIEWMODE actualMode = FVM_AUTO; int actualSize = 0;
        need(SUCCEEDED(view->GetCurrentViewMode(&legacyMode))
            && SUCCEEDED(view->GetViewModeAndIconSize(&actualMode, &actualSize)), "Cannot read effective view mode");
        std::printf("MODE name=%s legacy=%u actual=%u size=%d arranged=%08lX\n", mode.name,
            legacyMode, static_cast<unsigned>(actualMode), actualSize, static_cast<unsigned long>(view->GetAutoArrange()));
        if (explorer) presentation::fitView(shell, frame);
        else presentation::fitView(shell, frame, [&] {
            RECT bounds = {}; GetClientRect(frame, &bounds);
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot resize host");
        });
        if (observe || explorer) restoreExpose(frame);
        int count = 0;
        need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW, &count)) && count == fixtureCount, "Fixture count differs");
        std::vector<OwnedId> storage;
        std::vector<PCUITEMID_CHILD> ids;
        for (int i = 0; i < count; ++i) {
            CComHeapPtr<ITEMID_CHILD> id;
            need(SUCCEEDED(view->Item(i, &id)) && id, "Missing fixture identity");
            storage.emplace_back(id.Detach(), &CoTaskMemFree); ids.push_back(storage.back().get());
        }
        if (!mode.arranged) {
            need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID, 0))
                && view->GetAutoArrange() == S_FALSE, "Free layout flags were lost");
            need(SUCCEEDED(filespacer::restoreIconPositions(view, restoreFreeFixture(ids), current)), "Cannot seed real free positions");
        }
        filespacer::ShellSelectionClear binding;
        const auto bind = [&] { if (privateEligible) { binding.bind(shell, current); need(binding.accelerated(), "Missing qualified capability"); } };
        bind();
        need(SUCCEEDED(restoreExplorerAll(shell, view, frame, bulk)), "No native bulk selection");
        need(SUCCEEDED(shell->SelectItem(ids[7], static_cast<SVSIF>(SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS))), "Cannot seed focus");
        std::vector<uint8_t> saved;
        const double captureStarted = clockMs();
        need(SUCCEEDED(filespacer::captureReopenSelection(shell, current, saved)), "Cannot capture selection");
        std::printf("CAPTURE app=%s mode=%s ms=%.3f\n", explorer ? "Explorer" : "FS", mode.name, clockMs() - captureStarted);
        CComPtr<IShellItemArray> retained;
        need(SUCCEEDED(view->GetSelection(FALSE, &retained)), "Cannot retain source array");
        const auto wanted = arrayPaths(retained);
        std::vector<POINT> positions;
        if (!mode.arranged) for (const auto id : ids) {
            POINT point = {};
            need(SUCCEEDED(view->GetItemPosition(id, &point)), "Cannot capture free position");
            positions.push_back(point);
        }
        for (int repetition = 0; repetition < (observe ? 1 : 3); ++repetition) {
            for (int privateArm = 0; privateArm <= (privateEligible ? 1 : 0); ++privateArm) {
                bind();
                if (!retainSelection) need(SUCCEEDED(privateEligible ? binding.clear(shell, current)
                    : shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "Cannot clear setup selection");
                if (!privateArm) binding.reset();
                const HWND keyboard = restoreKeyboardFocus(frame);
                const uint64_t before = binding.acceleratedSelectAllCount();
                auto pending = saved;
                requireSession();
                std::unique_ptr<presentation::Observation> observation;
                if (observe && (!privateEligible || privateArm)) {
                    HWND body = nullptr; need(SUCCEEDED(shell->GetWindow(&body)), "No measured view window");
                    need(RedrawWindow(body, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW) != FALSE
                        && SUCCEEDED(DwmFlush()), "Cannot compose the initial empty selection");
                    observation = std::make_unique<presentation::Observation>(body,
                        std::wstring(output) + L"\\" + std::wstring(mode.name, mode.name + std::strlen(mode.name)));
                    observation->begin();
                }
                const double started = clockMs();
                const HRESULT restored = explorer ? restoreExplorerAll(shell, view, frame, bulk)
                    : filespacer::restoreReopenSelection(shell, pending, current, privateArm ? &binding : nullptr);
                const double duration = clockMs() - started;
                need(SUCCEEDED(restored) && (explorer || pending.empty()), "Restoration failed");
                if (observation) { observation->apiReturned(); observation->finish(); }
                CComPtr<IShellItemArray> selected;
                const double arrayStart = clockMs();
                need(SUCCEEDED(view->GetSelection(FALSE, &selected)), "Cannot materialize selection");
                const double arrayDuration = clockMs() - arrayStart;
                need(arrayPaths(selected) == wanted && arrayPaths(retained) == wanted, "Selected or retained identities differ");
                for (size_t i = 0; i < positions.size(); ++i) {
                    POINT point = {};
                    need(SUCCEEDED(view->GetItemPosition(ids[i], &point)), "Cannot inspect restored free position");
                    if (point.x != positions[i].x || point.y != positions[i].y)
                        std::fprintf(stderr, "POSITION index=%zu before=(%ld,%ld) after=(%ld,%ld) auto=%08lX\n",
                            i, positions[i].x, positions[i].y, point.x, point.y,
                            static_cast<unsigned long>(view->GetAutoArrange()));
                    need(point.x == positions[i].x && point.y == positions[i].y, "Free position changed");
                }
                int focused = -1;
                need(SUCCEEDED(view->GetFocusedItem(&focused)) && focused == 7
                    && restoreKeyboardFocus(frame) == keyboard, "Focus changed");
                const uint64_t applied = binding.acceleratedSelectAllCount() - before;
                std::fprintf(raw, "%s,%s,%s,%d,%.3f,%.3f,%zu,%llu,%s,%s\n", explorer ? "Explorer" : "FS",
                    mode.name, privateArm ? "qualified" : "public", repetition, duration, arrayDuration,
                    wanted.size(), static_cast<unsigned long long>(applied), explorer ? "native-all" : "snapshot-restore",
                    explorer || !classic ? "modern" : "classic");
                std::fflush(raw);
                std::printf("RESTORE app=%s mode=%s private=%d rep=%d ms=%.3f array_ms=%.3f native_all=%llu\n",
                    explorer ? "Explorer" : "FS", mode.name, privateArm, repetition, duration, arrayDuration,
                    static_cast<unsigned long long>(applied)); std::fflush(stdout);
            }
        }
#ifndef RESTORE_BASELINE
        if (!explorer && std::strcmp(mode.name, "medium") == 0)
            restoreSafety(shell, view, owner.events, fixture, ids, wanted, current, binding);
#endif
        binding.reset();
    }
}
}
#ifndef RESTORE_PATH_DRIVER
int wmain(int argc, wchar_t **argv) {
    if (argc == 4 && wcscmp(argv[1], L"/test") == 0 && wcscmp(argv[2], L"/selection-discovery") == 0)
        return selectionDiscovery::runWorker(argv[3]);
    if (argc == 3 && wcscmp(argv[1], L"--build-cache") == 0)
        return selectionDiscovery::runWorker(argv[2]);
    if (argc != 5) return 2;
    wchar_t machine[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine, &length) || _wcsicmp(machine, L"AGENTWIN10") != 0) return 3;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    int result = 0;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI(); headless = false;
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = testWindowProc; type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    try {
        filespacer::initializeShellSelectionClear(argv[3], false);
        const ULONGLONG resolvedDeadline = GetTickCount64() + 180000;
        while (!selectionDiscovery::resolved() && GetTickCount64() < resolvedDeadline) pump(1);
        need(selectionDiscovery::resolved(), "Selection discovery did not finish");
        std::printf("DISCOVERY supported=%d all=%d reason=%s\n", selectionDiscovery::image().supported,
            selectionDiscovery::image().selectAllSupported, selectionDiscovery::image().reason);
        std::fflush(stdout);
        need(selectionDiscovery::image().selectAllSupported, "Global selection capability unavailable");
        const bool explorer = wcscmp(argv[4], L"explorer") == 0 || wcscmp(argv[4], L"explorer-visible") == 0;
        const bool observe = wcscmp(argv[4], L"fs-visible") == 0 || wcscmp(argv[4], L"explorer-visible") == 0;
        const bool retainSelection = wcscmp(argv[4], L"fs-free-same") == 0;
        restoreMatrix(argv[1], argv[2], explorer, wcscmp(argv[4], L"fs-free") == 0 || retainSelection,
            observe, retainSelection);
    } catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName, type.hInstance); CoUninitialize(); return result;
}
#endif
