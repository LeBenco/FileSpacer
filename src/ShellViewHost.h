#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>

namespace filespacer {

// The host uses Shell view interfaces; only this component chooses and owns
// their implementation. FolderWindow retains navigation policy and callbacks.
class ShellViewHost {
public:
    ShellViewHost() = default;
    ShellViewHost(const ShellViewHost &) = delete;
    ShellViewHost &operator=(const ShellViewHost &) = delete;
    ~ShellViewHost() { destroy(); }
    HRESULT initialize(HWND parent, RECT bounds, const FOLDERSETTINGS &settings,
        IUnknown *site, IExplorerBrowserEvents *events);
    void destroy();
    explicit operator bool() const { return browser != nullptr; }
    HRESULT currentView(REFIID iid, void **view) const;
    HRESULT navigate(IShellItem *item);
    HRESULT lockNavigation(bool locked);
    HRESULT setFilter(IFolderFilter *filter);
    HRESULT setBounds(RECT bounds);
    HWND findListView(HWND viewWindow, bool &ambiguous) const;

private:
    CComPtr<IExplorerBrowser> browser;
    DWORD eventsCookie = 0;
    bool advised = false;
};

} // namespace filespacer
