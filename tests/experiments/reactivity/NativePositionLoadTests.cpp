// Isolated investigation: feed the observed legacy position format to its native loader.
// The runner requires the exact Shell image whose parser was inspected offline.
#define POSITION_DUMP_DRIVER
#include "PositionDumpTests.cpp"

namespace {
void requirePresentation(Host &owner,FOLDERVIEWMODE expected,const char *phase,int expectedSize = 48) {
    struct Search { HWND window = nullptr; unsigned count = 0; } search;
    HWND parent = nullptr;
    require(SUCCEEDED(owner.events->shell->GetWindow(&parent)) && parent,"No presentation window");
    EnumChildWindows(parent,[](HWND window,LPARAM data)->BOOL {
        wchar_t name[32] = {}; DWORD process = 0;
        if (GetClassNameW(window,name,_countof(name)) && std::wcscmp(name,WC_LISTVIEWW) == 0
            && GetWindowThreadProcessId(window,&process) == GetCurrentThreadId() && process == GetCurrentProcessId()) {
            auto &found = *reinterpret_cast<Search *>(data); found.window = window; ++found.count;
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&search));
    require(search.count == 1,"No unique native presentation control");
    FOLDERVIEWMODE actual = FVM_AUTO; int size = 0;
    require(SUCCEEDED(owner.view()->GetViewModeAndIconSize(&actual,&size)),"No presentation mode");
    const DWORD controlMode = ListView_GetView(search.window);
    std::printf("RENDERER phase=%s requested=%u reported=%u size=%d list_view=%lu\n",phase,
        static_cast<unsigned>(expected),static_cast<unsigned>(actual),size,controlMode); std::fflush(stdout);
    require(actual == expected && size == expectedSize && controlMode == static_cast<DWORD>(expected == FVM_TILE ? LV_VIEW_TILE : LV_VIEW_ICON),
        "Native presentation differs from the requested view");
}
CComPtr<IPropertyBag> findNativeBag(const wchar_t *fixture, const std::wstring &name, FOLDERVIEWMODE mode) {
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> folder;
    require(SUCCEEDED(SHParseDisplayName(fixture,nullptr,&folder,0,nullptr)), "No legacy bag folder");
    const GUID *types[] = {&FOLDERTYPEID_Generic,&FOLDERTYPEID_Documents,&FOLDERTYPEID_Pictures,&FOLDERTYPEID_Music,&FOLDERTYPEID_Videos};
    CComPtr<IPropertyBag> found;
    for (const GUID *type : types) {
        wchar_t text[40] = {};
        require(StringFromGUID2(*type,text,_countof(text)) != 0, "No folder-type name");
        const std::wstring full = name + L"\\" + text;
        CComPtr<IPropertyBag> bag; DWORD savedMode = 0;
        const HRESULT opened = SHGetViewStatePropertyBag(folder,full.c_str(),SHGVSPB_FOLDERNODEFAULTS,IID_PPV_ARGS(&bag));
        const HRESULT read = bag ? PSPropertyBag_ReadDWORD(bag,L"Mode",&savedMode) : opened;
        std::printf("NAMESPACE mode=%u suffix=%ls hr=%08lX value=%lu\n",static_cast<unsigned>(mode),text,static_cast<unsigned long>(read),savedMode);
        if (SUCCEEDED(read) && savedMode == static_cast<DWORD>(mode)) {
            require(!found,"Ambiguous native fixture property bag"); found = bag;
        }
    }
    return found;
}
std::wstring legacyKey(HWND window) {
    HDC dc = GetDC(nullptr); require(dc != nullptr,"No display context");
    const int width = GetDeviceCaps(dc,HORZRES),height = GetDeviceCaps(dc,VERTRES);
    require(ReleaseDC(nullptr,dc) != 0,"Cannot release display context");
    const UINT dpi = GetDpiForWindow(window);
    require(width > 0 && height > 0 && dpi != 0,"No legacy coordinate context");
    return L"ItemPos" + std::to_wstring(width) + L"x" + std::to_wstring(height)
        + L"x" + std::to_wstring(dpi) + L"(" + std::to_wstring(GetSystemMetrics(SM_CMONITORS)) + L")";
}
std::vector<uint8_t> legacyBlob(const Snapshot &items, const std::vector<POINT> &points) {
    require(items.ids.size() == points.size(),"Wrong legacy record count");
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(nullptr,0)); require(stream != nullptr,"No legacy data stream");
    for (size_t i = 0; i < items.ids.size(); ++i) {
        const USHORT length = items.ids[i]->mkid.cb;
        // The inspected native parser consumes one SHITEMID per record and
        // temporarily terminates it at the start of the following record.
        require(length >= sizeof(USHORT) && ILGetSize(items.ids[i]) == static_cast<UINT>(length) + sizeof(USHORT),
            "Fixture child is not a single legacy SHITEMID");
        require(SUCCEEDED(IStream_Write(stream,&points[i],sizeof(POINT)))
            && SUCCEEDED(IStream_Write(stream,items.ids[i],length)),"Cannot encode legacy position record");
    }
    const USHORT terminator = 0;
    require(SUCCEEDED(IStream_Write(stream,&terminator,sizeof(terminator))),"Cannot terminate legacy positions");
    return bytesOf(stream);
}
void writeLegacy(IPropertyBag *bag, const std::wstring &key, const std::vector<uint8_t> &bytes) {
    require(!bytes.empty() && bytes.size() <= INT32_MAX,"Invalid generated legacy blob size");
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(bytes.data(),static_cast<UINT>(bytes.size())));
    require(stream && SUCCEEDED(PSPropertyBag_WriteStream(bag,key.c_str(),stream)),"Cannot provide native position blob");
}
void exactLayout(IFolderView2 *view, const Snapshot &items, const std::vector<POINT> &points) {
    const auto observedPoints = readPositions(view,items);
    const size_t same = samePositions(observedPoints,points);
    if (same != items.ids.size()) {
        std::printf("LAYOUT_MISMATCH exact=%zu expected=%zu\n",same,items.ids.size());
        unsigned shown = 0;
        for (size_t i = 0; i < points.size() && shown < 8; ++i) if (observedPoints[i].x != points[i].x || observedPoints[i].y != points[i].y) {
            std::printf("COORDINATE index=%zu wanted=%ld,%ld observed=%ld,%ld\n",i,points[i].x,points[i].y,observedPoints[i].x,observedPoints[i].y); ++shown;
        }
        std::fflush(stdout);
    }
    require(same == items.ids.size(),"Native loader changed or omitted coordinates");
    const auto actual = capture(view); auto observed = actual.names, wanted = items.names;
    std::sort(observed.begin(),observed.end()); std::sort(wanted.begin(),wanted.end());
    int selected = -1;
    require(observed == wanted && SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&selected)) && selected == 0,
        "Native loader changed identities or selection");
    require(view->GetAutoArrange() == S_FALSE,"Native load enabled automatic arrangement");
}
void nativePositionLoad(const wchar_t *fixture, const wchar_t *output, bool initialDesktop = false) {
    const struct Mode { const char *name; FOLDERVIEWMODE mode; } modes[] = {{"icons",FVM_ICON},{"tiles",FVM_TILE}};
    for (const auto &mode : modes) {
        const int pixels = 48;
        const std::wstring name = L"FileSpacer.NativePositionLoad." + std::to_wstring(GetCurrentProcessId()) + L"."
            + std::wstring(mode.name,mode.name + std::strlen(mode.name));
        DumpHost source; source.createDump(fixture,mode.mode,false,{},name);
        auto view = source.dumpView();
        require(SUCCEEDED(view->SetViewModeAndIconSize(mode.mode,pixels))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID,0)),"Cannot configure source free layout");
        const auto items = capture(view); require(items.ids.size() == 5000,"Incomplete legacy fixture");
        Snapshot moved; moved.ids = {items.ids[0],items.ids[4000]};
        const std::vector<POINT> sentinels = {filespacer::scaleDPI(POINT{37,43}),filespacer::scaleDPI(POINT{6007,9009})};
        const auto current = [] { return true; };
        require(SUCCEEDED(filespacer::restoreIconPositions(view,positionBytes(moved,sentinels),current))
            && samePositions(readPositions(view,moved),sentinels) == 2,"Cannot seed native-load sentinels");
        auto original = readPositions(view,items);
        if (initialDesktop) for (size_t i = 0; i < items.ids.size(); ++i)
            original[i] = filespacer::scaleDPI(POINT{37 + static_cast<LONG>(i % 50)*317,43 + static_cast<LONG>(i / 50)*307});
        require(SUCCEEDED(source.events->shell->SaveViewState()),"Cannot save ordinary source settings");
        const std::wstring key = legacyKey(source.frame.get());
        view.Release(); source.closeDump();
        auto bag = findNativeBag(fixture,name,mode.mode);
        if (!bag) {
            std::printf("REJECT mode=%s reason=native_bag_not_located\n",mode.name); std::fflush(stdout); continue;
        }
        const auto initial = legacyBlob(items,original);
        writeBytes(std::wstring(output) + L"\\" + std::wstring(mode.name,mode.name + std::strlen(mode.name)) + L"-legacy.bin",initial);
        writeLegacy(bag,key,initial);
        DumpHost candidate; const double began = now(); candidate.createDump(fixture,mode.mode,false,{},name,initialDesktop ? FWF_DESKTOP : 0);
        if (initialDesktop) {
            require(SUCCEEDED(candidate.dumpView()->SetCurrentFolderFlags(FWF_DESKTOP,0))
                && SUCCEEDED(candidate.dumpView()->SetViewModeAndIconSize(mode.mode,pixels)),"Cannot restore presentation after the initial Desktop role");
            requirePresentation(candidate,mode.mode,"initial_bag",pixels);
        }
        candidate.paint();
        const double elapsed = now() - began;
        auto loaded = candidate.dumpView(); const auto actual = readPositions(loaded,items);
        const size_t same = samePositions(actual,original);
        const unsigned displaced = (actual[0].x == original[0].x && actual[0].y == original[0].y ? 1u : 0u)
            + (actual[4000].x == original[4000].x && actual[4000].y == original[4000].y ? 1u : 0u);
        std::printf("CAPABILITY mode=%s reopen_ms=%.3f exact=%zu sentinels=%u bytes=%zu key=%ls\n",
            mode.name,elapsed,same,displaced,initial.size(),key.c_str()); std::fflush(stdout);
        if (same != 5000 || displaced != 2) {
            std::printf("REJECT mode=%s reason=legacy_positions_not_applied\n",mode.name); std::fflush(stdout); continue;
        }
        exactLayout(loaded,items,original);
        if (initialDesktop) {
            FOLDERVIEWMODE actualMode = FVM_AUTO; int size = 0;
            require(SUCCEEDED(loaded->GetViewModeAndIconSize(&actualMode,&size)) && actualMode == mode.mode && size == pixels,
                "Initial native bag changed the expected view");
            require(SUCCEEDED(PSPropertyBag_Delete(bag,key.c_str())),"Cannot remove the temporary native position blob");
            exactLayout(loaded,items,original);
            std::printf("QUALIFIED mode=%s coordinates=5000 initial_desktop=1 temporary_blob_removed=1\n",mode.name); std::fflush(stdout);
            continue;
        }
        loaded.Release(); candidate.closeDump();
        std::vector<POINT> target;
        for (size_t i = 0; i < items.ids.size(); ++i)
            target.push_back(filespacer::scaleDPI(POINT{37 + static_cast<LONG>(i % 50) * 317,43 + static_cast<LONG>(i / 50) * 307}));
        const auto nativeBytes = legacyBlob(items,target), publicBytes = positionBytes(items,target);
        for (int repetition = 0; repetition < 3; ++repetition) for (int arm = 0; arm < 2; ++arm) {
            const bool native = (arm == 0) == (repetition % 2 == 0);
            if (native) writeLegacy(bag,key,nativeBytes);
            else require(SUCCEEDED(PSPropertyBag_Delete(bag,key.c_str())),"Cannot remove native import for public control");
            DumpHost owner; const double start = now(); owner.createDump(fixture,mode.mode,false,{},name);
            auto currentView = owner.dumpView();
            if (!native) require(SUCCEEDED(filespacer::restoreIconPositions(currentView,publicBytes,current)),"Public full restoration failed");
            owner.paint(); const double total = now() - start;
            exactLayout(currentView,items,target);
            std::printf("NATIVE_LOAD mode=%s rep=%d path=%s total_ms=%.3f coordinates=5000 identities=1 selection=1\n",
                mode.name,repetition,native?"legacy":"public",total); std::fflush(stdout);
            // An imported legacy snapshot must not replay over a later native move.
            if (native) {
                Snapshot one; one.ids = {items.ids[7]}; auto expected = target;
                expected[7] = filespacer::scaleDPI(POINT{89,101});
                require(SUCCEEDED(filespacer::restoreIconPositions(currentView,positionBytes(one,{expected[7]}),current)),"Cannot move imported layout item");
                owner.refresh(); exactLayout(owner.dumpView(),items,expected);
                std::printf("LIFECYCLE mode=%s rep=%d native_move_and_refresh=1\n",mode.name,repetition);
            }
        }
        std::printf("QUALIFIED mode=%s coordinates=5000 paired_rows=6\n",mode.name); std::fflush(stdout);
    }
}
}
#ifndef NATIVE_POSITION_LOAD_DRIVER
int wmain(int argc, wchar_t **argv) {
    if (argc != 3 && !(argc == 4 && std::wcscmp(argv[3],L"desktop") == 0)) return 2;
    wchar_t machine[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine,&length) || _wcsicmp(machine,L"AGENTWIN10") != 0) return 3;
    FILE *recorded = nullptr;
    if (_wfreopen_s(&recorded,(std::wstring(argv[2])+L"\\native-output.log").c_str(),L"w",stdout) != 0) return 2;
    std::setvbuf(stdout,nullptr,_IONBF,0);
    if (FAILED(OleInitialize(nullptr))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI();
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr); type.lpfnWndProc = dumpWindowProc;
    type.lpszClassName = L"Order transfer prototype";
    if (!RegisterClassW(&type)) { OleUninitialize(); return 2; }
    int result = 0;
    try { nativePositionLoad(argv[1],argv[2],argc == 4); }
    catch (const std::exception &error) { std::fprintf(stderr,"FAIL native position load: %s\n",error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName,type.hInstance); OleUninitialize(); return result;
}
#endif
