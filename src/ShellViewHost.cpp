#include "ShellViewHost.h"
#include "MixedFolder.h"
#include <shlwapi.h>
#include <shldisp.h>
#include <shdispid.h>
#include <atomic>
#include <new>
#include <utility>

namespace filespacer {
namespace {
const EXPLORER_BROWSER_OPTIONS BROWSER_OPTIONS =
    EBO_NOBORDER | EBO_NOTRAVELLOG | EBO_NOPERSISTVIEWSTATE;
// Shared across hosts so a notification cannot match another window after HWND reuse.
std::atomic<ULONG_PTR> nextViewGeneration{0};
}

struct ShellViewHost::ViewSignal {
    std::atomic<HWND> target{nullptr};
    std::atomic<ULONG_PTR> serial{0};
    ULONG_PTR generation = 0;
    UINT readyMessage = 0, enumDoneMessage = 0, changedMessage = 0;
};

// A retained callback owns only a cancellation token. No window, view or host
// reference is retained, including while the Shell finishes a late callback.
class ShellViewHost::ViewCallback final : public IEnumReadyCallback, public IDispatch {
public:
    explicit ViewCallback(std::shared_ptr<ViewSignal> value) : signal(std::move(value)) {}
    STDMETHODIMP QueryInterface(REFIID id, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IEnumReadyCallback))
            *object = static_cast<IEnumReadyCallback *>(this);
        else if (id == IID_IDispatch || id == DIID_DShellFolderViewEvents)
            *object = static_cast<IDispatch *>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = InterlockedDecrement(&references);
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP EnumReady() override {
        const HWND target = signal->target.load();
        if (target) PostMessage(target, signal->readyMessage, signal->generation, 0);
        return S_OK;
    }
    STDMETHODIMP GetTypeInfoCount(UINT *count) override {
        if (!count) return E_POINTER;
        *count = 0;
        return S_OK;
    }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        return DISP_E_BADINDEX;
    }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR *, UINT count, LCID, DISPID *ids) override {
        if (!ids) return E_POINTER;
        for (UINT index = 0; index < count; ++index) ids[index] = DISPID_UNKNOWN;
        return DISP_E_UNKNOWNNAME;
    }
    STDMETHODIMP Invoke(DISPID id, REFIID interfaceID, LCID, WORD, DISPPARAMS *,
            VARIANT *result, EXCEPINFO *, UINT *) override {
        if (interfaceID != IID_NULL) return DISP_E_UNKNOWNINTERFACE;
        if (result) VariantInit(result);
        if (id == DISPID_FILELISTENUMDONE) {
            const ULONG_PTR serial = signal->serial.fetch_add(1) + 1;
            const HWND target = signal->target.load();
            if (target) PostMessage(target, signal->enumDoneMessage,
                signal->generation, static_cast<LPARAM>(serial));
            return S_OK;
        }
        if (id == DISPID_VIEWMODECHANGED || id == DISPID_ORDERCHANGED || id == DISPID_FOLDERCHANGED) {
            const HWND target = signal->target.load();
            if (target && signal->changedMessage)
                PostMessage(target, signal->changedMessage, signal->generation, 0);
            return S_OK;
        }
        if (id == DISPID_VERBINVOKED || id == DISPID_DEFAULTVERBINVOKED) {
            const HWND target = signal->target.load();
            if (target && signal->changedMessage)
                PostMessage(target, signal->changedMessage, signal->generation, 0);
            // False would cancel the Shell's native command.
            if (result) { result->vt = VT_BOOL; result->boolVal = VARIANT_TRUE; }
            return S_OK;
        }
        return DISP_E_MEMBERNOTFOUND;
    }
private:
    LONG references = 1;
    std::shared_ptr<ViewSignal> signal;
};

HRESULT ShellViewHost::initialize(HWND parent, RECT bounds, const FOLDERSETTINGS &settings,
        IUnknown *site, IExplorerBrowserEvents *events, FOLDERVIEWOPTIONS layout) {
    if (browser) return E_UNEXPECTED;
    CComPtr<IExplorerBrowser> created;
    HRESULT hr = created.CoCreateInstance(__uuidof(ExplorerBrowser));
    if (SUCCEEDED(hr)) hr = created->SetOptions(BROWSER_OPTIONS);
    if (SUCCEEDED(hr) && layout != FVO_DEFAULT) {
        CComQIPtr<IFolderViewOptions> options(created);
        hr = options ? options->SetFolderViewOptions(layout, layout) : E_NOINTERFACE;
    }
    if (FAILED(hr)) return hr;
    FOLDERSETTINGS initialSettings = settings;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FileSpacer Shell Content";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return HRESULT_FROM_WIN32(GetLastError());
    contentWindow = CreateWindowExW(0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
        parent, nullptr, wc.hInstance, nullptr);
    if (!contentWindow) return HRESULT_FROM_WIN32(GetLastError());
    RECT contentBounds = {0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top};
    hr = created->Initialize(contentWindow, &contentBounds, &initialSettings);
    if (FAILED(hr)) {
        created->Destroy(); // Initialize may have allocated window resources before failing.
        DestroyWindow(contentWindow); contentWindow = nullptr;
        return hr;
    }
    browser = created;
    hr = IUnknown_SetSite(created, site);
    if (SUCCEEDED(hr)) {
        hr = created->Advise(events, &eventsCookie);
        advised = SUCCEEDED(hr);
    }
    if (FAILED(hr)) destroy();
    return hr;
}

HRESULT ShellViewHost::stopObserving() {
    if (signal) signal->target.store(nullptr);
    signal.reset();
    CComPtr<IConnectionPoint> previous;
    previous.Attach(viewEvents.Detach());
    const bool subscribed = viewEventsAdvised;
    const DWORD cookie = viewEventsCookie;
    viewEventsAdvised = false;
    viewEventsCookie = 0;
    return previous && subscribed ? previous->Unadvise(cookie) : S_OK;
}

HRESULT ShellViewHost::observeView(IShellView *view, HWND target, UINT readyMessage,
        UINT enumDoneMessage, ULONG_PTR &generation, UINT changedMessage) {
    generation = 0;
    HRESULT hr = stopObserving();
    if (FAILED(hr)) return hr;
    if (!view || !target) return E_INVALIDARG;
    std::shared_ptr<ViewSignal> current;
    try { current = std::make_shared<ViewSignal>(); }
    catch (const std::bad_alloc &) { return E_OUTOFMEMORY; }
    current->generation = ++nextViewGeneration;
    current->readyMessage = readyMessage;
    current->enumDoneMessage = enumDoneMessage;
    current->changedMessage = changedMessage;
    current->target.store(target);
    signal = current;
    generation = current->generation;
    CComPtr<IEnumReadyCallback> callback;
    callback.Attach(new (std::nothrow) ViewCallback(current));
    if (!callback) { stopObserving(); return E_OUTOFMEMORY; }
    CComPtr<IEnumerableView> enumerable;
    hr = IUnknown_QueryService(view, SID_EnumerableView, IID_PPV_ARGS(&enumerable));
    CComPtr<IDispatch> automation;
    if (SUCCEEDED(hr)) hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&automation));
    CComQIPtr<IConnectionPointContainer> container(automation);
    CComPtr<IConnectionPoint> connection;
    if (SUCCEEDED(hr)) hr = container
        ? container->FindConnectionPoint(DIID_DShellFolderViewEvents, &connection) : E_NOINTERFACE;
    DWORD cookie = 0;
    if (SUCCEEDED(hr) && signal != current) return E_ABORT;
    if (SUCCEEDED(hr)) {
        hr = connection->Advise(callback, &cookie);
        if (SUCCEEDED(hr)) {
            if (signal != current) { connection->Unadvise(cookie); return E_ABORT; }
            viewEvents = connection;
            viewEventsCookie = cookie;
            viewEventsAdvised = true;
            hr = enumerable->SetEnumReadyCallback(callback);
        }
    }
    if (signal != current) return E_ABORT;
    if (FAILED(hr)) stopObserving();
    return hr;
}

bool ShellViewHost::isViewNotification(WPARAM generation) const {
    return signal && signal->target.load() && generation == signal->generation;
}

ULONG_PTR ShellViewHost::enumerationSerial() const {
    return signal ? signal->serial.load() : 0;
}

HRESULT ShellViewHost::destroy() {
    const HWND previousContent = contentWindow;
    contentWindow = nullptr;
    masked = false;
    const HRESULT observationResult = stopObserving();
    // Detach first: callbacks during teardown must not find a dying browser.
    CComPtr<IExplorerBrowser> previous;
    previous.Attach(browser.Detach());
    const bool wasAdvised = advised;
    const DWORD cookie = eventsCookie;
    advised = false;
    eventsCookie = 0;
    HRESULT result = observationResult;
    if (previous) {
        if (wasAdvised) {
            const HRESULT hr = previous->Unadvise(cookie);
            if (SUCCEEDED(result)) result = hr;
        }
        const HRESULT siteResult = IUnknown_SetSite(previous, nullptr);
        if (SUCCEEDED(result)) result = siteResult;
        const HRESULT destroyResult = previous->Destroy();
        if (SUCCEEDED(result)) result = destroyResult;
    }
    if (previousContent) DestroyWindow(previousContent);
    return result;
}

HRESULT ShellViewHost::currentView(REFIID iid, void **view) const {
    if (!view) return E_POINTER;
    *view = nullptr;
    CComPtr<IExplorerBrowser> current = browser;
    return current ? current->GetCurrentView(iid, view) : E_UNEXPECTED;
}

HRESULT ShellViewHost::navigate(IShellItem *item, bool mixFolders, bool isolated) {
    CComPtr<IExplorerBrowser> current = browser;
    if (!current) return E_UNEXPECTED;
    mixedSorting = mixFolders;
    CComPtr<IShellItem> target = item;
    bool custom = false;
    if (mixFolders) {
        SFGAOF attributes = 0;
        const SFGAOF required = SFGAO_FOLDER | SFGAO_FILESYSTEM;
        HRESULT hr = item->GetAttributes(required | SFGAO_STREAM, &attributes);
        if (FAILED(hr)) return hr;
        // Virtual providers and archives retain their own native view contract.
        if ((attributes & required) == required && !(attributes & SFGAO_STREAM)) {
            custom = true;
            target.Release();
            hr = mixedFolderItem(item, isolated, &target);
            if (FAILED(hr)) return hr;
        }
    }
    if (custom) { const HRESULT hr = maskContent(); if (FAILED(hr)) revealContent(); }
    else revealContent();
    const HRESULT hr = current->BrowseToObject(target, SBSP_ABSOLUTE);
    if (FAILED(hr)) revealContent();
    return hr;
}

HRESULT ShellViewHost::lockNavigation(bool locked) {
    CComPtr<IExplorerBrowser> current = browser;
    return current ? current->SetOptions(locked ? BROWSER_OPTIONS | EBO_NAVIGATEONCE
        : BROWSER_OPTIONS) : E_UNEXPECTED;
}

HRESULT ShellViewHost::setFilter(IFolderFilter *filter) {
    CComPtr<IExplorerBrowser> current = browser;
    CComQIPtr<IFolderFilterSite> site(current);
    return site ? site->SetFilter(filter) : E_NOINTERFACE;
}

HRESULT ShellViewHost::setBounds(RECT bounds) {
    CComPtr<IExplorerBrowser> current = browser;
    if (!current || !contentWindow) return E_UNEXPECTED;
    if (!SetWindowPos(contentWindow, nullptr, bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE))
        return HRESULT_FROM_WIN32(GetLastError());
    RECT contentBounds = {0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top};
    return current->SetRect(nullptr, contentBounds);
}

HRESULT ShellViewHost::maskContent() {
    if (!contentWindow) return E_UNEXPECTED;
    const LONG_PTR style = GetWindowLongPtrW(contentWindow, GWL_EXSTYLE);
    SetWindowLongPtrW(contentWindow, GWL_EXSTYLE, style | WS_EX_LAYERED);
    if (!(GetWindowLongPtrW(contentWindow, GWL_EXSTYLE) & WS_EX_LAYERED)) return E_FAIL;
    masked = true;
    if (!SetLayeredWindowAttributes(contentWindow, 0, 0, LWA_ALPHA)) return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}

void ShellViewHost::revealContent() {
    if (!contentWindow || !masked) return;
    SetLayeredWindowAttributes(contentWindow, 0, 255, LWA_ALPHA);
    SetWindowLongPtrW(contentWindow, GWL_EXSTYLE, GetWindowLongPtrW(contentWindow, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    masked = false;
    RedrawWindow(contentWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
}
} // namespace filespacer
