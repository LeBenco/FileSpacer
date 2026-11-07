// Independent candidate, using FileSpacer's real Shell host. Not product code.
#include <common.h>
#include "ShellViewHost.h"
#include "DPI.h"
#include <shlobj.h>
#include <shldisp.h>
#include <shdispid.h>
#include <propkey.h>
#include <atlbase.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <share.h>
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
#include <atomic>
#include <thread>
#include <WtsApi32.h>
#ifdef FILESPACER_SELECTION_PRODUCT_TEST
#include "ShellViewSelection.h"
#include "ShellSelectionRuntime.h"
namespace selectionDiscovery=filespacer::selectionDetail;
#else
#include "SelectionDiscovery.h"
#endif
#include "PresentationTiming.h"
#else
#include "ShellViewSelection.h"
#endif

namespace {
LARGE_INTEGER frequency;
void need(bool value,const char *message){if(!value)throw std::runtime_error(message);}
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
struct Precondition : std::runtime_error {using std::runtime_error::runtime_error;};
bool locked=false,cancelled=false;
bool headless=false;int fixtureCount=5000;
bool sessionUnlocked() {
    LPWSTR raw=nullptr;DWORD bytes=0;
    if(!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,WTS_CURRENT_SESSION,WTSSessionInfoEx,&raw,&bytes))return false;
    std::unique_ptr<void,decltype(&WTSFreeMemory)> memory(raw,&WTSFreeMemory);
    const auto *info=static_cast<const WTSINFOEXW *>(memory.get());
    return bytes>=sizeof(WTSINFOEXW)&&info->Level==1&&info->Data.WTSInfoExLevel1.SessionFlags==WTS_SESSIONSTATE_UNLOCK;
}
void requireSession(){if(cancelled)throw Precondition("Test window closed by user");if(!headless&&(locked||!sessionUnlocked()))throw Precondition("Host session locked or unavailable");}
LRESULT CALLBACK testWindowProc(HWND window,UINT message,WPARAM first,LPARAM second) {
    if(message==WM_CLOSE){cancelled=true;return 0;}
    if(message==WM_WTSSESSION_CHANGE){if(first==WTS_SESSION_LOCK)locked=true;else if(first==WTS_SESSION_UNLOCK)locked=false;}
    if(message==WM_NCDESTROY)WTSUnRegisterSessionNotification(window);
    return DefWindowProcW(window,message,first,second);
}
#else
const bool headless=false;const int fixtureCount=5000;
#endif
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
        frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Selection clear candidate",WS_OVERLAPPEDWINDOW|(headless?0:WS_VISIBLE),
            50,50,850,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(frame!=nullptr,"Cannot create fixture frame");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        need(WTSRegisterSessionNotification(frame,NOTIFY_FOR_THIS_SESSION)!=FALSE,"Cannot observe session lock");
#endif
        RECT bounds={};GetClientRect(frame,&bounds);const FOLDERSETTINGS settings={FVM_ICON,FWF_AUTOARRANGE};events.Attach(new ViewEvents);
        need(SUCCEEDED(host.initialize(frame,bounds,settings,nullptr,events)),"Cannot initialize real FileSpacer Shell host");
        CComPtr<IShellItem> folder;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&folder))),"Cannot bind fixture");
        need(SUCCEEDED(host.navigate(folder,false,true)),"Cannot navigate fixture");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while((!events->navigated||!events->sink||!events->sink->ready)&&GetTickCount64()<deadline)pump(10);
        need(events->navigated&&events->sink&&events->sink->ready&&SUCCEEDED(events->observed),"No complete enumeration event");
        int count=0;need(SUCCEEDED(view()->ItemCount(SVGIO_ALLVIEW,&count))&&count==fixtureCount,"Incomplete fixture");
        need(SUCCEEDED(events->shell->UIActivate(headless?SVUIA_ACTIVATE_NOFOCUS:SVUIA_ACTIVATE_FOCUS)),"Cannot activate fixture view");
        if(headless)need(!IsWindowVisible(frame)&&GetForegroundWindow()!=frame,"Hidden qualifier became visible or foreground");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        if(!headless)presentation::fitView(events->shell,frame,[&](){RECT client={};need(GetClientRect(frame,&client)!=FALSE,"No client rectangle");need(SUCCEEDED(host.setBounds(client)),"Cannot resize Shell view");});
#endif
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
#ifdef FILESPACER_SELECTION_PRODUCT_TEST
void waitSelectedIdentities(IFolderView2 *view,const std::vector<std::wstring> &wanted) {
    const ULONGLONG deadline=GetTickCount64()+5000;ULONGLONG stable=0;
    do {
        int before=-1,after=-1;CComPtr<IShellItemArray> selected;
        const bool matches=SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&before))&&before==static_cast<int>(wanted.size())
            &&SUCCEEDED(view->GetSelection(FALSE,&selected))&&selected&&arrayPaths(selected)==wanted
            &&SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&after))&&after==before;
        if(matches){if(!stable)stable=GetTickCount64();if(GetTickCount64()-stable>=100)return;}
        else stable=0;
        pump(10);
    }while(GetTickCount64()<deadline);
    need(false,"Selection count and identities did not settle after the action");
}
#endif
void checkState(IFolderView2 *view,IShellView *shell,IShellFolder *folder,const wchar_t *path,
        const std::vector<PCUITEMID_CHILD> &ids,const std::vector<std::wstring> &wanted) {
    int count=-1;const HRESULT counted=view->ItemCount(SVGIO_SELECTION,&count);
    if(FAILED(counted)||count!=static_cast<int>(wanted.size())){
        std::fprintf(stderr,"STATE expected=%zu actual=%d hr=%08lX\n",wanted.size(),count,static_cast<unsigned long>(counted));
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        requireSession();
#endif
        need(false,"Selected count differs");}
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
        CComPtr<IShellItem> item;need(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,ids[index],IID_PPV_ARGS(&item))),"Cannot bind state sample");
        need(((state&static_cast<DWORD>(SVSI_SELECT))!=0)==std::binary_search(wanted.begin(),wanted.end(),itemPath(item)),"Per-item state differs");}
}
void runClear(const std::wstring &output,const wchar_t *path) {
    FILE *file=_wfsopen((std::wstring(output)+L"\\clear-results.csv").c_str(),L"wb",_SH_DENYNO);need(file!=nullptr,"Cannot create results CSV");
    std::unique_ptr<FILE,decltype(&std::fclose)> log(file,&std::fclose);
    std::fputs("view,mode,icon_size,selected_count,repetition,arm,private_available,private_applied,clear_api_ms,identities,caches,positions,logical_focus,keyboard_focus\n",file);
    std::fflush(file);
    struct Mode {const char *name=nullptr;FOLDERVIEWMODE mode=FVM_AUTO;int size=0;};
    const Mode modes[]={
        {"extra-large",FVM_ICON,256},{"large",FVM_ICON,96},{"medium",FVM_ICON,48},
        {"small",FVM_SMALLICON,16},{"tiles",FVM_TILE,48},{"content",FVM_CONTENT,32}
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        ,{"list",FVM_LIST,16},{"details",FVM_DETAILS,16},{"medium-free",FVM_ICON,48}
#endif
    };
    bool unavailable=false;
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
    struct Parking {
        std::unique_ptr<presentation::PointerParking> value;
        ~Parking(){presentation::pointer=nullptr;}
    } parking;
    if(!headless&&GetEnvironmentVariableW(L"FS_SELECTION_VISUAL",nullptr,0)){requireSession();parking.value=std::make_unique<presentation::PointerParking>();presentation::pointer=parking.value.get();presentation::pointer->park();}
#endif
    for(const auto &mode:modes) {
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        requireSession();
        if(GetEnvironmentVariableW(L"FS_SELECTION_ONLY_MEDIUM",nullptr,0)&&std::strcmp(mode.name,"medium")!=0)continue;
#endif
        std::printf("PHASE view=%s create\n",mode.name);std::fflush(stdout);
        SelectionHost owner;owner.create(path);
        auto view=owner.view();auto *shell=owner.events->shell.p;
        need(SUCCEEDED(view->SetViewModeAndIconSize(mode.mode,mode.size)),"Cannot set representation");pump(100);
        FOLDERVIEWMODE actual=FVM_AUTO;int size=0;
        need(SUCCEEDED(view->GetViewModeAndIconSize(&actual,&size))&&actual==mode.mode&&size==mode.size,"Actual representation differs");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        if(std::strcmp(mode.name,"medium-free")==0)need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE,0)),"Cannot disable auto-arrange");
#endif
        CComPtr<IShellFolder> folder;need(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder))),"No fixture folder");
        std::vector<OwnedId> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<std::wstring> wanted;
        CComPtr<IEnumIDList> all;need(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&all))),"No fixture identities");
        for(int i=0;i<fixtureCount;++i){CComHeapPtr<ITEMID_CHILD> id;need(all->Next(1,&id,nullptr)==S_OK&&id,"Fixture count differs");
            CComPtr<IShellItem> item;need(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,id,IID_PPV_ARGS(&item))),"Cannot bind fixture item");
            wanted.push_back(itemPath(item));ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
        const auto orderedPaths=wanted;std::sort(wanted.begin(),wanted.end());
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        if(std::strcmp(mode.name,"medium-free")==0){POINT custom[]={{37,43},{189,221},{431,157}};
            need(SUCCEEDED(view->SelectAndPositionItems(3,ids.data(),custom,SVSI_DESELECT)),"Cannot seed free positions");}
#endif
        std::vector<size_t> positionIndices;
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        positionIndices={0,1,2,7,ids.size()/2,ids.size()-1};
#else
        for(size_t i=0;i<ids.size();++i)positionIndices.push_back(i);
#endif
        std::vector<POINT> positions(positionIndices.size());const bool hasPositions=!headless&&SUCCEEDED(view->GetItemPosition(ids[positionIndices[0]],&positions[0]));
        if(hasPositions)for(size_t i=1;i<positionIndices.size();++i)need(SUCCEEDED(view->GetItemPosition(ids[positionIndices[i]],&positions[i])),"Position capture failed");
        const DWORD focusOnly=static_cast<DWORD>(SVSI_FOCUSED)|static_cast<DWORD>(SVSI_NOTAKEFOCUS);
        const DWORD selectNoFocus=static_cast<DWORD>(SVSI_SELECT)|static_cast<DWORD>(SVSI_NOTAKEFOCUS);
#ifdef FILESPACER_SELECTION_PRODUCT_TEST
        const bool selectAllOnly=GetEnvironmentVariableW(L"FS_SELECTION_ALL_ONLY",nullptr,0)!=0;
        CComQIPtr<IShellFolderView> bulk(shell);
        const auto checkAll=[&]{waitSelectedIdentities(view,wanted);checkState(view,shell,folder,path,ids,wanted);};
        if(selectAllOnly) {
            std::puts("SELECT_PHASE native-baseline");std::fflush(stdout);
            need(bulk&&SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Native all-selection baseline failed");
            checkAll();
            need(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Native all-selection baseline clear failed");
        }
#endif
        need(SUCCEEDED(shell->SelectItem(ids[7],focusOnly)),"Cannot seed independent focus");
        const HWND keyboard=GetFocus();
        filespacer::ShellSelectionClear accelerated, fallback;
        auto current=[&]() {return IsWindow(owner.frame)&&owner.events->shell.p==shell;};
        const double bindStarted=clockMs();
        accelerated.bind(shell,current);
        std::printf("BIND %s binding_ms=%.3f\n",mode.name,clockMs()-bindStarted);
        std::printf("VIEW %s private_available=%d mode=%d icon_size=%d\n",mode.name,accelerated.accelerated(),actual,size);std::fflush(stdout);
#ifdef FILESPACER_SELECTION_PRODUCT_TEST
        if(selectAllOnly) {
            std::puts("SELECT_PHASE candidate-all");std::fflush(stdout);
            const double started=clockMs();
            need(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Global select through product binding failed");
            const double elapsed=clockMs()-started;
            const bool expected=selectionDiscovery::image().selectAllSupported;
            need(accelerated.acceleratedSelectAllCount()==(expected?1u:0u),"Wrong global-selection backend used");
            checkAll();
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focusedId;
            need(SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focusedId))
                &&ILIsEqual(focusedId,ids[7])&&GetFocus()==keyboard,"Global selection changed focus");
            if(hasPositions)for(size_t i=0;i<positionIndices.size();++i){POINT currentPosition={};
                need(SUCCEEDED(view->GetItemPosition(ids[positionIndices[i]],&currentPosition))&&currentPosition.x==positions[i].x
                    &&currentPosition.y==positions[i].y,"Global selection changed positions");}
            CComPtr<IShellItemArray> retained;
            need(SUCCEEDED(view->GetSelection(FALSE,&retained)),"No retained global selection snapshot");
            const SORTCOLUMN sort={PKEY_ItemNameDisplay,SORT_DESCENDING};
            std::puts("SELECT_PHASE sort");std::fflush(stdout);
            need(SUCCEEDED(view->SetSortColumns(&sort,1)),"Global selection sort failed");
            checkAll();
            need(SUCCEEDED(accelerated.clear(shell,current)),"Global selection subsequent clear failed");
            std::puts("SELECT_PHASE post-sort-clear");std::fflush(stdout);
            checkState(view,shell,folder,path,ids,{});
            need(arrayPaths(retained)==wanted,"Global clear corrupted retained snapshot");
            for(int quantity:{1,1000,fixtureCount-1}) {
                need(SUCCEEDED(view->SelectAndPositionItems(static_cast<UINT>(quantity),ids.data(),nullptr,selectNoFocus)),"Partial selection setup failed");
                std::vector<std::wstring> partial(orderedPaths.begin(),orderedPaths.begin()+quantity);std::sort(partial.begin(),partial.end());
                CComPtr<IShellItemArray> previous;
                need(SUCCEEDED(view->GetSelection(FALSE,&previous))&&arrayPaths(previous)==partial,"Partial selection snapshot differs");
                std::puts("SELECT_PHASE partial-to-all");std::fflush(stdout);
                const uint64_t before=accelerated.acceleratedSelectAllCount();const double partialStarted=clockMs();
                need(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Partial-to-all selection failed");
                const double partialElapsed=clockMs()-partialStarted;
                need(accelerated.acceleratedSelectAllCount()==before+(expected?1u:0u),"Wrong partial-to-all backend used");
                checkAll();need(arrayPaths(previous)==partial,"Partial-to-all selection corrupted retained snapshot");
                need(SUCCEEDED(accelerated.clear(shell,current)),"Partial-to-all selection clear failed");
                std::printf("PARTIAL_ALL view=%s previous=%d count=%d private_applied=%d api_ms=%.3f identities=PASS retained=PASS\n",
                    mode.name,quantity,fixtureCount,expected,partialElapsed);std::fflush(stdout);
            }
            accelerated.reset();
            std::puts("SELECT_PHASE detached-all");std::fflush(stdout);
            need(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Detached global-selection fallback failed");
            checkAll();
            std::printf("SELECT_ALL view=%s count=%d private_applied=%d api_ms=%.3f identities=PASS caches=PASS payload=PASS sort=PASS focus=PASS fallback=PASS\n",
                mode.name,fixtureCount,expected,elapsed);std::fflush(stdout);
            continue;
        }
#endif
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        const bool smoke=GetEnvironmentVariableW(L"FS_SELECTION_SMOKE",nullptr,0)!=0;
        wchar_t smokeCount[16]={};GetEnvironmentVariableW(L"FS_SELECTION_SMOKE",smokeCount,_countof(smokeCount));
        const int smokeQuantity=wcscmp(smokeCount,L"5000")==0?5000:32;
        const bool visual=!headless&&GetEnvironmentVariableW(L"FS_SELECTION_VISUAL",nullptr,0)!=0;
        const std::vector<int> quantities=fixtureCount==64?std::vector<int>{0,1,32,64}:visual?std::vector<int>{5000}:smoke?std::vector<int>{smokeQuantity}:std::vector<int>{0,1,32,1000,5000};
        const int repetitions=(smoke||fixtureCount==64)?1:3;
        need(accelerated.clear(shell,[]{return false;})==E_ABORT,"Stale continuation was not stopped");
        need(fallback.clear(shell,[]{return false;})==E_ABORT,"Stale native continuation was not stopped");
#else
        const std::vector<int> quantities={5000};const int repetitions=3;
#endif
        for(int quantity:quantities)for(int repetition=0;repetition<repetitions;++repetition)for(int order=0;order<3;++order) {
            const int arm=(order+repetition)%3;
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
            if(locked||cancelled)requireSession();
            std::printf("OP_BEGIN view=%s count=%d repetition=%d arm=%d\n",mode.name,quantity,repetition,arm);std::fflush(stdout);
            const bool checkPositions=quantity==5000&&repetition==0;
#else
            const bool checkPositions=true;
#endif
            need(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Initial clear failed");
            std::vector<PCUITEMID_CHILD> chosen;std::vector<std::wstring> selectedPaths;
            for(int i=0;i<quantity;++i){const size_t index=(static_cast<size_t>(i)*37)%ids.size();chosen.push_back(ids[index]);selectedPaths.push_back(orderedPaths[index]);}
            std::sort(selectedPaths.begin(),selectedPaths.end());
            if(quantity)need(SUCCEEDED(view->SelectAndPositionItems(static_cast<UINT>(chosen.size()),chosen.data(),nullptr,selectNoFocus)),"Cannot seed fixture selection");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
            // Setup is outside the clear timer. Let native queued selection
            // notifications settle before asserting caches or timing a clear.
            pump(100);
#endif
            checkState(view,shell,folder,path,ids,selectedPaths);
            CComPtr<IShellItemArray> retained;
            if(quantity)need(SUCCEEDED(view->GetSelection(FALSE,&retained))&&retained,"Cannot retain selection snapshot");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
            std::unique_ptr<presentation::Observation> observation;
            if(visual&&arm<2){HWND body=nullptr;need(SUCCEEDED(shell->GetWindow(&body)),"No public view HWND");
                observation=std::make_unique<presentation::Observation>(body,output+L"\\"+std::wstring(mode.name,mode.name+std::strlen(mode.name))+(arm==0?L"-normal":L"-candidate")+L"-r"+std::to_wstring(repetition));observation->begin();}
#endif
            bool applied=false;const double began=clockMs();
            const HRESULT result=arm==0?shell->SelectItem(nullptr,SVSI_DESELECTOTHERS):arm==1?accelerated.clear(shell,current,&applied):fallback.clear(shell,current,&applied);
            const double elapsed=clockMs()-began;need(SUCCEEDED(result),"Clear or aggregate notification failed");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
            if(arm==1&&applied){bool known=false;const bool released=accelerated.backingBufferReleased(known);
                if(known)std::printf("ALLOCATION view=%s count=%d native_buffer_released=%d\n",mode.name,quantity,released);}
            if(observation){observation->apiReturned();observation->finish();}
#endif
            checkState(view,shell,folder,path,ids,{});
            pump(100);checkState(view,shell,folder,path,ids,{});
            if(retained)need(arrayPaths(retained)==selectedPaths,"Previously returned selection snapshot was corrupted");
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focus;
            need(SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focus))&&ILIsEqual(focus,ids[7]),"Clear changed logical focus");
            need(GetFocus()==keyboard,"Clear changed keyboard focus");
            if(hasPositions&&checkPositions)for(size_t i=0;i<positionIndices.size();++i){POINT point={};need(SUCCEEDED(view->GetItemPosition(ids[positionIndices[i]],&point))&&point.x==positions[i].x&&point.y==positions[i].y,"Clear moved an item");}
            if(arm==1&&quantity&&!applied)unavailable=true;
            std::fprintf(file,"%s,%d,%d,%d,%d,%s,%d,%d,%.3f,PASS,PASS,%s,PASS,PASS\n",mode.name,actual,size,quantity,repetition,
                arm==0?"normal":arm==1?"candidate":"forced-fallback",accelerated.accelerated(),arm==1&&applied,elapsed,hasPositions?(checkPositions?(positionIndices.size()==ids.size()?"PASS":"SAMPLED_6"):"NOT_CHECKED"):"UNAVAILABLE");std::fflush(file);
            // A normal subsequent action must remain usable after every arm.
            need(SUCCEEDED(shell->SelectItem(ids[7],selectNoFocus)),"Follow-up selection failed");
            CComPtr<IShellItemArray> next;CComPtr<IShellItem> expected;
            need(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,ids[7],IID_PPV_ARGS(&expected))),"Missing follow-up identity");
            need(SUCCEEDED(view->GetSelection(FALSE,&next))&&arrayPaths(next)==std::vector<std::wstring>{itemPath(expected)},"Follow-up snapshot identity failed");
            need(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Follow-up clear failed");
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
            pump(100);checkState(view,shell,folder,path,ids,{});
#endif
            std::printf("CHECK view=%s repetition=%d arm=%d clear_api_ms=%.3f private_applied=%d PASS\n",mode.name,repetition,arm,elapsed,arm==1&&applied);std::fflush(stdout);
        }
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
        owner.events->sink->ready=false;
        need(SUCCEEDED(shell->Refresh()),"Refresh failed");const ULONGLONG refreshed=GetTickCount64()+45000;
        while(!owner.events->sink->ready&&GetTickCount64()<refreshed)pump(10);
        need(owner.events->sink->ready,"Refresh enumeration did not complete");
        need(SUCCEEDED(shell->SelectItem(ids[7],selectNoFocus)),"Post-refresh selection failed");
        bool refreshedApplied=false;need(SUCCEEDED(accelerated.clear(shell,current,&refreshedApplied)),"Post-refresh clear failed");
        checkState(view,shell,folder,path,ids,{});
        std::printf("REFRESH %s private_applied=%d\n",mode.name,refreshedApplied);
        accelerated.reset();need(SUCCEEDED(accelerated.clear(shell,current)),"Reset binding did not use public fallback");
#endif
    }
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
    std::printf("COVERAGE acceleration=%s explorer=NOT_MEASURED\n",unavailable?"INCOMPLETE":"EXERCISED");
#else
    need(!unavailable,"UNSUPPORTED: the accelerated path was not exercised in every candidate case");
#endif
}
}
int wmain(int argc,wchar_t **argv) {
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
    if(argc==2&&wcscmp(argv[1],L"--session-state")==0){const bool unlocked=sessionUnlocked();std::printf("SESSION unlocked=%d\n",unlocked);return unlocked?0:3;}
    if(argc==3&&wcscmp(argv[1],L"--inspect-profile")==0){
#ifdef FILESPACER_SELECTION_PRODUCT_TEST
        return selectionDiscovery::Image::discover(argv[2]).supported?0:4;}
#else
        if(!SetEnvironmentVariableW(L"FS_SELECTION_DISCOVERY_DATA",argv[2]))return 2;
        filespacer::initializeShellSelectionClear();return selectionDiscovery::image().supported?0:4;}
#endif
#endif
    wchar_t machine[MAX_COMPUTERNAME_LENGTH+1]={};DWORD length=_countof(machine);
    if(!GetComputerNameW(machine,&length)||(argc!=3&&argc!=4))return 2;
    const bool allowHost=argc==4&&wcscmp(argv[3],L"--allow-host")==0;
    if(_wcsicmp(machine,L"AGENTWIN10")!=0&&!allowHost)return 2;
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
    headless=GetEnvironmentVariableW(L"FS_SELECTION_HEADLESS",nullptr,0)!=0;
    if(GetEnvironmentVariableW(L"FS_SELECTION_FIXTURE_64",nullptr,0))fixtureCount=64;
    std::printf("CONTEXT headless=%d fixture_count=%d presentation=%s\n",headless,fixtureCount,headless?"DISABLED":"SEPARATE_PASS");
#endif
    const double started=clockMs();
#ifdef FILESPACER_SELECTION_PRODUCT_TEST
    wchar_t cache[32768]={};
    const DWORD cacheLength=GetEnvironmentVariableW(L"FS_SELECTION_PRODUCT_CACHE",cache,_countof(cache));
    if(!cacheLength||cacheLength>=_countof(cache))return 2;
    filespacer::initializeShellSelectionClear(cache,false);
    const ULONGLONG resolvedDeadline=GetTickCount64()+10000;
    while(!selectionDiscovery::resolved()&&GetTickCount64()<resolvedDeadline)pump(1);
    if(!selectionDiscovery::resolved())return 3;
    std::printf("PRODUCT supported=%d reason=%s\n",selectionDiscovery::image().supported,selectionDiscovery::image().reason);
#else
    filespacer::initializeShellSelectionClear();
#endif
    std::printf("STARTUP selection_profile_ms=%.3f\n",clockMs()-started);
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Placement Prototype";
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
    type.lpfnWndProc=testWindowProc;
#endif
    int result=0;
    try{need(RegisterClassW(&type)!=0,"Cannot register prototype window");runClear(argv[1],argv[2]);}
#ifdef FILESPACER_SELECTION_DISCOVERY_TEST
    catch(const Precondition &error){std::fprintf(stderr,"PRECONDITION: %s\n",error.what());result=3;}
#endif
    catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
