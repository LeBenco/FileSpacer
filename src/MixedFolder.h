#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <functional>

namespace filespacer {
// FileSpacer's own COM contract at the provider DLL boundary. Shell clients use
// the standard IShellFolder2 interfaces. The view's child identifiers are the
// native folder's own; only their parent differs, so the application needs just
// the native Shell item of a view child.
MIDL_INTERFACE("91D72DCF-DE04-4CED-995C-A4870EB11D32") IMixedFolder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) = 0;
};

// Per-view, in-process policy. S_OK reserves an internal icon placement;
// S_FALSE delegates the file operation to the native folder drop target.
MIDL_INTERFACE("F9A9CA46-4665-4D2C-B63D-C702DE888047") IIconDropPolicy : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Placement(PCUITEMID_CHILD target, POINTL cursor, DWORD keys) = 0;
};
MIDL_INTERFACE("879ECF31-94B1-42F1-893F-441D9ACBA91C") IIconDropPolicySite : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetIconDropPolicy(IIconDropPolicy *policy) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIconDropPolicy(IIconDropPolicy **policy) = 0;
};

HRESULT mixedFolderItem(IShellItem *item, bool isolated, IShellItem **out);
HRESULT nativeShellItem(IShellItem *item, IShellItem **out);
// A guarded caller can use public large-selection capture; absent guards keep native access.
HRESULT nativeViewSelection(IFolderView2 *view, BOOL noneMeansFolder, IShellItemArray **out,
    const std::function<bool()> &current = {});
}
