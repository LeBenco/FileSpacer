// Investigation only: public replacement for a complete free-layout selection array.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"
#include "MixedFolder.h"

namespace {
ULONGLONG selectionSize(IShellItemArray *array) {
    DWORD count = 0;
    need(SUCCEEDED(array->GetCount(&count)), "No status selection count");
    ULONGLONG bytes = 0;
    for (DWORD i = 0; i < count; ++i) {
        CComPtr<IShellItem> item; ULONGLONG size = 0;
        need(SUCCEEDED(array->GetItemAt(i, &item)), "Missing status item");
        CComQIPtr<IShellItem2> properties(item);
        need(properties && SUCCEEDED(properties->GetUInt64(PKEY_Size, &size)), "Missing status size");
        bytes += size;
    }
    return bytes;
}
std::vector<std::wstring> orderedArrayPaths(IShellItemArray *array) {
    DWORD count = 0;
    need(array && SUCCEEDED(array->GetCount(&count)), "No array count");
    std::vector<std::wstring> paths;
    for (DWORD i = 0; i < count; ++i) {
        CComPtr<IShellItem> item;
        need(SUCCEEDED(array->GetItemAt(i, &item)), "Missing array item");
        paths.push_back(itemPath(item));
    }
    return paths;
}
void compareArrays(IShellItemArray *candidate, IShellItemArray *native) {
    need(orderedArrayPaths(candidate) == orderedArrayPaths(native), "Candidate differs from native ordered identities");
    SFGAOF attributes = 0, expected = 0;
    const SFGAOF mask = SFGAO_FILESYSTEM | SFGAO_CANCOPY | SFGAO_CANMOVE | SFGAO_CANDELETE;
    need(SUCCEEDED(candidate->GetAttributes(SIATTRIBFLAGS_AND, mask, &attributes))
        && SUCCEEDED(native->GetAttributes(SIATTRIBFLAGS_AND, mask, &expected)) && attributes == expected,
        "Candidate attributes differ");
    CComPtr<IDataObject> data;
    CComPtr<IShellItemArray> roundtrip;
    need(SUCCEEDED(candidate->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data))) && data
        && SUCCEEDED(SHCreateShellItemArrayFromDataObject(data, IID_PPV_ARGS(&roundtrip)))
        && orderedArrayPaths(roundtrip) == orderedArrayPaths(native), "Candidate data-object round trip differs");
}
void selectionArrays(const wchar_t *fixture) {
    struct Mode { const char *name; FOLDERVIEWMODE mode; int size; };
    const Mode modes[] = {{"medium", FVM_ICON, 48}, {"small", FVM_SMALLICON, 16}, {"large", FVM_ICON, 96}};
    for (const auto &mode : modes) {
        requireSession();
        RestoreHost owner;
        owner.createProfile(fixture, mode.mode, static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
        CComPtr<IShellView> shell = owner.events->shell;
        CComQIPtr<IFolderView2> view(shell);
        CComQIPtr<IShellFolderView> bulk(shell);
        const auto current = [&] { return !cancelled && !locked && IsWindow(owner.frame) != FALSE && owner.events->shell == shell; };
        need(view && SUCCEEDED(view->SetViewModeAndIconSize(mode.mode, mode.size))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID, 0)), "Cannot set free array profile");
        restoreEnumeration(shell, owner.events);
        presentation::fitView(shell, owner.frame, [&] {
            RECT bounds = {}; need(GetClientRect(owner.frame, &bounds) != FALSE, "No array fixture bounds");
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot size array fixture");
        });
        restoreExpose(owner.frame);
        filespacer::ShellSelectionClear binding;
        binding.bind(shell, current);
        need(binding.acceleratesSelectAll(), "No qualified fixture selection path");
        int count = 0;
        need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW, &count)) && count == fixtureCount, "Incomplete array fixture");
        std::vector<OwnedId> owned;
        std::vector<PCUITEMID_CHILD> ids;
        for (int i = 0; i < count; ++i) {
            CComHeapPtr<ITEMID_CHILD> id;
            need(SUCCEEDED(view->Item(i, &id)) && id, "No fixture item");
            owned.emplace_back(id.Detach(), &CoTaskMemFree); ids.push_back(owned.back().get());
        }
        need(SUCCEEDED(filespacer::restoreIconPositions(view, restoreFreeFixture(ids), current)), "Cannot seed free geometry");
        PCUITEMID_CHILD irregular[] = {ids[0], ids[17]}; POINT adjusted[] = {{1900, 1800}, {5, 5}};
        need(SUCCEEDED(view->SelectAndPositionItems(2, irregular, adjusted, SVSI_POSITIONITEM | SVSI_NOSTATECHANGE)), "Cannot seed irregular geometry");
        std::vector<POINT> positions;
        for (const auto id : ids) { POINT point = {}; need(SUCCEEDED(view->GetItemPosition(id, &point)), "No original position"); positions.push_back(point); }
        const auto selectAll = [&](int focus) {
            need(SUCCEEDED(binding.clear(shell, current)) && SUCCEEDED(restoreExplorerAll(shell, view, owner.frame, bulk))
                && SUCCEEDED(shell->SelectItem(ids[static_cast<size_t>(focus)], SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS)), "Cannot prepare complete selection");
        };
        for (const int focus : {0, 7, 4999}) for (int repetition = 0; repetition < 3; ++repetition) {
            CComPtr<IShellItemArray> native, candidate;
            double nativeMs = 0, candidateMs = 0;
            const HWND keyboard = restoreKeyboardFocus(owner.frame);
            for (int arm = 0; arm < 2; ++arm) {
                selectAll(focus);
                const bool reference = (arm == 0) == (repetition % 2 == 0);
                const double began = clockMs();
                if (reference) {
                    need(filespacer::nativeViewSelection(view, FALSE, &native) == S_OK && native, "No first native array");
                    nativeMs = clockMs() - began;
                } else {
                    need(filespacer::nativeViewSelection(view, FALSE, &candidate, current) == S_OK && candidate, "Integrated candidate failed");
                    candidateMs = clockMs() - began;
                }
                need(selectionSize(reference ? native.p : candidate.p) == 318120, "Status byte total differs");
                std::printf("STATUS mode=%s focus=%d rep=%d arm=%s total_ms=%.3f bytes=318120\n",
                    mode.name, focus, repetition, reference ? "native" : "integrated", clockMs() - began);
            }
            CComPtr<IShellItemArray> qualified;
            need(filespacer::captureSpatialSelection(view, current, &qualified) == S_OK && qualified,
                "Complete selection did not qualify for the integrated fast path");
            compareArrays(candidate, native);
            const auto wanted = orderedArrayPaths(native);
            int actualFocus = -1;
            need(SUCCEEDED(view->GetFocusedItem(&actualFocus)) && actualFocus == focus
                && restoreKeyboardFocus(owner.frame) == keyboard, "Array capture changed focus");
            need(SUCCEEDED(binding.clear(shell, current)) && orderedArrayPaths(candidate) == wanted
                && orderedArrayPaths(native) == wanted, "Clearing changed a retained array");
            for (size_t i = 0; i < ids.size(); ++i) { POINT point = {}; need(SUCCEEDED(view->GetItemPosition(ids[i], &point))
                && point.x == positions[i].x && point.y == positions[i].y, "Array capture changed a free position"); }
            std::printf("ARRAY mode=%s focus=%d rep=%d native_ms=%.3f candidate_ms=%.3f exact_order=1 data_object=1 retained=1 positions=1\n",
                mode.name, focus, repetition, nativeMs, candidateMs);
            std::fflush(stdout);
        }
        const auto reject = [&](const char *name) {
            CComPtr<IShellItemArray> before, after, rejected;
            const HRESULT observed = view->GetSelection(FALSE, &before);
            const double began = clockMs();
            need(filespacer::captureSpatialSelection(view, current, &rejected) == S_FALSE && !rejected, "Unsupported case did not retain native access");
            const double elapsed = clockMs() - began;
            CComPtr<IShellItemArray> integrated;
            need(filespacer::nativeViewSelection(view, FALSE, &integrated, current) == observed
                && static_cast<bool>(before) == static_cast<bool>(integrated)
                && (!before || orderedArrayPaths(before) == orderedArrayPaths(integrated)),
                "Integrated fallback differs from the native result");
            need(view->GetSelection(FALSE, &after) == observed && static_cast<bool>(before) == static_cast<bool>(after)
                && (!before || orderedArrayPaths(before) == orderedArrayPaths(after)), "Capability rejection changed selection");
            std::printf("FALLBACK mode=%s case=%s check_ms=%.3f unchanged=1\n", mode.name, name, elapsed);
        };
        reject("empty");
        CComPtr<IShellItemArray> nativeFolder, integratedFolder;
        need(view->GetSelection(TRUE, &nativeFolder) == S_OK && nativeFolder
            && filespacer::nativeViewSelection(view, TRUE, &integratedFolder, current) == S_OK && integratedFolder
            && orderedArrayPaths(nativeFolder) == orderedArrayPaths(integratedFolder), "Empty-selection folder fallback changed");
        need(SUCCEEDED(shell->SelectItem(ids[1], SVSI_SELECT | SVSI_NOTAKEFOCUS))
            && SUCCEEDED(shell->SelectItem(ids[300], SVSI_SELECT | SVSI_NOTAKEFOCUS)), "Cannot prepare partial selection");
        reject("partial-unselected-focus");
        selectAll(7);
        CComPtr<IShellItemArray> aborted;
        unsigned calls = 0;
        need(filespacer::captureSpatialSelection(view, [&] { return current() && ++calls < 64; }, &aborted) == E_ABORT && !aborted,
            "Cancellation did not reject the candidate");
        calls = 0;
        need(filespacer::nativeViewSelection(view, FALSE, &aborted, [&] { return current() && ++calls < 64; }) == E_ABORT && !aborted,
            "Integrated cancellation fell back to a stale view");
        int selected = 0;
        need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION, &selected)) && selected == count, "Cancellation changed selection");
        POINT duplicate = positions[0]; PCUITEMID_CHILD duplicateId = ids[1];
        need(SUCCEEDED(view->SelectAndPositionItems(1, &duplicateId, &duplicate, SVSI_POSITIONITEM | SVSI_NOSTATECHANGE)), "Cannot seed tied coordinates");
        reject("tied-coordinates");
        need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE, FWF_AUTOARRANGE)), "Cannot prepare arranged view");
        restoreEnumeration(shell, owner.events);
        reject("arranged");
        need(SUCCEEDED(view->SetGroupBy(PKEY_ItemTypeText, TRUE)), "Cannot prepare grouped view");
        restoreEnumeration(shell, owner.events);
        reject("grouped");
        std::printf("SAFETY mode=%s cancellation=1\n", mode.name);
    }
    // Check whether the slow public free-Tiles clear also affects the existing binding.
    {
        RestoreHost owner;
        owner.createProfile(fixture, FVM_TILE, static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
        CComPtr<IShellView> shell = owner.events->shell;
        CComQIPtr<IFolderView2> view(shell); CComQIPtr<IShellFolderView> bulk(shell);
        const auto current = [&] { return !cancelled && !locked && IsWindow(owner.frame) != FALSE && owner.events->shell == shell; };
        need(view && SUCCEEDED(view->SetViewModeAndIconSize(FVM_TILE,48))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID,0)), "Cannot set free Tiles control");
        restoreEnumeration(shell,owner.events);
        presentation::fitView(shell,owner.frame,[&] {
            RECT bounds = {}; need(GetClientRect(owner.frame,&bounds) != FALSE, "No Tiles control bounds");
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot fit Tiles control");
        });
        restoreExpose(owner.frame);
        std::vector<OwnedId> owned; std::vector<PCUITEMID_CHILD> ids; std::vector<POINT> positions;
        for (int i = 0; i < 5000; ++i) {
            CComHeapPtr<ITEMID_CHILD> id; need(SUCCEEDED(view->Item(i,&id)) && id, "No Tiles control identity");
            owned.emplace_back(id.Detach(),&CoTaskMemFree); ids.push_back(owned.back().get());
        }
        PCUITEMID_CHILD moved[] = {ids[0],ids[4000]}; POINT adjusted[] = {{13,17},{6000,9000}};
        need(SUCCEEDED(view->SelectAndPositionItems(2,moved,adjusted,SVSI_POSITIONITEM | SVSI_NOSTATECHANGE)), "Cannot seed Tiles control geometry");
        for (const auto id : ids) { POINT point = {}; need(SUCCEEDED(view->GetItemPosition(id,&point)), "No Tiles control position"); positions.push_back(point); }
        filespacer::ShellSelectionClear binding; binding.bind(shell,current);
        need(binding.accelerated() && binding.acceleratesSelectAll(), "No existing free Tiles capability");
        for (int repetition = 0; repetition < 3; ++repetition) for (const bool privateClear : {false,true}) for (const bool redraw : {false,true}) {
            need(SUCCEEDED(restoreExplorerAll(shell,view,owner.frame,bulk))
                && SUCCEEDED(shell->SelectItem(ids[7],SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS)), "Cannot seed Tiles control selection");
            CComPtr<IShellItemArray> retained;
            need(view->GetSelection(FALSE,&retained) == S_OK && retained, "No retained Tiles selection");
            const auto wanted = arrayPaths(retained); const HWND keyboard = restoreKeyboardFocus(owner.frame);
            HWND body = nullptr;
            need(SUCCEEDED(shell->GetWindow(&body))
                && RedrawWindow(body,nullptr,nullptr,RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)
                && SUCCEEDED(DwmFlush()), "Cannot compose the selected Tiles baseline");
            bool applied = false; const double began = clockMs();
            if (redraw) need(SUCCEEDED(view->SetRedraw(FALSE)), "Cannot suspend Tiles drawing");
            filespacer::ShellSelectionClear publicClear;
            need(SUCCEEDED(privateClear ? binding.clear(shell,current,&applied) : publicClear.clear(shell,current,&applied))
                && applied == privateClear, "Requested Tiles clear path was not applied");
            if (redraw) {
                need(SUCCEEDED(view->SetRedraw(TRUE)), "Cannot resume Tiles drawing");
                need(RedrawWindow(body,nullptr,nullptr,RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)
                    && SUCCEEDED(DwmFlush()), "Cannot compose resumed Tiles control");
            }
            const double elapsed = clockMs() - began;
            int count = -1,focus = -1;
            need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count)) && count == 0
                && SUCCEEDED(view->GetFocusedItem(&focus)) && focus == 7
                && restoreKeyboardFocus(owner.frame) == keyboard && arrayPaths(retained) == wanted, "Qualified Tiles clear changed focus or retained identities");
            for (size_t i = 0; i < ids.size(); ++i) { POINT point = {}; need(SUCCEEDED(view->GetItemPosition(ids[i],&point))
                && point.x == positions[i].x && point.y == positions[i].y, "Qualified Tiles clear changed geometry"); }
            std::printf("QUALIFIED mode=free-tiles rep=%d private_clear=%d redraw=%d clear_ms=%.3f applied=%d identities=1 positions=1 focus=1\n",repetition,privateClear,redraw,elapsed,applied);
        }
    }
    // Explorer has no free folder layout: its arranged view is an API-cost control.
    RestoreExplorer explorer; explorer.open(fixture);
    CComQIPtr<IFolderView2> view(explorer.shell);
    CComQIPtr<IShellFolderView> bulk(explorer.shell);
    need(view && SUCCEEDED(view->SetViewModeAndIconSize(FVM_ICON, 48)), "Cannot set Explorer array control");
    restoreEnumeration(explorer.shell, explorer.events);
    presentation::fitView(explorer.shell, explorer.frame); restoreExpose(explorer.frame);
    for (int repetition = 0; repetition < 3; ++repetition) {
        need(SUCCEEDED(explorer.shell->SelectItem(nullptr, SVSI_DESELECTOTHERS))
            && SUCCEEDED(restoreExplorerAll(explorer.shell, view, explorer.frame, bulk)), "Cannot seed Explorer array control");
        CComPtr<IShellItemArray> native;
        const double began = clockMs();
        need(view->GetSelection(FALSE, &native) == S_OK && native, "No Explorer first array");
        const double elapsed = clockMs() - began;
        DWORD count = 0; need(SUCCEEDED(native->GetCount(&count)) && count == 5000, "Incomplete Explorer array control");
        std::printf("EXPLORER rep=%d first_array_ms=%.3f count=%lu\n", repetition, elapsed, static_cast<unsigned long>(count));
    }
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc == 3 && wcscmp(argv[1], L"--build-cache") == 0) return selectionDiscovery::runWorker(argv[2]);
    if (argc != 4) return 2;
    wchar_t machine[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine, &length) || _wcsicmp(machine, L"AGENTWIN10") != 0) return 3;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI(); headless = false;
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = testWindowProc; type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    int result = 0;
    try {
        filespacer::initializeShellSelectionClear(argv[3], false);
        const ULONGLONG deadline = GetTickCount64() + 180000;
        while (!selectionDiscovery::resolved() && GetTickCount64() < deadline) pump(1);
        need(selectionDiscovery::resolved() && selectionDiscovery::image().selectAllSupported, "No fixture selection backend");
        selectionArrays(argv[2]);
    } catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName, type.hInstance); CoUninitialize(); return result;
}
