#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>

namespace filespacer {
// FileSpacer's own COM contract at the provider DLL boundary. Shell clients use
// the standard IShellFolder2 interfaces; only the application needs conversion.
MIDL_INTERFACE("A6AD59F1-76BE-48F4-A141-B4CB8C25E241") IMixedFolder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) = 0;
    virtual HRESULT STDMETHODCALLTYPE NativeChild(PCUITEMID_CHILD child, PITEMID_CHILD *out) = 0;
    virtual HRESULT STDMETHODCALLTYPE ViewChild(PCUITEMID_CHILD child, PITEMID_CHILD *out) = 0;
};

HRESULT mixedFolderItem(IShellItem *item, bool isolated, IShellItem **out);
HRESULT nativeShellItem(IShellItem *item, IShellItem **out);
HRESULT nativeViewSelection(IFolderView2 *view, BOOL noneMeansFolder, IShellItemArray **out);
HRESULT nativeViewChild(IFolderView *view, PCUITEMID_CHILD child, PITEMID_CHILD *out);
HRESULT mixedViewChild(IFolderView *view, PCUITEMID_CHILD child, PITEMID_CHILD *out);
HRESULT writeNativeViewPidl(IFolderView *view, IStream *stream, PCUITEMID_CHILD child);
HRESULT readMixedViewPidl(IFolderView *view, IStream *stream, PITEMID_CHILD *child);
}
