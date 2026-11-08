// Real Shell viewport capture/restore; the production helpers do not enumerate items.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"

namespace {
void scrollStates(const wchar_t *fixture) {
    struct Mode { const char *name; FOLDERVIEWMODE mode; int size; bool modern, free; };
    const Mode modes[] = {{"extra-large",FVM_ICON,256,false,false}, {"large",FVM_ICON,96,false,false},
        {"medium",FVM_ICON,48,false,false}, {"small",FVM_SMALLICON,16,false,false},
        {"list",FVM_LIST,16,true,false}, {"details",FVM_DETAILS,16,true,false},
        {"tiles",FVM_TILE,48,false,false}, {"free-icons",FVM_ICON,48,false,true},
        {"free-tiles",FVM_TILE,48,false,true}, {"content",FVM_CONTENT,32,true,false},
        {"classic-list",FVM_LIST,16,false,false}, {"classic-details",FVM_DETAILS,16,false,false}};
    for (const auto &mode : modes) {
        std::printf("START name=%s\n", mode.name); std::fflush(stdout);
        requireSession();
        RestoreHost owner;
        owner.createProfile(fixture, mode.mode, mode.modern ? FVO_DEFAULT
            : static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
        CComPtr<IShellView> shell = owner.events->shell;
        CComQIPtr<IFolderView2> view(shell);
        need(view && SUCCEEDED(view->SetViewModeAndIconSize(mode.mode, mode.size))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID, mode.free ? 0 : FWF_AUTOARRANGE)), "Cannot set viewport fixture");
        restoreEnumeration(shell, owner.events);
        presentation::fitView(shell, owner.frame, [&] {
            RECT bounds = {}; need(GetClientRect(owner.frame, &bounds) != FALSE, "No viewport bounds");
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot fit viewport");
        });
        const auto current = [&] { return !cancelled && !locked && IsWindow(owner.frame) != FALSE && owner.events->shell == shell; };
        CComHeapPtr<ITEMID_CHILD> first, focused, distant;
        need(SUCCEEDED(view->Item(0, &first)) && SUCCEEDED(view->Item(7, &focused))
            && SUCCEEDED(view->Item(4000, &distant)), "No viewport fixture identities");
        if (mode.free) {
            PCUITEMID_CHILD ids[] = {first, distant};
            POINT positions[] = {{13,17},{6000,9000}};
            need(SUCCEEDED(view->SelectAndPositionItems(2, ids, positions, SVSI_POSITIONITEM | SVSI_NOSTATECHANGE)), "Cannot seed free viewport");
        }
        need(SUCCEEDED(shell->SelectItem(focused, SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_FOCUSED | SVSI_NOTAKEFOCUS)), "Cannot seed viewport focus");
        std::vector<uint8_t> saved;
        for (int repetition = 0; repetition < 3; ++repetition) {
            need(SUCCEEDED(shell->SelectItem(distant, SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS)), "Cannot scroll far");
            double began = clockMs();
            need(filespacer::captureViewScroll(shell, current, saved) == S_OK && saved.size() == 48, "Cannot capture scrolled viewport");
            const double captureMs = clockMs() - began;
            need(SUCCEEDED(shell->SelectItem(first, SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS)), "Cannot displace viewport");
            std::vector<uint8_t> displaced;
            need(filespacer::captureViewScroll(shell, current, displaced) == S_OK && displaced != saved, "Viewport did not move before restoration");
            const HWND keyboard = restoreKeyboardFocus(owner.frame);
            began = clockMs();
            need(filespacer::restoreViewScroll(shell, saved, current) == S_OK, "Cannot restore viewport");
            const double restoreMs = clockMs() - began;
            std::vector<uint8_t> after;
            need(filespacer::captureViewScroll(shell, current, after) == S_OK && after == saved, "Viewport round trip differs");
            int selectedCount = 0, focus = -1;
            need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION, &selectedCount)) && selectedCount == 1
                && SUCCEEDED(view->GetFocusedItem(&focus)) && focus == 7
                && restoreKeyboardFocus(owner.frame) == keyboard, "Viewport restore changed selection or focus");
            DWORD state = 0;
            need(SUCCEEDED(view->GetSelectionState(focused, &state)) && (state & SVSI_SELECT), "Viewport changed the selected identity");
            std::printf("SCROLL name=%s rep=%d capture_ms=%.3f restore_ms=%.3f bytes=%zu exact=1 selected=1 focus=7\n",
                mode.name, repetition, captureMs, restoreMs, saved.size());
            std::fflush(stdout);
        }
        need(filespacer::restoreViewScroll(shell, saved, [] { return false; }) == E_ABORT, "Viewport cancellation was ignored");
        auto invalid = saved; invalid[0] = 0x7f;
        need(filespacer::restoreViewScroll(shell, invalid, current) == HRESULT_FROM_WIN32(ERROR_INVALID_DATA), "Invalid viewport version was accepted");
        invalid = saved; invalid.pop_back();
        need(filespacer::restoreViewScroll(shell, invalid, current) == HRESULT_FROM_WIN32(ERROR_INVALID_DATA), "Truncated viewport was accepted");
        std::vector<uint8_t> after;
        need(filespacer::captureViewScroll(shell, current, after) == S_OK && after == saved, "Rejected viewport changed the view");
    }
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 3) return 2;
    FILE *journal = nullptr;
    if (_wfreopen_s(&journal, argv[2], L"wb", stdout) != 0) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0);
    wchar_t machine[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine, &length) || _wcsicmp(machine, L"AGENTWIN10") != 0) return 3;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI(); headless = false;
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = testWindowProc; type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    int result = 0;
    try { scrollStates(argv[1]); }
    catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName, type.hInstance); CoUninitialize(); return result;
}
