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
    const bool lateRange=argc==4&&wcscmp(argv[1],L"--range-late")==0;
    if(argc!=3&&!lateRange)return 2;
    if(wcscmp(argv[1],L"--build-cache")==0)return filespacer::selectionDetail::runWorker(argv[2]);
    const bool live=wcscmp(argv[1],L"--live")==0;
    const bool publicOnly=wcscmp(argv[1],L"--public-persist")==0;
    struct Lock {HANDLE value=INVALID_HANDLE_VALUE;~Lock(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}} lock;
    if(lateRange) {
        lock.value=CreateFileW((std::wstring(argv[2])+L"\\discovery.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
            OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        if(lock.value==INVALID_HANDLE_VALUE)return 1;
    }
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=windowProc;type.lpszClassName=L"FileSpacer Cache Test";
    if(!RegisterClassW(&type))return 2;
    HWND closed=CreateWindowW(type.lpszClassName,L"",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,type.hInstance,nullptr);
    if(closed)DestroyWindow(closed);
    HWND window=CreateWindowW(type.lpszClassName,L"",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,type.hInstance,nullptr);
    if(!window)return 2;
    struct Window {HWND value;~Window(){DestroyWindow(value);UnregisterClassW(L"FileSpacer Cache Test",GetModuleHandleW(nullptr));}} cleanup={window};
    LARGE_INTEGER frequency={},began={},returned={};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&began);
    filespacer::initializeShellSelectionClear(argv[2],live||publicOnly||lateRange);
    QueryPerformanceCounter(&returned);
    const double elapsed=1000.0*static_cast<double>(returned.QuadPart-began.QuadPart)/static_cast<double>(frequency.QuadPart);
    std::printf("INITIALIZE dispatch_ms=%.3f\n",elapsed);
    PublicView view;filespacer::ShellSelectionClear operation;bool applied=true;
    if(FAILED(operation.clear(&view,[]{return true;},&applied)) || view.calls!=1 || applied)return 1;
    PostMessageW(window,WM_APP+1,0,0);
    if(!waitResolution(live?180000:10000))return 3;
    const bool supported=filespacer::selectionDetail::image().supported;
    if(lateRange) {
        if(!supported||filespacer::selectionDetail::rangeImage().supported||filespacer::selectionDetail::rangeResolved())return 1;
        if(FAILED(operation.clear(&view,[]{return true;},&applied))||view.calls!=2||applied)return 1;
        const wchar_t *name=wcsrchr(argv[3],L'\\');if(!name)return 2;
        const std::wstring final=std::wstring(argv[2])+name,pending=final+L".pending";
        if(!CopyFileW(argv[3],pending.c_str(),TRUE)||!MoveFileExW(pending.c_str(),final.c_str(),MOVEFILE_WRITE_THROUGH))return 1;
        // Complete the same publisher protocol as the worker. Releasing its
        // lock is also the final directory event after the rename has returned;
        // an earlier rename notification can precede read access to the file.
        CloseHandle(lock.value);lock.value=INVALID_HANDLE_VALUE;
        const ULONGLONG deadline=GetTickCount64()+10000;
        while(!filespacer::selectionDetail::rangeResolved()&&GetTickCount64()<deadline) {
            if(!filespacer::selectionDetail::image().supported)return 1;
            pump();
        }
        for(unsigned i=0;i<20&&notifications<2;++i)pump();
        if(!filespacer::selectionDetail::rangeImage().supported||!filespacer::selectionDetail::rangeResolved()||notifications<2) {
            const auto loaded=filespacer::selectionDetail::RangeImage::discover(final);
            const char *reason=nullptr;const bool valid=loaded.qualifies(&reason);
            std::printf("LATE FAILED range=%d resolved=%d notifications=%u disk=%d validation=%s\n",
                filespacer::selectionDetail::rangeImage().supported,filespacer::selectionDetail::rangeResolved(),notifications,valid,reason);
            return 1;
        }
        std::printf("PASS: existing private path retained, independent late range published, notifications=%u\n",notifications);return 0;
    }
    if(live) {
        for(unsigned i=0;i<20 && !notifications;++i)pump();
        std::printf("RESOLVED supported=%d reason=%s notifications=%u dispatched=%u\n",
            supported,filespacer::selectionDetail::image().reason,notifications,dispatched);
        if(!dispatched || IsWindowVisible(window))return 1;
        if(!supported) {
            // The public path was already armed; only publishing a private
            // capability notifies the UI. Unsupported discovery stays silent.
            if(notifications || FAILED(operation.clear(&view,[]{return true;},&applied))||view.calls!=2||applied)return 1;
            std::puts("FALLBACK: discovery completed; public clearing remains usable");
            return ERROR_NOT_SUPPORTED;
        }
        if(!notifications)return 1;
        // Shell acceleration is usable while its independent range capability
        // is still being qualified. Wait only in this test before reusing the
        // producer cache; resolved() intentionally no longer means producer idle.
        const ULONGLONG deadline=GetTickCount64()+180000;
        while(!filespacer::selectionDetail::rangeResolved()&&GetTickCount64()<deadline) {
            if(!filespacer::selectionDetail::image().supported)return 1;
            pump();
        }
        if(!filespacer::selectionDetail::rangeResolved())return 3;
        std::printf("PASS: early public call, UI dispatch=%u, publication notifications=%u, closed window safe\n",dispatched,notifications);return 0;
    }
    std::printf("RESOLVED supported=%d reason=%s\n",supported,filespacer::selectionDetail::image().reason);
    if(wcscmp(argv[1],L"--range-supported")==0)return supported&&filespacer::selectionDetail::rangeImage().supported?0:1;
    if(wcscmp(argv[1],L"--range-fallback")==0)return supported&&!filespacer::selectionDetail::rangeImage().supported?0:1;
    if(wcscmp(argv[1],L"--supported")==0)return supported?0:1;
    if(wcscmp(argv[1],L"--fallback")==0 || publicOnly)return supported?1:0;
    return 2;
}
