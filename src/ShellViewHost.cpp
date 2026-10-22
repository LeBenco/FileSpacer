#include "ShellViewHost.h"
#include <shlwapi.h>
#include <commctrl.h>

namespace filespacer {
namespace {
const EXPLORER_BROWSER_OPTIONS BROWSER_OPTIONS =
    EBO_NOBORDER | EBO_NOTRAVELLOG | EBO_NOPERSISTVIEWSTATE;
// Alt gestures and full-name painting require the classic ListView. The
// full-name preference must not select a different renderer.
const FOLDERVIEWOPTIONS CLASSIC_VIEW_OPTIONS =
    FVO_VISTALAYOUT | FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING;
}

HRESULT ShellViewHost::initialize(HWND parent, RECT bounds, const FOLDERSETTINGS &settings,
        IUnknown *site, IExplorerBrowserEvents *events) {
    if (browser) return E_UNEXPECTED;
    CComPtr<IExplorerBrowser> created;
    HRESULT hr = created.CoCreateInstance(__uuidof(ExplorerBrowser));
    if (SUCCEEDED(hr)) hr = created->SetOptions(BROWSER_OPTIONS);
    if (SUCCEEDED(hr)) {
        CComQIPtr<IFolderViewOptions> options(created);
        hr = options ? options->SetFolderViewOptions(CLASSIC_VIEW_OPTIONS,
            CLASSIC_VIEW_OPTIONS) : E_NOINTERFACE;
    }
    FOLDERSETTINGS initialSettings = settings;
    if (SUCCEEDED(hr)) hr = created->Initialize(parent, &bounds, &initialSettings);
    if (FAILED(hr)) return hr;
    browser = created;
    hr = IUnknown_SetSite(created, site);
    if (SUCCEEDED(hr)) {
        hr = created->Advise(events, &eventsCookie);
        advised = SUCCEEDED(hr);
    }
    if (FAILED(hr)) destroy();
    return hr;
}

void ShellViewHost::destroy() {
    // Detach first: callbacks during teardown must not find a dying browser.
    CComPtr<IExplorerBrowser> previous;
    previous.Attach(browser.Detach());
    const bool wasAdvised = advised;
    const DWORD cookie = eventsCookie;
    advised = false;
    eventsCookie = 0;
    if (previous) {
        if (wasAdvised) previous->Unadvise(cookie);
        IUnknown_SetSite(previous, nullptr);
        previous->Destroy();
    }
}

HRESULT ShellViewHost::currentView(REFIID iid, void **view) const {
    if (!view) return E_POINTER;
    *view = nullptr;
    CComPtr<IExplorerBrowser> current = browser;
    return current ? current->GetCurrentView(iid, view) : E_UNEXPECTED;
}

HRESULT ShellViewHost::navigate(IShellItem *item) {
    // Keep the implementation alive throughout a reentrant Shell call.
    CComPtr<IExplorerBrowser> current = browser;
    return current ? current->BrowseToObject(item, SBSP_ABSOLUTE) : E_UNEXPECTED;
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
    return current ? current->SetRect(nullptr, bounds) : E_UNEXPECTED;
}

HWND ShellViewHost::findListView(HWND viewWindow, bool &ambiguous) const {
    // Accepted compatibility dependency: the default Shell view's child
    // ListView is not a public Shell contract. Keep its discovery in this
    // backend so an owned ListView can later supply its handle directly.
    struct Candidates { HWND window = nullptr; bool ambiguous = false; } candidates;
    EnumChildWindows(viewWindow, [](HWND child, LPARAM data) -> BOOL {
        wchar_t className[32];
        if (GetClassName(child, className, _countof(className))
                && lstrcmp(className, WC_LISTVIEW) == 0
                && GetWindowThreadProcessId(child, nullptr) == GetCurrentThreadId()) {
            auto &found = *reinterpret_cast<Candidates *>(data);
            if (found.window) {
                found.ambiguous = true;
                return FALSE;
            }
            found.window = child;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&candidates));
    ambiguous = candidates.ambiguous;
    return ambiguous ? nullptr : candidates.window;
}

} // namespace filespacer
