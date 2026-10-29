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
MIDL_INTERFACE("A6AD59F1-76BE-48F4-A141-B4CB8C25E241") IMixedFolder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) = 0;
};

HRESULT mixedFolderItem(IShellItem *item, bool isolated, IShellItem **out);
HRESULT nativeShellItem(IShellItem *item, IShellItem **out);
HRESULT nativeViewSelection(IFolderView2 *view, BOOL noneMeansFolder, IShellItemArray **out);
}
