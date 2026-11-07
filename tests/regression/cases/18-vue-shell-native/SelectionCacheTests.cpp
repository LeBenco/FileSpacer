#include <common.h>
#include "ShellViewSelection.h"
#include "ShellSelectionRuntime.h"
#include <cstdio>
#include <string>
namespace {
unsigned notifications=0,dispatched=0;
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM first,LPARAM second) {
    if(message==WM_APP+1){++dispatched;return 0;}
    if(message>=0xC000 && message==filespacer::selectionDetail::readyMessage()){++notifications;return 0;}
    return DefWindowProcW(window,message,first,second);
}
void pump() {
    MSG message;
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);
}
bool waitResolution(DWORD milliseconds) {
    const ULONGLONG deadline=GetTickCount64()+milliseconds;
    while(!filespacer::selectionDetail::resolved() && GetTickCount64()<deadline)pump();
    return filespacer::selectionDetail::resolved();
}
// Dispatch-only fake for early-call ordering. Companion tests use real Shell views.
class PublicView : public IShellView {
    LONG references=1;
public:
    unsigned calls=0;
    STDMETHODIMP QueryInterface(REFIID id,void **out)override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=IID_IUnknown && id!=IID_IShellView && id!=IID_IOleWindow)return E_NOINTERFACE;
        *out=static_cast<IShellView *>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{return InterlockedDecrement(&references);}
    STDMETHODIMP GetWindow(HWND *)override{return E_NOTIMPL;}
    STDMETHODIMP ContextSensitiveHelp(BOOL)override{return E_NOTIMPL;}
    STDMETHODIMP TranslateAccelerator(MSG *)override{return E_NOTIMPL;}
    STDMETHODIMP EnableModeless(BOOL)override{return E_NOTIMPL;}
    STDMETHODIMP UIActivate(UINT)override{return E_NOTIMPL;}
    STDMETHODIMP Refresh()override{return E_NOTIMPL;}
    STDMETHODIMP CreateViewWindow(IShellView *,LPCFOLDERSETTINGS,IShellBrowser *,RECT *,HWND *)override{return E_NOTIMPL;}
    STDMETHODIMP DestroyViewWindow()override{return E_NOTIMPL;}
    STDMETHODIMP GetCurrentInfo(LPFOLDERSETTINGS)override{return E_NOTIMPL;}
    STDMETHODIMP AddPropertySheetPages(DWORD,LPFNSVADDPROPSHEETPAGE,LPARAM)override{return E_NOTIMPL;}
    STDMETHODIMP SaveViewState()override{return E_NOTIMPL;}
    STDMETHODIMP SelectItem(PCUITEMID_CHILD item,SVSIF flags)override {
        if(item || flags!=SVSI_DESELECTOTHERS)return E_INVALIDARG;++calls;return S_OK;
    }
    STDMETHODIMP GetItemObject(UINT,REFIID,void **)override{return E_NOTIMPL;}
};
}
int wmain(int argc,wchar_t **argv) {
    if(argc==4 && wcscmp(argv[1],L"/test")==0 && wcscmp(argv[2],L"/selection-discovery")==0)
        return filespacer::selectionDetail::runWorker(argv[3]);
    if(argc!=3)return 2;
    if(wcscmp(argv[1],L"--build-cache")==0)return filespacer::selectionDetail::runWorker(argv[2]);
    const bool live=wcscmp(argv[1],L"--live")==0;
    const bool publicOnly=wcscmp(argv[1],L"--public-persist")==0;
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=windowProc;type.lpszClassName=L"FileSpacer Cache Test";
    if(!RegisterClassW(&type))return 2;
    HWND closed=CreateWindowW(type.lpszClassName,L"",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,type.hInstance,nullptr);
    if(closed)DestroyWindow(closed);
    HWND window=CreateWindowW(type.lpszClassName,L"",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,type.hInstance,nullptr);
    if(!window)return 2;
    struct Window {HWND value;~Window(){DestroyWindow(value);UnregisterClassW(L"FileSpacer Cache Test",GetModuleHandleW(nullptr));}} cleanup={window};
    LARGE_INTEGER frequency={},began={},returned={};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&began);
    filespacer::initializeShellSelectionClear(argv[2],live||publicOnly);
    QueryPerformanceCounter(&returned);
    const double elapsed=1000.0*static_cast<double>(returned.QuadPart-began.QuadPart)/static_cast<double>(frequency.QuadPart);
    std::printf("INITIALIZE dispatch_ms=%.3f\n",elapsed);
    PublicView view;filespacer::ShellSelectionClear operation;bool applied=true;
    if(FAILED(operation.clear(&view,[]{return true;},&applied)) || view.calls!=1 || applied)return 1;
    PostMessageW(window,WM_APP+1,0,0);
    if(!waitResolution(live?180000:10000))return 3;
    const bool supported=filespacer::selectionDetail::image().supported;
    if(live) {
        for(unsigned i=0;i<20 && !notifications;++i)pump();
        if(!supported || !notifications || !dispatched || IsWindowVisible(window))return 1;
        std::printf("PASS: early public call, UI dispatch=%u, publication notifications=%u, closed window safe\n",dispatched,notifications);return 0;
    }
    std::printf("RESOLVED supported=%d reason=%s\n",supported,filespacer::selectionDetail::image().reason);
    if(wcscmp(argv[1],L"--supported")==0)return supported?0:1;
    if(wcscmp(argv[1],L"--fallback")==0 || publicOnly)return supported?1:0;
    return 2;
}
