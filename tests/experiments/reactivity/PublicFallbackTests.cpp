// Real public-only restoration/clear and the integrated FS selection reader.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"
#include "MixedFolder.h"

namespace {
void publicFallback(const wchar_t *output, const wchar_t *fixture) {
    struct Mode { const char *name; FOLDERVIEWMODE mode; int size; bool free; };
    const Mode modes[] = {{"extra-large",FVM_ICON,256,false},{"large",FVM_ICON,96,false},
        {"medium",FVM_ICON,48,false},{"small",FVM_SMALLICON,16,false},
        {"list",FVM_LIST,16,false},{"details",FVM_DETAILS,16,false},{"tiles",FVM_TILE,48,false},
        {"free-icons",FVM_ICON,48,true},{"free-tiles",FVM_TILE,48,true}};
    FILE *file = nullptr;
    need(_wfopen_s(&file, (std::wstring(output) + L"\\public-fallback.csv").c_str(), L"wb") == 0, "No fallback journal");
    std::unique_ptr<FILE, decltype(&std::fclose)> journal(file, &std::fclose);
    std::fputs("app,view,rep,clear_ms,restore_ms,first_array_ms,restore_and_read_ms,private_calls\n", file);
    for (const bool explorer : {false,true}) {
        RestoreExplorer native;
        if (explorer) native.open(fixture);
        for (const auto &mode : modes) {
            if (explorer && mode.free) continue;
            requireSession();
            RestoreHost owner;
            const bool modern = mode.mode == FVM_LIST || mode.mode == FVM_DETAILS;
            if (!explorer) owner.createProfile(fixture, mode.mode, modern ? FVO_DEFAULT
                : static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
            CComPtr<IShellView> shell = explorer ? native.shell : owner.events->shell;
            CComQIPtr<IFolderView2> view(shell); CComQIPtr<IShellFolderView> bulk(shell);
            const HWND frame = explorer ? native.frame : owner.frame;
            const auto current = [&] { return !cancelled && !locked && IsWindow(frame) != FALSE
                && (explorer || owner.events->shell == shell); };
            need(view && SUCCEEDED(view->SetViewModeAndIconSize(mode.mode, mode.size)), "Cannot set fallback mode");
            if (!explorer) need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID,
                mode.free ? 0 : FWF_AUTOARRANGE)), "Cannot set fallback arrangement");
            restoreEnumeration(shell, explorer ? native.events.p : owner.events.p);
            if (explorer) presentation::fitView(shell, frame);
            else presentation::fitView(shell, frame, [&] {
                RECT bounds = {}; need(GetClientRect(frame, &bounds) != FALSE, "No fallback bounds");
                need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot fit fallback view");
            });
            restoreExpose(frame);
            HWND body = nullptr; need(SUCCEEDED(shell->GetWindow(&body)), "No fallback view window");
            int count = 0; need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW, &count)) && count == 5000, "Incomplete fallback fixture");
            std::vector<OwnedId> owned; std::vector<PCUITEMID_CHILD> ids;
            for (int i = 0; i < count; ++i) {
                CComHeapPtr<ITEMID_CHILD> id;
                need(SUCCEEDED(view->Item(i, &id)) && id, "No fallback identity");
                owned.emplace_back(id.Detach(), &CoTaskMemFree); ids.push_back(owned.back().get());
            }
            if (mode.free) {
                PCUITEMID_CHILD moved[] = {ids[0],ids[4000]}; POINT positions[] = {{13,17},{6000,9000}};
                need(SUCCEEDED(view->SelectAndPositionItems(2, moved, positions, SVSI_POSITIONITEM | SVSI_NOSTATECHANGE)), "Cannot seed free fallback positions");
            }
            std::vector<POINT> positions;
            if (mode.free) for (const auto id : ids) {
                POINT point = {}; need(SUCCEEDED(view->GetItemPosition(id, &point)), "No original free position"); positions.push_back(point);
            }
            // Never bind or initialize the private adapter in this executable.
            filespacer::ShellSelectionClear publicClear;
            need(SUCCEEDED(restoreExplorerAll(shell, view, frame, bulk))
                && SUCCEEDED(shell->SelectItem(ids[7], SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS)), "Cannot seed fallback snapshot");
            std::vector<uint8_t> saved;
            if (!explorer) need(SUCCEEDED(filespacer::captureReopenSelection(shell, current, saved)), "No fallback snapshot");
            CComPtr<IShellItemArray> retained;
            need(view->GetSelection(FALSE, &retained) == S_OK && retained, "No original fallback array");
            const auto wanted = arrayPaths(retained);
            const std::wstring app = explorer ? L"explorer" : L"fs";
            const std::wstring label(mode.name, mode.name + std::strlen(mode.name));
            for (int repetition = 0; repetition < 3; ++repetition) {
                const HWND keyboard = restoreKeyboardFocus(frame);
                const auto observe = [&](const wchar_t *operation) {
                    std::unique_ptr<presentation::Observation> observation;
                    if (!repetition) {
                        need(RedrawWindow(body,nullptr,nullptr,RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)
                            && SUCCEEDED(DwmFlush()), "Cannot compose fallback baseline");
                        observation = std::make_unique<presentation::Observation>(body,
                            std::wstring(output) + L"\\" + app + L"-" + label + L"-" + operation);
                        observation->begin();
                    }
                    return observation;
                };
                auto clearing = observe(L"clear");
                double began = clockMs();
                need(SUCCEEDED(publicClear.clear(shell, current)), "Public clear failed");
                const double clearMs = clockMs() - began;
                if (clearing) { clearing->apiReturned(); clearing->finish(); }
                int selected = -1;
                need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION, &selected)) && selected == 0, "Public clear did not empty the selection");
                auto pending = saved;
                auto restoring = observe(L"restore-read");
                began = clockMs();
                need(SUCCEEDED(explorer ? restoreExplorerAll(shell, view, frame, bulk)
                    : filespacer::restoreReopenSelection(shell, pending, current)), "Public restore failed");
                const double restoreMs = clockMs() - began;
                CComPtr<IShellItemArray> selectedArray;
                const double arrayStart = clockMs();
                need((explorer ? view->GetSelection(FALSE, &selectedArray)
                    : filespacer::nativeViewSelection(view, FALSE, &selectedArray, current)) == S_OK && selectedArray, "Cannot read restored selection");
                const double arrayMs = clockMs() - arrayStart;
                const double combinedMs = clockMs() - began;
                if (restoring) { restoring->apiReturned(); restoring->finish(); }
                CComPtr<IShellItemArray> truth;
                need(view->GetSelection(FALSE, &truth) == S_OK && truth && arrayPaths(truth) == wanted
                    && arrayPaths(selectedArray) == wanted && arrayPaths(retained) == wanted, "Fallback selected identities or retained arrays differ");
                for (size_t i = 0; i < positions.size(); ++i) {
                    POINT point = {}; need(SUCCEEDED(view->GetItemPosition(ids[i], &point))
                        && point.x == positions[i].x && point.y == positions[i].y, "Fallback changed a free position");
                }
                int focused = -1;
                need(SUCCEEDED(view->GetFocusedItem(&focused)) && focused == 7
                    && restoreKeyboardFocus(frame) == keyboard, "Fallback changed focus");
                need(!publicClear.accelerated() && !publicClear.acceleratesSelectAll()
                    && publicClear.acceleratedSelectAllCount() == 0, "Private selection unexpectedly applied");
                std::fprintf(file,"%s,%s,%d,%.3f,%.3f,%.3f,%.3f,0\n",explorer ? "Explorer" : "FS",mode.name,repetition,clearMs,restoreMs,arrayMs,combinedMs);
                std::fflush(file);
                std::printf("PUBLIC app=%s mode=%s rep=%d clear_ms=%.3f restore_ms=%.3f array_ms=%.3f combined_ms=%.3f identities=1 focus=1 positions=1 private=0\n",
                    explorer ? "Explorer" : "FS",mode.name,repetition,clearMs,restoreMs,arrayMs,combinedMs);
                std::fflush(stdout);
            }
        }
    }
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 3) return 2;
    wchar_t machine[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine, &length) || _wcsicmp(machine,L"AGENTWIN10") != 0) return 3;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI(); headless = false;
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = testWindowProc; type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    int result = 0;
    try { publicFallback(argv[1],argv[2]); }
    catch (const std::exception &error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName,type.hInstance); CoUninitialize(); return result;
}
