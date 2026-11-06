// Independent candidate, using FileSpacer's real Shell host. Not product code.
#include <common.h>
#include "ShellViewHost.h"
#include "DPI.h"
#include <shlobj.h>
#include <shldisp.h>
#include <shdispid.h>
#include <atlbase.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "SelectionClearPrototype.h"

namespace {
LARGE_INTEGER frequency;
void need(bool value,const char *message){if(!value)throw std::runtime_error(message);}
double clockMs(){LARGE_INTEGER value;QueryPerformanceCounter(&value);return 1000.0*static_cast<double>(value.QuadPart)/static_cast<double>(frequency.QuadPart);}
void pump(DWORD milliseconds){
    const ULONGLONG end=GetTickCount64()+milliseconds;
    do{MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(GetTickCount64()>=end)return;MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);
    }while(true);
}
struct EnumerationSink : IDispatch {
    LONG references=1;bool ready=false;
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=IID_IUnknown&&id!=IID_IDispatch&&id!=DIID_DShellFolderViewEvents)return E_NOINTERFACE;
        *out=static_cast<IDispatch *>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP GetTypeInfoCount(UINT *count)override{if(!count)return E_POINTER;*count=0;return S_OK;}
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo **)override{return E_NOTIMPL;}
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR *,UINT,LCID,DISPID *)override{return E_NOTIMPL;}
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS *,VARIANT *out,EXCEPINFO *,UINT *)override{
        if(out)VariantInit(out);if(id==DISPID_FILELISTENUMDONE)ready=true;return S_OK;
    }
};
struct ViewEvents : IExplorerBrowserEvents {
    LONG references=1;bool navigated=false;HRESULT observed=E_PENDING;
    CComPtr<IShellView> shell;CComPtr<EnumerationSink> sink;CComPtr<IConnectionPoint> connection;DWORD cookie=0;
    void detach(){if(connection)connection->Unadvise(cookie);connection.Release();sink.Release();shell.Release();}
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IExplorerBrowserEvents)return E_NOINTERFACE;
        *out=static_cast<IExplorerBrowserEvents *>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE)override{navigated=true;return S_OK;}
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnViewCreated(IShellView *view)override{
        detach();shell=view;CComPtr<IDispatch> automation;observed=view->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&automation));
        if(FAILED(observed))return S_OK;
        CComQIPtr<IConnectionPointContainer> container(automation);
        observed=container?container->FindConnectionPoint(DIID_DShellFolderViewEvents,&connection):E_NOINTERFACE;
        if(SUCCEEDED(observed)){sink.Attach(new EnumerationSink);observed=connection->Advise(sink,&cookie);}return S_OK;
    }
};
struct SelectionHost {
    HWND frame=nullptr;filespacer::ShellViewHost host;CComPtr<ViewEvents> events;
    ~SelectionHost(){if(events)events->detach();host.destroy();if(frame)DestroyWindow(frame);}
    CComPtr<IFolderView2> view(){CComPtr<IFolderView2> result;need(SUCCEEDED(host.currentView(IID_PPV_ARGS(&result))),"No Shell view");return result;}
    void create(const wchar_t *path){
        frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Selection clear candidate",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            50,50,850,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(frame!=nullptr,"Cannot create fixture frame");
        RECT bounds={};GetClientRect(frame,&bounds);const FOLDERSETTINGS settings={FVM_ICON,FWF_AUTOARRANGE};events.Attach(new ViewEvents);
        need(SUCCEEDED(host.initialize(frame,bounds,settings,nullptr,events)),"Cannot initialize real FileSpacer Shell host");
        CComPtr<IShellItem> folder;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&folder))),"Cannot bind fixture");
        need(SUCCEEDED(host.navigate(folder,false,true)),"Cannot navigate fixture");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while((!events->navigated||!events->sink||!events->sink->ready)&&GetTickCount64()<deadline)pump(10);
        need(events->navigated&&events->sink&&events->sink->ready&&SUCCEEDED(events->observed),"No complete enumeration event");
        int count=0;need(SUCCEEDED(view()->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Incomplete fixture");
        need(SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"Cannot activate fixture view");
    }
};
using OwnedId=std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>;
std::wstring itemPath(IShellItem *item) {
    CComHeapPtr<wchar_t> name;
    need(item&&SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&name)),"Cannot read selected identity");
    return name.m_pData;
}
std::vector<std::wstring> arrayPaths(IShellItemArray *array) {
    DWORD count=0;need(array&&SUCCEEDED(array->GetCount(&count)),"Cannot read selection array");
    std::vector<std::wstring> names;
    for(DWORD i=0;i<count;++i){CComPtr<IShellItem> item;need(SUCCEEDED(array->GetItemAt(i,&item)),"Short selection array");names.push_back(itemPath(item));}
    std::sort(names.begin(),names.end());return names;
}
void checkState(IFolderView2 *view,IShellView *shell,IShellFolder *folder,const wchar_t *path,
        const std::vector<PCUITEMID_CHILD> &ids,const std::vector<std::wstring> &wanted) {
    int count=-1;need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count))&&count==static_cast<int>(wanted.size()),"Selected count differs");
    for(BOOL parent:{FALSE,TRUE}) {
        CComPtr<IShellItemArray> array;const HRESULT hr=view->GetSelection(parent,&array);
        if(wanted.empty()&&!parent) {
            need((hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)&&!array)||(SUCCEEDED(hr)&&(!array||arrayPaths(array).empty())),"Stale ordinary selection cache");
        } else {
            need(SUCCEEDED(hr)&&array,"Selection array is missing");
            const auto names=arrayPaths(array);
            const std::vector<std::wstring> expected=wanted.empty()?std::vector<std::wstring>{path}:wanted;
            need(names==expected,"Selected identities or parent-folder cache differ");
        }
    }
    CComPtr<IEnumIDList> enumeration;const HRESULT enumerated=view->Items(SVGIO_SELECTION,IID_PPV_ARGS(&enumeration));
    std::vector<std::wstring> actual;
    if(enumeration) {
        while(true){CComHeapPtr<ITEMID_CHILD> id;const HRESULT hr=enumeration->Next(1,&id,nullptr);if(hr==S_FALSE)break;
            need(hr==S_OK&&id,"Selected enumeration failed");CComPtr<IShellItem> item;
            need(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,id,IID_PPV_ARGS(&item))),"Cannot bind selected identity");actual.push_back(itemPath(item));
            need(actual.size()<=wanted.size(),"Selected enumeration contains stale items");}
    } else need(wanted.empty()&&enumerated==HRESULT_FROM_WIN32(ERROR_NOT_FOUND),"Selected enumeration missing");
    std::sort(actual.begin(),actual.end());need(actual==wanted,"Enumerated selected identities differ");
    CComPtr<IDataObject> data;const HRESULT object=shell->GetItemObject(SVGIO_SELECTION,IID_PPV_ARGS(&data));
    if(SUCCEEDED(object)&&data){CComPtr<IShellItemArray> array;const HRESULT converted=SHCreateShellItemArrayFromDataObject(data,IID_PPV_ARGS(&array));
        need((SUCCEEDED(converted)&&arrayPaths(array)==wanted)||(wanted.empty()&&FAILED(converted)),"Selection data object differs");}
    else need(wanted.empty(),"Selection data object missing");
    for(size_t index:{size_t(0),size_t(7),ids.size()-1}){DWORD state=0;
        need(SUCCEEDED(view->GetSelectionState(ids[index],&state)),"Cannot query item selection state");
        need(((state&static_cast<DWORD>(SVSI_SELECT))!=0)==!wanted.empty(),"Per-item state differs");}
}
void runClear(const std::wstring &output,const wchar_t *path) {
    FILE *file=nullptr;need(_wfopen_s(&file,(std::wstring(output)+L"\\clear-results.csv").c_str(),L"wb")==0,"Cannot create results CSV");
    std::unique_ptr<FILE,decltype(&std::fclose)> log(file,&std::fclose);
    std::fputs("view,mode,icon_size,repetition,arm,private_available,private_applied,clear_api_ms,identities,caches,positions,logical_focus,keyboard_focus\n",file);
    struct Mode {const char *name=nullptr;FOLDERVIEWMODE mode=FVM_AUTO;int size=0;};
    const Mode modes[]={
        {"extra-large",FVM_ICON,256},{"large",FVM_ICON,96},{"medium",FVM_ICON,48},
        {"small",FVM_SMALLICON,16},{"tiles",FVM_TILE,48},{"content",FVM_CONTENT,32}};
    bool unavailable=false;
    for(const auto &mode:modes) {
        SelectionHost owner;owner.create(path);
        auto view=owner.view();auto *shell=owner.events->shell.p;
        need(SUCCEEDED(view->SetViewModeAndIconSize(mode.mode,mode.size)),"Cannot set representation");pump(100);
        FOLDERVIEWMODE actual=FVM_AUTO;int size=0;
        need(SUCCEEDED(view->GetViewModeAndIconSize(&actual,&size))&&actual==mode.mode&&size==mode.size,"Actual representation differs");
        CComPtr<IShellFolder> folder;need(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder))),"No fixture folder");
        std::vector<OwnedId> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<std::wstring> wanted;
        CComPtr<IEnumIDList> all;need(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&all))),"No fixture identities");
        for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(all->Next(1,&id,nullptr)==S_OK&&id,"Fixture count differs");
            CComPtr<IShellItem> item;need(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,id,IID_PPV_ARGS(&item))),"Cannot bind fixture item");
            wanted.push_back(itemPath(item));ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
        std::sort(wanted.begin(),wanted.end());
        std::vector<POINT> positions(ids.size());const bool hasPositions=SUCCEEDED(view->GetItemPosition(ids[0],&positions[0]));
        if(hasPositions)for(size_t i=1;i<ids.size();++i)need(SUCCEEDED(view->GetItemPosition(ids[i],&positions[i])),"Position capture failed");
        const DWORD focusOnly=static_cast<DWORD>(SVSI_FOCUSED)|static_cast<DWORD>(SVSI_NOTAKEFOCUS);
        const DWORD selectNoFocus=static_cast<DWORD>(SVSI_SELECT)|static_cast<DWORD>(SVSI_NOTAKEFOCUS);
        need(SUCCEEDED(shell->SelectItem(ids[7],focusOnly)),"Cannot seed independent focus");
        const HWND keyboard=GetFocus();
        auto accelerated=selectionDispatch::Operations::choose(shell,false);
        auto fallback=selectionDispatch::Operations::choose(shell,true);
        std::printf("VIEW %s private_available=%d mode=%d icon_size=%d\n",mode.name,accelerated.accelerated,actual,size);std::fflush(stdout);
        for(int repetition=0;repetition<3;++repetition)for(int order=0;order<3;++order) {
            const int arm=(order+repetition)%3;
            need(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Initial clear failed");
            need(SUCCEEDED(view->SelectAndPositionItems(5000,ids.data(),nullptr,selectNoFocus)),"Cannot select all fixture items");
            checkState(view,shell,folder,path,ids,wanted); // Warm both CDefView caches.
            accelerated.applied=false;const double began=clockMs();
            const HRESULT result=arm==0?shell->SelectItem(nullptr,SVSI_DESELECTOTHERS):arm==1?accelerated.clear(shell):fallback.clear(shell);
            const double elapsed=clockMs()-began;need(SUCCEEDED(result),"Clear or aggregate notification failed");
            checkState(view,shell,folder,path,ids,{});
            pump(100);checkState(view,shell,folder,path,ids,{});
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focus;
            need(SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focus))&&ILIsEqual(focus,ids[7]),"Clear changed logical focus");
            need(GetFocus()==keyboard,"Clear changed keyboard focus");
            if(hasPositions)for(size_t i=0;i<ids.size();++i){POINT point={};need(SUCCEEDED(view->GetItemPosition(ids[i],&point))&&point.x==positions[i].x&&point.y==positions[i].y,"Clear moved an item");}
            if(arm==1&&!accelerated.applied)unavailable=true;
            std::fprintf(file,"%s,%d,%d,%d,%s,%d,%d,%.3f,PASS,PASS,%s,PASS,PASS\n",mode.name,actual,size,repetition,
                arm==0?"normal":arm==1?"candidate":"forced-fallback",accelerated.accelerated,arm==1&&accelerated.applied,elapsed,hasPositions?"PASS":"UNAVAILABLE");std::fflush(file);
            // A normal subsequent action must remain usable after every arm.
            need(SUCCEEDED(shell->SelectItem(ids[7],selectNoFocus)),"Follow-up selection failed");
            CComPtr<IShellItemArray> next;CComPtr<IShellItem> expected;
            need(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,ids[7],IID_PPV_ARGS(&expected))),"Missing follow-up identity");
            need(SUCCEEDED(view->GetSelection(FALSE,&next))&&arrayPaths(next)==std::vector<std::wstring>{itemPath(expected)},"Follow-up snapshot identity failed");
            need(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Follow-up clear failed");
            std::printf("CHECK view=%s repetition=%d arm=%d clear_api_ms=%.3f private_applied=%d PASS\n",mode.name,repetition,arm,elapsed,arm==1&&accelerated.applied);std::fflush(stdout);
        }
    }
    need(!unavailable,"UNSUPPORTED: the accelerated path was not exercised in every candidate case");
}
}
int wmain(int argc,wchar_t **argv) {
    wchar_t machine[MAX_COMPUTERNAME_LENGTH+1]={};DWORD length=_countof(machine);
    if(!GetComputerNameW(machine,&length)||_wcsicmp(machine,L"AGENTWIN10")!=0||argc!=3)return 2;
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Placement Prototype";
    int result=0;
    try{need(RegisterClassW(&type)!=0,"Cannot register prototype window");runClear(argv[1],argv[2]);}
    catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
