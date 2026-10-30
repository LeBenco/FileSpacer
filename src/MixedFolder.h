#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>

namespace filespacer {
// FileSpacer's own COM contract at the provider DLL boundary. Shell clients use
// the standard IShellFolder2 interfaces. The view's child identifiers are the
// native folder's own; only their parent differs, so the application needs just
// the native Shell item of a view child.
MIDL_INTERFACE("91D72DCF-DE04-4CED-995C-A4870EB11D32") IMixedFolder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) = 0;
};

HRESULT mixedFolderItem(IShellItem *item, bool isolated, IShellItem **out);
HRESULT nativeShellItem(IShellItem *item, IShellItem **out);
HRESULT nativeViewSelection(IFolderView2 *view, BOOL noneMeansFolder, IShellItemArray **out);
}
