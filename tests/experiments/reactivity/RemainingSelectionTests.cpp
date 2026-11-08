// Host-only investigation of the remaining classic selection costs.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"
#include <dbghelp.h>
#include <psapi.h>
#include <docobj.h>

namespace {
struct NoticeTiming {
    HWND window = nullptr;
    double elapsed = 0;
    unsigned notices = 0;
    bool active = false;
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM first, LPARAM second,
            UINT_PTR, DWORD_PTR data) {
        auto &self = *reinterpret_cast<NoticeTiming *>(data);
        const bool observe = self.active && message == WM_NOTIFY && second
            && (reinterpret_cast<const NMHDR *>(second)->code == LVN_ITEMCHANGED
                || reinterpret_cast<const NMHDR *>(second)->code == LVN_ODSTATECHANGED);
        const double start = observe ? clockMs() : 0;
        const LRESULT result = DefSubclassProc(window, message, first, second);
        if (observe) { self.elapsed += clockMs() - start; ++self.notices; }
        return result;
    }
    explicit NoticeTiming(HWND target) : window(target) {
        if(window)need(GetWindowThreadProcessId(window,nullptr)==GetCurrentThreadId()
            &&SetWindowSubclass(window, procedure, 54, reinterpret_cast<DWORD_PTR>(this)) != FALSE,
            "Cannot observe selection notifications");
    }
    ~NoticeTiming() { if(window)RemoveWindowSubclass(window, procedure, 54); }
};
BOOL CALLBACK selectionList(HWND child, LPARAM data) {
    wchar_t name[32] = {};
    if (GetClassNameW(child, name, _countof(name)) && wcscmp(name, WC_LISTVIEWW) == 0) {
        *reinterpret_cast<HWND *>(data) = child; return FALSE;
    }
    return TRUE;
}
struct SelectionEvents {
    static SelectionEvents *observer;
    HWND frame; HWINEVENTHOOK hook = nullptr; DWORD began = 0; unsigned changes = 0; bool active = false;
    static void CALLBACK received(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD time) {
        auto *self = observer;
        if (self && self->active && static_cast<DWORD>(time - self->began) < 0x80000000u
            && (window == self->frame || IsChild(self->frame, window))
            && event == EVENT_OBJECT_SELECTIONWITHIN && object == OBJID_CLIENT && child == CHILDID_SELF) {
            ++self->changes;wchar_t name[64]={};GetClassNameW(window,name,_countof(name));
            std::printf("SELECTION_EVENT window=%p class=%ls object=%ld child=%ld\n",window,name,object,child);
        }
    }
    explicit SelectionEvents(HWND owner) : frame(owner) {
        hook = SetWinEventHook(EVENT_OBJECT_SELECTION, EVENT_OBJECT_SELECTIONWITHIN, nullptr, received,
            GetCurrentProcessId(), GetCurrentThreadId(), WINEVENT_OUTOFCONTEXT);
        need(hook != nullptr, "Cannot observe native selection events");
        observer = this;
    }
    ~SelectionEvents() { if (hook) UnhookWinEvent(hook); observer = nullptr; }
    void begin() { changes = 0; began = GetTickCount(); active = true; }
    unsigned finish() {
        const ULONGLONG deadline = GetTickCount64() + 1000;
        while (!changes && GetTickCount64() < deadline) pump(10);
        active = false; return changes;
    }
};
SelectionEvents *SelectionEvents::observer = nullptr;
HRESULT remainingAll(IShellView *shell,IFolderView2 *view,HWND frame,IShellFolderView *bulk) {
    if(bulk)return bulk->Select(SFVS_SELECT_ALLITEMS);
    // Standard document command, when exposed by this Explorer view. This
    // permits API-only measurements without manufacturing keyboard state.
    CComQIPtr<IOleCommandTarget> commands(shell);OLECMD command={OLECMDID_SELECTALL,0};
    if(commands&&SUCCEEDED(commands->QueryStatus(nullptr,1,&command,nullptr))&&(command.cmdf&OLECMDF_ENABLED)) {
        const HRESULT result=commands->Exec(nullptr,OLECMDID_SELECTALL,OLECMDEXECOPT_DONTPROMPTUSER,nullptr,nullptr);
        if(FAILED(result))return result;
        const ULONGLONG deadline=GetTickCount64()+10000;int count=0;
        do {if(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count))&&count==fixtureCount)return S_OK;pump(1);}
        while(GetTickCount64()<deadline);
        return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    }
    return restoreExplorerAll(shell,view,frame,bulk);
}
void captureSelectionStack(const std::wstring &file) {
    HANDLE output = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output != INVALID_HANDLE_VALUE) {
        // A dedicated worker writes this process's dump. No cross-process
        // attachment, executable patch, or modified thread context is used.
        const BOOL wrote = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), output,
            static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo
                | MiniDumpWithProcessThreadData), nullptr, nullptr, nullptr);
        std::printf("SNAPSHOT file=%ls wrote=%d error=%lu\n", file.c_str(), wrote, wrote ? 0 : GetLastError());
        CloseHandle(output);
    }
}
void preserveSelectionImages(const std::wstring &directory) {
    CreateDirectoryW(directory.c_str(), nullptr);
    HMODULE modules[1024] = {}; DWORD bytes = 0;
    need(EnumProcessModulesEx(GetCurrentProcess(), modules, sizeof(modules), &bytes, LIST_MODULES_64BIT)
        && bytes <= sizeof(modules), "Cannot preserve matching loaded images");
    for (DWORD i = 0; i < bytes / sizeof(HMODULE); ++i) {
        wchar_t path[32768] = {};
        if (!GetModuleFileNameExW(GetCurrentProcess(), modules[i], path, _countof(path))) continue;
        const wchar_t *name = wcsrchr(path, L'\\');
        if (name) CopyFileW(path, (directory + name).c_str(), TRUE);
    }
}
void remainingSelection(const wchar_t *output, const wchar_t *fixture, const wchar_t *variant) {
    const bool explorer = wcscmp(variant, L"explorer") == 0;
    const bool trace = wcscmp(variant, L"trace") == 0;
    const bool adopted = wcscmp(variant, L"adopted") == 0 || wcscmp(variant, L"visible-adopted") == 0;
    const bool visible = wcscmp(variant, L"visible") == 0 || wcscmp(variant, L"visible-adopted") == 0;
    const bool columns = wcscmp(variant, L"columns") == 0;
    const bool sort = wcscmp(variant, L"sort") == 0;
    const bool events = wcscmp(variant, L"events") == 0;
    struct Parking {
        std::unique_ptr<presentation::PointerParking> owner;
        ~Parking(){presentation::pointer=nullptr;}
    } parking;
    if(!headless){parking.owner=std::make_unique<presentation::PointerParking>();presentation::pointer=parking.owner.get();}
    if (columns) need(!selectionDiscovery::rangeImage().supported, "Columns control requires a cache without the private range");
    RestoreExplorer native;
    if (explorer) native.open(fixture);
    FILE *raw = nullptr;
    raw = _wfsopen((std::wstring(output) + L"\\remaining-selection.csv").c_str(), L"wb", _SH_DENYNO);
    need(raw != nullptr, "No selection journal");
    std::unique_ptr<FILE, decltype(&std::fclose)> journal(raw, &std::fclose);
    std::fputs("app,view,arm,rep,select_ms,clear_ms,restore_ms,notify_ms,notices,private_applied,range_calls,selection_events\n", raw);
    for (int modeIndex = 0; modeIndex < (explorer ? 2 : 4); ++modeIndex) {
        if ((visible || columns || events) && modeIndex != 3) continue;
        if (sort && modeIndex != 1) continue;
        wchar_t only[16] = {};
        if (GetEnvironmentVariableW(L"FS_REMAINING_MODE", only, _countof(only)) && _wtoi(only) != modeIndex) continue;
        const bool tiles = (modeIndex & 1) != 0, free = modeIndex >= 2;
        const char *label = free ? (tiles ? "free-tiles" : "free-icons") : (tiles ? "tiles" : "medium");
        std::printf("STAGE view=%s creating\n", label); std::fflush(stdout);
        RestoreHost owner;
        if (!explorer) owner.createProfile(fixture, tiles ? FVM_TILE : FVM_ICON,
            static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING));
        need(explorer || owner.events != nullptr, "Missing Shell navigation events");
        IShellView *source = explorer ? native.shell.p : owner.events->shell.p;
        need(source != nullptr, "Missing enumerated Shell view");
        CComPtr<IShellView> shell(source);
        CComQIPtr<IFolderView2> view(shell); CComQIPtr<IShellFolderView> bulk(shell);
        const HWND frame = explorer ? native.frame : owner.frame;
        const auto current = [&] { return !cancelled && !locked && IsWindow(frame) && (explorer || owner.events->shell == shell); };
        need(view && (explorer || bulk) && SUCCEEDED(view->SetViewModeAndIconSize(tiles ? FVM_TILE : FVM_ICON, 48)), "Cannot set representation");
        if (!explorer) need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID,
            free ? 0 : FWF_AUTOARRANGE)), "Cannot set arrangement");
        restoreEnumeration(shell, explorer ? native.events.p : owner.events.p);
        FOLDERVIEWMODE actual = FVM_AUTO; int actualSize = 0;
        need(SUCCEEDED(view->GetViewModeAndIconSize(&actual, &actualSize)) && actual == (tiles ? FVM_TILE : FVM_ICON)
            && actualSize == 48, "Actual representation differs");
        if (!explorer) need(view->GetAutoArrange() == (free ? S_FALSE : S_OK), "Actual arrangement differs");
        std::printf("STAGE view=%s enumerated\n", label); std::fflush(stdout);
        if (explorer) presentation::fitView(shell, frame);
        else presentation::fitView(shell, frame, [&] {
            RECT bounds = {}; need(GetClientRect(frame, &bounds) != FALSE, "No selection bounds");
            need(SUCCEEDED(owner.host.setBounds(bounds)), "Cannot fit selection view");
        });
        if (!headless) { restoreExpose(frame); parking.owner->park(); }
        int count = 0; need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW, &count)) && count == fixtureCount, "Incomplete fixture");
        std::vector<OwnedId> owned; std::vector<PCUITEMID_CHILD> ids;
        for (int index = 0; index < count; ++index) {
            CComHeapPtr<ITEMID_CHILD> id; need(SUCCEEDED(view->Item(index, &id)) && id, "Missing identity");
            owned.emplace_back(id.Detach(), &CoTaskMemFree); ids.push_back(owned.back().get());
        }
        CComPtr<IShellFolder> folder; need(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder))), "No fixture folder");
        if (free) {
            PCUITEMID_CHILD moved[] = {ids[0], ids[4000]}; POINT points[] = {{13,17}, {6000,9000}};
            need(SUCCEEDED(view->SelectAndPositionItems(2, moved, points, SVSI_POSITIONITEM | SVSI_NOSTATECHANGE)), "Cannot seed positions");
        }
        std::vector<POINT> positions;
        std::printf("STAGE view=%s reading_positions\n", label); std::fflush(stdout);
        if (!explorer) for (const auto id : ids) {
            POINT point = {}; need(SUCCEEDED(view->GetItemPosition(id, &point)), "No original position"); positions.push_back(point);
        }
        HWND body = nullptr; need(SUCCEEDED(shell->GetWindow(&body)), "No Shell window");
        HWND list = nullptr; EnumChildWindows(body, selectionList, reinterpret_cast<LPARAM>(&list));
        if (list) {
            // Fixture-only: exclude hover-driven tooltip hit testing from API
            // measurements even when the input desktop cannot move the cursor.
            const HWND tooltip = ListView_GetToolTips(list);
            if (tooltip) SendMessageW(tooltip, TTM_ACTIVATE, FALSE, 0);
            if(!explorer&&tooltip&&GetWindowThreadProcessId(list,nullptr)==GetCurrentThreadId()
                    &&GetWindowThreadProcessId(tooltip,nullptr)==GetCurrentThreadId()) {
                // An inactive tooltip can still answer UIA GetText requests,
                // which trigger a costly free-position hit test. Remove only
                // this fixture's own tooltip, outside the measured operation.
                need(ListView_SetToolTips(list,nullptr)==tooltip&&ListView_GetToolTips(list)==nullptr,
                    "Cannot detach fixture tooltip");
                need(DestroyWindow(tooltip)!=FALSE,"Cannot remove fixture tooltip");
            }
        }
        std::printf("STAGE view=%s ready\n", label); std::fflush(stdout);
        filespacer::ShellSelectionClear accelerated;
        if (!explorer) accelerated.bind(shell, current);
        if (sort) {
            std::vector<std::wstring> expected;
            for (int index = 0; index < count; ++index) {
                CComPtr<IShellItem> item; need(SUCCEEDED(view->GetItem(index, IID_PPV_ARGS(&item))), "No sort identity");
                expected.push_back(itemPath(item));
            }
            std::sort(expected.begin(), expected.end());
            for (int repetition = 0; repetition < 3; ++repetition) for (bool candidate : {false,true}) {
                accelerated.reset();
                need(SUCCEEDED(shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "Cannot clear sort setup");
                const SORTCOLUMN ascending = {PKEY_ItemNameDisplay,SORT_ASCENDING};
                need(SUCCEEDED(view->SetSortColumns(&ascending,1)), "Cannot set initial sort"); pump(100);
                if (candidate) accelerated.bind(shell,current);
                need(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)), "Cannot select sort fixture");
                waitSelectedIdentities(view,expected);
                const SORTCOLUMN descending = {PKEY_ItemNameDisplay,SORT_DESCENDING};
                need(SUCCEEDED(view->SetSortColumns(&descending,1)), "Cannot sort complete selection");
                const double checking = clockMs();
                try {waitSelectedIdentities(view,expected);}
                catch (const std::exception &) {
                    const double checkMs = clockMs() - checking;
                    int selected = -1; view->ItemCount(SVGIO_SELECTION,&selected);
                    CComPtr<IShellItemArray> array; const HRESULT got = view->GetSelection(FALSE,&array);
                    const auto actualPaths = array ? arrayPaths(array) : std::vector<std::wstring>{};
                    std::printf("SORT FAIL candidate=%d range_available=%d selected=%d array=%zu identities_match=%d check_ms=%.3f hr=%08lX private_all=%llu\n",candidate,
                        selectionDiscovery::rangeImage().supported,selected,actualPaths.size(),actualPaths==expected,checkMs,static_cast<unsigned long>(got),
                        static_cast<unsigned long long>(accelerated.acceleratedSelectAllCount())); std::fflush(stdout);
                    size_t shown=0;
                    for(size_t i=0;i<(std::min)(actualPaths.size(),expected.size())&&shown<3;++i)if(actualPaths[i]!=expected[i]) {
                        std::printf("SORT MISMATCH wanted=%ls actual=%ls\n",expected[i].c_str(),actualPaths[i].c_str());++shown;
                    }
                    throw;
                }
                std::printf("SORT PASS candidate=%d range_available=%d repetition=%d private_all=%llu\n",candidate,
                    selectionDiscovery::rangeImage().supported,repetition,
                    static_cast<unsigned long long>(accelerated.acceleratedSelectAllCount())); std::fflush(stdout);
            }
            continue;
        }
        NoticeTiming notices(explorer?nullptr:body);
        std::unique_ptr<SelectionEvents> eventObserver;
        if (events) eventObserver = std::make_unique<SelectionEvents>(frame);
        if (trace) preserveSelectionImages(std::wstring(output) + L"\\images");
        for (int rep = 0; rep < (trace || (visible && adopted) ? 1 : 3); ++rep) for (int order = 0; order < (columns ? 4 : adopted || explorer ? 1 : 3); ++order) {
            const int columnArms[] = {0,2,3,4};
            const int arm = columns ? columnArms[rep % 2 ? 3 - order : order] : adopted ? 2 : rep % 2 ? (explorer ? 0 : 2 - order) : order;
            // arm 0 ordinary IShellView; arm 1 legacy public bulk; arm 2 adopted private.
            if (arm != 2 && arm != 4) accelerated.reset();
            else accelerated.bind(shell, current);
            need(SUCCEEDED(shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "Cannot reset selection");
            const double began = clockMs();
            need(SUCCEEDED(remainingAll(shell, view, frame, bulk)), "Cannot select complete fixture");
            const double selectMs = clockMs() - began;
            need(SUCCEEDED(shell->SelectItem(ids[7], SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS)), "Cannot set logical focus");
            CComPtr<IShellItemArray> retained; need(SUCCEEDED(view->GetSelection(FALSE, &retained)) && retained, "No retained selection");
            const auto wanted = arrayPaths(retained);
            std::vector<uint8_t> saved;
            if (!explorer) need(SUCCEEDED(filespacer::captureReopenSelection(shell, current, saved)), "Cannot capture selection snapshot");
            const HWND keyboard = restoreKeyboardFocus(frame);
            const int anchor = list ? ListView_GetSelectionMark(list) : -1;
            std::unique_ptr<presentation::Observation> display;
            if (visible && rep == 0) {
                need(RedrawWindow(body, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)
                    && SUCCEEDED(DwmFlush()), "Cannot compose selected baseline");
                display = std::make_unique<presentation::Observation>(body,
                    std::wstring(output) + L"\\clear-" + std::to_wstring(arm));
                display->begin();
            }
            std::atomic<bool> done{false}; std::thread sampler;
            struct JoinSample {
                std::atomic<bool> &done;std::thread &thread;
                ~JoinSample(){done.store(true);if(thread.joinable())thread.join();}
            } joinSample{done,sampler};
            if (trace) {
                std::wstring file = std::wstring(output) + L"\\" + std::wstring(label, label + strlen(label)) + L"-" + std::to_wstring(arm) + L".dmp";
                sampler = std::thread([&done, file=std::move(file)] { Sleep(150); if (!done.load()) captureSelectionStack(file); });
            }
            notices.elapsed = 0; notices.notices = 0; notices.active = true;
            bool applied = false;
            const auto beforeRanges = selectionDiscovery::rangeCalls().load(std::memory_order_relaxed);
            if (eventObserver) { pump(100); eventObserver->begin(); }
            const double clearing = clockMs();
            HRESULT result = S_OK;
            if (arm == 3 || arm == 4) {
                LVTILEVIEWINFO previous = {}; previous.cbSize = sizeof(previous); previous.dwMask = LVTVIM_COLUMNS;
                need(list && ListView_GetTileViewInfo(list, &previous), "Cannot read tile caption lines");
                LVTILEVIEWINFO temporary = previous; temporary.cLines = 0;
                need(SUCCEEDED(view->SetRedraw(FALSE)), "Cannot suspend candidate redraw");
                const bool changed = ListView_SetTileViewInfo(list, &temporary) != FALSE;
                if (changed) result = arm == 4 ? accelerated.clear(shell, current, &applied)
                    : shell->SelectItem(nullptr, SVSI_DESELECTOTHERS);
                else result = E_NOTIMPL;
                const bool restored = ListView_SetTileViewInfo(list, &previous) != FALSE;
                const HRESULT redraw = view->SetRedraw(TRUE);
                need(restored && SUCCEEDED(redraw), "Cannot restore tile caption lines");
                RedrawWindow(body, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                std::printf("COLUMNS original=%d temporary=0 restored=1\n", previous.cLines);
            } else result = arm == 2 ? accelerated.clear(shell, current, &applied)
                : arm == 1 ? bulk->Select(SFVS_SELECT_NONE) : shell->SelectItem(nullptr, SVSI_DESELECTOTHERS);
            const double clearMs = clockMs() - clearing;
            const auto rangeCount = selectionDiscovery::rangeCalls().load(std::memory_order_relaxed) - beforeRanges;
            notices.active = false; done.store(true);
            if (sampler.joinable()) sampler.join();
            if (display) {
                display->apiReturned();
                // Measure actual repaint work, including any cost deferred by
                // the clear. Force the standard paint before unrelated UIA
                // requests enter the helper's message loop.
                need(RedrawWindow(body,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW)
                    &&SUCCEEDED(DwmFlush()),"Cannot compose deselection");
                const ULONGLONG deadline=GetTickCount64()+6000;
                while(!display->done.load()&&GetTickCount64()<deadline)Sleep(10);
                need(display->done.load(),"Deselection frame did not settle");
                display->finish();
            }
            const unsigned selectionEvents = eventObserver ? eventObserver->finish() : 0;
            need(SUCCEEDED(result), "Clear failed");
            checkState(view, shell, folder, fixture, ids, {});
            need(arrayPaths(retained) == wanted, "Retained selection changed");
            int focused = -1; need(SUCCEEDED(view->GetFocusedItem(&focused)) && focused == 7
                && restoreKeyboardFocus(frame) == keyboard && (!list || ListView_GetSelectionMark(list) == anchor), "Focus or anchor changed");
            double restoreMs = 0;
            if (!explorer) {
                auto pending = saved;
                const double restoring = clockMs();
                need(SUCCEEDED(filespacer::restoreReopenSelection(shell, pending, current, arm == 2 || arm == 4 ? &accelerated : nullptr))
                    && pending.empty(), "Selection restore failed");
                restoreMs = clockMs() - restoring;
                CComPtr<IShellItemArray> restored;
                need(SUCCEEDED(view->GetSelection(FALSE, &restored)) && arrayPaths(restored) == wanted, "Restored identities differ");
                need(SUCCEEDED(arm == 2 || arm == 4 ? accelerated.clear(shell, current) : shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)),
                    "Cannot clear restored selection");
            } else {
                const double restoring = clockMs();
                need(SUCCEEDED(remainingAll(shell, view, frame, bulk)), "Explorer restoration failed");
                restoreMs = clockMs() - restoring;
                CComPtr<IShellItemArray> restored;
                need(SUCCEEDED(view->GetSelection(FALSE, &restored)) && arrayPaths(restored) == wanted, "Explorer restored identities differ");
                need(SUCCEEDED(shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "Cannot clear restored Explorer selection");
            }
            for (size_t i = 0; i < positions.size(); ++i) {
                POINT point = {}; need(SUCCEEDED(view->GetItemPosition(ids[i], &point))
                    && point.x == positions[i].x && point.y == positions[i].y, "Position changed");
            }
            need(SUCCEEDED(view->SelectItem(17, SVSI_SELECT | SVSI_NOTAKEFOCUS)), "Subsequent select failed");
            CComPtr<IShellItemArray> next; need(SUCCEEDED(view->GetSelection(FALSE, &next)) && next, "Subsequent selection missing");
            DWORD nextCount = 0; need(SUCCEEDED(next->GetCount(&nextCount)) && nextCount == 1, "Subsequent selection stale");
            std::fprintf(raw, "%s,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%u,%d,%llu,%u\n", explorer ? "Explorer" : "FS", label, arm, rep,
                selectMs, clearMs, restoreMs, notices.elapsed, notices.notices, applied, static_cast<unsigned long long>(rangeCount), selectionEvents); std::fflush(raw);
            std::printf("RESULT app=%s view=%s arm=%d rep=%d select_ms=%.3f clear_ms=%.3f restore_ms=%.3f range_calls=%llu selection_events=%u correctness=PASS\n",
                explorer ? "Explorer" : "FS", label, arm, rep, selectMs, clearMs, restoreMs, static_cast<unsigned long long>(rangeCount), selectionEvents); std::fflush(stdout);
            if(!(visible&&adopted))pump(20);
        }
    }
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc == 4 && wcscmp(argv[1], L"/test") == 0 && wcscmp(argv[2], L"/selection-discovery") == 0)
        return selectionDiscovery::runWorker(argv[3]);
    if (argc == 3 && wcscmp(argv[1], L"--build-cache") == 0) return selectionDiscovery::runWorker(argv[2]);
    if (argc != 5) return 2;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI();
    wchar_t background[4] = {};
    headless = GetEnvironmentVariableW(L"FS_REMAINING_BACKGROUND", background, _countof(background)) != 0;
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = testWindowProc; type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    int result = 0;
    try {
        filespacer::initializeShellSelectionClear(argv[3], false);
        const ULONGLONG deadline = GetTickCount64() + 180000;
        while (!selectionDiscovery::resolved() && GetTickCount64() < deadline) pump(1);
        need(selectionDiscovery::resolved(), "Discovery timeout");
        std::printf("DISCOVERY supported=%d all=%d range=%d reason=%s ui_thread=%lu\n", selectionDiscovery::image().supported,
            selectionDiscovery::image().selectAllSupported, selectionDiscovery::rangeImage().supported,
            selectionDiscovery::image().reason, GetCurrentThreadId()); std::fflush(stdout);
        remainingSelection(argv[1], argv[2], argv[4]);
    } catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName, type.hInstance); CoUninitialize(); return result;
}
