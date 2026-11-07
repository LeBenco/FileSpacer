// Investigation only: shared native fixtures, current product operations.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"

namespace {
enum class LayoutScenario { Matrix, Free };
BOOL CALLBACK logLayoutWindow(HWND window, LPARAM) {
    wchar_t name[128] = {};
    GetClassNameW(window, name, _countof(name));
    std::printf("WINDOW class=%ls\n", name);
    return TRUE;
}
std::vector<std::wstring> orderedPaths(IShellItemArray *array) {
    DWORD count = 0;
    need(array && SUCCEEDED(array->GetCount(&count)), "No ordered array");
    std::vector<std::wstring> result;
    for (DWORD i = 0; i < count; ++i) {
        CComPtr<IShellItem> item;
        need(SUCCEEDED(array->GetItemAt(i, &item)), "Short ordered array");
        result.push_back(itemPath(item));
    }
    return result;
}
void layoutPaths(const wchar_t *output, const wchar_t *fixture, LayoutScenario scenario) {
    presentation::PointerParking parking;
    presentation::pointer = &parking;
    struct Reset { ~Reset() { presentation::pointer = nullptr; } } reset;
    struct Mode { const char *name; FOLDERVIEWMODE mode; bool modern, explorer, free; };
    const Mode modes[] = {{"classic-content", FVM_CONTENT, false, false, false},
        {"modern-content", FVM_CONTENT, true, false, false},
        {"explorer-content", FVM_CONTENT, true, true, false},
        {"classic-tiles", FVM_TILE, false, false, false},
        {"modern-tiles", FVM_TILE, true, false, false},
        {"explorer-tiles", FVM_TILE, true, true, false},
        {"free-icons", FVM_ICON, false, false, true}};
    RestoreExplorer native;
    for (const auto &mode : modes) {
        if (scenario == LayoutScenario::Free && !mode.free) continue;
        requireSession();
        RestoreHost owner;
        if (mode.explorer && !native.shell) native.open(fixture);
        if (!mode.explorer) owner.createProfile(fixture, mode.mode, mode.modern ? FVO_DEFAULT
            : static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
        const CComPtr<IShellView> shell = mode.explorer ? native.shell : owner.events->shell;
        CComQIPtr<IFolderView2> view(shell);
        CComQIPtr<IShellFolderView> bulk(shell);
        need(view != nullptr, "No path view");
        const HWND frame = mode.explorer ? native.frame : owner.frame;
        const auto current = [&] { return !cancelled && !locked && IsWindow(frame) != FALSE; };
        need(SUCCEEDED(view->SetViewModeAndIconSize(mode.mode, mode.mode == FVM_CONTENT ? 32 : 48)), "Cannot set path mode");
        if (!mode.explorer) need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID,
            mode.free ? 0 : FWF_AUTOARRANGE)), "Cannot set layout flags");
        restoreEnumeration(shell, mode.explorer ? native.events.p : owner.events.p);
        if (mode.explorer) presentation::fitView(shell, frame);
        else presentation::fitView(shell, frame, [&] {
            RECT bounds = {}; need(GetClientRect(frame, &bounds) != FALSE, "No bounds");
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot size path view");
        });
        restoreExpose(frame);
        FOLDERVIEWMODE actual = FVM_AUTO; int size = 0, count = 0;
        need(SUCCEEDED(view->GetViewModeAndIconSize(&actual, &size))
            && SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW, &count)) && count == fixtureCount, "Invalid effective view");
        std::printf("PATH name=%s requested=%u actual=%u size=%d auto=%08lX\n", mode.name,
            static_cast<unsigned>(mode.mode), static_cast<unsigned>(actual), size,
            static_cast<unsigned long>(view->GetAutoArrange()));
        HWND body = nullptr; need(SUCCEEDED(shell->GetWindow(&body)), "No path window");
        EnumChildWindows(body, logLayoutWindow, 0);
        CComPtr<IShellFolder> folder;
        need(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder))), "No path folder");
        std::vector<OwnedId> storage;
        std::vector<PCUITEMID_CHILD> ids;
        for (int i = 0; i < count; ++i) {
            CComHeapPtr<ITEMID_CHILD> id;
            need(SUCCEEDED(view->Item(i, &id)) && id, "No path identity");
            storage.emplace_back(id.Detach(), &CoTaskMemFree); ids.push_back(storage.back().get());
        }
        std::vector<POINT> freePositions;
        if (mode.free) {
            need(SUCCEEDED(filespacer::restoreIconPositions(view, restoreFreeFixture(ids), current)), "Cannot seed free path");
            PCUITEMID_CHILD displaced[] = {ids[0], ids[17]};
            POINT adjusted[] = {{1900, 1800}, {5, 5}};
            need(SUCCEEDED(view->SelectAndPositionItems(2, displaced, adjusted, SVSI_POSITIONITEM | SVSI_NOTAKEFOCUS)), "Cannot seed irregular free geometry");
            for (const auto id : ids) {
                POINT point = {};
                need(SUCCEEDED(view->GetItemPosition(id, &point)), "Cannot retain free geometry");
                freePositions.push_back(point);
            }
        }
        for (int i = 0; i < 4; ++i) {
            POINT point = {};
            const HRESULT positioned = view->GetItemPosition(ids[static_cast<size_t>(i)], &point);
            std::printf("GEOMETRY index=%d hr=%08lX x=%ld y=%ld\n", i, static_cast<unsigned long>(positioned), point.x, point.y);
        }
        filespacer::ShellSelectionClear binding;
        if (!mode.modern) {
            binding.bind(shell, current);
            need(binding.acceleratesSelectAll(), "No qualified path capability");
        }
        need(SUCCEEDED(restoreExplorerAll(shell, view, frame, bulk)), "Cannot select path fixture");
        need(SUCCEEDED(shell->SelectItem(ids[7], static_cast<SVSIF>(SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS))), "Cannot seed path focus");
        std::vector<uint8_t> saved;
        double started = 0;
        for (int repetition = 0; repetition < (mode.free ? 3 : 1); ++repetition) {
            started = clockMs();
            need(SUCCEEDED(filespacer::captureReopenSelection(shell, current, saved)), "Cannot capture path selection");
            std::printf("TIMING name=%s operation=capture ms=%.3f\n", mode.name, clockMs() - started);
        }
        std::vector<uint8_t> savedViewport;
        if (!mode.explorer) {
            uint32_t header[5] = {};
            need(saved.size() >= sizeof(header) + 48, "No shared viewport in snapshot");
            std::memcpy(header, saved.data(), sizeof(header));
            need(header[0] == 3 && (header[2] & 16), "No version-3 viewport");
            savedViewport.assign(saved.begin() + sizeof(header), saved.begin() + sizeof(header) + 48);
            std::vector<uint8_t> actualViewport;
            need(filespacer::captureViewScroll(shell, current, actualViewport) == S_OK
                && actualViewport == savedViewport, "Snapshot viewport differs from actual view");
        }
        CComPtr<IShellItemArray> retained;
        need(SUCCEEDED(view->GetSelection(FALSE, &retained)), "No original selection");
        const auto wanted = arrayPaths(retained);
        for (int repetition = 0; repetition < 3; ++repetition) {
            need(SUCCEEDED(mode.modern ? shell->SelectItem(nullptr, SVSI_DESELECTOTHERS) : binding.clear(shell, current)), "Cannot clear path fixture");
            // Keep repetition zero comparable with Explorer at the original viewport.
            // The remaining repetitions must also restore a genuinely displaced view.
            if (!mode.explorer && repetition) {
                uint32_t kind = 0; int32_t x = 0, y = 0; double horizontal = -1, vertical = -1;
                std::memcpy(&kind, savedViewport.data() + 4, sizeof(kind));
                std::memcpy(&x, savedViewport.data() + 16, sizeof(x));
                std::memcpy(&y, savedViewport.data() + 20, sizeof(y));
                std::memcpy(&horizontal, savedViewport.data() + 32, sizeof(horizontal));
                std::memcpy(&vertical, savedViewport.data() + 40, sizeof(vertical));
                const bool towardStart = kind == 2 ? horizontal > 50 || vertical > 50 : x < 0 || y < 0;
                std::printf("DISPLACE name=%s rep=%d kind=%u x=%d y=%d horizontal=%.6f vertical=%.6f toward_start=%d\n",
                    mode.name, repetition, kind, x, y, horizontal, vertical, towardStart);
                // Use a distinct distant identity for each independent trial.
                const size_t target = towardStart ? static_cast<size_t>(repetition)
                    : ids.size() - 1 - static_cast<size_t>(repetition);
                need(SUCCEEDED(shell->SelectItem(ids[target], SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS)), "Cannot displace the viewport");
                pump(350);
                std::vector<uint8_t> displaced;
                need(filespacer::captureViewScroll(shell, current, displaced) == S_OK
                    && displaced != savedViewport, "Viewport displacement did not occur");
            }
            need(RedrawWindow(body, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)
                && SUCCEEDED(DwmFlush()), "Cannot compose empty path selection");
            std::unique_ptr<presentation::Observation> observation;
            if (!repetition && scenario != LayoutScenario::Free) {
                const std::wstring label(mode.name, mode.name + std::strlen(mode.name));
                observation = std::make_unique<presentation::Observation>(body, std::wstring(output) + L"\\" + label);
                observation->begin();
            }
            auto pending = saved;
            const HWND keyboard = restoreKeyboardFocus(frame);
            started = clockMs();
            need(SUCCEEDED(mode.explorer ? restoreExplorerAll(shell, view, frame, bulk)
                : filespacer::restoreReopenSelection(shell, pending, current, mode.modern ? nullptr : &binding)), "Cannot restore path selection");
            const double restoreMs = clockMs() - started;
            if (observation) { observation->apiReturned(); observation->finish(); }
            else if (!mode.explorer && repetition) pump(350);
            if (!mode.explorer) {
                std::vector<uint8_t> actualViewport;
                need(filespacer::captureViewScroll(shell, current, actualViewport) == S_OK
                    && actualViewport == savedViewport, "Selection restoration lost the saved viewport");
            }
            CComPtr<IShellItemArray> selected;
            started = clockMs();
            need(SUCCEEDED(view->GetSelection(FALSE, &selected)), "Cannot obtain path array");
            const double arrayMs = clockMs() - started;
            need(arrayPaths(selected) == wanted && arrayPaths(retained) == wanted, "Path selection identities changed");
            for (size_t i = 0; i < freePositions.size(); ++i) {
                POINT point = {};
                need(SUCCEEDED(view->GetItemPosition(ids[i], &point)) && point.x == freePositions[i].x
                    && point.y == freePositions[i].y, "Path restoration changed a free position");
            }
            int focused = -1;
            need(SUCCEEDED(view->GetFocusedItem(&focused)) && focused == 7
                && restoreKeyboardFocus(frame) == keyboard, "Path restoration changed focus");
            std::printf("TIMING name=%s rep=%d operation=restore ms=%.3f first_array_ms=%.3f\n", mode.name, repetition, restoreMs, arrayMs);
            if (mode.free) {
                // Public prototype: one position lookup per item, then in-memory ordering.
                struct Entry { PCUITEMID_CHILD id; POINT position; };
                std::vector<Entry> entries;
                std::vector<OwnedId> copied;
                started = clockMs();
                entries.reserve(static_cast<size_t>(count));
                copied.reserve(static_cast<size_t>(count));
                for (int i = 0; i < count; ++i) {
                    CComHeapPtr<ITEMID_CHILD> id; POINT point = {};
                    need(SUCCEEDED(view->Item(i, &id)) && id
                        && SUCCEEDED(view->GetItemPosition(id, &point)), "Cannot inspect indexed geometry");
                    copied.emplace_back(id.Detach(), &CoTaskMemFree);
                    entries.push_back({copied.back().get(), point});
                }
                const double geometryMs = clockMs() - started;
                std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
                    return a.position.x != b.position.x ? a.position.x < b.position.x : a.position.y < b.position.y;
                });
                // The inspected native selection enumeration starts at its focused item.
                const auto first = std::find_if(entries.begin(), entries.end(), [&](const Entry &entry) {
                    return entry.id == copied[static_cast<size_t>(focused)].get();
                });
                need(first != entries.end(), "No focused array entry");
                std::rotate(entries.begin(), first, entries.end());
                std::vector<PCUITEMID_CHILD> spatial;
                spatial.reserve(entries.size());
                for (const auto &entry : entries) spatial.push_back(entry.id);
                CComPtr<IShellItemArray> candidate;
                need(SUCCEEDED(SHCreateShellItemArray(nullptr, folder, static_cast<UINT>(spatial.size()), spatial.data(), &candidate)), "Cannot create public array");
                const double publicArrayMs = clockMs() - started;
                const bool sameOrder = orderedPaths(candidate) == orderedPaths(selected);
                need(sameOrder, "Public array order differs from the native spatial array");
                need(SUCCEEDED(binding.clear(shell, current)) && arrayPaths(candidate) == wanted
                    && arrayPaths(selected) == wanted, "Retained public or native array changed after clear");
                std::printf("PROTOTYPE geometry_ms=%.3f ordered_array_ms=%.3f exact_order=%d retained=1\n", geometryMs, publicArrayMs, sameOrder);
            }
            std::fflush(stdout);
        }
    }
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc == 3 && wcscmp(argv[1], L"--build-cache") == 0) return selectionDiscovery::runWorker(argv[2]);
    LayoutScenario scenario = LayoutScenario::Matrix;
    if (argc == 5 && wcscmp(argv[4], L"--free-only") == 0) scenario = LayoutScenario::Free;
    else if (argc != 4) return 2;
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
        need(selectionDiscovery::resolved() && selectionDiscovery::image().selectAllSupported, "No resolved selection backend");
        layoutPaths(argv[1], argv[2], scenario);
    } catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName, type.hInstance); CoUninitialize(); return result;
}
