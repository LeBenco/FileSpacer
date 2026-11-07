#include <common.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shldisp.h>
#include <shdispid.h>
#include <docobj.h>
#include <propkey.h>
#include <propvarutil.h>
#include <atlbase.h>
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "DPI.h"
#include "ShellViewHost.h"
#include "ShellViewIcons.h"
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
FILE *journal=nullptr;
LARGE_INTEGER frequency;
CComPtr<IShellView> keyboardView;
double clockMs(){LARGE_INTEGER n;QueryPerformanceCounter(&n);return 1000.0*static_cast<double>(n.QuadPart)/static_cast<double>(frequency.QuadPart);}
void need(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
void pump(DWORD duration){
    const ULONGLONG deadline=GetTickCount64()+duration;
    do{MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){
            CComPtr<IShellView> current=keyboardView;
            if(!current||current->TranslateAccelerator(&m)!=S_OK){TranslateMessage(&m);DispatchMessageW(&m);}
        }
        if(GetTickCount64()>=deadline)return;MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);
    }while(true);
}
struct Sink : IDispatch {
    LONG refs=1;unsigned done=0;
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=IID_IUnknown&&id!=IID_IDispatch&&id!=DIID_DShellFolderViewEvents)return E_NOINTERFACE;
        *out=static_cast<IDispatch*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&refs);}
    STDMETHODIMP_(ULONG) Release()override{ULONG left=InterlockedDecrement(&refs);if(!left)delete this;return left;}
    STDMETHODIMP GetTypeInfoCount(UINT *n)override{if(!n)return E_POINTER;*n=0;return S_OK;}
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo**)override{return E_NOTIMPL;}
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*)override{return E_NOTIMPL;}
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*,VARIANT *out,EXCEPINFO*,UINT*)override{
        if(out)VariantInit(out);if(id==DISPID_FILELISTENUMDONE)++done;return S_OK;
    }
};
struct Events : IExplorerBrowserEvents {
    LONG refs=1;unsigned generation=0,navigation=0;HRESULT observation=E_PENDING;
    CComPtr<IShellView> shell;CComPtr<Sink> sink;CComPtr<IConnectionPoint> connection;DWORD cookie=0;
    void detach(){if(keyboardView==shell)keyboardView.Release();if(connection)connection->Unadvise(cookie);connection.Release();sink.Release();shell.Release();}
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IExplorerBrowserEvents)return E_NOINTERFACE;
        *out=static_cast<IExplorerBrowserEvents*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&refs);}
    STDMETHODIMP_(ULONG) Release()override{ULONG left=InterlockedDecrement(&refs);if(!left)delete this;return left;}
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE)override{++navigation;return S_OK;}
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnViewCreated(IShellView *view)override{
        detach();++generation;shell=view;keyboardView=view;CComPtr<IDispatch> automation;
        observation=view->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&automation));
        if(FAILED(observation))return S_OK;
        CComQIPtr<IConnectionPointContainer> container(automation);
        observation=container?container->FindConnectionPoint(DIID_DShellFolderViewEvents,&connection):E_NOINTERFACE;
        if(SUCCEEDED(observation)){sink.Attach(new Sink);observation=connection->Advise(sink,&cookie);}
        return S_OK;
    }
};
struct PlacementHost {
    HWND frame=nullptr;
    filespacer::ShellViewHost host;
    CComPtr<Events> events;
    ~PlacementHost(){if(events)events->detach();host.destroy();if(frame)DestroyWindow(frame);}
    CComPtr<IFolderView2> view(){CComPtr<IFolderView2> v;need(SUCCEEDED(host.currentView(IID_PPV_ARGS(&v))),"No active view");return v;}
    HRESULT restorePositions(IFolderView *view,const std::vector<uint8_t> &bytes,const std::function<bool()> &current){
        // Rejected optimization, retained only in this experiment. The parent
        // belongs to the test; no private Shell window is discovered or altered.
        CComQIPtr<IShellView> shell(view);HWND body=nullptr;
        need(shell&&SUCCEEDED(shell->GetWindow(&body))&&body,"Owned view window unavailable");
        const HWND suspended=IsWindowVisible(frame)?frame:nullptr;
        struct Resume{HWND window;~Resume(){if(window&&IsWindow(window)){SendMessageW(window,WM_SETREDRAW,TRUE,0);RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN);}}}resume{suspended};
        if(suspended)SendMessageW(suspended,WM_SETREDRAW,FALSE,0);
        std::printf("OWNED PARENT: suspended=%d effective_view_visible=%d\n",suspended!=nullptr,IsWindowVisible(body)!=FALSE);std::fflush(stdout);
        return filespacer::restoreIconPositions(view,bytes,current);
    }
    void create(const wchar_t *path){
        frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Placement prototype",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            50,50,850,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(!!frame,"No frame");
        RECT bounds={};GetClientRect(frame,&bounds);FOLDERSETTINGS settings={FVM_ICON,0};events.Attach(new Events);
        need(SUCCEEDED(host.initialize(frame,bounds,settings,nullptr,events)),"Host initialize failed");
        CComPtr<IShellItem> item;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&item))),"Fixture unavailable");
        need(SUCCEEDED(host.navigate(item,false,true)),"Navigation failed");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while((!events->sink||!events->sink->done||!events->navigation)&&GetTickCount64()<deadline)pump(10);
        need(events->sink&&events->sink->done&&events->navigation,"Enumeration did not complete");
        auto v=view();int count=0;need(SUCCEEDED(v->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Wrong fixture count");
        need(SUCCEEDED(v->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0))&&v->GetAutoArrange()==S_FALSE,"Free positioning unavailable");
        events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS);pump(100);
    }
};
std::vector<uint8_t> positionBlob(const std::vector<PCUITEMID_CHILD> &ids,const std::vector<POINT> &points){
    CComPtr<IStream> stream;stream.Attach(SHCreateMemStream(nullptr,0));need(!!stream,"No position stream");
    for(size_t i=0;i<ids.size();++i){const POINT dip=filespacer::invScaleDPI(points[i]);need(SUCCEEDED(IStream_WritePidl(stream,ids[i])),"PIDL write failed");need(SUCCEEDED(IStream_Write(stream,&dip,sizeof(dip))),"Position write failed");}
    ULARGE_INTEGER size={};need(SUCCEEDED(IStream_Size(stream,&size))&&size.QuadPart<=INT32_MAX,"Bad stream size");
    std::vector<uint8_t> bytes(static_cast<size_t>(size.QuadPart));LARGE_INTEGER zero={};need(SUCCEEDED(stream->Seek(zero,STREAM_SEEK_SET,nullptr)),"Seek failed");
    need(SUCCEEDED(IStream_Read(stream,bytes.data(),static_cast<ULONG>(bytes.size()))),"Stream read failed");return bytes;
}
void placement(const wchar_t *path){
    PlacementHost owner;owner.create(path);auto view=owner.view();
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;
    std::vector<PCUITEMID_CHILD> ids;std::vector<POINT> base,target;
    CComPtr<IEnumIDList> enumeration;need(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&enumeration))),"No items");
    for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(enumeration->Next(1,&id,nullptr)==S_OK,"Short enumeration");
        // Explicit fixture coordinates avoid measuring the Shell's lazy initial layout.
        POINT dip={30+(i%50)*120,30+(i/50)*100};const POINT p=filespacer::scaleDPI(dip);dip.x+=17;dip.y+=19;
        ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);base.push_back(p);target.push_back(filespacer::scaleDPI(dip));}
    need(SUCCEEDED(view->SelectAndPositionItems(2,ids.data(),nullptr,static_cast<DWORD>(SVSI_SELECT)|SVSI_NOTAKEFOCUS)),"Seed selection failed");
    need(SUCCEEDED(owner.events->shell->SelectItem(ids[1],static_cast<UINT>(SVSI_SELECT)|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Seed focus failed");
    int focused=-1;view->GetFocusedItem(&focused);need(focused>=0,"No focused item");CComHeapPtr<ITEMID_CHILD> focus;view->Item(focused,&focus);
    HWND body=nullptr;owner.events->shell->GetWindow(&body);const HWND keyboard=GetFocus();
    const auto baseline=positionBlob(ids,base), shifted=positionBlob(ids,target);
    const auto partial=positionBlob(std::vector<PCUITEMID_CHILD>(ids.begin(),ids.begin()+16),std::vector<POINT>(target.begin(),target.begin()+16));
    auto valid=[](){return true;};
    auto verify=[&](const std::vector<POINT> &expected){
        for(size_t i=0;i<ids.size();++i){POINT p={};const HRESULT hr=view->GetItemPosition(ids[i],&p);if(FAILED(hr)||p.x!=expected[i].x||p.y!=expected[i].y){std::fprintf(stderr,"Coordinate %zu actual=(%ld,%ld) expected=(%ld,%ld) hr=%08lX\n",i,p.x,p.y,expected[i].x,expected[i].y,static_cast<unsigned long>(hr));need(false,"Coordinate mismatch");}
            DWORD state=0;need(SUCCEEDED(view->GetSelectionState(ids[i],&state))&&((state&SVSI_SELECT)!=0)==(i<2),"Selection changed");}
        int index=-1;view->GetFocusedItem(&index);CComHeapPtr<ITEMID_CHILD> actual;if(index>=0)view->Item(index,&actual);
        need(actual&&ILIsEqual(actual,focus),"Item focus changed");need(GetFocus()==keyboard,"Keyboard focus changed");need(IsWindowVisible(body)!=FALSE,"View left hidden");
    };
    for(int rep=0;rep<3;++rep){
        // Use the existing implementation to establish and reset the fixture.
        need(SUCCEEDED(filespacer::restoreIconPositions(view,baseline,valid)),"Baseline reset failed");verify(base);
        for(int strategy=0;strategy<5;++strategy){
            const char *names[]={"public-16","public-redraw-16","current-guard-16","current-guard-5000","owned-parent-5000"};
            const char *name=names[strategy];
            const double start=clockMs();HRESULT hr=S_OK;
            if(strategy==0)hr=view->SelectAndPositionItems(16,ids.data(),target.data(),static_cast<DWORD>(SVSI_POSITIONITEM)|SVSI_NOSTATECHANGE);
            else if(strategy==1){
                hr=view->SetRedraw(FALSE);
                if(SUCCEEDED(hr)){
                    hr=view->SelectAndPositionItems(16,ids.data(),target.data(),static_cast<DWORD>(SVSI_POSITIONITEM)|SVSI_NOSTATECHANGE);
                    const HRESULT resumed=view->SetRedraw(TRUE);if(SUCCEEDED(hr))hr=resumed;
                }
            }
            else if(strategy==2)hr=filespacer::restoreIconPositions(view,partial,valid);
            else if(strategy==3)hr=filespacer::restoreIconPositions(view,shifted,valid);
            else hr=owner.restorePositions(view,shifted,valid);
            const double ms=clockMs()-start;std::fprintf(journal,"%s,%d,%.3f,0x%08lX\n",name,rep,ms,static_cast<unsigned long>(hr));std::fflush(journal);
            need(SUCCEEDED(hr),"Position application failed");auto expected=target;if(strategy<3)for(size_t i=16;i<ids.size();++i)expected[i]=base[i];verify(expected);
            need(SUCCEEDED(filespacer::restoreIconPositions(view,baseline,valid)),"Reset after strategy failed");verify(base);
        }
        const double start=clockMs();need(SUCCEEDED(owner.restorePositions(view,baseline,valid)),"Unchanged application failed");
        std::fprintf(journal,"owned-parent-unchanged,%d,%.3f,0x00000000\n",rep,clockMs()-start);std::fflush(journal);verify(base);
        std::printf("PLACEMENT repetition %d verified: 5000 identities, coordinates, selection and focus\n",rep);std::fflush(stdout);
    }
}
}
int wmain(int argc,wchar_t **argv){
    if(argc!=3||FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();WNDCLASSW wc={};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"FileSpacer Placement Prototype";RegisterClassW(&wc);int result=0;
    try{const std::wstring out=std::wstring(argv[1])+L"\\placement.csv";need(_wfopen_s(&journal,out.c_str(),L"wb")==0,"No journal");std::fputs("strategy,repetition,ms,hresult\n",journal);placement(argv[2]);}
    catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());result=1;}
    if(journal)std::fclose(journal);UnregisterClassW(wc.lpszClassName,wc.hInstance);CoUninitialize();return result;
}
