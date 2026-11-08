// Investigation only: native state persistence versus explicit spatial coordinates.
#define wmain orderTransferReferenceMain
#include "OrderTransferTests.cpp"
#undef wmain
#include "ShellViewIcons.h"
#include "DPI.h"
#include <propsys.h>

namespace {
std::vector<uint8_t> bytesOf(IStream *stream) {
    STATSTG stat = {}; LARGE_INTEGER start = {};
    require(stream && SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME))
        && stat.cbSize.QuadPart <= INT32_MAX, "Cannot inspect state stream");
    std::vector<uint8_t> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    require(SUCCEEDED(stream->Seek(start, STREAM_SEEK_SET, nullptr))
        && (bytes.empty() || SUCCEEDED(IStream_Read(stream, bytes.data(), static_cast<ULONG>(bytes.size())))),
        "Cannot read state stream");
    return bytes;
}
void writeBytes(const std::wstring &path, const std::vector<uint8_t> &bytes) {
    FILE *raw = nullptr;
    require(_wfopen_s(&raw, path.c_str(), L"wb") == 0, "Cannot open state artifact");
    std::unique_ptr<FILE, decltype(&std::fclose)> file(raw, &std::fclose);
    require(bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), file.get()) == bytes.size(),
        "Cannot write state artifact");
}
struct StateBrowser : IShellBrowser, IServiceProvider, ICommDlgBrowser2 {
    LONG references = 1; HWND window = nullptr; unsigned reads = 0, writes = 0;
    CComPtr<IStream> state;
    IShellView *active = nullptr; // Borrowed until the host destroys its view.
    struct ServiceKey { GUID service, iid; } seen[64] = {};
    unsigned seenCount = 0;
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IServiceProvider) *out = static_cast<IServiceProvider *>(this);
        else if (iid == IID_ICommDlgBrowser || iid == IID_ICommDlgBrowser2) *out = static_cast<ICommDlgBrowser2 *>(this);
        else if (iid == IID_IUnknown || iid == IID_IOleWindow || iid == IID_IShellBrowser) *out = static_cast<IShellBrowser *>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP QueryService(REFGUID service, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (seenCount < _countof(seen) && std::none_of(seen,seen+seenCount,[&](const ServiceKey &key) {
                return key.service == service && key.iid == iid;
            })) {
            seen[seenCount++] = {service,iid};
            wchar_t serviceName[40] = {}, interfaceName[40] = {};
            if (StringFromGUID2(service,serviceName,_countof(serviceName))
                    && StringFromGUID2(iid,interfaceName,_countof(interfaceName)))
                std::printf("SERVICE sid=%ls iid=%ls\n",serviceName,interfaceName);
        }
        if (service == SID_SFolderView && active) return active->QueryInterface(iid,out);
        return service == SID_STopLevelBrowser || service == SID_SExplorerBrowserFrame ? QueryInterface(iid,out) : E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG left = InterlockedDecrement(&references); if (!left) delete this; return left; }
    STDMETHODIMP GetWindow(HWND *out) override { if (!out) return E_POINTER; *out = window; return S_OK; }
    STDMETHODIMP ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }
    STDMETHODIMP InsertMenusSB(HMENU, LPOLEMENUGROUPWIDTHS) override { return E_NOTIMPL; }
    STDMETHODIMP SetMenuSB(HMENU, HOLEMENU, HWND) override { return S_OK; }
    STDMETHODIMP RemoveMenusSB(HMENU) override { return S_OK; }
    STDMETHODIMP SetStatusTextSB(LPCWSTR) override { return S_OK; }
    STDMETHODIMP EnableModelessSB(BOOL) override { return S_OK; }
    STDMETHODIMP TranslateAcceleratorSB(MSG *, WORD) override { return S_FALSE; }
    STDMETHODIMP BrowseObject(PCUIDLIST_RELATIVE, UINT) override { return E_NOTIMPL; }
    STDMETHODIMP GetViewStateStream(DWORD mode, IStream **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        const bool writing = (mode & (STGM_WRITE | STGM_READWRITE)) != 0;
        if (writing) ++writes; else ++reads;
        if (!state) state.Attach(SHCreateMemStream(nullptr, 0));
        if (!state) return E_OUTOFMEMORY;
        if (writing) { ULARGE_INTEGER empty = {}; const HRESULT hr = state->SetSize(empty); if (FAILED(hr)) return hr; }
        CComPtr<IStream> copy;
        HRESULT hr = state->Clone(&copy); LARGE_INTEGER start = {};
        if (SUCCEEDED(hr)) hr = copy->Seek(start, STREAM_SEEK_SET, nullptr);
        std::printf("STATE_STREAM access=%lu hr=%08lX\n",mode,static_cast<unsigned long>(hr));
        std::fflush(stdout);
        return FAILED(hr) ? hr : copy.CopyTo(out);
    }
    STDMETHODIMP GetControlWindow(UINT, HWND *out) override { if (out) *out = nullptr; return E_NOTIMPL; }
    STDMETHODIMP SendControlMsg(UINT, UINT, WPARAM, LPARAM, LRESULT *out) override { if (out) *out = 0; return E_NOTIMPL; }
    STDMETHODIMP QueryActiveShellView(IShellView **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        return active ? active->QueryInterface(IID_PPV_ARGS(out)) : E_FAIL;
    }
    STDMETHODIMP OnViewWindowActive(IShellView *) override { return S_OK; }
    STDMETHODIMP SetToolbarItems(LPTBBUTTONSB, UINT, UINT) override { return E_NOTIMPL; }
    STDMETHODIMP OnDefaultCommand(IShellView *) override { return S_FALSE; }
    STDMETHODIMP OnStateChange(IShellView *, ULONG) override { return S_OK; }
    STDMETHODIMP IncludeObject(IShellView *, PCUITEMID_CHILD) override { return S_OK; }
    STDMETHODIMP Notify(IShellView *, DWORD) override { return S_OK; }
    STDMETHODIMP GetDefaultMenuText(IShellView *, LPWSTR, int) override { return S_FALSE; }
    STDMETHODIMP GetViewFlags(DWORD *flags) override { if (!flags) return E_POINTER; *flags = CDB2GVF_NOSELECTVERB; return S_OK; }
};
LRESULT CALLBACK dumpWindowProc(HWND window, UINT message, WPARAM first, LPARAM second) {
    // Same compatibility hook as FolderWindow; observed Shell hosting behavior,
    // not an extra persistence contract. The stream still uses public COM calls.
    if (message == WM_USER + 7) return GetWindowLongPtrW(window,GWLP_USERDATA);
    return DefWindowProcW(window,message,first,second);
}
struct DumpHost : Host {
    CComPtr<StateBrowser> stateBrowser;
    bool streamBacked = false;
    ~DumpHost() { closeDump(); }
    void closeDump() {
        if (browser) IUnknown_SetSite(browser,nullptr);
        if (stateBrowser) stateBrowser->active = nullptr;
        close(); stateBrowser.Release();
    }
    void createDump(const wchar_t *path, FOLDERVIEWMODE mode, bool useStream,
            const std::vector<uint8_t> &saved, const std::wstring &bag, DWORD extraFlags = 0) {
        streamBacked = useStream;
        frame.reset(CreateWindowW(L"Order transfer prototype", L"Position dump investigation",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 850, 500,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
        require(frame != nullptr, "No dump host window");
        RECT bounds = {0,0,800,400};
        const FOLDERSETTINGS settings = {static_cast<UINT>(saved.empty() ? mode : FVM_AUTO), static_cast<UINT>(FWF_NOWEBVIEW | FWF_NOHEADERINALLVIEWS | extraFlags)};
        events.Attach(new BrowserEvents);
        if (streamBacked) {
            stateBrowser.Attach(new StateBrowser); stateBrowser->window = frame.get();
            stateBrowser->state.Attach(SHCreateMemStream(saved.empty() ? nullptr : saved.data(), static_cast<UINT>(saved.size())));
            require(stateBrowser->state != nullptr, "No owned persistence stream");
            SetLastError(ERROR_SUCCESS);
            const auto previous = SetWindowLongPtrW(frame.get(),GWLP_USERDATA,reinterpret_cast<LONG_PTR>(static_cast<IShellBrowser *>(stateBrowser.p)));
            require(previous != 0 || GetLastError() == ERROR_SUCCESS, "Cannot register browser compatibility hook");
        }
        require(SUCCEEDED(browser.CoCreateInstance(CLSID_ExplorerBrowser))
            && SUCCEEDED(browser->QueryInterface(IID_PPV_ARGS(&options))), "No browser for native state");
        const auto profile = static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING);
        auto flags = static_cast<EXPLORER_BROWSER_OPTIONS>(EBO_NOBORDER | EBO_NOTRAVELLOG);
        if (useStream) flags = static_cast<EXPLORER_BROWSER_OPTIONS>(flags | EBO_NOPERSISTVIEWSTATE);
        require(SUCCEEDED(options->SetFolderViewOptions(profile,profile))
            && SUCCEEDED(browser->SetOptions(flags))
            && SUCCEEDED(browser->Initialize(frame.get(),&bounds,&settings))
            && SUCCEEDED(browser->SetPropertyBag(bag.c_str())), "Cannot configure isolated native persistence");
        if (streamBacked) {
            require(SUCCEEDED(IUnknown_SetSite(browser,static_cast<IShellBrowser *>(stateBrowser.p))), "Cannot install stream site");
        }
        require(SUCCEEDED(browser->Advise(events, &cookie)), "Cannot observe dump browser"); advised = true;
        CComPtr<IShellItem> folder;
        require(SUCCEEDED(SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&folder)))
            && SUCCEEDED(browser->BrowseToObject(folder, 0)), "Cannot navigate dump browser");
        waitFor([&] { return events->enumeration && events->enumeration->done && events->navigation; },
            "Dump view enumeration did not complete");
        if (stateBrowser) stateBrowser->active = events->shell;
        require(SUCCEEDED(events->observation), "Cannot observe dump enumeration");
        int count = 0;
        require(SUCCEEDED(dumpView()->ItemCount(SVGIO_ALLVIEW, &count)) && count == 5000, "Wrong dump fixture population");
        HWND body = nullptr;
        require(SUCCEEDED(events->shell->GetWindow(&body)) && body
            && SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_NOFOCUS)), "Cannot activate dump view");
        ShowWindow(body,SW_SHOW);
        require(IsWindowVisible(body) != FALSE, "Dump view is not visible");
    }
    CComPtr<IFolderView2> dumpView() {
        CComQIPtr<IFolderView2> result(events->shell); require(result != nullptr, "No dump folder view"); return result;
    }
};
std::vector<uint8_t> positionBytes(const Snapshot &items, const std::vector<POINT> &points) {
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(nullptr, 0)); require(stream != nullptr, "No coordinate stream");
    for (size_t i = 0; i < items.ids.size(); ++i) {
        const POINT point = filespacer::invScaleDPI(points[i]);
        require(SUCCEEDED(IStream_WritePidl(stream, items.ids[i])) && SUCCEEDED(IStream_Write(stream, &point, sizeof(point))), "Cannot encode coordinates");
    }
    return bytesOf(stream);
}
std::vector<POINT> readPositions(IFolderView2 *view, const Snapshot &items) {
    std::vector<POINT> result(items.ids.size());
    for (size_t i = 0; i < result.size(); ++i)
        require(SUCCEEDED(view->GetItemPosition(items.ids[i], &result[i])), "Native position unavailable");
    return result;
}
size_t samePositions(const std::vector<POINT> &a, const std::vector<POINT> &b) {
    require(a.size() == b.size(), "Wrong coordinate population"); size_t same = 0;
    for (size_t i = 0; i < a.size(); ++i) if (a[i].x == b[i].x && a[i].y == b[i].y) ++same;
    return same;
}
void positionDump(const wchar_t *fixture, const wchar_t *output) {
    const struct Mode { const char *name; FOLDERVIEWMODE mode; } modes[] = {{"icons",FVM_ICON},{"tiles",FVM_TILE}};
    for (const auto &mode : modes) for (const bool stream : {true,false}) {
        const std::wstring label = std::wstring(stream ? L"stream-" : L"bag-") + std::wstring(mode.name, mode.name + std::strlen(mode.name));
        const std::wstring bag = L"FileSpacer.PositionDump." + label + L"." + std::to_wstring(GetCurrentProcessId());
        DumpHost owner; owner.createDump(fixture, mode.mode, stream, {}, bag);
        auto view = owner.dumpView();
        require(SUCCEEDED(view->SetViewModeAndIconSize(mode.mode,48))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID,0))
            && view->GetAutoArrange() == S_FALSE, "Cannot establish free dump layout");
        const auto items = capture(view); require(items.ids.size() == 5000, "Wrong dump identities");
        // Establish capability with two unmistakable displaced identities before
        // paying for a full 5000-item movement, especially in free Tiles.
        Snapshot moved; moved.ids = {items.ids[0],items.ids[4000]};
        const std::vector<POINT> targets = {filespacer::scaleDPI(POINT{37,43}),filespacer::scaleDPI(POINT{6007,9009})};
        const std::vector<POINT> otherTargets = {filespacer::scaleDPI(POINT{56,66}),filespacer::scaleDPI(POINT{6026,9032})};
        const auto current = [] { return true; };
        const auto coordinates = positionBytes(moved,targets);
        std::printf("SEED mode=%s path=%s moved=2 population=5000\n",mode.name,stream?"stream":"bag"); std::fflush(stdout);
        double began = now();
        require(SUCCEEDED(filespacer::restoreIconPositions(view, coordinates, current)), "Cannot seed full saved coordinates");
        const double seedMs = now() - began;
        require(samePositions(readPositions(view,moved),targets) == 2, "Coordinate sentinel seed is incomplete");
        const auto a = readPositions(view,items);
        began = now(); const HRESULT savedHr = owner.events->shell->SaveViewState();
        const double saveMs = now() - began;
        std::printf("SAVE mode=%s stream=%d hr=%08lX seed_ms=%.3f\n",mode.name,stream,static_cast<unsigned long>(savedHr),seedMs); std::fflush(stdout);
        require(SUCCEEDED(savedHr), "Cannot save native state A");
        const auto saved = stream ? bytesOf(owner.stateBrowser->state) : std::vector<uint8_t>{};
        std::vector<uint8_t> control;
        if (stream) writeBytes(std::wstring(output) + L"\\" + label + L"-A.bin",saved);
        // Read-only capability probes, never reinterpret an absent private interface.
        CComPtr<IPersistStream> persist; CComPtr<IPersistStreamInit> initialized;
        const HRESULT persistHr = owner.events->shell->QueryInterface(IID_PPV_ARGS(&persist));
        const HRESULT initHr = owner.events->shell->QueryInterface(IID_PPV_ARGS(&initialized));
        if (stream) {
            require(SUCCEEDED(filespacer::restoreIconPositions(view,positionBytes(moved,otherTargets),current))
                && samePositions(readPositions(view,moved),otherTargets) == 2
                && SUCCEEDED(owner.events->shell->SaveViewState()), "Cannot save distinct native state B");
            const auto other = bytesOf(owner.stateBrowser->state);
            writeBytes(std::wstring(output) + L"\\" + label + L"-B.bin",other);
            std::printf("STREAM mode=%s bytes=%zu same_after_2_moves=%d reads=%u writes=%u\n",
                mode.name,saved.size(),saved==other,owner.stateBrowser->reads,owner.stateBrowser->writes);
            // Positive control: persisted state must react to an ordinary view-setting change.
            require(SUCCEEDED(view->SetViewModeAndIconSize(FVM_SMALLICON,16))
                && SUCCEEDED(owner.events->shell->SaveViewState()), "Cannot save setting control");
            control = bytesOf(owner.stateBrowser->state);
            if (!owner.stateBrowser->reads || !owner.stateBrowser->writes) {
                std::printf("UNQUALIFIED mode=%s path=stream reason=stream_not_used reads=%u writes=%u\n",
                    mode.name,owner.stateBrowser->reads,owner.stateBrowser->writes);
                std::fflush(stdout);
                continue;
            }
            require(control != other, "State stream is not responding to settings");
            writeBytes(std::wstring(output) + L"\\" + label + L"-setting.bin",control);
        }
        view.Release(); owner.closeDump();
        if (!stream) {
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> folderId;
            require(SUCCEEDED(SHParseDisplayName(fixture,nullptr,&folderId,0,nullptr)), "No bag folder identity");
            for (const wchar_t *name : {bag.c_str(),L"Shell"}) {
                const bool application = name == bag.c_str();
                CComPtr<IPropertyBag> properties; CComPtr<IStream> layouts; DWORD persistedMode = 0;
                const HRESULT bagHr = SHGetViewStatePropertyBag(folderId,name,SHGVSPB_FOLDERNODEFAULTS,IID_PPV_ARGS(&properties));
                const HRESULT modeHr = properties ? PSPropertyBag_ReadDWORD(properties,L"Mode",&persistedMode) : bagHr;
                const HRESULT layoutHr = properties ? PSPropertyBag_ReadStream(properties,L"IconLayouts",&layouts) : bagHr;
                std::vector<uint8_t> layoutBytes;
                if (SUCCEEDED(layoutHr) && layouts) {
                    layoutBytes = bytesOf(layouts);
                    writeBytes(std::wstring(output) + L"\\" + label + (application ? L"-app-IconLayouts.bin" : L"-native-IconLayouts.bin"),layoutBytes);
                }
                std::printf("BAG mode=%s scope=%s bag_hr=%08lX mode_hr=%08lX mode_value=%lu layout_hr=%08lX layout_bytes=%zu\n",
                    mode.name,application?"app":"native",static_cast<unsigned long>(bagHr),static_cast<unsigned long>(modeHr),persistedMode,
                    static_cast<unsigned long>(layoutHr),layoutBytes.size());
            }
        }
        if (stream) {
            DumpHost settingControl; settingControl.createDump(fixture,mode.mode,true,control,bag + L".control");
            FOLDERVIEWMODE controlMode = FVM_AUTO; int controlSize = 0;
            require(settingControl.stateBrowser->reads > 0
                && SUCCEEDED(settingControl.dumpView()->GetViewModeAndIconSize(&controlMode,&controlSize))
                && controlMode == FVM_SMALLICON && controlSize == 16, "State setting control did not reload");
            std::printf("SETTING_CONTROL mode=%s read_back=1\n",mode.name);
        }
        DumpHost reopened; began = now(); reopened.createDump(fixture,mode.mode,stream,saved,bag);
        const double reopenMs = now() - began;
        auto restored = reopened.dumpView();
        const auto fresh = capture(restored);
        auto names = fresh.names, expectedNames = items.names;
        std::sort(names.begin(),names.end()); std::sort(expectedNames.begin(),expectedNames.end());
        require(names == expectedNames, "Native state reopen changed identities");
        const auto observed = readPositions(restored,items);
        const size_t matched = samePositions(observed,a);
        FOLDERVIEWMODE reopenedMode = FVM_AUTO; int reopenedSize = 0;
        require(SUCCEEDED(restored->GetViewModeAndIconSize(&reopenedMode,&reopenedSize)), "No reopened mode");
        require(!stream || (reopened.stateBrowser->reads > 0 && reopenedMode == mode.mode && reopenedSize == 48),
            "Native state did not reload ordinary view settings");
        const unsigned sentinels = (observed[0].x == a[0].x && observed[0].y == a[0].y ? 1u : 0u)
            + (observed[4000].x == a[4000].x && observed[4000].y == a[4000].y ? 1u : 0u);
        std::printf("DUMP mode=%s path=%s seed_ms=%.3f save_ms=%.3f reopen_ms=%.3f restored_positions=%zu sentinels=%u total=5000 mode_after=%u size_after=%d auto_after=%ld stream_reads=%u persist_hr=%08lX init_hr=%08lX identities=1\n",
            mode.name,stream?"stream":"bag",seedMs,saveMs,reopenMs,matched,sentinels,static_cast<unsigned>(reopenedMode),reopenedSize,
            static_cast<long>(restored->GetAutoArrange()),stream?reopened.stateBrowser->reads:0,
            static_cast<unsigned long>(persistHr),static_cast<unsigned long>(initHr));
        std::fflush(stdout);
        if (!stream) {
            // Prove the native bag actually persists an ordinary setting before
            // treating missing positions as a negative serialization result.
            require(SUCCEEDED(restored->SetViewModeAndIconSize(FVM_SMALLICON,16))
                && SUCCEEDED(reopened.events->shell->SaveViewState()), "Cannot save native bag setting control");
            restored.Release(); reopened.closeDump();
            DumpHost controlHost; controlHost.createDump(fixture,FVM_AUTO,false,{},bag);
            FOLDERVIEWMODE controlMode = FVM_AUTO; int controlSize = 0;
            require(SUCCEEDED(controlHost.dumpView()->GetViewModeAndIconSize(&controlMode,&controlSize)), "Cannot read native bag control");
            const bool applied = controlMode == FVM_SMALLICON && controlSize == 16;
            std::printf("BAG_CONTROL mode=%s setting_reloaded=%d actual_mode=%u actual_size=%d\n",
                mode.name,applied,static_cast<unsigned>(controlMode),controlSize);
            std::fflush(stdout);
            require(applied,"Native property bag did not reload its ordinary setting");
            auto controlView = controlHost.dumpView();
            require(SUCCEEDED(controlView->SetViewModeAndIconSize(FVM_DETAILS,16))
                && SUCCEEDED(controlHost.events->shell->SaveViewState()), "Cannot save contrasting bag control");
            controlView.Release(); controlHost.closeDump();
            DumpHost contrasting; contrasting.createDump(fixture,FVM_AUTO,false,{},bag);
            require(SUCCEEDED(contrasting.dumpView()->GetViewModeAndIconSize(&controlMode,&controlSize))
                && controlMode == FVM_DETAILS, "Contrasting bag setting did not reload");
            std::printf("BAG_CONTRAST mode=%s read_back_details=1\n",mode.name);
        }
    }
}
}
#ifndef POSITION_DUMP_DRIVER
int wmain(int argc, wchar_t **argv) {
    if (argc != 3) return 2;
    wchar_t machine[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine,&length) || _wcsicmp(machine,L"AGENTWIN10") != 0) return 3;
    if (FAILED(OleInitialize(nullptr))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI();
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr); type.lpfnWndProc = dumpWindowProc;
    type.lpszClassName = L"Order transfer prototype";
    if (!RegisterClassW(&type)) { OleUninitialize(); return 2; }
    int result = 0;
    try { positionDump(argv[1],argv[2]); }
    catch (const std::exception &error) { std::fprintf(stderr,"FAIL position dump: %s\n",error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName,type.hInstance); OleUninitialize(); return result;
}
#endif
