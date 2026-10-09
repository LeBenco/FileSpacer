#include "ExecuteCommand.h"
#include "main.h"
#include "CreateItemWindow.h"
#include "ShellUtils.h"
#include "Update.h"

namespace filespacer {

FSExecute::FSExecute() {
    lockProcess();
}

FSExecute::~FSExecute() {
    unlockProcess();
}

STDMETHODIMP_(ULONG) FSExecute::AddRef() { return UnknownImpl::AddRef(); }
STDMETHODIMP_(ULONG) FSExecute::Release() { return UnknownImpl::Release(); }

STDMETHODIMP FSExecute::QueryInterface(REFIID id, void **obj) {
    static const QITAB interfaces[] = {
        QITABENT(FSExecute, IObjectWithSelection),
        QITABENT(FSExecute, IExecuteCommand),
        QITABENT(FSExecute, IDropTarget),
        {},
    };
    HRESULT hr = QISearch(this, interfaces, id, obj);
    if (SUCCEEDED(hr))
        return hr;
    return UnknownImpl::QueryInterface(id, obj);
}

/* IObjectWithSelection */

STDMETHODIMP FSExecute::SetSelection(IShellItemArray *const array) {
    selection = array;
    return S_OK;
}

STDMETHODIMP FSExecute::GetSelection(REFIID id, void **obj) {
    if (selection)
        return selection->QueryInterface(id, obj);
    *obj = nullptr;
    return E_NOINTERFACE;
}

/* IExecuteCommand */

STDMETHODIMP FSExecute::SetKeyState(DWORD) { return S_OK; }
STDMETHODIMP FSExecute::SetParameters(const wchar_t *) { return S_OK; }
STDMETHODIMP FSExecute::SetNoShowUI(BOOL) { return S_OK; }

STDMETHODIMP FSExecute::SetDirectory(const wchar_t *path) {
    int size = lstrlen(path) + 1;
    workingDir = wstr_ptr(new wchar_t[size]);
    CopyMemory(workingDir.get(), path, size * sizeof(wchar_t));
    return S_OK;
}

STDMETHODIMP FSExecute::SetPosition(POINT point) {
    monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    return S_OK;
}

STDMETHODIMP FSExecute::SetShowWindow(int show) {
    showCommand = show;
    return S_OK;
}

STDMETHODIMP FSExecute::Execute() {
    debugPrintf(L"Invoked with DelegateExecute\n");
    if (!selection) {
        if (!workingDir)
            return E_UNEXPECTED;
        // this happens when invoked on background
        CComPtr<IShellItem> item;
        if (checkHR(SHCreateItemFromParsingName(workingDir.get(), nullptr, IID_PPV_ARGS(&item)))) {
            openItem(item);
        }
    } else {
        HRESULT hr;
        if (FAILED(hr = openArray(selection))) return hr;
    }
    // autoUpdateCheck(); // Deferred public update service.
    return S_OK;
}

/* IDropTarget */

STDMETHODIMP FSExecute::DragEnter(IDataObject *, DWORD, POINTL, DWORD *effect) {
    *effect &= DROPEFFECT_LINK;
    return S_OK;
}

STDMETHODIMP FSExecute::DragOver(DWORD, POINTL, DWORD *effect) {
    *effect &= DROPEFFECT_LINK;
    return S_OK;
}

STDMETHODIMP FSExecute::DragLeave() {
    return S_OK;
}

STDMETHODIMP FSExecute::Drop(IDataObject *const dataObject, DWORD keyState, POINTL pt,
        DWORD *effect) {
    debugPrintf(L"Invoked with DropTarget\n");
    // https://devblogs.microsoft.com/oldnewthing/20130204-00/?p=5363
    SetKeyState(keyState);
    SetPosition({pt.x, pt.y});
    HRESULT hr;
    CComPtr<IShellItemArray> itemArray;
    if (!checkHR(hr = SHCreateShellItemArrayFromDataObject(dataObject, IID_PPV_ARGS(&itemArray))))
        return hr;
    if (FAILED(hr = openArray(itemArray)))
        return hr;
    *effect &= DROPEFFECT_LINK;
    // autoUpdateCheck(); // Deferred public update service.
    return S_OK;
}

HRESULT FSExecute::openArray(IShellItemArray *const array) {
    HRESULT hr;
    CComPtr<IEnumShellItems> enumItems;
    if (!checkHR(hr = array->EnumItems(&enumItems)))
        return hr;
    CComPtr<IShellItem> item;
    while (enumItems->Next(1, &item, nullptr) == S_OK) {
        openItem(item);
        item = nullptr;
    }
    return S_OK;
}

void FSExecute::openItem(IShellItem *const item) {
    CComPtr<IShellItem> resolved = resolveLink(item);
    SFGAOF attributes = 0;
    if (!resolved) return;
    HRESULT access = resolved->GetAttributes(SFGAO_FOLDER, &attributes);
    if (!checkHR(access)) {
        recordFolderItemFailure(resolved, access);
        return;
    }
    if (!(attributes & SFGAO_FOLDER)) {
        invokeDefaultVerb(item, nullptr, showCommand);
        return;
    }

    openFolderWindow(resolved, monitor, showCommand);
}

/* Factory */

STDMETHODIMP_(ULONG) FSExecuteFactory::AddRef() {
    return 2;
}

STDMETHODIMP_(ULONG) FSExecuteFactory::Release() {
    return 1;
}

STDMETHODIMP FSExecuteFactory::QueryInterface(REFIID id, void **obj) {
    static const QITAB interfaces[] = {
        QITABENT(FSExecuteFactory, IClassFactory),
        {},
    };
    return QISearch(this, interfaces, id, obj);
}

STDMETHODIMP FSExecuteFactory::CreateInstance(IUnknown *const outer, REFIID id, void **obj) {
    *obj = nullptr;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    CComPtr<FSExecute> ext;
    ext.Attach(new FSExecute());
    HRESULT hr = ext->QueryInterface(id, obj);
    return hr;
}

STDMETHODIMP FSExecuteFactory::LockServer(BOOL lock) {
    if (lock)
        lockProcess();
    else
        unlockProcess();
    return S_OK;
}

} // namespace
