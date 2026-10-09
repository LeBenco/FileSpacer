#pragma once
#include "common.h"

#include "COMUtils.h"
#include "ShObjIdl.h"
#include "WinUtils.h"
#include <atlbase.h>
#include <ExDisp.h>

namespace filespacer {

// https://devblogs.microsoft.com/oldnewthing/20100312-01/?p=14623
// https://devblogs.microsoft.com/oldnewthing/20100503-00/?p=14183
// https://devblogs.microsoft.com/oldnewthing/20100528-01/?p=13883

// {c79db990-30a8-47e7-9f84-9db1316ae89b}
const CLSID CLSID_FSExecute =
    {0xc79db990, 0x30a8, 0x47e7, {0x9f, 0x84, 0x9d, 0xb1, 0x31, 0x6a, 0xe8, 0x9b}};
class FSExecute : public UnknownImpl, public IObjectWithSelection, public IExecuteCommand,
        public IDropTarget {
public:
    FSExecute();
    ~FSExecute();

    // IUnknown
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP QueryInterface(REFIID id, void **obj) override;
    // IObjectWithSelection
    STDMETHODIMP SetSelection(IShellItemArray *array) override;
    STDMETHODIMP GetSelection(REFIID id, void **obj) override;
    // IExecuteCommand
    STDMETHODIMP SetKeyState(DWORD) override;
    STDMETHODIMP SetParameters(const wchar_t *params) override;
    STDMETHODIMP SetPosition(POINT) override;
    STDMETHODIMP SetShowWindow(int) override;
    STDMETHODIMP SetNoShowUI(BOOL) override;
    STDMETHODIMP SetDirectory(const wchar_t *path) override;
    STDMETHODIMP Execute() override;
    // IDropTarget
    STDMETHODIMP DragEnter(IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect)
        override;
    STDMETHODIMP DragOver(DWORD keyState, POINTL pt, DWORD *effect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop(IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect) override;

private:
    HRESULT openArray(IShellItemArray *array);
    void openItem(IShellItem *item);

    CComPtr<IShellItemArray> selection;
    HMONITOR monitor = nullptr;
    int showCommand = SW_SHOWNORMAL;
    wstr_ptr workingDir;
};

class FSExecuteFactory : public IClassFactory {
public:
    // IUnknown
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP QueryInterface(REFIID id, void **obj) override;
    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown *outer, REFIID id, void **obj) override;
    STDMETHODIMP LockServer(BOOL lock) override;
};

} // namespace
