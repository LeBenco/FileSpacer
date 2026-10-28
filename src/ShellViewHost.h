#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <ocidl.h>
#include <memory>

namespace filespacer {

// Hosts the Windows view through public Shell interfaces. The Shell owns its
// controls, rendering and interactions; FolderWindow owns navigation policy.
class ShellViewHost {
public:
    ShellViewHost() = default;
    ShellViewHost(const ShellViewHost &) = delete;
    ShellViewHost &operator=(const ShellViewHost &) = delete;
    ~ShellViewHost() { destroy(); }
    HRESULT initialize(HWND parent, RECT bounds, const FOLDERSETTINGS &settings,
        IUnknown *site, IExplorerBrowserEvents *events,
        FOLDERVIEWOPTIONS layout = FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING);
    HRESULT destroy();
    explicit operator bool() const { return browser != nullptr; }
    HRESULT currentView(REFIID iid, void **view) const;
    HRESULT navigate(IShellItem *item, bool mixFolders = false, bool isolated = false);
    bool mixedSortingPreference() const { return mixedSorting; }
    HRESULT lockNavigation(bool locked);
    HRESULT setFilter(IFolderFilter *filter);
    HRESULT setBounds(RECT bounds);
    // Notifications contain a unique observation generation in wParam. EnumDone
    // also carries its monotonically increasing serial in lParam.
    HRESULT observeView(IShellView *view, HWND target, UINT readyMessage,
        UINT enumDoneMessage, ULONG_PTR &generation);
    HRESULT stopObserving();
    bool isViewNotification(WPARAM generation) const;
    ULONG_PTR enumerationSerial() const;

private:
    struct ViewSignal;
    class ViewCallback;
    CComPtr<IExplorerBrowser> browser;
    bool mixedSorting = false;
    DWORD eventsCookie = 0;
    bool advised = false;
    std::shared_ptr<ViewSignal> signal;
    CComPtr<IConnectionPoint> viewEvents;
    DWORD viewEventsCookie = 0;
    bool viewEventsAdvised = false;
};

} // namespace filespacer
