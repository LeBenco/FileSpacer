// Reuse the real Shell host, readiness events and coordinate helpers.
#define wmain positioningProbeMain
#include "PlacementTests.cpp"
#undef wmain
#include <algorithm>
#include <map>
#include <atomic>
#include <shlguid.h>
#define captureReopenSelection captureReopenSelectionReference
#define restoreReopenSelection restoreReopenSelectionReference
#include "SelectionReference.inc"
#undef captureReopenSelection
#undef restoreReopenSelection

namespace {
struct BareEvents : IExplorerBrowserEvents,IEnumReadyCallback {
    LONG refs=1;unsigned navigation=0;std::atomic<bool> ready{false};HRESULT observed=E_PENDING;
    STDMETHODIMP QueryInterface(REFIID iid,void **out)override{if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_IExplorerBrowserEvents)*out=static_cast<IExplorerBrowserEvents *>(this);else if(iid==__uuidof(IEnumReadyCallback))*out=static_cast<IEnumReadyCallback *>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&refs);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&refs);if(!left)delete this;return left;}
    STDMETHODIMP EnumReady()override{ready.store(true);return S_OK;}
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE)override{++navigation;return S_OK;}
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnViewCreated(IShellView *view)override{ready.store(false);CComPtr<IEnumerableView> contents;observed=IUnknown_QueryService(view,SID_EnumerableView,IID_PPV_ARGS(&contents));if(SUCCEEDED(observed))observed=contents->SetEnumReadyCallback(this);return S_OK;}
};
struct SelectionHost {
    HWND frame=nullptr;filespacer::ShellViewHost host;CComPtr<Events> events;CComPtr<BareEvents> bareEvents;
    ~SelectionHost(){if(events)events->detach();host.destroy();if(frame)DestroyWindow(frame);}
    CComPtr<IFolderView2> view(){CComPtr<IFolderView2> value;need(SUCCEEDED(host.currentView(IID_PPV_ARGS(&value))),"No selection view");return value;}
    void observe(bool enabled){
        if(events->connection)need(SUCCEEDED(events->connection->Unadvise(events->cookie)),"Cannot detach selection observer");
        events->connection.Release();events->sink.Release();events->cookie=0;
        if(!enabled)return;
        CComPtr<IDispatch> automation;need(SUCCEEDED(events->shell->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&automation))),"No view automation");
        CComQIPtr<IConnectionPointContainer> container(automation);need(container&&SUCCEEDED(container->FindConnectionPoint(DIID_DShellFolderViewEvents,&events->connection)),"No view event connection");
        events->sink.Attach(new Sink);need(SUCCEEDED(events->connection->Advise(events->sink,&events->cookie)),"Cannot reattach selection observer");
    }
    void create(const wchar_t *path,FOLDERVIEWOPTIONS profile,bool bare){
        frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Selection strategies",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,850,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(frame!=nullptr,"No selection frame");
        RECT bounds={};GetClientRect(frame,&bounds);FOLDERSETTINGS settings={FVM_ICON,FWF_AUTOARRANGE|FWF_NOWEBVIEW|FWF_NOHEADERINALLVIEWS};events.Attach(new Events);
        if(bare)bareEvents.Attach(new BareEvents);
        IExplorerBrowserEvents *observer=bare?static_cast<IExplorerBrowserEvents *>(bareEvents):static_cast<IExplorerBrowserEvents *>(events);
        need(SUCCEEDED(host.initialize(frame,bounds,settings,nullptr,observer,profile)),"Selection host initialization failed");
        CComPtr<IShellItem> folder;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&folder)))&&SUCCEEDED(host.navigate(folder,false,true)),"Selection fixture navigation failed");
        const ULONGLONG end=GetTickCount64()+45000;
        const auto completed=[&](){return bare?bareEvents->navigation&&bareEvents->ready.load():events->navigation&&events->sink&&events->sink->done;};
        while(!completed()&&GetTickCount64()<end)pump(10);
        need(completed(),"Selection enumeration did not finish");
        if(bare)need(SUCCEEDED(host.currentView(IID_PPV_ARGS(&events->shell))),"No unobserved Shell view");
        auto current=view();int count=0;need(SUCCEEDED(current->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Incomplete selection fixture");
        need(SUCCEEDED(current->SetViewModeAndIconSize(FVM_ICON,256)),"Extra-large mode unavailable");pump(200);
        for(int attempt=0;attempt<8;++attempt){
            HWND body=nullptr;RECT region={},outer={};need(SUCCEEDED(events->shell->GetWindow(&body))&&GetWindowRect(body,&region)&&GetWindowRect(frame,&outer),"No selection geometry");
            if(region.right-region.left==800&&region.bottom-region.top==400)break;
            need(SetWindowPos(frame,nullptr,0,0,outer.right-outer.left+800-(region.right-region.left),outer.bottom-outer.top+400-(region.bottom-region.top),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"Cannot resize selection frame");
            GetClientRect(frame,&bounds);need(SUCCEEDED(host.setBounds(bounds)),"Cannot resize selection host");pump(20);
        }
        HWND body=nullptr;RECT region={};need(SUCCEEDED(events->shell->GetWindow(&body))&&GetWindowRect(body,&region)&&region.right-region.left==800&&region.bottom-region.top==400,"Wrong selection viewport");
        FOLDERVIEWMODE mode=FVM_AUTO;int size=0;need(SUCCEEDED(current->GetViewModeAndIconSize(&mode,&size))&&mode==FVM_ICON&&size==256,"Wrong selection representation");
        std::printf("VIEW profile=%u mode=%d icon_size=%d dpi=%u rectangle=800x400\n",static_cast<unsigned>(profile),static_cast<int>(mode),size,GetDpiForWindow(body));std::fflush(stdout);
        keyboardView=events->shell;need(SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"Cannot activate selection view");pump(100);
    }
};
std::string pidlKey(PCUIDLIST_RELATIVE id){return std::string(reinterpret_cast<const char *>(id),ILGetSize(id));}
void verifySelection(IFolderView2 *view,const std::vector<PCUITEMID_CHILD> &ids,int wanted){
    int count=-1;need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count))&&count==wanted,"Wrong selection count");
    if(!wanted)return;
    CComPtr<IEnumIDList> selected;const HRESULT enumerated=view->Items(SVGIO_SELECTION,IID_PPV_ARGS(&selected));
    if(FAILED(enumerated))std::fprintf(stderr,"Selected identities count=%d HRESULT=0x%08lX\n",wanted,static_cast<unsigned long>(enumerated));
    need(SUCCEEDED(enumerated)&&selected,"No selected identities");
    std::vector<std::string> actual,expected;
    for(int i=0;i<wanted;++i){CComHeapPtr<ITEMID_CHILD> id;need(selected->Next(1,&id,nullptr)==S_OK,"Short selected enumeration");actual.push_back(pidlKey(id));expected.push_back(pidlKey(ids[static_cast<size_t>(i)]));}
    std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());need(actual==expected,"Wrong selected identities");
}
HRESULT withPublicRedraw(IFolderView2 *view,const std::function<HRESULT()> &operation){
    HRESULT result=view->SetRedraw(FALSE);if(FAILED(result))return result;
    struct Resume{IFolderView2 *view;bool active=true;explicit Resume(IFolderView2 *value):view(value){}~Resume(){if(active)view->SetRedraw(TRUE);}}resume(view);
    result=operation();const HRESULT resumed=view->SetRedraw(TRUE);resume.active=false;return FAILED(result)?result:resumed;
}
void selectionStrategies(const wchar_t *root,const wchar_t *path,bool indexes,bool callbacks,const wchar_t *fresh){
    const bool isolated=fresh!=nullptr,bare=isolated&&std::wcscmp(fresh,L"bare-batch")==0;
    const int isolatedStrategy=!isolated||std::wcscmp(fresh,L"fresh-batch")==0||bare?0:std::wcscmp(fresh,L"fresh-keyboard")==0?12:std::wcscmp(fresh,L"fresh-details")==0?11:std::wcscmp(fresh,L"fresh-index")==0?5:10;
    FILE *file=nullptr;need(_wfopen_s(&file,(std::wstring(root)+L"\\selection-strategies.csv").c_str(),L"wb")==0,"No strategies journal");
    std::unique_ptr<FILE,decltype(&std::fclose)> output(file,&std::fclose);
    std::fputs("profile,repetition,strategy,selected,select_ms,clear_ms,free_positions,focus_preserved,keyboard_preserved\n",file);
    const FOLDERVIEWOPTIONS profiles[]={static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING),FVO_CUSTOMPOSITION,FVO_CUSTOMORDERING,FVO_DEFAULT};
    for(const auto profile:profiles){
        if((indexes||callbacks||isolated)&&profile!=profiles[0])continue;
        SelectionHost owner;owner.create(path,profile,bare);auto view=owner.view();
        if(isolated&&isolatedStrategy==12){
            struct Cursor{POINT original={};~Cursor(){SetCursorPos(original.x,original.y);}}cursor;
            need(GetCursorPos(&cursor.original)!=FALSE,"No guest cursor position");
            for(int attempt=0;attempt<3&&GetForegroundWindow()!=owner.frame;++attempt){
                INPUT escape[3]={};for(auto &event:escape)event.type=INPUT_KEYBOARD;
                escape[0].ki.wVk=VK_CONTROL;escape[0].ki.dwFlags=KEYEVENTF_KEYUP;
                escape[1].ki.wVk=VK_ESCAPE;escape[2].ki.wVk=VK_ESCAPE;escape[2].ki.dwFlags=KEYEVENTF_KEYUP;
                need(SendInput(3,escape,sizeof(INPUT))==3,"Guest menu dismissal failed");pump(100);
                need(SetWindowPos(owner.frame,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE,"Cannot expose owned caption");
                RECT bounds={};need(GetWindowRect(owner.frame,&bounds)!=FALSE,"No owned caption bounds");const POINT caption={bounds.left+180,bounds.top+10};
                if(GetAncestor(WindowFromPoint(caption),GA_ROOT)!=owner.frame)continue;
                need(SetCursorPos(caption.x,caption.y)!=FALSE,"Cannot point at owned caption");
                INPUT click[2]={};for(auto &event:click)event.type=INPUT_MOUSE;click[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;click[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
                need(SendInput(2,click,sizeof(INPUT))==2,"Owned caption click failed");pump(100);
            }
            need(GetForegroundWindow()==owner.frame,"Keyboard fixture did not obtain foreground");
            need(SUCCEEDED(owner.events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"Keyboard fixture focus failed");pump(100);
        }
        std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;CComPtr<IEnumIDList> items;
        need(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"No strategy identities");
        for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(items->Next(1,&id,nullptr)==S_OK,"Short strategy enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
        std::vector<int> viewIndexes;
        if(indexes||isolatedStrategy==5||isolatedStrategy==10){const double mappingStart=clockMs();std::map<std::string,int> mapping;for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(SUCCEEDED(view->Item(i,&id))&&id,"No indexed identity");need(mapping.emplace(pidlKey(id),i).second,"Duplicate indexed identity");}
            for(const auto id:ids){const auto found=mapping.find(pidlKey(id));need(found!=mapping.end(),"Missing indexed identity");viewIndexes.push_back(found->second);}std::printf("INDEX mapping_ms=%.3f\n",clockMs()-mappingStart);}
        const HRESULT arranged=view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0);
        // Only the explicit custom-position contract can qualify as a spatial candidate.
        const bool free=(profile&FVO_CUSTOMPOSITION)!=0&&SUCCEEDED(arranged)&&view->GetAutoArrange()==S_FALSE;
        std::vector<POINT> positions;
        if(free){for(size_t i=0;i<ids.size();++i)positions.push_back(filespacer::scaleDPI(POINT{30+static_cast<LONG>(i%50)*300,30+static_cast<LONG>(i/50)*300}));
            need(SUCCEEDED(filespacer::restoreIconPositions(view,positionBlob(ids,positions),[](){return true;})),"Cannot seed free positions");}
        std::printf("CAPABILITY profile=%u free=%d flags_hr=0x%08lX\n",static_cast<unsigned>(profile),free?1:0,static_cast<unsigned long>(arranged));std::fflush(stdout);
        need(SUCCEEDED(owner.events->shell->SelectItem(ids[7],SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed logical focus");const HWND keyboard=GetFocus();
        CComPtr<IOleCommandTarget> command;CComPtr<IShellFolderView> bulk;
        if(!bare){owner.events->shell->QueryInterface(IID_PPV_ARGS(&command));owner.events->shell->QueryInterface(IID_PPV_ARGS(&bulk));}
        OLECMD selectAll={OLECMDID_SELECTALL,0};const HRESULT queried=command?command->QueryStatus(nullptr,1,&selectAll,nullptr):E_NOINTERFACE;
        const bool supported=SUCCEEDED(queried)&&(selectAll.cmdf&OLECMDF_SUPPORTED)&&(selectAll.cmdf&OLECMDF_ENABLED);
        std::printf("COMMAND profile=%u select_all_supported=%d query_hr=0x%08lX\n",static_cast<unsigned>(profile),supported?1:0,static_cast<unsigned long>(queried));
        const std::vector<int> sizes=isolated?std::vector<int>{5000}:indexes||callbacks?std::vector<int>{100,1000,5000}:std::vector<int>{100,500,1000,2500,5000};
        const int strategies=isolated?1:callbacks?2:5;
        for(int repetition=0;repetition<(isolated?1:3);++repetition)for(int selected:sizes)for(int order=0;order<strategies;++order){
            const int candidate=(order+repetition)%strategies;const int strategy=isolated?isolatedStrategy:callbacks?(candidate?9:0):indexes&&candidate?candidate+4:candidate;
            if(!indexes&&!callbacks&&!isolated){if(strategy==2&&(selected!=5000||!supported))continue;if(strategy>=3&&(!bulk||(strategy==3&&selected!=5000)))continue;}
            if(callbacks)owner.observe(strategy==0);
            need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Initial strategy clear failed");pump(100);
            const double began=clockMs();HRESULT result=S_OK;
            const auto select=[&](){return view->SelectAndPositionItems(static_cast<UINT>(selected),ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS);};
            const auto byIndex=[&](bool selecting,bool reverse){for(int i=0;i<selected;++i){const int offset=reverse?selected-1-i:i;const HRESULT value=view->SelectItem(viewIndexes[static_cast<size_t>(offset)],(selecting?static_cast<DWORD>(SVSI_SELECT):0u)|SVSI_NOTAKEFOCUS);if(FAILED(value))return value;}return S_OK;};
            const auto inDetails=[&](const std::function<HRESULT()> &operation){return withPublicRedraw(view,[&](){
                FOLDERVIEWMODE mode=FVM_AUTO;int iconSize=0;HRESULT changed=view->GetViewModeAndIconSize(&mode,&iconSize);if(FAILED(changed))return changed;
                changed=view->SetViewModeAndIconSize(FVM_DETAILS,16);if(FAILED(changed))return changed;
                const HRESULT operated=operation();const HRESULT restored=view->SetViewModeAndIconSize(mode,iconSize);return FAILED(operated)?operated:restored;
            });};
            const auto byKeyboard=[&](){
                if(GetForegroundWindow()!=owner.frame||GetAncestor(GetFocus(),GA_ROOT)!=owner.frame||(GetAsyncKeyState(VK_CONTROL)&0x8000))return E_UNEXPECTED;
                INPUT input[4]={};for(auto &event:input)event.type=INPUT_KEYBOARD;
                input[0].ki.wVk=VK_CONTROL;input[1].ki.wVk='A';input[2].ki.wVk='A';input[2].ki.dwFlags=KEYEVENTF_KEYUP;input[3].ki.wVk=VK_CONTROL;input[3].ki.dwFlags=KEYEVENTF_KEYUP;
                if(SendInput(4,input,sizeof(INPUT))!=4){INPUT release=input[3];SendInput(1,&release,sizeof(INPUT));return E_FAIL;}
                const ULONGLONG deadline=GetTickCount64()+30000;
                do{pump(1);int count=0;if(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count))&&count==selected)return S_OK;}while(GetTickCount64()<deadline);
                return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            };
            if(strategy==12)result=byKeyboard();else if(strategy==11)result=inDetails(select);else if(strategy==10)result=withPublicRedraw(view,[&](){return byIndex(true,false);});else if(strategy>=5&&strategy<=8)result=byIndex(true,strategy>=7);else if(strategy==1)result=withPublicRedraw(view,select);else if(strategy==2)result=command->Exec(nullptr,OLECMDID_SELECTALL,OLECMDEXECOPT_DODEFAULT,nullptr,nullptr);else if(strategy==3)result=bulk->Select(SFVS_SELECT_ALLITEMS);else result=select();
            const double selectMs=clockMs()-began;if(FAILED(result))std::fprintf(stderr,"Strategy=%d selection HRESULT=0x%08lX\n",strategy,static_cast<unsigned long>(result));need(SUCCEEDED(result),"Strategy selection failed");pump(0);verifySelection(view,ids,selected);
            const double clearBegan=clockMs();const auto clear=[&](){return owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS);};
            result=strategy==11?inDetails(clear):strategy==10?withPublicRedraw(view,[&](){return byIndex(false,false);}):strategy==9||strategy==12?clear():strategy>=5?byIndex(false,strategy==6||strategy==8):strategy>=3?bulk->Select(SFVS_SELECT_NONE):strategy==1?withPublicRedraw(view,clear):clear();const double clearMs=clockMs()-clearBegan;need(SUCCEEDED(result),"Strategy clear failed");pump(0);verifySelection(view,ids,0);
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focus;const bool focusPreserved=SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focus))&&ILIsEqual(focus,ids[7]);
            if(free)for(size_t i=0;i<ids.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==positions[i].x&&actual.y==positions[i].y,"Selection changed a free position");}
            const char *names[]={"batch","public-redraw","ole-select-all","shell-select-all","batch-shell-clear","index-forward-forward","index-forward-reverse","index-reverse-forward","index-reverse-reverse","batch-without-observer","index-public-redraw","details-roundtrip","keyboard-select-all"};const char *name=bare?"bare-batch":names[strategy];
            std::fprintf(file,"%u,%d,%s,%d,%.3f,%.3f,%d,%d,%d\n",static_cast<unsigned>(profile),repetition,name,selected,selectMs,clearMs,free?1:0,focusPreserved?1:0,GetFocus()==keyboard?1:0);std::fflush(file);
            std::printf("MEASURE profile=%u strategy=%d selected=%d rep=%d select_ms=%.3f clear_ms=%.3f positions=%d verified\n",static_cast<unsigned>(profile),strategy,selected,repetition,selectMs,clearMs,free?5000:0);std::fflush(stdout);
            need(focusPreserved&&GetFocus()==keyboard,"Strategy changed logical or keyboard focus");
        }
    }
}
void selectionRestore(const wchar_t *root,const wchar_t *path,bool errorsOnly,bool paired){
    SelectionHost owner;owner.create(path,static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING),false);auto view=owner.view();
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<POINT> points;
    CComPtr<IEnumIDList> items;need(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"No restore identities");
    for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(items->Next(1,&id,nullptr)==S_OK,"Short restore fixture");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);points.push_back(filespacer::scaleDPI(POINT{30+(i%50)*300,30+(i/50)*300}));}
    need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0))&&view->GetAutoArrange()==S_FALSE,"Restore free positions unavailable");
    const auto current=[](){return true;};need(SUCCEEDED(filespacer::restoreIconPositions(view,positionBlob(ids,points),current)),"Restore layout seed failed");
    need(SUCCEEDED(owner.events->shell->SelectItem(ids[7],SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Restore focus seed failed");const HWND keyboard=GetFocus();
    std::vector<uint8_t> empty,all,minus,half;
    need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS))&&SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,current,empty)),"Cannot capture empty snapshot");
    need(SUCCEEDED(view->SelectAndPositionItems(5000,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS))&&SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,current,all)),"Cannot capture full snapshot");
    need(SUCCEEDED(owner.events->shell->SelectItem(ids.back(),SVSI_NOTAKEFOCUS))&&SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,current,minus)),"Cannot capture partial snapshot");
    need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS))&&SUCCEEDED(view->SelectAndPositionItems(2500,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS))&&SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,current,half)),"Cannot capture disjoint snapshot");
    FILE *file=nullptr;need(_wfopen_s(&file,(std::wstring(root)+L"\\selection-restore.csv").c_str(),L"wb")==0,"No restore journal");std::unique_ptr<FILE,decltype(&std::fclose)> output(file,&std::fclose);
    std::fputs("operation,repetition,ms,selection,positions,focus_preserved,keyboard_preserved\n",file);
    auto apply=[&](const std::vector<uint8_t> &snapshot,int wanted,const char *label,int repetition,bool reference=false){
        auto pending=snapshot;const double began=clockMs();const HRESULT result=reference?filespacer::restoreReopenSelectionReference(owner.events->shell,pending,current):filespacer::restoreReopenSelection(owner.events->shell,pending,current);const double elapsed=clockMs()-began;
        need(SUCCEEDED(result)&&pending.empty(),"Production selection restore failed");verifySelection(view,ids,wanted);
        for(size_t i=0;i<ids.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==points[i].x&&actual.y==points[i].y,"Production restore changed a position");}
        int index=-1;CComHeapPtr<ITEMID_CHILD> focused;need(SUCCEEDED(view->GetFocusedItem(&index))&&index>=0&&SUCCEEDED(view->Item(index,&focused))&&ILIsEqual(focused,ids[7]),"Production restore changed focus");need(GetFocus()==keyboard,"Production restore changed keyboard focus");
        std::fprintf(file,"%s,%d,%.3f,%d,5000,1,1\n",label,repetition,elapsed,wanted);std::fflush(file);std::printf("RESTORE %s rep=%d ms=%.3f identities=%d positions=5000 verified\n",label,repetition,elapsed,wanted);std::fflush(stdout);
    };
    if(paired)for(int repetition=0;repetition<3;++repetition)for(int operation=0;operation<6;++operation)for(int order=0;order<2;++order){
        const bool reference=(order+repetition)%2==0;
        need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot clear paired input");
        const int initial=operation==0?5000:operation==1||operation==2?4999:operation==3?5000:operation==5?2500:0;
        if(initial)need(SUCCEEDED(view->SelectAndPositionItems(static_cast<UINT>(initial),ids.data()+(operation==5?2500:0),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot seed paired input");
        if(operation==5){const std::vector<PCUITEMID_CHILD> second(ids.begin()+2500,ids.end());verifySelection(view,second,initial);}else verifySelection(view,ids,initial);pump(100);
        const char *names[]={"remove-one","unchanged","add-one","clear-all","full-selection","replace-disjoint"};const std::string label=std::string(reference?"reference-":"delta-")+names[operation];
        const auto &snapshot=operation<=1?minus:operation==3?empty:operation==5?half:all;const int wanted=operation<=1?4999:operation==3?0:operation==5?2500:5000;
        apply(snapshot,wanted,label.c_str(),repetition,reference);
    }
    for(int repetition=0;repetition<(errorsOnly||paired?0:3);++repetition){
        need(SUCCEEDED(view->SelectAndPositionItems(5000,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot prepare restore selection");pump(100);
        apply(minus,4999,"remove-one",repetition);apply(minus,4999,"unchanged",repetition);apply(all,5000,"add-one",repetition);apply(empty,0,"clear-all",repetition);apply(all,5000,"full-selection",repetition);
    }
    if(errorsOnly){
        need(SUCCEEDED(view->SelectAndPositionItems(5000,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot seed cancellation selection");
        unsigned checks=0;auto abandoned=minus;const HRESULT cancelled=filespacer::restoreReopenSelection(owner.events->shell,abandoned,[&](){return ++checks<20;});
        need(cancelled==E_ABORT&&abandoned.empty(),"Cancellation was not propagated");verifySelection(view,ids,5000);
        HWND body=nullptr;need(SUCCEEDED(owner.events->shell->GetWindow(&body))&&IsWindowVisible(body),"Cancellation left the view hidden");
        for(size_t i=0;i<ids.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==points[i].x&&actual.y==points[i].y,"Cancellation changed a position");}
        std::printf("PASS cancellation checks=%u: exact selection and 5000 positions retained; redraw resumed\n",checks);
        CComPtr<IShellFolder> folder;need(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder))),"No fixture folder");STRRET display={};wchar_t name[32768]={};
        need(SUCCEEDED(folder->GetDisplayNameOf(ids.back(),static_cast<SHGDNF>(SHGDN_INFOLDER|SHGDN_FORPARSING),&display))&&SUCCEEDED(StrRetToBufW(&display,ids.back(),name,32768)),"No owned deletion name");
        need(PathFindFileNameW(name)==name&&std::wcsncmp(name,L"item-",5)==0,"Deletion is not an owned fixture leaf");
        const unsigned enumeration=owner.events->sink->done;need(DeleteFileW((std::wstring(path)+L"\\"+name).c_str())!=FALSE,"Cannot remove owned stale-snapshot fixture");
        need(SUCCEEDED(owner.events->shell->Refresh()),"Stale-snapshot refresh failed");const ULONGLONG end=GetTickCount64()+45000;
        while((!owner.events->sink||owner.events->sink->done<=enumeration)&&GetTickCount64()<end)pump(10);
        need(owner.events->sink&&owner.events->sink->done>enumeration,"Stale-snapshot enumeration did not complete");view=owner.view();
        int count=0;need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW,&count))&&count==4999,"Wrong surviving fixture count");
        need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0))&&view->GetAutoArrange()==S_FALSE,"Cannot establish stale-fixture free placement");
        const std::vector<PCUITEMID_CHILD> survivingIds(ids.begin(),ids.end()-1);
        const std::vector<POINT> survivingPoints(points.begin(),points.end()-1);
        need(SUCCEEDED(filespacer::restoreIconPositions(view,positionBlob(survivingIds,survivingPoints),current)),"Cannot establish surviving free coordinates");
        std::vector<POINT> before;
        for(size_t i=0;i+1<ids.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==points[i].x&&actual.y==points[i].y,"Surviving free coordinates were not established");before.push_back(actual);}
        need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot clear stale-snapshot selection");
        auto stale=all;const HRESULT restored=filespacer::restoreReopenSelection(owner.events->shell,stale,current);need(stale.empty(),"Stale snapshot not consumed");
        int staleSelected=-1,staleAll=-1;view->ItemCount(SVGIO_SELECTION,&staleSelected);view->ItemCount(SVGIO_ALLVIEW,&staleAll);
        std::printf("STALE HRESULT=0x%08lX selected=%d all=%d\n",static_cast<unsigned long>(restored),staleSelected,staleAll);std::fflush(stdout);
        verifySelection(view,ids,4999);
        for(size_t i=0;i<before.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==before[i].x&&actual.y==before[i].y,"Stale snapshot changed a surviving position");}
        std::printf("PASS stale snapshot HRESULT=0x%08lX: all 4999 surviving identities selected; positions retained\n",static_cast<unsigned long>(restored));std::fflush(stdout);
    }
}
}
int wmain(int argc,wchar_t **argv){
#ifdef FILESPACER_SELECTION_OPTIMIZED
    std::puts("BUILD /O2; C++14; debug CRT /MTd; _ITERATOR_DEBUG_LEVEL=0; reference and candidate share the same build");std::fflush(stdout);
#endif
    if((argc!=3&&argc!=4)||FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Placement Prototype";int result=0;
    if(!RegisterClassW(&type)){CoUninitialize();return 2;}
    try{if(argc==4&&std::wcsncmp(argv[3],L"restore",7)==0)selectionRestore(argv[1],argv[2],std::wcscmp(argv[3],L"restore-errors")==0,std::wcscmp(argv[3],L"restore-paired")==0);else{const wchar_t *fresh=argc==4&&(std::wcsncmp(argv[3],L"fresh-",6)==0||std::wcscmp(argv[3],L"bare-batch")==0)?argv[3]:nullptr;selectionStrategies(argv[1],argv[2],argc==4&&std::wcscmp(argv[3],L"index")==0,argc==4&&std::wcscmp(argv[3],L"callbacks")==0,fresh);}}catch(const std::exception &error){std::fprintf(stderr,"FAIL selection strategies: %s\n",error.what());result=1;}
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
