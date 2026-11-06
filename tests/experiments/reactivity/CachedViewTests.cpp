// Reuse the real-Shell event and position helpers from the positioning probe.
#define wmain positioningProbeMain
#include "PlacementTests.cpp"
#undef wmain
#include <psapi.h>

namespace {
struct CachedHost {
    HWND panel=nullptr;
    filespacer::ShellViewHost host;
    CComPtr<Events> events;
    ~CachedHost(){destroy();}
    void destroy(){if(events)events->detach();host.destroy();events.Release();if(panel)DestroyWindow(panel);panel=nullptr;}
    CComPtr<IFolderView2> view(){CComPtr<IFolderView2> current;need(SUCCEEDED(host.currentView(IID_PPV_ARGS(&current))),"Current cached view unavailable");return current;}
    void create(HWND frame,const wchar_t *path,bool spatial){
        RECT bounds={};GetClientRect(frame,&bounds);
        panel=CreateWindowW(L"FileSpacer Placement Prototype",L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,bounds.right,bounds.bottom,frame,nullptr,GetModuleHandleW(nullptr),nullptr);need(!!panel,"No owned view panel");
        FOLDERSETTINGS initial={static_cast<UINT>(spatial?FVM_ICON:FVM_DETAILS),FWF_NOWEBVIEW|FWF_NOHEADERINALLVIEWS};events.Attach(new Events);
        const auto profile=spatial?static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING):FVO_DEFAULT;
        need(SUCCEEDED(host.initialize(panel,bounds,initial,nullptr,events,profile)),"Cached host initialize failed");
        CComPtr<IShellItem> item;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&item)))&&SUCCEEDED(host.navigate(item,false,true)),"Cached host navigation failed");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while((!events->sink||!events->sink->done||!events->navigation)&&GetTickCount64()<deadline)pump(10);
        need(events->sink&&events->sink->done&&events->navigation,"Cached host enumeration did not complete");
        int count=0;need(SUCCEEDED(view()->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Incomplete cache fixture");
        if(spatial)need(SUCCEEDED(view()->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0))&&view()->GetAutoArrange()==S_FALSE,"Free cached layout unavailable");
    }
    void hide(){need(SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_NOFOCUS)),"View focus deactivation failed");ShowWindow(panel,SW_HIDE);}
    void show(){ShowWindow(panel,SW_SHOW);keyboardView=events->shell;need(SUCCEEDED(events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"View activation failed");}
};
SIZE_T privateBytes(){PROCESS_MEMORY_COUNTERS_EX memory={};memory.cb=sizeof(memory);need(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory))!=FALSE,"Private memory unavailable");return memory.PrivateUsage;}
void runCache(const wchar_t *root,const wchar_t *fixture){
    HWND frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Cached spatial view experiment",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,850,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(!!frame,"No cache frame");
    struct Frame{HWND value;~Frame(){DestroyWindow(value);}}frameScope{frame};
    CachedHost spatial;spatial.create(frame,fixture,true);auto initialView=spatial.view();
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<POINT> positions;
    CComPtr<IEnumIDList> items;need(SUCCEEDED(initialView->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"Cache items unavailable");
    for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(items->Next(1,&id,nullptr)==S_OK,"Short cache enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);positions.push_back(filespacer::scaleDPI(POINT{30+(i%50)*120,30+(i/50)*100}));}
    const auto saved=positionBlob(ids,positions);auto current=[](){return true;};
    need(SUCCEEDED(filespacer::restoreIconPositions(initialView,saved,current)),"Initial spatial fixture failed");
    spatial.show();CComPtr<IUnknown> original;initialView->QueryInterface(IID_PPV_ARGS(&original));initialView.Release();items.Release();
    const SIZE_T before=privateBytes();spatial.hide();CachedHost modern;modern.create(frame,fixture,false);modern.show();pump(100);
    const SIZE_T after=privateBytes();std::printf("CACHE extra_private_bytes=%lld\n",static_cast<long long>(after)-static_cast<long long>(before));std::fflush(stdout);
    const std::wstring csvPath=std::wstring(root)+L"\\cache.csv";FILE *out=nullptr;need(_wfopen_s(&out,csvPath.c_str(),L"wb")==0,"Cache log unavailable");
    struct File{FILE *value;~File(){std::fclose(value);}}log{out};std::fputs("strategy,repetition,switch_ms,positions,selection,focus,keyboard_focus\n",out);
    for(int repetition=0;repetition<3;++repetition){
        for(int recreate=0;recreate<2;++recreate){
            modern.show();auto modernView=modern.view();
            PCUITEMID_CHILD selected[]={ids[2+repetition],ids[9+repetition]};
            need(SUCCEEDED(modern.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Modern clear failed");
            need(SUCCEEDED(modernView->SelectAndPositionItems(2,selected,nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Modern selection failed");
            need(SUCCEEDED(modern.events->shell->SelectItem(selected[1],SVSI_SELECT|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Modern item focus failed");
            std::vector<uint8_t> selection;need(SUCCEEDED(filespacer::captureReopenSelection(modern.events->shell,current,selection)),"Modern state capture failed");
            const double began=clockMs();modern.hide();
            if(recreate){spatial.destroy();spatial.create(frame,fixture,true);need(SUCCEEDED(filespacer::restoreIconPositions(spatial.view(),saved,current)),"Recreated positions failed");}
            auto view=spatial.view();CComPtr<IUnknown> identity;view->QueryInterface(IID_PPV_ARGS(&identity));
            if(!recreate)need(identity==original,"Hidden cached view was replaced");
            need(SUCCEEDED(filespacer::restoreReopenSelection(spatial.events->shell,selection,current)),"Spatial state transfer failed");spatial.show();
            const double elapsed=clockMs()-began;
            for(size_t i=0;i<ids.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==positions[i].x&&actual.y==positions[i].y,"Cached spatial coordinate changed");DWORD state=0;need(SUCCEEDED(view->GetSelectionState(ids[i],&state))&&((state&SVSI_SELECT)!=0)==(i==static_cast<size_t>(2+repetition)||i==static_cast<size_t>(9+repetition)),"Cached selection changed");}
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focus;need(SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focus))&&ILIsEqual(focus,selected[1]),"Cached logical focus changed");
            HWND body=nullptr;spatial.events->shell->GetWindow(&body);need(GetFocus()==body||IsChild(body,GetFocus()),"Cached keyboard focus lost");
            std::fprintf(out,"%s,%d,%.3f,5000,2,verified,verified\n",recreate?"recreate":"retain",repetition,elapsed);std::fflush(out);
            std::printf("CACHE %s repetition %d verified: 5000 positions, 2 selected items, both kinds of focus\n",recreate?"recreate":"retain",repetition);std::fflush(stdout);
            original=identity;spatial.hide();modern.show();pump(100);
        }
    }
}
}
int wmain(int argc,wchar_t **argv){
    if(argc!=3||FAILED(OleInitialize(nullptr)))return 2;QueryPerformanceFrequency(&frequency);filespacer::initDPI();WNDCLASSW wc={};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"FileSpacer Placement Prototype";RegisterClassW(&wc);int result=0;
    try{runCache(argv[1],argv[2]);}catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    UnregisterClassW(wc.lpszClassName,wc.hInstance);OleUninitialize();return result;
}
