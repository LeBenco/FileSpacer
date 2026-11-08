// Exact-image experiment only: optional native settings transaction, no code patching.
#define NATIVE_POSITION_LOAD_DRIVER
#include "NativePositionLoadTests.cpp"
#include "ShellSelectionRuntime.h"
#include <processsnapshot.h>
#include <dbghelp.h>

namespace {
namespace nativeImage = filespacer::selectionDetail;
class TransactionRedrawGuard {
    struct State {
        HWND window = nullptr;
        HWND view = nullptr;
        bool holding = true;
        bool detachedOwner = false; unsigned suppressed = 0;
        DWORD layoutStyles = 0; bool restoreLayout = false;
    };
    struct Search { HWND window = nullptr; unsigned count = 0; };
    static BOOL CALLBACK findList(HWND window, LPARAM data) {
        wchar_t name[32] = {};
        DWORD process = 0;
        if (GetClassNameW(window, name, _countof(name)) && wcscmp(name, WC_LISTVIEWW) == 0
                && GetWindowThreadProcessId(window, &process) == GetCurrentThreadId()
                && process == GetCurrentProcessId()) {
            auto &search = *reinterpret_cast<Search *>(data);
            search.window = window;
            ++search.count;
        }
        return TRUE;
    }
    static LRESULT CALLBACK subclass(HWND window, UINT message, WPARAM wParam,
            LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
        auto state = reinterpret_cast<State *>(data);
        if (message == WM_SETREDRAW && wParam && state->holding) { ++state->suppressed; return 0; }
        if (message == WM_NCDESTROY) {
            state->window = nullptr;
            RemoveWindowSubclass(window, subclass, id);
            if (state->detachedOwner) delete state;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }
    CComPtr<IFolderView2> redraw;
    std::unique_ptr<State> state;
public:
    TransactionRedrawGuard(IFolderView *view, const std::function<bool()> &current, bool suspendLayout = false) {
        CComQIPtr<IFolderView2> candidate(view);
        if (!current() || !candidate) return;
        CComQIPtr<IShellView> source(view);
        if (!current() || !source) return;
        HWND parent = nullptr;
        const HRESULT result = source->GetWindow(&parent);
        if (!current() || FAILED(result) || !parent || !IsWindowVisible(parent)) return;
        DWORD process = 0;
        if (GetWindowThreadProcessId(parent, &process) != GetCurrentThreadId()
                || process != GetCurrentProcessId()) return;
        Search search;
        EnumChildWindows(parent, findList, reinterpret_cast<LPARAM>(&search));
        DWORD_PTR existing = 0;
        if (search.count != 1 || (!suspendLayout && (!IsWindowVisible(search.window)
                || GetPropW(search.window, L"SysSetRedraw")))
                || GetWindowSubclass(search.window, subclass, 1, &existing)) return;
        state.reset(new State);
        state->window = search.window;
        state->view = parent;
        if (!SetWindowSubclass(search.window, subclass, 1, reinterpret_cast<DWORD_PTR>(state.get()))) {
            state.reset();
            return;
        }
        if (state && state->window && suspendLayout) {
            state->holding = false; // Only borrow the control for its styles; never resume another owner's redraw.
            const DWORD mask = LVS_EX_AUTOAUTOARRANGE | LVS_EX_SNAPTOGRID;
            state->layoutStyles = ListView_GetExtendedListViewStyle(state->window);
            state->restoreLayout = true;
            ListView_SetExtendedListViewStyleEx(state->window,mask,0);
            std::printf("LAYOUT_STYLES before=%08lX suspended=%08lX\n",state->layoutStyles,
                ListView_GetExtendedListViewStyle(state->window)); std::fflush(stdout);
        } else {
            redraw = candidate;
            const HRESULT suspended = redraw->SetRedraw(FALSE);
            if (FAILED(suspended) || !current()) finish();
        }
    }
    ~TransactionRedrawGuard() { finish(); }
    TransactionRedrawGuard(const TransactionRedrawGuard &) = delete;
    TransactionRedrawGuard &operator=(const TransactionRedrawGuard &) = delete;
    bool layoutSuspended() const {
        return state && state->window && state->restoreLayout
            && !(ListView_GetExtendedListViewStyle(state->window)&(LVS_EX_AUTOAUTOARRANGE|LVS_EX_SNAPTOGRID));
    }
    bool redrawSuspended() const { return state && state->window && redraw; }
    HRESULT finish() {
        if (!state) return S_OK;
        std::printf("REDRAW_GUARD suppressed=%u alive=%d\n",state->suppressed,state->window != nullptr); std::fflush(stdout);
        HRESULT result = S_OK;
        if (state->window && state->restoreLayout) {
            const DWORD mask = LVS_EX_AUTOAUTOARRANGE | LVS_EX_SNAPTOGRID;
            ListView_SetExtendedListViewStyleEx(state->window,mask,state->layoutStyles);
            if ((ListView_GetExtendedListViewStyle(state->window)&mask) != (state->layoutStyles&mask)) result = E_FAIL;
            std::printf("LAYOUT_RESTORED styles=%08lX hr=%08lX\n",ListView_GetExtendedListViewStyle(state->window),
                static_cast<unsigned long>(result)); std::fflush(stdout);
        }
        state->holding = false;
        // WM_NCDESTROY clears window before a handle can be reused. Resume our
        // original control even when the caller's active generation changed.
        // Only the Shell resumes its control; a failure leaves it untouched.
        if (state->window) {
            const HRESULT resumed = redraw ? redraw->SetRedraw(TRUE) : S_OK;
            if (SUCCEEDED(result)) result = resumed;
            if (state->window) {
                if (RemoveWindowSubclass(state->window, subclass, 1)) state->window = nullptr;
                if (SUCCEEDED(result))
                    RedrawWindow(state->view, nullptr, nullptr,
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
            }
        }
        if (state->window) {
            // If removal fails, keep a disarmed heap object until NCDESTROY.
            // It must never point into a completed restoration's stack frame.
            state->detachedOwner = true;
            (void)state.release();
        } else state.reset();
        redraw.Release();
        return result;
    }
};


struct SlowCapture {
    HANDLE done = nullptr; std::thread worker;
    explicit SlowCapture(const std::wstring &path,DWORD delay = 10000) {
        done = CreateEventW(nullptr,TRUE,FALSE,nullptr); require(done != nullptr,"No diagnostic event");
        try {
            worker = std::thread([this,path,delay] {
                if (WaitForSingleObject(done,delay) != WAIT_TIMEOUT) return;
                HPSS snapshot = nullptr;
                const DWORD error = PssCaptureSnapshot(GetCurrentProcess(),static_cast<PSS_CAPTURE_FLAGS>(
                    PSS_CAPTURE_VA_CLONE|PSS_CAPTURE_THREADS|PSS_CAPTURE_THREAD_CONTEXT|PSS_CAPTURE_VA_SPACE),CONTEXT_ALL,&snapshot);
                if (error) { std::printf("DIAGNOSTIC capture_error=%lu\n",error); std::fflush(stdout); return; }
                HANDLE file = CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                MINIDUMP_CALLBACK_INFORMATION callback = {};
                callback.CallbackRoutine = [](PVOID,const PMINIDUMP_CALLBACK_INPUT input,PMINIDUMP_CALLBACK_OUTPUT output)->BOOL {
                    if (input->CallbackType == IsProcessSnapshotCallback) output->Status = S_FALSE;
                    return TRUE;
                };
                const BOOL wrote = file != INVALID_HANDLE_VALUE && MiniDumpWriteDump(reinterpret_cast<HANDLE>(snapshot),
                    GetCurrentProcessId(),file,static_cast<MINIDUMP_TYPE>(MiniDumpNormal|MiniDumpWithThreadInfo),nullptr,nullptr,&callback);
                const DWORD dumpError = wrote ? ERROR_SUCCESS : GetLastError();
                if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
                PssFreeSnapshot(GetCurrentProcess(),snapshot);
                std::printf("DIAGNOSTIC dump=%ls error=%lu\n",path.c_str(),dumpError); std::fflush(stdout);
            });
        } catch (...) { CloseHandle(done); throw; }
    }
    ~SlowCapture() { SetEvent(done); if (worker.joinable()) worker.join(); CloseHandle(done); }
    SlowCapture(const SlowCapture &) = delete;
    SlowCapture &operator=(const SlowCapture &) = delete;
};
// Signatures and slots are checked against the inspected current-image vtable.
struct NativeSettings : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE onChanged(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE onLoad(IPropertyBag *) = 0;
    virtual HRESULT STDMETHODCALLTYPE onSave(IPropertyBag *) = 0;
};
struct PositionBag : IPropertyBag {
    LONG references = 1;
    DWORD mode = 0; std::vector<uint8_t> bytes; unsigned positionReads = 0;
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IPropertyBag) return E_NOINTERFACE;
        *out = static_cast<IPropertyBag *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG left = InterlockedDecrement(&references); if (!left) delete this; return left; }
    STDMETHODIMP Read(LPCOLESTR key, VARIANT *out, IErrorLog *) override {
        if (!key || !out) return E_POINTER; VariantInit(out);
        std::printf("READ_PROPERTY key=%ls\n",key); std::fflush(stdout);
        if (std::wcscmp(key,L"Mode") == 0) { out->vt = VT_UI4; out->ulVal = mode; return S_OK; }
        if (std::wcsncmp(key,L"ItemPos",7) == 0) {
            CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(bytes.data(),static_cast<UINT>(bytes.size())));
            if (!stream) return E_OUTOFMEMORY;
            out->vt = VT_UNKNOWN; out->punkVal = stream.Detach(); ++positionReads; return S_OK;
        }
        return E_FAIL; // The inspected optional native stream reader accepts E_FAIL as absence.
    }
    STDMETHODIMP Write(LPCOLESTR, VARIANT *) override { return E_ACCESSDENIED; }
};
CComPtr<NativeSettings> settingsSink(IShellView *shell, const nativeImage::Image &image) {
    const GUID service = {0xc30e04c9,0x7d34,0x4ad0,{0xa1,0xdc,0xaf,0xff,0xae,0xc1,0xb2,0x52}};
    const GUID controlId = {0xc401f7ac,0x9572,0x42cd,{0xae,0x05,0x3b,0xd0,0x84,0x27,0x0c,0xb1}};
    CComQIPtr<IServiceProvider> services(shell); CComPtr<IUnknown> control; void *rawControl = nullptr;
    require(services && SUCCEEDED(services->QueryService(service,controlId,&rawControl)) && rawControl,"No qualified native host service");
    control.Attach(static_cast<IUnknown *>(rawControl));
    void *table = nullptr;
    require(nativeImage::read(control.p,table) && table == image.base + image.fields[nativeImage::HostTable],"Native host vtable mismatch");
    auto root = reinterpret_cast<unsigned char *>(control.p) - image.fields[nativeImage::HostOffset];
    DWORD flags = 0; void *model = nullptr, *bag = nullptr, *positions = nullptr; int count = -1;
    require(nativeImage::read(root+0x408,flags) && nativeImage::read(root+0xd8,model)
        && nativeImage::read(root+0x1d0,bag) && nativeImage::read(root+0xc0,positions),"Cannot inspect native layout ownership");
    if (positions) nativeImage::read(positions,count);
    std::printf("MODEL flags=%08lX data_model=%d property_bag=%d legacy_count=%d host_offset=%lu\n",
        flags,model!=nullptr,bag!=nullptr,count,image.fields[nativeImage::HostOffset]);
    // Reuse the existing automatic selection descriptor's QISearch anchor.
    LONG displacement = 0;
    require(nativeImage::read(image.base+image.fields[nativeImage::HostQI]+9,displacement),"No host QITAB displacement");
    const LONGLONG qit = static_cast<LONGLONG>(image.fields[nativeImage::HostQI])+13+displacement;
    require(qit >= 0 && qit < image.size,"Host QITAB outside image");
    const DWORD load = 0xd4420, save = 0xd1030; // Exact inspected VM image; never a production profile.
    require(image.cfg(load) && image.cfg(save),"Native settings methods are not CFG-qualified");
    CComPtr<NativeSettings> result;
    for (DWORD i = 0; i < 64; ++i) {
        struct Entry { const GUID *id; DWORD offset,padding; } entry = {};
        require(image.range(static_cast<DWORD>(qit)+16*i,sizeof(entry))
            && nativeImage::read(image.base+qit+16*i,entry),"Cannot read host QITAB");
        if (!entry.id) break;
        if (entry.offset != 0x50) continue;
        GUID iid = {};
        require(nativeImage::read(entry.id,iid),"Cannot read settings IID");
        void *raw = nullptr;
        require(SUCCEEDED(control->QueryInterface(iid,&raw)) && raw,"No native settings interface");
        CComPtr<NativeSettings> candidate; candidate.Attach(static_cast<NativeSettings *>(raw));
        require(reinterpret_cast<unsigned char *>(candidate.p) == root+entry.offset,"Settings interface offset mismatch");
        require(nativeImage::read(candidate.p,table),"No settings vtable");
        const auto tableRva = reinterpret_cast<ULONG_PTR>(table)-reinterpret_cast<ULONG_PTR>(image.base);
        require(tableRva < image.size && image.range(static_cast<DWORD>(tableRva),64*sizeof(void *)),"Settings vtable outside image");
        int loadSlot = -1, saveSlot = -1;
        for (int slot = 0; slot < 64; ++slot) {
            void *method = nullptr;
            require(nativeImage::read(static_cast<unsigned char *>(table)+slot*sizeof(void *),method),"Unreadable settings method");
            if (method == image.base+load) loadSlot = slot;
            if (method == image.base+save) saveSlot = slot;
        }
        std::printf("SETTINGS_TABLE rva=%llX load_slot=%d save_slot=%d\n",static_cast<unsigned long long>(tableRva),loadSlot,saveSlot);
        void *actualLoad = nullptr, *actualSave = nullptr;
        require(nativeImage::read(static_cast<unsigned char *>(table)+4*sizeof(void *),actualLoad)
            && nativeImage::read(static_cast<unsigned char *>(table)+5*sizeof(void *),actualSave),"No settings methods");
        std::printf("SETTINGS load_match=%d save_match=%d\n",actualLoad==image.base+load,actualSave==image.base+save);
        require(actualLoad == image.base+load && actualSave == image.base+save,"Unknown native settings ABI");
        require(!result,"Ambiguous settings interface"); result = candidate;
    }
    require(result != nullptr,"Native settings interface absent"); return result;
}
struct TransactionEvents : IExplorerBrowserEvents {
    LONG references = 1; CComPtr<BrowserEvents> observer; CComPtr<PositionBag> bag;
    bool retainGate = false; CComPtr<IFolderView2> gatedView; DWORD savedFlags = 0, gateFlags = 0;
    const nativeImage::Image *image = nullptr; HRESULT setup = E_PENDING;
    HRESULT finishGate() {
        if (!gatedView) return S_OK;
        DWORD after = 0;
        const HRESULT result = gatedView->SetCurrentFolderFlags(savedFlags^gateFlags,savedFlags);
        if (FAILED(result)) return result;
        const HRESULT observed = gatedView->GetCurrentFolderFlags(&after);
        if (FAILED(observed)) return observed;
        if (after != savedFlags) return E_FAIL;
        gatedView.Release(); return S_OK;
    }
    STDMETHODIMP QueryInterface(REFIID iid,void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IExplorerBrowserEvents) return E_NOINTERFACE;
        *out = static_cast<IExplorerBrowserEvents *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG left = InterlockedDecrement(&references); if (!left) delete this; return left; }
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE id) override { return observer->OnNavigationPending(id); }
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE id) override { return observer->OnNavigationComplete(id); }
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE id) override { return observer->OnNavigationFailed(id); }
    STDMETHODIMP OnViewCreated(IShellView *shell) override {
        observer->OnViewCreated(shell);
        try {
            CComQIPtr<IFolderView2> view(shell); DWORD before = 0, during = 0, after = 0;
            require(view != nullptr,"No transaction folder view");
            if (!bag) { setup = S_OK; return S_OK; }
            auto sink = settingsSink(shell,*image);
            require(SUCCEEDED(view->GetCurrentFolderFlags(&before)),"No original folder flags");
            setup = view->SetCurrentFolderFlags(FWF_DESKTOP,FWF_DESKTOP);
            if (FAILED(setup)) return S_OK;
            const HRESULT read = view->GetCurrentFolderFlags(&during);
            if (SUCCEEDED(read)) setup = sink->onLoad(bag); else setup = read;
            if (retainGate && SUCCEEDED(setup)) {
                require(!gatedView,"Initial native load gate was reentered");
                gatedView = view; savedFlags = before; gateFlags = during;
                std::printf("TRANSACTION_EARLY before=%08lX during=%08lX\n",before,during);
                return S_OK;
            }
            const HRESULT restored = view->SetCurrentFolderFlags(SUCCEEDED(read) ? before^during : static_cast<DWORD>(FWF_DESKTOP),before);
            require(SUCCEEDED(restored) && SUCCEEDED(view->GetCurrentFolderFlags(&after)) && after == before,"Cannot restore folder flags after transaction");
            std::printf("TRANSACTION hr=%08lX before=%08lX during=%08lX after=%08lX\n",static_cast<unsigned long>(setup),before,during,after);
        } catch (const std::exception &error) {
            setup = E_FAIL; std::printf("SETUP_REJECT reason=%s\n",error.what());
        }
        std::fflush(stdout); return S_OK;
    }
};
struct TransactionHost : Host {
    CComPtr<TransactionEvents> transaction;
    void createTransaction(const wchar_t *fixture,FOLDERVIEWMODE mode,PositionBag *bag,const nativeImage::Image &image,int pixels = 48,bool retainGate = false,bool primeViewState = false) {
        frame.reset(CreateWindowW(L"Order transfer prototype",L"Native position transaction",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            40,40,850,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)); require(frame != nullptr,"No transaction frame");
        require(SUCCEEDED(browser.CoCreateInstance(CLSID_ExplorerBrowser)) && SUCCEEDED(browser->QueryInterface(IID_PPV_ARGS(&options))),"No transaction browser");
        const auto profile = static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING);
        RECT bounds = {0,0,800,400}; const FOLDERSETTINGS settings = {static_cast<UINT>(mode),FWF_NOWEBVIEW|FWF_NOHEADERINALLVIEWS};
        require(SUCCEEDED(options->SetFolderViewOptions(profile,profile))
            && SUCCEEDED(browser->SetOptions(static_cast<EXPLORER_BROWSER_OPTIONS>(EBO_NOBORDER|EBO_NOTRAVELLOG|(primeViewState?0:EBO_NOPERSISTVIEWSTATE))))
            && SUCCEEDED(browser->Initialize(frame.get(),&bounds,&settings)),"Cannot initialize transaction browser");
        if (primeViewState) require(SUCCEEDED(browser->SetPropertyBag((L"FileSpacer.PositionCacheProbe."+std::to_wstring(GetCurrentProcessId())).c_str())),
            "Cannot isolate primed native view state");
        events.Attach(new BrowserEvents); transaction.Attach(new TransactionEvents);
        transaction->observer = events; transaction->bag = bag; transaction->image = &image;
        transaction->retainGate = retainGate;
        require(SUCCEEDED(browser->Advise(transaction,&cookie)),"Cannot observe transaction browser"); advised = true;
        CComPtr<IShellItem> folder;
        require(SUCCEEDED(SHCreateItemFromParsingName(fixture,nullptr,IID_PPV_ARGS(&folder)))
            && SUCCEEDED(browser->BrowseToObject(folder,0)),"Cannot navigate transaction fixture");
        waitFor([&] { return events->navigation && events->enumeration && events->enumeration->done; },"Transaction enumeration did not complete");
        require(SUCCEEDED(events->observation),"Cannot observe transaction enumeration");
        require(SUCCEEDED(transaction->finishGate()),"Cannot restore the initial native load gate");
        HWND body = nullptr; RECT client = {};
        require(SUCCEEDED(events->shell->GetWindow(&body)) && body
            && SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_NOFOCUS))
            && IsWindowVisible(body) && GetClientRect(body,&client),"Cannot activate the completed transaction view");
        auto current = view(); FOLDERVIEWMODE actual = FVM_AUTO; int size = 0, count = 0;
        require(SUCCEEDED(current->SetViewModeAndIconSize(mode,pixels))
            && SUCCEEDED(current->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0))
            && SUCCEEDED(current->GetViewModeAndIconSize(&actual,&size)) && actual == mode && size == pixels
            && SUCCEEDED(current->ItemCount(SVGIO_ALLVIEW,&count)) && count == 5000,
            "Cannot establish the same completed free presentation");
        std::printf("PRESENTATION mode=%u size=%d width=%ld height=%ld visible=1\n",static_cast<unsigned>(actual),size,
            client.right-client.left,client.bottom-client.top); std::fflush(stdout);
        if (primeViewState) {
            EXPLORER_BROWSER_OPTIONS active = EBO_NONE;
            require(SUCCEEDED(browser->SetOptions(EBO_NOBORDER|EBO_NOTRAVELLOG|EBO_NOPERSISTVIEWSTATE))
                && SUCCEEDED(browser->GetOptions(&active)) && (active&EBO_NOPERSISTVIEWSTATE),"Cannot disable persistence after native state initialization");
            std::printf("VIEW_STATE_PRIMED persistence_disabled=1\n");
        }
    }
};
void positionTransaction(const wchar_t *fixture,const wchar_t *descriptor,const wchar_t *output,const wchar_t *layout,const wchar_t *publicStyle,int pixels) {
    const bool suspendLayout = std::wcscmp(publicStyle,L"native-layout") == 0;
    const bool desktopRole = std::wcscmp(publicStyle,L"desktop-role") == 0;
    const bool initialGate = std::wcscmp(publicStyle,L"native-initial") == 0;
    const bool iconStage = std::wcscmp(publicStyle,L"icon-stage") == 0;
    const bool primeViewState = std::wcscmp(publicStyle,L"view-state") == 0;
    const bool warmEnumeration = std::wcscmp(publicStyle,L"enumeration") == 0;
    const bool positionOnly = std::wcscmp(publicStyle,L"position-only") == 0;
    const bool arrangePrime = std::wcscmp(publicStyle,L"arrange-prime") == 0;
    const bool nativeOnly = initialGate || std::wcscmp(publicStyle,L"native-only") == 0;
    const auto image = nativeImage::Image::discover(descriptor); require(image.supported,image.reason);
    for (const auto mode : {FVM_ICON,FVM_TILE}) {
        const char *name = mode == FVM_ICON ? "icons" : "tiles";
        if ((mode == FVM_ICON) != (std::wcscmp(layout,L"icons") == 0)) continue;
        SlowCapture workflow(std::wstring(output)+L"\\"+std::wstring(name,name+std::strlen(name))+L"-workflow.dmp",60000);
        DumpHost source; source.createDump(fixture,mode,false,{},L"FileSpacer.TransactionFixture");
        auto view = source.dumpView(); FOLDERVIEWMODE defaultMode = FVM_AUTO; int defaultSize = 0;
        require(SUCCEEDED(view->GetViewModeAndIconSize(&defaultMode,&defaultSize)),"Cannot observe native defaults");
        std::printf("NATIVE_DEFAULT mode=%u size=%d\n",static_cast<unsigned>(defaultMode),defaultSize);
        require(SUCCEEDED(view->SetViewModeAndIconSize(mode,pixels))
            && SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0)),"Cannot seed free transaction view");
        const auto items = capture(view); require(items.ids.size() == 5000,"Wrong transaction population");
        std::vector<POINT> points; points.reserve(items.ids.size());
        for (size_t i = 0; i < items.ids.size(); ++i)
            points.push_back(filespacer::scaleDPI(POINT{37 + static_cast<LONG>(i % 50)*317,43 + static_cast<LONG>(i / 50)*307}));
        Snapshot moved; moved.ids = {items.ids[0],items.ids[4000]};
        CComPtr<PositionBag> bag; bag.Attach(new PositionBag); bag->mode = static_cast<DWORD>(mode);
        bag->bytes = legacyBlob(items,points);
        view.Release(); source.closeDump();
        double publicTotal = 0; std::vector<POINT> publicRefreshed;
        if (!nativeOnly) {
            TransactionHost control; const double publicStart = now();
            control.createTransaction(fixture,mode,nullptr,image,pixels,false,primeViewState);
            requirePresentation(control,mode,"public_before",pixels);
            require(SUCCEEDED(control.view()->SelectItem(7,SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed public-control focus");
            int publicFocus = -1;
            require(SUCCEEDED(control.view()->GetFocusedItem(&publicFocus)),"Cannot observe public-control focus");
            const HWND publicKeyboard = GetFocus();
            CComPtr<IEnumIDList> geometryLease;
            if (warmEnumeration) {
                const double warm = now();
                require(SUCCEEDED(control.view()->Items(SVGIO_ALLVIEW|SVGIO_FLAG_VIEWORDER,IID_PPV_ARGS(&geometryLease))) && geometryLease,
                    "Cannot warm the public view enumeration");
                std::printf("ENUMERATION_WARMUP ms=%.3f\n",now()-warm);
            }
            std::unique_ptr<TransactionRedrawGuard> stageRedraw;
            if (iconStage || arrangePrime) {
                stageRedraw.reset(new TransactionRedrawGuard(control.view(),[] { return true; }));
                require(stageRedraw->redrawSuspended(),"Cannot protect the intermediate Icons presentation");
                if (iconStage) require(SUCCEEDED(control.view()->SetViewModeAndIconSize(FVM_ICON,pixels)),"Cannot stage public placement in Icons");
            }
            if (arrangePrime) {
                const double prime = now(); DWORD before = 0, during = 0, after = 0;
                require(SUCCEEDED(control.view()->GetCurrentFolderFlags(&before))
                    && SUCCEEDED(control.view()->SetCurrentFolderFlags(FWF_AUTOARRANGE,FWF_AUTOARRANGE))
                    && SUCCEEDED(control.view()->GetCurrentFolderFlags(&during))
                    && SUCCEEDED(control.view()->SetCurrentFolderFlags(before^during,before))
                    && SUCCEEDED(control.view()->GetCurrentFolderFlags(&after)) && after == before,
                    "Cannot prime and restore automatic placement");
                std::printf("ARRANGE_PRIME ms=%.3f before=%08lX during=%08lX after=%08lX\n",now()-prime,before,during,after);
            }
            DWORD publicFlags = 0, roleFlags = 0, restoredFlags = 0;
            if (desktopRole) {
                require(SUCCEEDED(control.view()->GetCurrentFolderFlags(&publicFlags))
                    && SUCCEEDED(control.view()->SetCurrentFolderFlags(FWF_DESKTOP,FWF_DESKTOP))
                    && SUCCEEDED(control.view()->GetCurrentFolderFlags(&roleFlags)),"Cannot arm public Desktop-role trial");
                std::printf("PUBLIC_ROLE before=%08lX during=%08lX\n",publicFlags,roleFlags);
            }
            std::unique_ptr<TransactionRedrawGuard> publicLayout;
            if (suspendLayout) {
                publicLayout.reset(new TransactionRedrawGuard(control.view(),[] { return true; },true));
                require(publicLayout->layoutSuspended(),"Public layout-style suspension did not arm");
            }
            require(SUCCEEDED(control.transaction->setup),"Cannot initialize the public control");
            if (positionOnly) {
                int selected = -1;
                require(SUCCEEDED(control.view()->ItemCount(SVGIO_SELECTION,&selected)) && selected == 0,"Position-only probe requires an empty selection");
                filespacer::PreparedIconPositions prepared;
                require(SUCCEEDED(filespacer::prepareIconPositions(control.view(),positionBytes(items,points),[] { return true; },prepared)),
                    "Cannot prepare the position-only public control");
                TransactionRedrawGuard redraw(control.view(),[] { return true; });
                require(redraw.redrawSuspended(),"Cannot protect position-only placement");
                require(SUCCEEDED(control.view()->SelectAndPositionItems(static_cast<UINT>(prepared.items.size()),prepared.items.data(),prepared.points.data(),
                    SVSI_POSITIONITEM|SVSI_NOTAKEFOCUS)) && SUCCEEDED(redraw.finish()),"Position-only public placement failed");
            } else require(SUCCEEDED(filespacer::restoreIconPositions(control.view(),positionBytes(items,points),[] { return true; })),
                "Cannot restore the public control");
            if (publicLayout) require(SUCCEEDED(publicLayout->finish()),"Cannot restore public-control layout styles");
            if (desktopRole) require(SUCCEEDED(control.view()->SetCurrentFolderFlags(publicFlags^roleFlags,publicFlags))
                && SUCCEEDED(control.view()->GetCurrentFolderFlags(&restoredFlags)) && restoredFlags == publicFlags,
                "Cannot restore public Desktop-role trial flags");
            if (iconStage) require(SUCCEEDED(control.view()->SetViewModeAndIconSize(mode,pixels)),"Cannot restore the original public view");
            if (stageRedraw) require(SUCCEEDED(stageRedraw->finish()),"Cannot resume staged public placement");
            requirePresentation(control,mode,"public_after",pixels);
            control.paint(); publicTotal = now()-publicStart;
            exactLayout(control.view(),items,points);
            int afterPublicFocus = -1;
            require(SUCCEEDED(control.view()->GetFocusedItem(&afterPublicFocus)) && afterPublicFocus == publicFocus
                && GetFocus() == publicKeyboard,"Public layout suspension changed logical or keyboard focus");
            std::printf("PUBLIC_CONTROL mode=%s layout_suspend=%d desktop_role=%d icon_stage=%d view_state=%d total_ms=%.3f exact=5000\n",name,suspendLayout,desktopRole,iconStage,primeViewState,publicTotal); std::fflush(stdout);
            Snapshot controlOne; controlOne.ids = {items.ids[7]}; auto controlChanged = points;
            controlChanged[7] = filespacer::scaleDPI(POINT{89,101});
            require(SUCCEEDED(filespacer::restoreIconPositions(control.view(),positionBytes(controlOne,{controlChanged[7]}),[] { return true; })),
                "Cannot move a public-control item");
            exactLayout(control.view(),items,controlChanged);
            control.refresh();
            publicRefreshed = readPositions(control.view(),items);
            std::printf("PUBLIC_REFRESH_PROOF mode=%s exact=%zu\n",name,samePositions(publicRefreshed,controlChanged)); std::fflush(stdout);
            control.close();
            }
        TransactionHost owner; const double began = now(); owner.createTransaction(fixture,mode,bag,image,pixels,initialGate); owner.paint(); const double elapsed = now()-began;
        requirePresentation(owner,mode,"native_before",pixels);
        require(SUCCEEDED(owner.view()->SelectItem(7,SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed native import focus");
        double total = elapsed;
        int originalFocus = -1;
        require(SUCCEEDED(owner.view()->GetFocusedItem(&originalFocus)),"Cannot observe transaction focus");
        CComHeapPtr<ITEMID_CHILD> focusedId;
        if (originalFocus >= 0) require(SUCCEEDED(owner.view()->Item(originalFocus,&focusedId)) && focusedId,"Cannot retain focused identity");
        const HWND keyboardFocus = GetFocus();
        const auto actual = readPositions(owner.view(),items); size_t same = samePositions(actual,points);
        std::printf("TRANSACTION_RESULT mode=%s setup_hr=%08lX total_ms=%.3f exact=%zu position_reads=%u\n",name,
            static_cast<unsigned long>(owner.transaction->setup),elapsed,same,bag->positionReads); std::fflush(stdout);
        if (initialGate && same != items.ids.size()) {
            std::printf("REJECT mode=%s reason=initial_snapshot_not_loaded\n",name); continue;
        }
        const auto sentinelsMatch = [&] {
            const auto observed = readPositions(owner.view(),moved);
            return observed[0].x == points[0].x && observed[0].y == points[0].y
                && observed[1].x == points[4000].x && observed[1].y == points[4000].y;
        };
        if (SUCCEEDED(owner.transaction->setup) && !sentinelsMatch()) {
            const double transactionStart = now();
            settingsSink(owner.events->shell,image);
            auto refreshing = owner.view(); DWORD before = 0, during = 0, after = 0;
            require(SUCCEEDED(refreshing->GetCurrentFolderFlags(&before))
                && SUCCEEDED(refreshing->SetCurrentFolderFlags(FWF_DESKTOP,FWF_DESKTOP))
                && SUCCEEDED(refreshing->GetCurrentFolderFlags(&during)),"Cannot configure the native collection-load gate");
            TransactionRedrawGuard redraw(refreshing,[] { return true; });
            SlowCapture capture(std::wstring(output)+L"\\"+std::wstring(name,name+std::strlen(name))+L"-refresh.dmp");
            std::printf("REFRESH_BEGIN mode=%s ui_thread=%lu records=5000\n",name,GetCurrentThreadId()); std::fflush(stdout);
            const double refresh = now(); owner.refresh();
            require(SUCCEEDED(refreshing->SetCurrentFolderFlags(before^during,before))
                && SUCCEEDED(refreshing->GetCurrentFolderFlags(&after)) && after == before,
                "Collection-load gate did not restore folder flags");
            require(SUCCEEDED(refreshing->SetViewModeAndIconSize(mode,pixels)),"Cannot restore requested view after native import");
            requirePresentation(owner,mode,"native_after",pixels);
            require(SUCCEEDED(redraw.finish()),"Cannot resume transaction redraw");
            if (focusedId) {
                DWORD selection = 0;
                require(SUCCEEDED(refreshing->GetSelectionState(focusedId,&selection))
                    && SUCCEEDED(owner.events->shell->SelectItem(focusedId,(selection&SVSI_SELECT)|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),
                    "Cannot restore focused identity after native refresh");
                std::printf("FOCUS_RESTORED through_public_shell=1\n");
            }
            owner.paint(); const double refreshed = now()-refresh;
            total += now()-transactionStart;
            same = samePositions(readPositions(owner.view(),items),points);
            std::printf("REFRESH_RESULT mode=%s refresh_ms=%.3f exact=%zu position_reads=%u\n",name,refreshed,same,bag->positionReads);
            std::fflush(stdout);
        }
        if (FAILED(owner.transaction->setup) || !sentinelsMatch()) { std::printf("REJECT mode=%s reason=transaction_not_qualified\n",name); continue; }
        std::printf("%s mode=%s exact=%zu position_reads=%u\n",same == items.ids.size() ? "QUALIFIED_LAYOUT" : "READY_FOR_FULL",name,same,bag->positionReads); std::fflush(stdout);
        if (same != items.ids.size()) continue;
        exactLayout(owner.view(),items,points);
        std::printf("IMPORT_PROOF mode=%s exact=5000 identities=1 selection=1\n",name); std::fflush(stdout);
        int importedFocus = -1;
        require(SUCCEEDED(owner.view()->GetFocusedItem(&importedFocus)),"Cannot read imported focus");
        std::printf("FOCUS_PROOF before=%d after=%d keyboard_equal=%d\n",originalFocus,importedFocus,GetFocus()==keyboardFocus);
        require(importedFocus == originalFocus && GetFocus() == keyboardFocus,
            "Native transaction changed logical or keyboard focus");
        Snapshot one; one.ids = {items.ids[7]}; auto changed = points;
        changed[7] = filespacer::scaleDPI(POINT{89,101});
        require(SUCCEEDED(filespacer::restoreIconPositions(owner.view(),positionBytes(one,{changed[7]}),[] { return true; })),
            "Cannot move an imported native layout item");
        exactLayout(owner.view(),items,changed);
        require(SUCCEEDED(owner.view()->GetFocusedItem(&importedFocus)) && importedFocus == originalFocus && GetFocus() == keyboardFocus,
            "Moving an imported position changed focus");
        std::printf("MOVE_PROOF mode=%s exact=5000\n",name); std::fflush(stdout);
        if (nativeOnly) {
            std::printf("NATIVE_ONLY mode=%s import=5000 move=5000 focus=1 refresh=unqualified native_total_ms=%.3f\n",name,total);
            continue;
        }
        owner.refresh();
        const auto nativeRefreshed = readPositions(owner.view(),items);
        const size_t retained = samePositions(nativeRefreshed,changed);
        std::printf("REFRESH_PROOF mode=%s exact=%zu native_total_ms=%.3f public_total_ms=%.3f\n",name,retained,total,publicTotal); std::fflush(stdout);
        // A plain Shell refresh drops positions in the public control too. FS
        // separately captures and reapplies them. Reject private stale replay or
        // any additional difference instead of inventing a persistence contract.
        exactLayout(owner.view(),items,publicRefreshed);
        std::printf("LIFECYCLE mode=%s move=1 refresh_public_equivalence=1 snapshot_reapply_required=%d focus=1 selection=1 native_total_ms=%.3f public_total_ms=%.3f\n",
            name,retained != items.ids.size(),total,publicTotal); std::fflush(stdout);
    }
}
}
int wmain(int argc,wchar_t **argv) {
    if (argc != 7 || (std::wcscmp(argv[4],L"icons") && std::wcscmp(argv[4],L"tiles"))
        || (std::wcscmp(argv[5],L"current") && std::wcscmp(argv[5],L"native-layout") && std::wcscmp(argv[5],L"desktop-role")
            && std::wcscmp(argv[5],L"native-only") && std::wcscmp(argv[5],L"native-initial") && std::wcscmp(argv[5],L"icon-stage") && std::wcscmp(argv[5],L"view-state")
            && std::wcscmp(argv[5],L"enumeration") && std::wcscmp(argv[5],L"position-only") && std::wcscmp(argv[5],L"arrange-prime"))
        || (std::wcscmp(argv[6],L"32") && std::wcscmp(argv[6],L"48"))) return 2;
    wchar_t machine[MAX_COMPUTERNAME_LENGTH+1] = {}; DWORD length = _countof(machine);
    if (!GetComputerNameW(machine,&length) || _wcsicmp(machine,L"AGENTWIN10") != 0) return 3;
    FILE *recorded = nullptr;
    if (_wfreopen_s(&recorded,(std::wstring(argv[3])+L"\\native-output.log").c_str(),L"w",stdout) != 0) return 2;
    std::setvbuf(stdout,nullptr,_IONBF,0); // Keep phase evidence even if the watchdog interrupts the child.
    if (FAILED(OleInitialize(nullptr))) return 2;
    QueryPerformanceFrequency(&frequency); filespacer::initDPI();
    WNDCLASSW type = {}; type.hInstance = GetModuleHandleW(nullptr); type.lpfnWndProc = dumpWindowProc; type.lpszClassName = L"Order transfer prototype";
    if (!RegisterClassW(&type)) { OleUninitialize(); return 2; }
    int result = 0;
    try {
        const std::wstring images = std::wstring(argv[3])+L"\\images";
        require(CreateDirectoryW(images.c_str(),nullptr) || GetLastError() == ERROR_ALREADY_EXISTS,"Cannot create diagnostic image directory");
        for (const auto name : {L"comctl32.dll",L"ntdll.dll",L"windows.storage.dll",L"shell32.dll",L"propsys.dll",L"user32.dll",L"kernel32.dll",L"kernelbase.dll",L"win32u.dll",L"shcore.dll"}) {
            const HMODULE module = GetModuleHandleW(name); if (!module) continue;
            wchar_t path[32768] = {}; const DWORD count = GetModuleFileNameW(module,path,_countof(path));
            require(count && count < _countof(path) && CopyFileW(path,(images+L"\\"+name).c_str(),TRUE),"Cannot retain an actual diagnostic module image");
        }
        positionTransaction(argv[1],argv[2],argv[3],argv[4],argv[5],std::wcscmp(argv[6],L"32") == 0 ? 32 : 48);
    }
    catch (const std::exception &error) { std::fprintf(stderr,"FAIL native transaction: %s\n",error.what()); result = 1; }
    UnregisterClassW(type.lpszClassName,type.hInstance); OleUninitialize(); return result;
}
