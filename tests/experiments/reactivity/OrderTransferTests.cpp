// Independent Win32/Shell experiment: no FileSpacer hosting or state code.
#include <common.h>
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shldisp.h>
#include <shdispid.h>
#include <propkey.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <oleacc.h>
#include <dwmapi.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
LARGE_INTEGER frequency;
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
double now(){LARGE_INTEGER value;QueryPerformanceCounter(&value);return 1000.0*static_cast<double>(value.QuadPart)/static_cast<double>(frequency.QuadPart);}
void pump(DWORD duration){const auto end=GetTickCount64()+duration;do{MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}if(GetTickCount64()>=end)return;MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);}while(true);}
template<class Predicate> void waitFor(Predicate ready,const char *message){const auto end=GetTickCount64()+45000;while(!ready()&&GetTickCount64()<end)pump(5);require(ready(),message);}

struct Enumeration : IDispatch {
    LONG references=1;unsigned done=0;
    STDMETHODIMP QueryInterface(REFIID iid,void **out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IDispatch&&iid!=DIID_DShellFolderViewEvents)return E_NOINTERFACE;*out=static_cast<IDispatch *>(this);AddRef();return S_OK;}
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP GetTypeInfoCount(UINT *count)override{if(!count)return E_POINTER;*count=0;return S_OK;}
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo **)override{return E_NOTIMPL;}
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR *,UINT,LCID,DISPID *)override{return E_NOTIMPL;}
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS *,VARIANT *out,EXCEPINFO *,UINT *)override{if(out)VariantInit(out);if(id==DISPID_FILELISTENUMDONE)++done;return S_OK;}
};
struct BrowserEvents : IExplorerBrowserEvents {
    LONG references=1;unsigned navigation=0,generation=0;HRESULT observation=E_PENDING;
    CComPtr<IShellView> shell;CComPtr<Enumeration> enumeration;CComPtr<IConnectionPoint> connection;DWORD cookie=0;
    void detach(){if(connection)connection->Unadvise(cookie);connection.Release();enumeration.Release();shell.Release();}
    STDMETHODIMP QueryInterface(REFIID iid,void **out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IExplorerBrowserEvents)return E_NOINTERFACE;*out=static_cast<IExplorerBrowserEvents *>(this);AddRef();return S_OK;}
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE)override{++navigation;return S_OK;}
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnViewCreated(IShellView *view)override{detach();++generation;shell=view;CComPtr<IDispatch> dispatch;observation=view->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&dispatch));if(FAILED(observation))return S_OK;CComQIPtr<IConnectionPointContainer> container(dispatch);observation=container?container->FindConnectionPoint(DIID_DShellFolderViewEvents,&connection):E_NOINTERFACE;if(SUCCEEDED(observation)){enumeration.Attach(new Enumeration);observation=connection->Advise(enumeration,&cookie);}return S_OK;}
};
struct Host {
    std::unique_ptr<std::remove_pointer<HWND>::type,decltype(&DestroyWindow)> frame{nullptr,&DestroyWindow};
    CComPtr<IExplorerBrowser> browser;CComPtr<IFolderViewOptions> options;CComPtr<BrowserEvents> events;DWORD cookie=0;bool advised=false;
    ~Host(){close();}
    void close(){if(events)events->detach();if(browser&&advised)browser->Unadvise(cookie);advised=false;if(browser)browser->Destroy();options.Release();browser.Release();events.Release();frame.reset();}
    CComPtr<IFolderView2> view(){CComPtr<IFolderView2> result;require(SUCCEEDED(browser->GetCurrentView(IID_PPV_ARGS(&result))),"No current view");return result;}
    void create(const wchar_t *path,FOLDERVIEWOPTIONS profile,bool persist){
        frame.reset(CreateWindowW(L"Order transfer prototype",L"Independent order transfer",WS_OVERLAPPEDWINDOW|WS_VISIBLE,40,40,850,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr));require(frame!=nullptr,"No host window");
        require(SUCCEEDED(browser.CoCreateInstance(CLSID_ExplorerBrowser)),"No ExplorerBrowser");require(SUCCEEDED(browser->QueryInterface(IID_PPV_ARGS(&options))),"No folder-view options");
        const auto mask=static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING);require(SUCCEEDED(options->SetFolderViewOptions(mask,profile)),"Cannot configure profile");
        auto flags=static_cast<EXPLORER_BROWSER_OPTIONS>(EBO_NOBORDER|EBO_NOTRAVELLOG);if(!persist)flags=static_cast<EXPLORER_BROWSER_OPTIONS>(flags|EBO_NOPERSISTVIEWSTATE);require(SUCCEEDED(browser->SetOptions(flags)),"Cannot configure browser");
        RECT bounds={0,0,800,400};FOLDERSETTINGS settings={FVM_ICON,FWF_AUTOARRANGE|FWF_NOWEBVIEW|FWF_NOHEADERINALLVIEWS};require(SUCCEEDED(browser->Initialize(frame.get(),&bounds,&settings)),"Cannot initialize browser");
        events.Attach(new BrowserEvents);require(SUCCEEDED(browser->Advise(events,&cookie)),"Cannot advise browser");advised=true;CComPtr<IShellItem> folder;require(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&folder)))&&SUCCEEDED(browser->BrowseToObject(folder,0)),"Cannot browse fixture");
        waitFor([&](){return events->navigation&&events->enumeration&&events->enumeration->done;},"Initial enumeration timed out");require(SUCCEEDED(events->observation),"Cannot observe enumeration");auto current=view();require(SUCCEEDED(current->SetViewModeAndIconSize(FVM_ICON,48)),"Cannot set medium icons");FOLDERVIEWMODE mode=FVM_AUTO;int size=0,count=0;require(SUCCEEDED(current->GetViewModeAndIconSize(&mode,&size))&&mode==FVM_ICON&&size==48&&SUCCEEDED(current->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Wrong initial representation");
    }
    void refresh(){CComPtr<Enumeration> previous=events->enumeration;const unsigned baseline=previous->done;require(SUCCEEDED(events->shell->Refresh()),"Cannot refresh");waitFor([&](){return events->enumeration&&(events->enumeration==previous?events->enumeration->done>baseline:events->enumeration->done>0);},"Refresh enumeration timed out");}
    void paint(){require(RedrawWindow(frame.get(),nullptr,nullptr,RDW_UPDATENOW|RDW_ALLCHILDREN)!=FALSE,"Cannot finish painting");const HRESULT result=DwmFlush();std::printf("COMPOSITION hr=0x%08lX\n",static_cast<unsigned long>(result));}
};
using Identity=std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>;
struct Snapshot {std::vector<Identity> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<std::wstring> names;};
Snapshot capture(IFolderView2 *view,UINT scope=SVGIO_ALLVIEW){
    Snapshot result;CComPtr<IShellFolder> folder;CComPtr<IEnumIDList> items;require(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder)))&&SUCCEEDED(view->Items(scope|static_cast<UINT>(SVGIO_FLAG_VIEWORDER),IID_PPV_ARGS(&items))),"Cannot enumerate view order");
    for(;;){CComHeapPtr<ITEMID_CHILD> id;const HRESULT next=items->Next(1,&id,nullptr);if(next==S_FALSE)break;require(next==S_OK,"Cannot read view order");STRRET text={};CComHeapPtr<wchar_t> name;require(SUCCEEDED(folder->GetDisplayNameOf(id,static_cast<SHGDNF>(SHGDN_FORPARSING|SHGDN_INFOLDER),&text))&&SUCCEEDED(StrRetToStrW(&text,id,&name))&&name,"Cannot resolve fixture identity");result.names.emplace_back(name);result.ids.push_back(id);result.owned.emplace_back(id.Detach(),&CoTaskMemFree);}
    return result;
}
bool checkOrder(Host &host,const std::vector<std::wstring> &expected,const char *phase){const double began=now();const auto actual=capture(host.view());auto a=actual.names,b=expected;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());require(a==b&&a.size()==5000,"Transfer changed fixture identities");const bool same=actual.names==expected;POINT point={};const HRESULT positioned=host.view()->GetItemPosition(actual.ids[0],&point);FOLDERVIEWOPTIONS flags=FVO_DEFAULT;require(SUCCEEDED(host.options->GetFolderViewOptions(&flags)),"Cannot read profile");std::printf("ORDER phase=%s preserved=%d identities=5000 check_ms=%.3f profile=%u position_hr=0x%08lX\n",phase,same?1:0,now()-began,static_cast<unsigned>(flags),static_cast<unsigned long>(positioned));std::fflush(stdout);return same;}
void timing(Host &host,const char *phase){
    auto view=host.view();auto all=capture(view);require(all.ids.size()==5000,"Wrong timing fixture");require(SUCCEEDED(host.events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"Cannot activate timing view");
    for(int repetition=0;repetition<3;++repetition){require(SUCCEEDED(host.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot prepare selection");pump(0);double began=now();require(SUCCEEDED(view->SelectAndPositionItems(5000,all.ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot select fixture");const double selected=now()-began;const auto selection=capture(view,SVGIO_SELECTION);require(selection.names==all.names,"Selection identities or order changed");CComPtr<IShellItemArray> array;DWORD count=0;require(SUCCEEDED(view->GetSelection(FALSE,&array))&&array&&SUCCEEDED(array->GetCount(&count))&&count==5000,"Selection array is stale");pump(0);began=now();require(SUCCEEDED(host.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot clear fixture");const double cleared=now()-began;int empty=-1;require(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&empty))&&empty==0,"Clear is incomplete");CComPtr<IShellItemArray> emptyArray;DWORD emptyCount=0;const HRESULT clearedArray=view->GetSelection(FALSE,&emptyArray);require((SUCCEEDED(clearedArray)&&(!emptyArray||(SUCCEEDED(emptyArray->GetCount(&emptyCount))&&emptyCount==0)))||(clearedArray==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)&&!emptyArray),"Cleared selection array is stale");std::printf("TIMING phase=%s repetition=%d select_ms=%.3f clear_ms=%.3f identities=PASS selection_array=PASS cleared_array=PASS\n",phase,repetition,selected,cleared);std::fflush(stdout);}
}
using Locations=std::map<std::wstring,RECT>;
void locations(IAccessible *object,LONG child,unsigned depth,unsigned &visited,Locations &result){
    if(depth>8||visited>=256||result.size()>=4)return;++visited;CComVariant target(child),role;if(SUCCEEDED(object->get_accRole(target,&role))&&role.vt==VT_I4&&role.lVal==ROLE_SYSTEM_LISTITEM){CComBSTR name;CComVariant state;LONG x=0,y=0,width=0,height=0;if(SUCCEEDED(object->get_accName(target,&name))&&name&&SUCCEEDED(object->get_accState(target,&state))&&state.vt==VT_I4&&!(state.lVal&STATE_SYSTEM_OFFSCREEN)&&SUCCEEDED(object->accLocation(&x,&y,&width,&height,target))&&width>0&&height>0)result.emplace(std::wstring(name,name.Length()),RECT{x,y,x+width,y+height});return;}
    if(child!=CHILDID_SELF)return;LONG count=0;if(FAILED(object->get_accChildCount(&count)))return;for(LONG start=0;start<count&&visited<256&&result.size()<4;){struct Children{VARIANT values[16]={};~Children(){for(auto &value:values)VariantClear(&value);}}children;LONG fetched=0;if(FAILED(AccessibleChildren(object,start,(std::min)(count-start,16L),children.values,&fetched))||fetched<=0)break;for(LONG i=0;i<fetched;++i){auto &value=children.values[i];if(value.vt==VT_I4)locations(object,value.lVal,depth+1,visited,result);else if(value.vt==VT_DISPATCH&&value.pdispVal){CComQIPtr<IAccessible> nested(value.pdispVal);if(nested)locations(nested,CHILDID_SELF,depth+1,visited,result);}}start+=fetched;}
}
void mouse(POINT point,DWORD buttons=0){INPUT input={};input.type=INPUT_MOUSE;const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);require(width>0&&height>0,"No display");input.mi.dx=static_cast<LONG>((static_cast<long long>(point.x-x)*65536+32768)/width);input.mi.dy=static_cast<LONG>((static_cast<long long>(point.y-y)*65536+32768)/height);input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|buttons;require(SendInput(1,&input,sizeof(input))==1,"Mouse input failed");}
void drag(Host &host){
    POINT original={};require(GetCursorPos(&original)!=FALSE,"Cannot save cursor");struct Cursor{POINT point;~Cursor(){INPUT release={};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&release,sizeof(INPUT));SetCursorPos(point.x,point.y);}}cursor{original};
    SetForegroundWindow(host.frame.get());pump(100);if(GetForegroundWindow()!=host.frame.get()){require(SetWindowPos(host.frame.get(),HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE,"Cannot expose fixture");TITLEBARINFO title={};title.cbSize=sizeof(title);require(GetTitleBarInfo(host.frame.get(),&title)!=FALSE,"No title bar");const POINT point={(title.rcTitleBar.left+title.rcTitleBar.right)/2,(title.rcTitleBar.top+title.rcTitleBar.bottom)/2};require(GetAncestor(WindowFromPoint(point),GA_ROOT)==host.frame.get(),"Caption is covered");mouse(point);mouse(point,MOUSEEVENTF_LEFTDOWN);mouse(point,MOUSEEVENTF_LEFTUP);pump(100);}require(GetForegroundWindow()==host.frame.get(),"No foreground");
    HWND body=nullptr;require(SUCCEEDED(host.events->shell->GetWindow(&body)),"No view window");CComPtr<IAccessible> root;require(SUCCEEDED(AccessibleObjectFromWindow(body,static_cast<DWORD>(OBJID_CLIENT),IID_PPV_ARGS(&root))),"No accessibility tree");Locations visible;unsigned visited=0;locations(root,CHILDID_SELF,0,visited,visible);require(visible.size()==4,"Four visible drag targets are required");auto source=visible.begin(),destination=visible.begin();std::advance(destination,3);const RECT &a=source->second,&b=destination->second;const POINT start={(a.left+a.right)/2,(a.top+a.bottom)/2},end={b.left+2,(b.top+b.bottom)/2};require(GetAncestor(WindowFromPoint(start),GA_ROOT)==host.frame.get()&&GetAncestor(WindowFromPoint(end),GA_ROOT)==host.frame.get(),"Drag is covered");
    for(const int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU})require(!(GetAsyncKeyState(key)&0x8000),"Input is already held");require(SUCCEEDED(host.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot prepare drag");const DWORD gui=GetCurrentThreadId();std::atomic<bool> sent{false},captured{false};std::exception_ptr error;
    std::thread worker([&](){try{mouse(start);Sleep(30);mouse(start,MOUSEEVENTF_LEFTDOWN);Sleep(50);for(int step=1;step<=8;++step){mouse(POINT{start.x+(end.x-start.x)*step/8,start.y+(end.y-start.y)*step/8});Sleep(35);GUITHREADINFO info={};info.cbSize=sizeof(info);if(step>=2&&GetGUIThreadInfo(gui,&info)&&info.hwndCapture)captured.store(true);}Sleep(80);mouse(end,MOUSEEVENTF_LEFTUP);}catch(...){error=std::current_exception();INPUT release={};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&release,sizeof(INPUT));}sent.store(true);});while(!sent.load())pump(5);worker.join();if(error)std::rethrow_exception(error);GUITHREADINFO info={};info.cbSize=sizeof(info);waitFor([&](){return GetGUIThreadInfo(gui,&info)&&!info.hwndCapture&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000);},"Drag capture did not finish");require(captured.load(),"Drag did not capture mouse");pump(0);
    CComPtr<IShellItemArray> selection;CComPtr<IShellItem> item;CComHeapPtr<wchar_t> name;DWORD count=0;require(SUCCEEDED(host.view()->GetSelection(FALSE,&selection))&&selection&&SUCCEEDED(selection->GetCount(&count))&&count==1&&SUCCEEDED(selection->GetItemAt(0,&item))&&SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&name&&std::wstring(name)==source->first,"Drag selected the wrong identity");std::puts("DRAG capture=PASS release=PASS source_identity=PASS");
}
void run(const wchar_t *fixture,bool toggle,bool persist,bool handoff){
    const char *strategy=toggle?"toggle":handoff?"handoff":"fresh";
    std::printf("CASE strategy=%s persistence=%d items=5000\n",strategy,persist?1:0);std::fflush(stdout);
    std::vector<std::wstring> natural;{Host control;control.create(fixture,FVO_DEFAULT,persist);control.refresh();natural=capture(control.view()).names;checkOrder(control,natural,"fresh-modern-control");timing(control,"fresh-modern-control");}
    Host classic;classic.create(fixture,static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING),persist);SORTCOLUMN sort={PKEY_ItemNameDisplay,SORT_ASCENDING};require(SUCCEEDED(classic.view()->SetSortColumns(&sort,1)),"Cannot establish initial sort");classic.refresh();require(capture(classic.view()).names==natural,"Reference initial order differs");drag(classic);const double captureBegan=now();auto ordered=capture(classic.view());const double captured=now()-captureBegan;require(ordered.names!=natural,"Reference drag did not change enumerated order");checkOrder(classic,ordered.names,"classic-after-drag");timing(classic,"classic-after-drag");std::printf("CAPTURE_ORDER ms=%.3f\n",captured);
    bool preserved=false,afterRefresh=false;int afterReopen=-1,fallback=-1;
    if(toggle){CComPtr<IUnknown> before;require(SUCCEEDED(classic.view()->QueryInterface(IID_PPV_ARGS(&before))),"No initial view identity");const double began=now();const auto mask=static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING);require(SUCCEEDED(classic.options->SetFolderViewOptions(mask,FVO_DEFAULT)),"Cannot switch profile");CComPtr<IUnknown> after;require(SUCCEEDED(classic.view()->QueryInterface(IID_PPV_ARGS(&after))),"No switched view identity");preserved=checkOrder(classic,ordered.names,"toggle-before-refresh");classic.paint();std::printf("SWITCH strategy=toggle ms=%.3f same_view=%d\n",now()-began,before==after?1:0);timing(classic,"toggle-before-refresh");const double refreshed=now();classic.refresh();afterRefresh=checkOrder(classic,ordered.names,"toggle-after-refresh");classic.paint();std::printf("REFRESH strategy=toggle ms=%.3f\n",now()-refreshed);timing(classic,"toggle-after-refresh");}
    else{const double began=now();if(handoff){const HRESULT saved=classic.events->shell->SaveViewState();std::printf("SAVE_CLASSIC hr=0x%08lX\n",static_cast<unsigned long>(saved));classic.close();}{Host modern;modern.create(fixture,FVO_DEFAULT,persist);preserved=checkOrder(modern,ordered.names,"fresh-before-refresh");modern.paint();std::printf("SWITCH strategy=%s ms=%.3f\n",strategy,now()-began);timing(modern,"fresh-before-refresh");const double refreshed=now();modern.refresh();afterRefresh=checkOrder(modern,ordered.names,"fresh-after-refresh");modern.paint();std::printf("REFRESH strategy=%s ms=%.3f\n",strategy,now()-refreshed);timing(modern,"fresh-after-refresh");}{Host reopened;reopened.create(fixture,FVO_DEFAULT,persist);afterReopen=checkOrder(reopened,ordered.names,"fresh-reopened");}if(!handoff){fallback=checkOrder(classic,ordered.names,"retained-classic-fallback");require(fallback,"Preparing the modern view damaged the retained classic order");}}
    std::printf("TRANSFER_RESULT strategy=%s persistence=%d before_refresh=%d after_refresh=%d after_reopen=%d retained_classic_fallback=%d\n",strategy,persist?1:0,preserved?1:0,afterRefresh?1:0,afterReopen,fallback);std::fflush(stdout);
}
}
int wmain(int argc,wchar_t **argv){
    if(argc!=4||(std::wcscmp(argv[2],L"fresh")!=0&&std::wcscmp(argv[2],L"toggle")!=0&&std::wcscmp(argv[2],L"handoff")!=0)||(std::wcscmp(argv[3],L"persist")!=0&&std::wcscmp(argv[3],L"no-persist")!=0))return 2;
    if(FAILED(OleInitialize(nullptr)))return 2;QueryPerformanceFrequency(&frequency);INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&controls);WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"Order transfer prototype";if(!RegisterClassW(&type)){OleUninitialize();return 2;}int result=0;try{run(argv[1],std::wcscmp(argv[2],L"toggle")==0,std::wcscmp(argv[3],L"persist")==0,std::wcscmp(argv[2],L"handoff")==0);}catch(const std::exception &error){std::fprintf(stderr,"FAIL independent transfer protocol: %s\n",error.what());result=1;}UnregisterClassW(type.lpszClassName,type.hInstance);OleUninitialize();return result;
}
