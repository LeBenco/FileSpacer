// Keep the same launch, snapshot and measurement code as the Explorer comparison.
#define wmain comparisonProbeMain
#include "ExplorerObservationTests.cpp"
#undef wmain
#include "ShellViewHost.h"
#include "ShellViewIcons.h"
#include "DPI.h"

namespace {
constexpr DWORD positionFlags = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
bool apiOnly=false;
bool profilePositions=false;
SIZE desktopSize={};int desktopIconSize=0;
constexpr int spatialItems=100;
struct Desktop {
    CComPtr<IShellView> shell;
    CComPtr<IFolderView2> view;
    CComPtr<IShellFolder> folder;
    std::wstring path;
    HWND window = nullptr;
    void currentView() {
        const ULONGLONG deadline=GetTickCount64()+45000;
        while(GetTickCount64()<deadline){
        CComPtr<IShellWindows> windows;require(SUCCEEDED(windows.CoCreateInstance(CLSID_ShellWindows)),"ShellWindows unavailable");
        CComVariant location(CSIDL_DESKTOP),empty;long handle=0;CComPtr<IDispatch> dispatch;
        require(SUCCEEDED(windows->FindWindowSW(&location,&empty,SWC_DESKTOP,&handle,SWFO_NEEDDISPATCH,&dispatch))&&dispatch,"Desktop unavailable");
        CComQIPtr<IServiceProvider> services(dispatch);CComPtr<IShellBrowser> browser;
        require(services&&SUCCEEDED(services->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&browser))),"Desktop browser unavailable");
        CComPtr<IShellView> source;CComPtr<IFolderView2> current;CComPtr<IShellFolder> provider;HWND handleWindow=nullptr;
        if(SUCCEEDED(browser->QueryActiveShellView(&source))&&source&&SUCCEEDED(source->QueryInterface(IID_PPV_ARGS(&current)))&&SUCCEEDED(current->GetFolder(IID_PPV_ARGS(&provider)))&&SUCCEEDED(source->GetWindow(&handleWindow))&&IsWindow(handleWindow)){
            shell=source;view=current;folder=provider;window=handleWindow;DWORD pid=0;const DWORD thread=GetWindowThreadProcessId(window,&pid);std::printf("DESKTOP current window=%p pid=%lu thread=%lu\n",window,pid,thread);std::fflush(stdout);return;
        }pump(20);
        }require(false,"Desktop current view unavailable before watchdog");
    }
    Desktop() {
        currentView();
        CComHeapPtr<wchar_t> name;require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&name)),"Desktop directory unavailable");path=name;
    }
};
struct Original {
    std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)> id{nullptr,&CoTaskMemFree};
    POINT point={};DWORD selected=0;
};
struct BackupHeader {DWORD magic=0x46535042,flags=0,count=0,focused=0;wchar_t prefix[80]={};};
std::wstring fixtureName(const std::wstring &prefix,int index){wchar_t suffix[24]={};swprintf_s(suffix,L"-%05d.txt",index);return prefix+suffix;}
void saveDesktop(Desktop &desktop,const std::wstring &root){
    BackupHeader header;require(SUCCEEDED(desktop.view->GetCurrentFolderFlags(&header.flags)),"Desktop flags unavailable");
    GUID guid={};wchar_t name[40]={};require(SUCCEEDED(CoCreateGuid(&guid))&&StringFromGUID2(guid,name,40),"No fixture identifier");
    const std::wstring prefix=std::wstring(L"FileSpacerSpatialProbe-")+name;wcscpy_s(header.prefix,prefix.c_str());
    int count=0;require(SUCCEEDED(desktop.view->ItemCount(SVGIO_ALLVIEW,&count))&&count>=0&&count<100,"Unexpected pre-existing desktop population");header.count=static_cast<DWORD>(count);
    CComPtr<IEnumIDList> items;require(SUCCEEDED(desktop.view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"Desktop enumeration unavailable");
    CComPtr<IStream> stream;require(SUCCEEDED(SHCreateStreamOnFileEx((root+L"\\desktop-backup.bin").c_str(),STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,FILE_ATTRIBUTE_NORMAL,TRUE,nullptr,&stream)),"Cannot save desktop backup");
    int focused=-1;desktop.view->GetFocusedItem(&focused);header.focused=focused>=0?1u:0u;
    require(SUCCEEDED(IStream_Write(stream,&header,sizeof(header))),"Backup header failed");
    if(header.focused){CComHeapPtr<ITEMID_CHILD> id;require(SUCCEEDED(desktop.view->Item(focused,&id))&&SUCCEEDED(IStream_WritePidl(stream,id)),"Focus backup failed");}
    for(int i=0;i<count;++i){CComHeapPtr<ITEMID_CHILD> id;POINT point={};DWORD state=0;
        require(items->Next(1,&id,nullptr)==S_OK&&SUCCEEDED(desktop.view->GetItemPosition(id,&point))&&SUCCEEDED(desktop.view->GetSelectionState(id,&state)),"Original desktop state unavailable");
        require(SUCCEEDED(IStream_WritePidl(stream,id))&&SUCCEEDED(IStream_Write(stream,&point,sizeof(point)))&&SUCCEEDED(IStream_Write(stream,&state,sizeof(state))),"Desktop backup failed");
    }
    require(SUCCEEDED(stream->Commit(STGC_DEFAULT)),"Desktop backup commit failed");
}
BackupHeader readDesktop(const std::wstring &root,std::vector<Original> &original,CComHeapPtr<ITEMID_CHILD> &focused){
    CComPtr<IStream> stream;require(SUCCEEDED(SHCreateStreamOnFileEx((root+L"\\desktop-backup.bin").c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,&stream)),"Desktop backup unavailable");
    BackupHeader header;require(SUCCEEDED(IStream_Read(stream,&header,sizeof(header)))&&header.magic==0x46535042&&header.count<100&&header.focused<=1,"Invalid desktop backup");
    const std::wstring marker=L"FileSpacerSpatialProbe-";const size_t length=wcsnlen_s(header.prefix,80);
    require(length<80&&length==marker.size()+38&&std::wcsncmp(header.prefix,marker.c_str(),marker.size())==0,"Invalid owned fixture prefix");
    GUID guid={};require(SUCCEEDED(CLSIDFromString(header.prefix+marker.size(),&guid)),"Invalid fixture identifier");
    if(header.focused)require(SUCCEEDED(IStream_ReadPidl(stream,&focused)),"Focus backup unreadable");
    for(DWORD i=0;i<header.count;++i){CComHeapPtr<ITEMID_CHILD> id;Original value;
        require(SUCCEEDED(IStream_ReadPidl(stream,&id))&&id&&SUCCEEDED(IStream_Read(stream,&value.point,sizeof(value.point)))&&SUCCEEDED(IStream_Read(stream,&value.selected,sizeof(value.selected))),"Original state unreadable");
        value.id.reset(id.Detach());original.push_back(std::move(value));
    }return header;
}
void restoreDesktop(const std::wstring &root){
    Desktop desktop;std::vector<Original> original;CComHeapPtr<ITEMID_CHILD> focused;
    const BackupHeader header=readDesktop(root,original,focused);
    for(int i=0;i<5000;++i){const std::wstring file=desktop.path+L"\\"+fixtureName(header.prefix,i);if(!DeleteFileW(file.c_str()))require(GetLastError()==ERROR_FILE_NOT_FOUND,"Owned desktop fixture cleanup failed");}
    require(SUCCEEDED(desktop.shell->Refresh()),"Desktop refresh failed");
    desktop.currentView();
    const ULONGLONG deadline=GetTickCount64()+45000;int count=-1;
    do{desktop.view->ItemCount(SVGIO_ALLVIEW,&count);if(count==static_cast<int>(header.count))break;pump(20);}while(GetTickCount64()<deadline);
    require(count==static_cast<int>(header.count),"Desktop fixture items remain");
    require(SUCCEEDED(desktop.view->SetCurrentFolderFlags(positionFlags,0)),"Cannot restore desktop coordinates");
    desktop.currentView();
    std::vector<PCUITEMID_CHILD> ids;std::vector<POINT> points;
    for(const auto &value:original){ids.push_back(value.id.get());points.push_back(value.point);}
    if(!ids.empty())require(SUCCEEDED(desktop.view->SelectAndPositionItems(static_cast<UINT>(ids.size()),ids.data(),points.data(),SVSI_POSITIONITEM|SVSI_NOSTATECHANGE)),"Original coordinates not restored");
    HRESULT selectionResult=S_OK;
    for(const auto &value:original){DWORD state=0;const HRESULT read=desktop.view->GetSelectionState(value.id.get(),&state);
        if(FAILED(read)){selectionResult=read;continue;}
        if((state&SVSI_SELECT)==(value.selected&SVSI_SELECT))continue;
        const HRESULT applied=desktop.shell->SelectItem(value.id.get(),SVSI_NOTAKEFOCUS|(value.selected&SVSI_SELECT));
        if(FAILED(applied))selectionResult=applied;
    }
    if(focused){DWORD state=0;desktop.view->GetSelectionState(focused,&state);const HRESULT applied=desktop.shell->SelectItem(focused,SVSI_FOCUSED|SVSI_NOTAKEFOCUS|(state&SVSI_SELECT));if(FAILED(applied))selectionResult=applied;}
    require(SUCCEEDED(desktop.view->SetCurrentFolderFlags(positionFlags,header.flags&positionFlags)),"Desktop flags not restored");pump(300);
    desktop.currentView();
    for(const auto &value:original){POINT p={};DWORD state=0;require(SUCCEEDED(desktop.view->GetItemPosition(value.id.get(),&p))&&p.x==value.point.x&&p.y==value.point.y,"Original desktop coordinates changed");require(SUCCEEDED(desktop.view->GetSelectionState(value.id.get(),&state))&&((state&SVSI_SELECT)==(value.selected&SVSI_SELECT)),"Original desktop selection changed");}
    DWORD flags=0;require(SUCCEEDED(desktop.view->GetCurrentFolderFlags(&flags))&&(flags&positionFlags)==(header.flags&positionFlags),"Desktop flag verification failed");
    if(FAILED(selectionResult)){std::fprintf(stderr,"Desktop selection restore HRESULT=0x%08lX\n",static_cast<unsigned long>(selectionResult));require(false,"Original selection or item focus not restored");}
    std::puts("DESKTOP CLEANUP verified: owned fixture deleted; original coordinates, selection and flags restored");std::fflush(stdout);
}
std::atomic<bool> dragEnded{false};std::atomic<DWORD> dragEndTick{0};
void CALLBACK dragObserved(HWINEVENTHOOK,DWORD event,HWND,LONG,LONG,DWORD,DWORD tick){if(event==EVENT_SYSTEM_DRAGDROPEND){dragEndTick.store(tick);dragEnded.store(true);}}
void mouseButton(DWORD flags){INPUT input={};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;require(SendInput(1,&input,sizeof(input))==1,"Mouse input rejected");}
void profilePlacement(IFolderView2 *view,HWND body,std::vector<PCUITEMID_CHILD> &ids,const std::vector<POINT> &points,const std::wstring &root,const char *label){
    DWORD pid=0;const DWORD thread=GetWindowThreadProcessId(body,&pid);const std::string name(label);const std::wstring wide(name.begin(),name.end());
    std::atomic<bool> stop{false},done{false},started{false};std::atomic<unsigned> iterations{0};
    std::thread sampler([&](){
        try{
            while(!started.load()&&!stop.load())Sleep(1);
            for(int sample=0;sample<5&&!stop.load();++sample){
                const unsigned before=iterations.load();
                snapshot(pid,thread,root+L"\\"+wide+L"-position-sample-"+std::to_wstring(sample)+L".dmp");
                std::printf("PROFILE %s sample=%d iterations_before=%u iterations_after=%u\n",label,sample,before,iterations.load());std::fflush(stdout);
            }
        }catch(const std::exception &error){std::fprintf(stderr,"Position profiling failed: %s\n",error.what());}
        done.store(true);
    });
    struct Sampling{std::atomic<bool>&stop,&done;std::thread &worker;~Sampling(){stop.store(true);while(!done.load())pump(5);if(worker.joinable())worker.join();}}sampling{stop,done,sampler};
    std::vector<POINT> shifted(16);const ULONGLONG deadline=GetTickCount64()+45000;unsigned count=0;
    while(!done.load()&&GetTickCount64()<deadline){
        for(size_t i=0;i<shifted.size();++i)shifted[i]={points[i].x+((count%2)?16:32),points[i].y+16};
        started.store(true);
        require(SUCCEEDED(view->SelectAndPositionItems(16,ids.data(),shifted.data(),SVSI_POSITIONITEM|SVSI_NOSTATECHANGE)),"Diagnostic positioning failed");
        iterations.store(++count);pump(0);
    }
    require(done.load()&&count>0,"Position profiling exceeded its watchdog");
    for(size_t i=0;i<ids.size();++i){POINT actual={};const POINT expected=i<16?shifted[i]:points[i];require(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==expected.x&&actual.y==expected.y,"Diagnostic positioning changed an unexpected coordinate");}
    std::printf("PROFILE %s completed iterations=%u; all %zu coordinates verified; diagnostic timings excluded\n",label,count,ids.size());std::fflush(stdout);
}
void dragPlacement(IShellView *shell,IFolderView2 *view,HWND frame,std::vector<PCUITEMID_CHILD> &ids,const std::wstring &root,const char *label,bool guardedSeed=false){
    require(ids.size()==spatialItems,"Wrong drag fixture size");require(SUCCEEDED(view->SetCurrentFolderFlags(positionFlags,0))&&view->GetAutoArrange()==S_FALSE,"Free placement unavailable");
    HWND body=nullptr;require(SUCCEEDED(shell->GetWindow(&body)),"No view window");
    RECT bounds={};GetClientRect(body,&bounds);POINT position={(bounds.right-bounds.left)/2,(bounds.bottom-bounds.top)/2};
    FOLDERVIEWMODE mode=FVM_AUTO;int iconSize=0;DWORD flags=0;
    require(SUCCEEDED(view->GetViewModeAndIconSize(&mode,&iconSize))&&SUCCEEDED(view->GetCurrentFolderFlags(&flags)),"View metadata unavailable");
    std::printf("METADATA %s mode=%d icon_size=%d dpi=%u view_client=%ldx%ld flags=0x%08lX\n",label,static_cast<int>(mode),iconSize,GetDpiForWindow(body),bounds.right-bounds.left,bounds.bottom-bounds.top,flags);std::fflush(stdout);
    if(std::strcmp(label,"Explorer-Desktop")==0){desktopSize={bounds.right-bounds.left,bounds.bottom-bounds.top};desktopIconSize=iconSize;}
    std::vector<POINT> points(ids.size(),position);
    if(apiOnly)for(size_t i=0;i<points.size();++i){const POINT p={30+static_cast<LONG>(i%10)*70,30+static_cast<LONG>(i/10)*50};points[i]=filespacer::scaleDPI(filespacer::invScaleDPI(p));}
    DWORD pid=0;const DWORD thread=GetWindowThreadProcessId(body,&pid);
    std::atomic<bool> seedDone{false};const std::string name(label);const std::wstring wideName(name.begin(),name.end());
    std::thread seedSampler;
    if(!guardedSeed)seedSampler=std::thread([&](){try{Sleep(35);if(!seedDone.load())snapshot(pid,thread,root+L"\\"+wideName+L"-api-placement.dmp");}catch(const std::exception &error){std::fprintf(stderr,"Placement snapshot failed: %s\n",error.what());}});
    const double seed=now();HRESULT seeded=S_OK;
    if(guardedSeed){
        CComPtr<IStream> stream;stream.Attach(SHCreateMemStream(nullptr,0));require(!!stream,"No fixture stream");
        for(size_t i=0;i<ids.size();++i){const POINT dip=filespacer::invScaleDPI(points[i]);require(SUCCEEDED(IStream_WritePidl(stream,ids[i]))&&SUCCEEDED(IStream_Write(stream,&dip,sizeof(dip))),"Fixture encoding failed");}
        ULARGE_INTEGER size={};LARGE_INTEGER zero={};require(SUCCEEDED(IStream_Size(stream,&size))&&size.QuadPart<=INT32_MAX&&SUCCEEDED(stream->Seek(zero,STREAM_SEEK_SET,nullptr)),"Invalid fixture stream");
        std::vector<uint8_t> bytes(static_cast<size_t>(size.QuadPart));require(SUCCEEDED(IStream_Read(stream,bytes.data(),static_cast<ULONG>(bytes.size()))),"Fixture stream read failed");
        seeded=filespacer::restoreIconPositions(view,bytes,[](){return true;});
    }else seeded=view->SelectAndPositionItems(spatialItems,ids.data(),points.data(),SVSI_POSITIONITEM|SVSI_NOSTATECHANGE);
    const double seededMs=now()-seed;seedDone.store(true);if(seedSampler.joinable())seedSampler.join();require(SUCCEEDED(seeded),"Explicit drag layout rejected");
    std::printf("SEED %s items=%d %s_ms=%.3f\n",label,spatialItems,guardedSeed?"production_guard":"raw",seededMs);std::fflush(stdout);pump(500);
    std::atomic<bool> verified{false};const DWORD clientThread=GetCurrentThreadId();
    std::thread verificationWatch([&](){try{for(int i=0;i<20&&!verified.load();++i)Sleep(50);if(!verified.load()){snapshot(pid,thread,root+L"\\"+wideName+L"-get-position.dmp");snapshot(GetCurrentProcessId(),clientThread,root+L"\\"+wideName+L"-client-get-position.dmp");}}catch(const std::exception &error){std::fprintf(stderr,"Position diagnostic failed: %s\n",error.what());}});
    struct Verification{std::atomic<bool>&done;std::thread&worker;~Verification(){done.store(true);if(worker.joinable())worker.join();}}verification{verified,verificationWatch};
    for(size_t i=0;i<ids.size();++i){
        if(i<3){std::printf("POSITION %s read begin %zu\n",label,i);std::fflush(stdout);}
        POINT actual={};const HRESULT result=view->GetItemPosition(ids[i],&actual);
        if(i<3){std::printf("POSITION %s read end %zu hr=0x%08lX point=%ld,%ld\n",label,i,static_cast<unsigned long>(result),actual.x,actual.y);std::fflush(stdout);}
        require(SUCCEEDED(result)&&actual.x==points[i].x&&actual.y==points[i].y,"Explicit layout not applied");
    }
    verified.store(true);verificationWatch.join();
    if(apiOnly){
        struct BorrowedTarget:Target{~BorrowedTarget(){window=nullptr;}}target;
        target.window=frame;target.processId=pid;target.threadId=thread;target.view=view;target.shell=shell;
        captureEvents=false;captureSnapshots=true;
        for(int repetition=0;repetition<3;++repetition){
            std::vector<POINT> partial;for(size_t i=0;i<16;++i)partial.push_back({points[i].x+(repetition+1)*16,points[i].y+(repetition+1)*16});
            require(SUCCEEDED(measure(target,label,spatialItems,repetition,"position_16",[&](){return view->SelectAndPositionItems(16,ids.data(),partial.data(),SVSI_POSITIONITEM|SVSI_NOSTATECHANGE);})),"Public icon move failed");
            for(size_t i=0;i<ids.size();++i){POINT actual={};const POINT expected=i<16?partial[i]:points[i];require(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==expected.x&&actual.y==expected.y,"Coordinate verification failed");}
            std::printf("API %s repetition=%d all %d coordinates verified\n",label,repetition,spatialItems);std::fflush(stdout);
        }
        if(profilePositions)profilePlacement(view,body,ids,points,root,label);
        return;
    }
    require(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS))&&SUCCEEDED(view->SelectAndPositionItems(spatialItems,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Drag selection failed");pump(500);
    int selected=0;require(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&selected))&&selected==spatialItems,"Incomplete drag selection");
    CComPtr<IDataObject> data;const HRESULT got=shell->GetItemObject(SVGIO_SELECTION,IID_PPV_ARGS(&data));
    if(SUCCEEDED(got)){FORMATETC format={static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_SHELLIDLISTOFFSET)),nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};std::printf("FORMAT %s offsets_query=0x%08lX\n",label,static_cast<unsigned long>(data->QueryGetData(&format)));}
    FILE *tree=nullptr;const std::string narrow(label);const std::wstring wide(narrow.begin(),narrow.end());
    if(_wfopen_s(&tree,(root+L"\\"+wide+L"-windows.txt").c_str(),L"wb")==0){EnumChildWindows(frame,windowObserved,reinterpret_cast<LPARAM>(tree));std::fclose(tree);}
    POINT cursor={};GetCursorPos(&cursor);struct InputCleanup{POINT point;~InputCleanup(){INPUT input={};input.type=INPUT_MOUSE;input.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&input,sizeof(input));SetCursorPos(point.x,point.y);}}cleanup{cursor};
    for(int repetition=0;repetition<3;++repetition){
        SetForegroundWindow(frame);pump(150);POINT start={position.x+16,position.y+16};require(ClientToScreen(body,&start)!=FALSE,"Coordinate conversion failed");
        HWND hit=WindowFromPoint(start);require(hit==body||IsChild(body,hit),"The fixture is covered by another window");
        const HWND oldForeground=GetForegroundWindow();(void)oldForeground;
        dragEnded.store(false);dragEndTick.store(0);HWINEVENTHOOK hook=SetWinEventHook(EVENT_SYSTEM_DRAGDROPSTART,EVENT_SYSTEM_DRAGDROPEND,nullptr,dragObserved,pid,thread,WINEVENT_OUTOFCONTEXT);
        require(!!hook,"Drag observer unavailable");
        struct Hook {HWINEVENTHOOK value;~Hook(){if(value)UnhookWinEvent(value);}}hookScope{hook};
        std::atomic<bool> stop{false};double maxPing=0;unsigned timeouts=0;
        std::thread monitor([&](){while(!stop.load()){DWORD_PTR answer=0;const double began=now();const bool ok=SendMessageTimeoutW(frame,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&answer)!=0;maxPing=(std::max)(maxPing,now()-began);if(!ok)++timeouts;Sleep(5);}});
        std::thread sampler;
        struct Threads {std::atomic<bool> &stop;std::thread &monitor,&sampler;~Threads(){stop.store(true);if(monitor.joinable())monitor.join();if(sampler.joinable())sampler.join();}}threads{stop,monitor,sampler};
        require(SetCursorPos(start.x,start.y)!=FALSE,"Cursor move failed");const double began=now();const DWORD tick=GetTickCount();
        mouseButton(MOUSEEVENTF_LEFTDOWN);pump(40);
        for(int step=1;step<=4;++step){SetCursorPos(start.x+step*16,start.y+step*4);pump(25);}
        mouseButton(MOUSEEVENTF_LEFTUP);
        if(repetition==0)sampler=std::thread([&,wide](){try{Sleep(35);snapshot(pid,thread,root+L"\\"+wide+L"-drop.dmp");Sleep(245);snapshot(pid,thread,root+L"\\"+wide+L"-after-drop.dmp");}catch(const std::exception &error){std::fprintf(stderr,"Drag snapshot failed: %s\n",error.what());}});
        const ULONGLONG deadline=GetTickCount64()+60000;const POINT wanted={position.x+64,position.y+16};bool changed=false;
        while(GetTickCount64()<deadline){POINT first={},last={};if(SUCCEEDED(view->GetItemPosition(ids.front(),&first))&&SUCCEEDED(view->GetItemPosition(ids.back(),&last))&&first.x==wanted.x&&first.y==wanted.y&&last.x==wanted.x&&last.y==wanted.y){changed=true;break;}pump(10);}
        const double positioned=now()-began;
        pump(1000);stop.store(true);monitor.join();if(sampler.joinable())sampler.join();UnhookWinEvent(hook);hookScope.value=nullptr;
        std::fprintf(csv,"%s,%d,%.3f,%.3f,%u,%u,%lu\n",label,repetition,positioned,maxPing,timeouts,dragEnded.load()?1u:0u,dragEnded.load()?dragEndTick.load()-tick:0);std::fflush(csv);
        require(changed,"Native group drag did not move the fixture");
        for(const auto id:ids){POINT actual={};require(SUCCEEDED(view->GetItemPosition(id,&actual))&&actual.x==wanted.x&&actual.y==wanted.y,"A grouped icon did not follow the drag");}
        require(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&selected))&&selected==spatialItems,"Group drag changed the selection");
        std::printf("DRAG %s repetition=%d positions=%d verified ms=%.3f max_ping=%.3f\n",label,repetition,spatialItems,positioned,maxPing);std::fflush(stdout);position=wanted;
    }
}
void runDesktop(const std::wstring &root){
    Desktop desktop;saveDesktop(desktop,root);std::vector<Original> original;CComHeapPtr<ITEMID_CHILD> focus;const BackupHeader header=readDesktop(root,original,focus);
    Target observation;observation.shell=desktop.shell;observation.view=desktop.view;observation.subscribe();
    require(observation.sink&&observation.connection,"Desktop does not expose enumeration completion events");
    const unsigned baseline=observation.sink->enumerated;
    require(SUCCEEDED(desktop.view->SetCurrentFolderFlags(positionFlags,0)),"Cannot prepare free desktop layout");
    desktop.currentView();
    for(int i=0;i<spatialItems;++i){const std::wstring path=desktop.path+L"\\"+fixtureName(header.prefix,i);Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr),&CloseHandle);require(file.get()!=INVALID_HANDLE_VALUE,"Desktop fixture collision or creation failure");}
    desktop.shell->Refresh();desktop.currentView();const ULONGLONG deadline=GetTickCount64()+60000;int count=0;
    do{desktop.view->ItemCount(SVGIO_ALLVIEW,&count);if(count==static_cast<int>(header.count)+spatialItems&&observation.sink->enumerated>baseline)break;pump(20);}while(GetTickCount64()<deadline);
    require(count==static_cast<int>(header.count)+spatialItems&&observation.sink->enumerated>baseline,"Desktop fixture enumeration did not complete");
    desktop.currentView();CComPtr<IUnknown> before,current;observation.shell->QueryInterface(IID_PPV_ARGS(&before));desktop.shell->QueryInterface(IID_PPV_ARGS(&current));require(before==current,"Desktop replaced the observed view");
    std::printf("DESKTOP enumeration completed events=%u\n",observation.sink->enumerated-baseline);std::fflush(stdout);
    CComPtr<IEnumIDList> items;require(SUCCEEDED(desktop.view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"Desktop items unavailable");
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;
    for(int i=0;i<count;++i){CComHeapPtr<ITEMID_CHILD> id;require(items->Next(1,&id,nullptr)==S_OK,"Short desktop enumeration");STRRET value={};wchar_t name[32768]={};
        require(SUCCEEDED(desktop.folder->GetDisplayNameOf(id,static_cast<SHGDNF>(SHGDN_INFOLDER|SHGDN_FORPARSING),&value))&&SUCCEEDED(StrRetToBufW(&value,id,name,32768)),"Desktop name unavailable");
        if(std::wcsncmp(PathFindFileNameW(name),header.prefix,std::wcslen(header.prefix))==0){ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}}
    dragPlacement(desktop.shell,desktop.view,GetAncestor(desktop.window,GA_ROOT),ids,root,"Explorer-Desktop");
}
void runFileSpacer(const std::wstring &root,const std::wstring &fixture){
    const std::wstring directory=root+L"\\app";require(!!CreateDirectoryW(directory.c_str(),nullptr),"No binary directory");extract(101,directory+L"\\FileSpacer.exe");extract(102,directory+L"\\FileSpacerFolder.dll");
    launch(directory+L"\\FileSpacer.exe",L"/test \""+fixture+L"\"",root+L"\\app.log");Target target;CComPtr<IShellWindows> windows;require(SUCCEEDED(windows.CoCreateInstance(CLSID_ShellWindows)),"ShellWindows unavailable");
    const ULONGLONG deadline=GetTickCount64()+45000;while(!target.window&&GetTickCount64()<deadline){findView(windows,fixture,true,target);pump(10);}require(!!target.window,"FileSpacer view unavailable");
    int count=0;do{target.view->ItemCount(SVGIO_ALLVIEW,&count);if(count==spatialItems)break;pump(20);}while(GetTickCount64()<deadline);require(count==spatialItems,"FileSpacer fixture incomplete");
    require(SUCCEEDED(target.view->SetViewModeAndIconSize(FVM_ICON,48)),"Icons unavailable");pump(500);
    CComPtr<IEnumIDList> items;require(SUCCEEDED(target.view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"No FileSpacer items");
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;
    for(int i=0;i<spatialItems;++i){CComHeapPtr<ITEMID_CHILD> id;require(items->Next(1,&id,nullptr)==S_OK,"Short FileSpacer enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
    dragPlacement(target.shell,target.view,target.window,ids,root,"FileSpacer-spatial");
    HWND frame=target.window;target.disconnect();PostMessageW(frame,WM_CLOSE,0,0);const ULONGLONG end=GetTickCount64()+20000;while(IsWindow(frame)&&GetTickCount64()<end)pump(10);require(!IsWindow(frame),"Owned FileSpacer window did not close");target.window=nullptr;
}
struct HostEvents:IExplorerBrowserEvents{
    LONG references=1;
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IExplorerBrowserEvents)return E_NOINTERFACE;*out=static_cast<IExplorerBrowserEvents *>(this);AddRef();return S_OK;}
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnViewCreated(IShellView *)override{return S_OK;}
};
void runSpatialHost(const std::wstring &root,const std::wstring &fixture,bool positionOnly=false){
    filespacer::initDPI();WNDCLASSW wc={};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"FileSpacer Spatial Placement Host";require(RegisterClassW(&wc)!=0,"Host class unavailable");
    struct Frame{HWND window=nullptr;filespacer::ShellViewHost host;~Frame(){host.destroy();if(window)DestroyWindow(window);UnregisterClassW(L"FileSpacer Spatial Placement Host",GetModuleHandleW(nullptr));}}frame;
    frame.window=CreateWindowW(wc.lpszClassName,L"Spatial placement control",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,850,600,nullptr,nullptr,wc.hInstance,nullptr);require(!!frame.window,"Host frame unavailable");
    RECT bounds={};GetClientRect(frame.window,&bounds);FOLDERSETTINGS initial={FVM_ICON,FWF_AUTOARRANGE|FWF_NOWEBVIEW|FWF_NOHEADERINALLVIEWS};CComPtr<HostEvents> events;events.Attach(new HostEvents);
    const auto profile=positionOnly?FVO_CUSTOMPOSITION:static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING);
    require(SUCCEEDED(frame.host.initialize(frame.window,bounds,initial,nullptr,events,profile)),"Production host initialization failed");
    CComPtr<IShellItem> folder;require(SUCCEEDED(SHCreateItemFromParsingName(fixture.c_str(),nullptr,IID_PPV_ARGS(&folder)))&&SUCCEEDED(frame.host.navigate(folder,false,true)),"Host fixture unavailable");
    CComPtr<IFolderView2> view;int count=0;const ULONGLONG deadline=GetTickCount64()+45000;
    do{view.Release();if(SUCCEEDED(frame.host.currentView(IID_PPV_ARGS(&view))))view->ItemCount(SVGIO_ALLVIEW,&count);if(count==spatialItems)break;pump(20);}while(GetTickCount64()<deadline);require(view&&count==spatialItems,"Host fixture incomplete");
    CComQIPtr<IShellView> shell(view);require(!!shell,"Host view unavailable");CComPtr<IEnumIDList> items;require(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"Host enumeration unavailable");
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;
    for(int i=0;i<spatialItems;++i){CComHeapPtr<ITEMID_CHILD> id;require(items->Next(1,&id,nullptr)==S_OK,"Short host enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
    dragPlacement(shell,view,frame.window,ids,root,positionOnly?"FileSpacer-position-only":"FileSpacer-native-host",true);
    if(profilePositions&&!positionOnly){
        require(desktopSize.cx>0&&desktopSize.cy>0&&desktopIconSize>0,"Desktop geometry metadata unavailable");
        RECT outer={0,0,desktopSize.cx,desktopSize.cy};require(AdjustWindowRectEx(&outer,WS_OVERLAPPEDWINDOW,FALSE,0)!=FALSE,"Matched geometry unavailable");
        require(SetWindowPos(frame.window,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"Matched frame resize failed");
        GetClientRect(frame.window,&bounds);require(SUCCEEDED(frame.host.setBounds(bounds)),"Matched host resize failed");
        require(SUCCEEDED(view->SetViewModeAndIconSize(FVM_ICON,desktopIconSize)),"Matched icon size unavailable");pump(500);
        dragPlacement(shell,view,frame.window,ids,root,"FileSpacer-larger-host",true);
    }
}
}
int desktopProbeMain(int argc,wchar_t **argv){
    if((argc!=3&&argc!=4)||FAILED(OleInitialize(nullptr)))return 2;QueryPerformanceFrequency(&frequency);int result=0;
    profilePositions=argc==4&&std::wcscmp(argv[3],L"profile")==0;
    apiOnly=profilePositions||(argc==4&&std::wcscmp(argv[3],L"api")==0);
    const std::wstring root=argv[1];observationRoot=root;filespacer::initDPI();
    try{
        if(std::wcscmp(argv[2],L"recover")==0)restoreDesktop(root);
        else{
            require((GetAsyncKeyState(VK_LBUTTON)&0x8000)==0&&(GetAsyncKeyState(VK_CONTROL)&0x8000)==0&&(GetAsyncKeyState(VK_SHIFT)&0x8000)==0&&(GetAsyncKeyState(VK_MENU)&0x8000)==0,"Desktop input is already in use");
            require(_wfopen_s(&csv,(root+(apiOnly?L"\\positions.csv":L"\\drag.csv")).c_str(),L"wb")==0,"No placement CSV");
            require(_wfopen_s(&waits,(root+L"\\wait-chains.csv").c_str(),L"wb")==0,"No wait-chain log");
            std::fputs(apiOnly?"app,items,repetition,operation,hresult,call_ms,max_ping_ms,ping_timeouts,cpu_ms,enum_events,change_events,observed_items,observed_selection\n":"app,repetition,positions_observed_ms,max_ping_ms,ping_timeouts,drag_end_event,drag_end_ms\n",csv);
            try{runDesktop(root);}catch(const std::exception &error){std::fprintf(stderr,"Desktop experiment failed: %s\n",error.what());restoreDesktop(root);throw;}
            restoreDesktop(root);if(apiOnly){runSpatialHost(root,argv[2]);if(profilePositions)runSpatialHost(root,argv[2],true);}else runFileSpacer(root,argv[2]);std::fclose(csv);csv=nullptr;
        }
    }catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    if(csv){std::fclose(csv);csv=nullptr;}if(waits){std::fclose(waits);waits=nullptr;}OleUninitialize();return result;
}
#ifndef FILESPACER_VISUAL_TIMING
int wmain(int argc,wchar_t **argv){return desktopProbeMain(argc,argv);}
#endif
