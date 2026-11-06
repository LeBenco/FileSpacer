// Qualification of the arranged-view candidate, not a product policy change.
#define wmain placementProbeMain
#include "PlacementTests.cpp"
#undef wmain
#include <algorithm>
#include <atomic>
#include <oleacc.h>
#include <map>
#include <thread>
#include <type_traits>

namespace {
struct TransitionHost {
    HWND frame=nullptr,sentinel=nullptr;std::unique_ptr<std::remove_pointer<HWND>::type,decltype(&DestroyWindow)> frameOwner{nullptr,&DestroyWindow};filespacer::ShellViewHost host;CComPtr<Events> events;unsigned generation=0;
    TransitionHost(){frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Arrangement transitions",WS_OVERLAPPEDWINDOW|WS_VISIBLE,40,40,850,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(frame!=nullptr,"No transition frame");frameOwner.reset(frame);sentinel=CreateWindowW(L"EDIT",L"focus sentinel",WS_CHILD|WS_VISIBLE|WS_BORDER,10,410,180,24,frame,nullptr,GetModuleHandleW(nullptr),nullptr);need(sentinel!=nullptr,"No focus sentinel");}
    ~TransitionHost(){destroyView();}
    void destroyView(){++generation;if(events)events->detach();host.destroy();events.Release();}
    CComPtr<IFolderView2> view(){CComPtr<IFolderView2> result;need(SUCCEEDED(host.currentView(IID_PPV_ARGS(&result))),"No transition view");return result;}
    void create(const wchar_t *path,FOLDERVIEWOPTIONS profile,FOLDERVIEWMODE mode,int size,bool arranged,int expected=5000){
        destroyView();events.Attach(new Events);RECT bounds={0,0,800,400};FOLDERSETTINGS settings={static_cast<UINT>(mode),FWF_NOWEBVIEW|FWF_NOHEADERINALLVIEWS};if(arranged)settings.fFlags|=FWF_AUTOARRANGE;
        need(SUCCEEDED(host.initialize(frame,bounds,settings,nullptr,events,profile)),"Cannot initialize transition host");
        CComPtr<IShellItem> folder;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&folder)))&&SUCCEEDED(host.navigate(folder,false,true)),"Cannot navigate transition fixture");
        ULONGLONG deadline=GetTickCount64()+45000;while((!events->navigation||!events->sink||!events->sink->done)&&GetTickCount64()<deadline)pump(5);
        need(events->navigation&&events->sink&&events->sink->done,"Initial transition enumeration did not finish");
        auto current=view();need(SUCCEEDED(current->SetViewModeAndIconSize(mode,size)),"Cannot set transition representation");
        current=view();need(SUCCEEDED(current->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,arranged?FWF_AUTOARRANGE:0)),"Cannot set transition arrangement");
        // A representation change need not enumerate. Explicitly refresh this
        // functional probe and wait for its public event, rather than inferring
        // readiness from a delay or from the number of loaded items.
        const unsigned baseline=events->sink->done;CComPtr<Sink> initialSink=events->sink;
        need(SUCCEEDED(events->shell->Refresh()),"Cannot refresh transition representation");deadline=GetTickCount64()+45000;const auto ready=[&](){return events->sink&&(events->sink==initialSink?events->sink->done>baseline:events->sink->done>0);};while(!ready()&&GetTickCount64()<deadline)pump(5);need(ready(),"Refreshed transition representation did not become ready");current=view();
        FOLDERVIEWMODE actual=FVM_AUTO;int actualSize=0,count=0;need(SUCCEEDED(current->GetViewModeAndIconSize(&actual,&actualSize))&&actual==mode&&actualSize==size,"Wrong transition representation");need(SUCCEEDED(current->ItemCount(SVGIO_ALLVIEW,&count))&&count==expected,"Wrong transition fixture count");
        need(current->GetAutoArrange()==(arranged?S_OK:S_FALSE),"Wrong transition arrangement");
    }
    void focusView(){keyboardView=events->shell;need(SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"Cannot activate transition view");}
    void verifyKeyboard(bool outside){HWND body=nullptr;need(SUCCEEDED(events->shell->GetWindow(&body)),"No transition body");const HWND focused=GetFocus();need(outside?focused==sentinel:focused==body||IsChild(body,focused),"Transition changed keyboard focus policy");}
};
using Identity=std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>;
std::string key(PCUIDLIST_RELATIVE id){return std::string(reinterpret_cast<const char *>(id),ILGetSize(id));}
void verifyState(IFolderView2 *view,const std::vector<PCUITEMID_CHILD> &wanted,PCUITEMID_CHILD focused){
    int count=-1;need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count))&&count==static_cast<int>(wanted.size()),"Wrong transition selection count");
    CComPtr<IEnumIDList> selected;need(SUCCEEDED(view->Items(SVGIO_SELECTION,IID_PPV_ARGS(&selected))),"No transition selected identities");std::vector<std::string> actual,expected;
    for(size_t i=0;i<wanted.size();++i){CComHeapPtr<ITEMID_CHILD> id;need(selected->Next(1,&id,nullptr)==S_OK,"Short transition selection");actual.push_back(key(id));expected.push_back(key(wanted[i]));}std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());need(actual==expected,"Transition changed selected identities");
    CComPtr<IShellItemArray> array;DWORD arrayCount=0;need(SUCCEEDED(view->GetSelection(FALSE,&array))&&array&&SUCCEEDED(array->GetCount(&arrayCount))&&arrayCount==wanted.size(),"Transition selection array is stale");
    int index=-1;CComHeapPtr<ITEMID_CHILD> id;need(SUCCEEDED(view->GetFocusedItem(&index))&&index>=0&&SUCCEEDED(view->Item(index,&id))&&ILIsEqual(id,focused),"Transition changed logical focus");
}
void verifyPositions(IFolderView2 *view,const std::vector<PCUITEMID_CHILD> &ids,const std::vector<POINT> &positions){for(size_t i=0;i<ids.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==positions[i].x&&actual.y==positions[i].y,"Transition changed a saved free coordinate");}}
void transitions(const wchar_t *fixture){
    TransitionHost owner;const auto spatial=static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING);owner.create(fixture,spatial,FVM_ICON,256,false);owner.focusView();
    std::vector<Identity> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<POINT> positions;CComPtr<IEnumIDList> items;need(SUCCEEDED(owner.view()->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"No transition fixture identities");
    for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(items->Next(1,&id,nullptr)==S_OK,"Short transition fixture");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);positions.push_back(POINT{30+(i%50)*300,30+(i/50)*300});}items.Release();
    const auto saved=positionBlob(ids,positions);auto valid=[](){return true;};need(SUCCEEDED(filespacer::restoreIconPositions(owner.view(),saved,valid)),"Cannot seed transition coordinates");
    struct Mode{const char *name;FOLDERVIEWMODE mode;int size;};const Mode modes[]={{"extra-large",FVM_ICON,256},{"large",FVM_ICON,96},{"medium",FVM_ICON,48},{"small",FVM_SMALLICON,16},{"tiles",FVM_TILE,48},{"content",FVM_CONTENT,32}};
    std::vector<PCUITEMID_CHILD> wanted={ids[7],ids[19]};
    for(const auto &mode:modes)for(int outside=0;outside<2;++outside){
        auto freeView=owner.view();need(SUCCEEDED(freeView->SelectAndPositionItems(2,wanted.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS))&&SUCCEEDED(owner.events->shell->SelectItem(wanted[1],SVSI_SELECT|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed transition selection");
        std::vector<uint8_t> selection;need(SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,valid,selection)),"Cannot capture transition selection");
        const unsigned oldGeneration=owner.generation;const auto obsolete=[&owner,oldGeneration](){return owner.generation==oldGeneration;};
        if(outside)SetFocus(owner.sentinel);owner.create(fixture,FVO_DEFAULT,mode.mode,mode.size,true);if(!outside)owner.focusView();else owner.verifyKeyboard(true);
        auto modern=owner.view();auto pending=selection;need(SUCCEEDED(filespacer::restoreReopenSelection(owner.events->shell,pending,valid))&&pending.empty(),"Cannot restore arranged selection");verifyState(modern,wanted,wanted[1]);owner.verifyKeyboard(outside!=0);
        auto stale=selection;need(filespacer::restoreReopenSelection(owner.events->shell,stale,obsolete)==E_ABORT&&stale.empty(),"Old generation was not rejected");verifyState(modern,wanted,wanted[1]);owner.verifyKeyboard(outside!=0);
        std::vector<uint8_t> modernSelection;need(SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,valid,modernSelection)),"Cannot capture arranged selection");
        owner.create(fixture,spatial,FVM_ICON,256,false);if(!outside)owner.focusView();else owner.verifyKeyboard(true);auto restored=owner.view();need(SUCCEEDED(filespacer::restoreIconPositions(restored,saved,valid)),"Cannot restore free coordinates after arrangement");need(SUCCEEDED(filespacer::restoreReopenSelection(owner.events->shell,modernSelection,valid)),"Cannot return arranged selection to free view");
        verifyPositions(restored,ids,positions);verifyState(restored,wanted,wanted[1]);owner.verifyKeyboard(outside!=0);
        std::printf("PASS transition %s outside_focus=%d: 5000 coordinates, two identities, selection array, logical/keyboard focus, obsolete generation\n",mode.name,outside);std::fflush(stdout);
    }
    // A candidate must also survive a full selection across teardown/recreation.
    auto view=owner.view();need(SUCCEEDED(view->SelectAndPositionItems(5000,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot seed full transition selection");std::vector<uint8_t> all;need(SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,valid,all)),"Cannot capture full transition selection");
    owner.create(fixture,FVO_DEFAULT,FVM_ICON,256,true);owner.focusView();need(SUCCEEDED(filespacer::restoreReopenSelection(owner.events->shell,all,valid)),"Cannot restore full selection in arranged view");verifyState(owner.view(),ids,wanted[1]);
    need(SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,valid,all)),"Cannot capture full arranged selection");owner.create(fixture,spatial,FVM_ICON,256,false);owner.focusView();need(SUCCEEDED(filespacer::restoreIconPositions(owner.view(),saved,valid))&&SUCCEEDED(filespacer::restoreReopenSelection(owner.events->shell,all,valid)),"Cannot restore full free selection");verifyPositions(owner.view(),ids,positions);verifyState(owner.view(),ids,wanted[1]);owner.verifyKeyboard(false);
    std::puts("PASS full-selection roundtrip: 5000 identities and coordinates; both focus states");
}
using Locations=std::map<std::wstring,RECT>;
void locations(IAccessible *accessible,LONG child,unsigned depth,unsigned &visited,Locations &result){
    if(depth>8||visited>=256)return;++visited;CComVariant target(child),role;
    if(SUCCEEDED(accessible->get_accRole(target,&role))&&role.vt==VT_I4&&role.lVal==ROLE_SYSTEM_LISTITEM){
        CComBSTR name;LONG x=0,y=0,width=0,height=0;CComVariant state;
        if(SUCCEEDED(accessible->get_accName(target,&name))&&name&&SUCCEEDED(accessible->get_accState(target,&state))&&state.vt==VT_I4&&!(state.lVal&STATE_SYSTEM_OFFSCREEN)
                &&SUCCEEDED(accessible->accLocation(&x,&y,&width,&height,target))&&width>0&&height>0)
            need(result.emplace(std::wstring(name,name.Length()),RECT{x,y,x+width,y+height}).second,"Duplicate accessible reorder identity");
        return;
    }
    if(child!=CHILDID_SELF)return;LONG count=0;if(FAILED(accessible->get_accChildCount(&count)))return;
    for(LONG start=0;start<count&&visited<256;){
        struct Children{VARIANT values[16]={};~Children(){for(auto &value:values)VariantClear(&value);}}children;
        LONG fetched=0;if(FAILED(AccessibleChildren(accessible,start,(std::min)(count-start,16L),children.values,&fetched))||fetched<=0)break;
        for(LONG i=0;i<fetched;++i){auto &value=children.values[i];if(value.vt==VT_I4)locations(accessible,value.lVal,depth+1,visited,result);else if(value.vt==VT_DISPATCH&&value.pdispVal){CComQIPtr<IAccessible> nested(value.pdispVal);if(nested)locations(nested,CHILDID_SELF,depth+1,visited,result);}}start+=fetched;
    }
}
Locations readLocations(IShellView *view){HWND body=nullptr;need(SUCCEEDED(view->GetWindow(&body)),"No reorder accessibility window");CComPtr<IAccessible> root;need(SUCCEEDED(AccessibleObjectFromWindow(body,static_cast<DWORD>(OBJID_CLIENT),IID_PPV_ARGS(&root)))&&root,"No reorder accessibility tree");Locations result;unsigned visited=0;locations(root,CHILDID_SELF,0,visited,result);need(result.size()==20,"Expected twenty visible reorder identities");return result;}
std::atomic<bool> dragStarted{false},dragFinished{false};
void CALLBACK dragEvent(HWINEVENTHOOK,DWORD event,HWND,LONG,LONG,DWORD,DWORD){if(event==EVENT_SYSTEM_DRAGDROPSTART)dragStarted.store(true);if(event==EVENT_SYSTEM_DRAGDROPEND)dragFinished.store(true);}
void sendMouse(POINT point,DWORD buttons=0){INPUT input={};input.type=INPUT_MOUSE;const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);need(width>0&&height>0,"No reorder virtual display");input.mi.dx=static_cast<LONG>((static_cast<long long>(point.x-x)*65536+32768)/width);input.mi.dy=static_cast<LONG>((static_cast<long long>(point.y-y)*65536+32768)/height);input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|buttons;need(SendInput(1,&input,sizeof(input))==1,"Reorder mouse input failed");}
bool reorder(const wchar_t *fixture,FOLDERVIEWOPTIONS profile){
    TransitionHost owner;owner.create(fixture,profile,FVM_ICON,48,true,20);
    POINT original={};need(GetCursorPos(&original)!=FALSE,"Cannot save reorder cursor");struct Cursor{POINT original;~Cursor(){INPUT release={};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&release,sizeof(INPUT));SetCursorPos(original.x,original.y);}}cursor{original};
    SetForegroundWindow(owner.frame);pump(100);
    if(GetForegroundWindow()!=owner.frame){need(SetWindowPos(owner.frame,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE,"Cannot expose reorder fixture");TITLEBARINFO title={};title.cbSize=sizeof(title);need(GetTitleBarInfo(owner.frame,&title)!=FALSE,"No reorder activation caption");const POINT caption={(title.rcTitleBar.left+title.rcTitleBar.right)/2,(title.rcTitleBar.top+title.rcTitleBar.bottom)/2};need(GetAncestor(WindowFromPoint(caption),GA_ROOT)==owner.frame,"Reorder caption is covered");sendMouse(caption);sendMouse(caption,MOUSEEVENTF_LEFTDOWN);sendMouse(caption,MOUSEEVENTF_LEFTUP);pump(100);}
    need(GetForegroundWindow()==owner.frame,"Reorder fixture did not obtain foreground");owner.focusView();
    const auto before=readLocations(owner.events->shell);auto source=before.begin(),destination=before.begin();std::advance(destination,3);const RECT &a=source->second,&b=destination->second;const POINT start={(a.left+a.right)/2,(a.top+a.bottom)/2},end={b.left+2,(b.top+b.bottom)/2};
    need(GetAncestor(WindowFromPoint(start),GA_ROOT)==owner.frame&&GetAncestor(WindowFromPoint(end),GA_ROOT)==owner.frame,"Reorder fixture is covered");
    dragStarted.store(false);dragFinished.store(false);HWINEVENTHOOK hook=SetWinEventHook(EVENT_SYSTEM_DRAGDROPSTART,EVENT_SYSTEM_DRAGDROPEND,nullptr,dragEvent,GetCurrentProcessId(),GetCurrentThreadId(),WINEVENT_OUTOFCONTEXT);need(hook!=nullptr,"Cannot observe reorder gesture");std::unique_ptr<std::remove_pointer<HWINEVENTHOOK>::type,decltype(&UnhookWinEvent)> hookScope(hook,&UnhookWinEvent);
    need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot clear reorder fixture selection");
    for(const int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU})need(!(GetAsyncKeyState(key)&0x8000),"Reorder input is already held");
    const DWORD guiThread=GetCurrentThreadId();std::atomic<bool> sent{false},captured{false};std::exception_ptr inputError;
    std::thread input([&](){try{sendMouse(start);Sleep(30);sendMouse(start,MOUSEEVENTF_LEFTDOWN);Sleep(50);for(int step=1;step<=8;++step){sendMouse(POINT{start.x+(end.x-start.x)*step/8,start.y+(end.y-start.y)*step/8});Sleep(35);GUITHREADINFO info={};info.cbSize=sizeof(info);if(step>=2&&GetGUIThreadInfo(guiThread,&info)&&info.hwndCapture)captured.store(true);}Sleep(80);sendMouse(end,MOUSEEVENTF_LEFTUP);}catch(...){inputError=std::current_exception();INPUT release={};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&release,sizeof(INPUT));}sent.store(true);});
    while(!sent.load())pump(5);input.join();if(inputError)std::rethrow_exception(inputError);
    // Drag/drop WinEvents depend on the application emitting them; their absence
    // is diagnostic, not proof that Windows did not process the physical input.
    const ULONGLONG deadline=GetTickCount64()+5000;GUITHREADINFO info={};info.cbSize=sizeof(info);const auto idle=[&](){return GetGUIThreadInfo(guiThread,&info)&&!info.hwndCapture&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000);};while(!idle()&&GetTickCount64()<deadline)pump(5);need(idle()&&captured.load(),"Reorder did not complete its mouse capture");pump(100);
    CComPtr<IShellItemArray> selection;CComPtr<IShellItem> selected;CComHeapPtr<wchar_t> name;DWORD selectedCount=0;need(SUCCEEDED(owner.view()->GetSelection(FALSE,&selection))&&selection&&SUCCEEDED(selection->GetCount(&selectedCount))&&selectedCount==1&&SUCCEEDED(selection->GetItemAt(0,&selected))&&SUCCEEDED(selected->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&name&&std::wstring(name)==source->first,"Reorder gesture did not select the intended source identity");
    const auto after=readLocations(owner.events->shell);bool moved=false;for(const auto &item:before){const auto found=after.find(item.first);need(found!=after.end(),"Reorder changed an item identity");if(found->second.left!=item.second.left||found->second.top!=item.second.top)moved=true;}
    int count=0;need(SUCCEEDED(owner.view()->ItemCount(SVGIO_ALLVIEW,&count))&&count==20,"Reorder changed fixture contents");
    std::printf("REORDER profile=%u input_idle=1 capture_observed=1 source_identity=1 identities=20 auto_arranged=1 visual_order_changed=%d drag_start_event=%d drag_end_event=%d\n",static_cast<unsigned>(profile),moved?1:0,dragStarted.load()?1:0,dragFinished.load()?1:0);std::fflush(stdout);return moved;
}
}
int wmain(int argc,wchar_t **argv){
    if((argc!=4&&argc!=5)||(argc==5&&std::wcscmp(argv[4],L"reorder-only")!=0)||FAILED(OleInitialize(nullptr)))return 2;QueryPerformanceFrequency(&frequency);filespacer::initDPI();WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Placement Prototype";if(!RegisterClassW(&type)){OleUninitialize();return 2;}int result=0;
    try{if(argc==4)transitions(argv[2]);const bool legacy=reorder(argv[3],static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING));const bool modern=reorder(argv[3],FVO_DEFAULT);std::printf("QUALIFICATION manual_reordering_preserved=%d legacy_reorders=%d modern_reorders=%d\n",!legacy||modern?1:0,legacy?1:0,modern?1:0);std::fflush(stdout);need(legacy,"Reference gesture did not reorder; comparison is inconclusive");need(modern,"Modern arranged profile does not preserve manual reordering");}catch(const std::exception &error){std::fprintf(stderr,"FAIL arrangement transitions: %s\n",error.what());result=1;}UnregisterClassW(type.lpszClassName,type.hInstance);OleUninitialize();return result;
}
