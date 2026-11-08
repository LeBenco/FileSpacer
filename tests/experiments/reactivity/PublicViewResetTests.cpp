// Independent public-API prototype; never used for manually arranged folders.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"
#include "ShellViewSettings.h"
#include <propkey.h>

namespace {
std::vector<std::vector<BYTE>> orderedIds(IFolderView2 *view) {
    CComPtr<IEnumIDList> items;
    need(SUCCEEDED(view->Items(SVGIO_ALLVIEW | SVGIO_FLAG_VIEWORDER, IID_PPV_ARGS(&items))), "No view-order enumeration");
    std::vector<std::vector<BYTE>> result;
    for (;;) {
        CComHeapPtr<ITEMID_CHILD> child; ULONG fetched = 0;
        need(SUCCEEDED(items->Next(1, &child, &fetched)), "Cannot enumerate view order");
        if (!fetched) return result;
        const BYTE *bytes = reinterpret_cast<const BYTE *>(child.m_pData);
        result.emplace_back(bytes, bytes + ILGetSize(child));
    }
}
void composeView(IShellView *shell) {
    HWND window = nullptr;
    need(SUCCEEDED(shell->GetWindow(&window)) && window
        && RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)
        && SUCCEEDED(DwmFlush()), "Cannot compose final public view");
}
void publicReset(const wchar_t *output, const wchar_t *fixture, const wchar_t *only, int repetitions) {
    FILE *raw = nullptr;
    raw = _wfsopen((std::wstring(output) + L"\\public-reset.csv").c_str(), L"w", _SH_DENYNO);
    need(raw != nullptr, "No reset output");
    std::unique_ptr<FILE, decltype(&fclose)> file(raw, &fclose);
    std::fprintf(raw, "view,arm,rep,workflow_ms,capture_ms,destroy_ms,create_ms,restore_ms,paint_ms,correct\n");
    std::fflush(raw);
    bool allCorrect = true;
    for (const auto mode : {FVM_ICON, FVM_TILE}) for (int rep = 0; rep < repetitions; ++rep) for (int arm = 0; arm < 2; ++arm) {
        if ((wcscmp(only,L"medium") == 0 && mode != FVM_ICON) || (wcscmp(only,L"tiles") == 0 && mode != FVM_TILE)) continue;
        RestoreHost owner;
        owner.createProfile(fixture, mode, static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
        CComPtr<IShellView> shell(owner.events->shell);
        CComQIPtr<IFolderView2> view(shell);
        CComQIPtr<IShellFolderView> bulk(shell);
        need(view && bulk && SUCCEEDED(view->SetViewModeAndIconSize(mode, 48)), "No classic public view");
        const SORTCOLUMN sort = {PKEY_ItemNameDisplay, rep % 2 ? SORT_DESCENDING : SORT_ASCENDING};
        const PROPERTYKEY noGroup = {};
        need(SUCCEEDED(view->SetSortColumns(&sort, 1)) && SUCCEEDED(view->SetGroupBy(noGroup, TRUE))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID, FWF_AUTOARRANGE)), "Cannot set explicit sorted arrangement");
        restoreEnumeration(shell, owner.events);
        presentation::fitView(shell, owner.frame, [&] {
            RECT bounds = {}; need(GetClientRect(owner.frame, &bounds) != FALSE, "No frame bounds");
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot fit public view");
        });
        restoreExpose(owner.frame);
        need(SUCCEEDED(shell->UIActivate(SVUIA_ACTIVATE_FOCUS)), "Cannot focus prepared public view");
        const auto current = [&] { return !cancelled && !locked && IsWindow(owner.frame); };
        need(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS))
            && SUCCEEDED(view->SelectItem(2507, SVSI_SELECT | SVSI_FOCUSED | SVSI_SELECTIONMARK
                | SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS)), "Cannot prepare full selection and scrolled focus");
        pump(100);
        const auto order = orderedIds(view);
        need(order.size() == static_cast<size_t>(fixtureCount), "Incomplete initial order");
        CComPtr<IShellItemArray> retained;
        need(SUCCEEDED(view->GetSelection(FALSE, &retained)) && retained, "Cannot retain original selection");
        const auto retainedNames = arrayPaths(retained);
        // Finish setup's queued activation before taking the initial focus
        // snapshot. A freshly activated top-level frame can have no focus yet.
        need(SUCCEEDED(shell->UIActivate(SVUIA_ACTIVATE_FOCUS)), "Cannot focus settled public fixture");
        const double start = clockMs();
        const auto options = filespacer::captureOptions(view, {});
        need((options.present & (filespacer::FolderViewSettings::Mode | filespacer::FolderViewSettings::Flags
            | filespacer::FolderViewSettings::Sort | filespacer::FolderViewSettings::Group))
            == (filespacer::FolderViewSettings::Mode | filespacer::FolderViewSettings::Flags
                | filespacer::FolderViewSettings::Sort | filespacer::FolderViewSettings::Group), "Incomplete public presentation snapshot");
        int focusedIndex = -1, markedIndex = -1; CComHeapPtr<ITEMID_CHILD> focused, marked;
        need(SUCCEEDED(view->GetFocusedItem(&focusedIndex)) && focusedIndex >= 0
            && SUCCEEDED(view->Item(focusedIndex, &focused)), "Cannot capture logical focus");
        need(SUCCEEDED(view->GetSelectionMarkedItem(&markedIndex)) && markedIndex >= 0
            && SUCCEEDED(view->Item(markedIndex, &marked)), "Cannot capture selection anchor");
        wchar_t focusClass[128] = {};
        need(GetClassNameW(restoreKeyboardFocus(owner.frame), focusClass, _countof(focusClass)) != 0, "Cannot capture focus role");
        std::vector<uint8_t> viewport;
        need(filespacer::captureViewScroll(shell, current, viewport) == S_OK && !viewport.empty(), "Cannot capture exact public viewport");
        const double captured = clockMs();
        double destroyed = captured, created = captured;
        if (!arm) {
            need(SUCCEEDED(shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "Public clear failed");
        } else {
            owner.events->detach(); bulk.Release(); view.Release(); shell.Release();
            need(SUCCEEDED(owner.host.destroy()), "Cannot destroy old public browser");
            owner.events.Release();
            destroyed = clockMs();
            owner.events.Attach(new ViewEvents);
            RECT bounds = {}; need(GetClientRect(owner.frame, &bounds) != FALSE, "Cannot read recreation bounds");
            const FOLDERSETTINGS settings = {static_cast<UINT>(mode), FWF_AUTOARRANGE | FWF_NOWEBVIEW | FWF_NOHEADERINALLVIEWS};
            need(SUCCEEDED(owner.host.initialize(owner.frame, bounds, settings, nullptr, owner.events,
                static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING))), "Cannot recreate public browser");
            CComPtr<IShellItem> folder;
            need(SUCCEEDED(SHCreateItemFromParsingName(fixture, nullptr, IID_PPV_ARGS(&folder)))
                && SUCCEEDED(owner.host.navigate(folder, false, true)), "Cannot navigate recreated view");
            const ULONGLONG deadline = GetTickCount64() + 30000;
            while ((!owner.events->navigated || !owner.events->sink || !owner.events->sink->ready)
                && GetTickCount64() < deadline) pump(1);
            need(owner.events->navigated && owner.events->sink && owner.events->sink->ready, "Recreated enumeration did not finish");
            shell = owner.events->shell; view = shell; bulk = shell;
            need(view && bulk, "Recreated view interfaces missing");
            created = clockMs();
            need(SUCCEEDED(filespacer::restoreOptions(view, options)), "Cannot restore public presentation");
            restoreEnumeration(shell, owner.events);
            need(SUCCEEDED(shell->SelectItem(marked, SVSI_SELECTIONMARK | SVSI_NOTAKEFOCUS))
                && SUCCEEDED(shell->SelectItem(focused, SVSI_FOCUSED | SVSI_NOTAKEFOCUS))
                && filespacer::restoreViewScroll(shell, viewport, current) == S_OK
                && SUCCEEDED(shell->UIActivate(SVUIA_ACTIVATE_FOCUS)), "Cannot restore focus and viewport");
        }
        const double restored = clockMs();
        composeView(shell);
        const double finish = clockMs();
        auto verify = [&] {
            int count = -1, focus = -1, anchor = -1; CComHeapPtr<ITEMID_CHILD> actualFocus, actualAnchor;
            std::vector<uint8_t> actualViewport; HWND body = nullptr;
            wchar_t actualClass[128] = {};
            const bool empty = SUCCEEDED(view->ItemCount(SVGIO_SELECTION, &count)) && count == 0;
            const bool ordered = orderedIds(view) == order;
            const bool configured = filespacer::sameView(filespacer::captureOptions(view, {}), options);
            const bool focusedItem = SUCCEEDED(view->GetFocusedItem(&focus)) && focus >= 0
                && SUCCEEDED(view->Item(focus, &actualFocus)) && ILIsEqual(focused, actualFocus);
            const bool anchored = SUCCEEDED(view->GetSelectionMarkedItem(&anchor)) && anchor >= 0
                && SUCCEEDED(view->Item(anchor, &actualAnchor)) && ILIsEqual(marked, actualAnchor);
            const bool scrolled = filespacer::captureViewScroll(shell, current, actualViewport) == S_OK && actualViewport == viewport;
            const bool keyboard = GetClassNameW(restoreKeyboardFocus(owner.frame), actualClass, _countof(actualClass)) != 0
                && wcscmp(focusClass, actualClass) == 0 && SUCCEEDED(shell->GetWindow(&body))
                && (restoreKeyboardFocus(owner.frame) == body || IsChild(body, restoreKeyboardFocus(owner.frame)));
            const bool retainedArray = arrayPaths(retained) == retainedNames;
            std::printf("RESET CHECK mode=%d arm=%d empty=%d order=%d settings=%d focus=%d anchor=%d scroll=%d keyboard=%d array=%d before_focus=%d after_focus=%d before_anchor=%d after_anchor=%d\n",
                mode, arm, empty, ordered, configured, focusedItem, anchored, scrolled, keyboard, retainedArray,
                focusedIndex, focus, markedIndex, anchor); std::fflush(stdout);
            return empty && ordered && configured && focusedItem && anchored && scrolled && keyboard && retainedArray;
        };
        const bool initialCorrect = verify(); pump(100); const bool stableCorrect = verify();
        const bool correct = initialCorrect && stableCorrect; allCorrect = allCorrect && correct;
        std::fprintf(raw, "%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n", mode == FVM_TILE ? "tiles" : "medium",
            arm ? "recreate" : "clear", rep, finish-start, captured-start, destroyed-captured, created-destroyed, restored-created, finish-restored, correct);
        std::fflush(raw);
        std::printf("RESET %s view=%d arm=%d rep=%d workflow_ms=%.3f\n", correct ? "PASS" : "FAIL", mode, arm, rep, finish-start); std::fflush(stdout);
    }
    need(allCorrect, "At least one public workflow lost a presentation invariant; no integration permitted");
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 5 || (wcscmp(argv[3], L"all") != 0 && wcscmp(argv[3], L"medium") != 0 && wcscmp(argv[3], L"tiles") != 0)
        || _wtoi(argv[4]) < 1 || _wtoi(argv[4]) > 3 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI();
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = testWindowProc; type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    int result = 0;
    try { publicReset(argv[1], argv[2], argv[3], _wtoi(argv[4])); }
    catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName, type.hInstance); CoUninitialize(); return result;
}
