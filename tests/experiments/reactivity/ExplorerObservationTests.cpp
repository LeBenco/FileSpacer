#include <common.h>
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shldisp.h>
#include <exdisp.h>
#include <shlguid.h>
#include <shdispid.h>
#include <propkey.h>
#include <wct.h>
#include <atlbase.h>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <processsnapshot.h>
#include <dbghelp.h>
#include <psapi.h>
#include <cstring>
#ifdef FILESPACER_VISUAL_TIMING
#include "PresentationTiming.h"
#endif

namespace {
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;
FILE *csv = nullptr, *waits = nullptr;
LARGE_INTEGER frequency;
double now() { LARGE_INTEGER n; QueryPerformanceCounter(&n); return 1000.0 * static_cast<double>(n.QuadPart) / static_cast<double>(frequency.QuadPart); }
void require(bool ok, const char *text) { if (!ok) throw std::runtime_error(text); }
void pump(DWORD milliseconds) {
    const ULONGLONG deadline = GetTickCount64() + milliseconds;
    do {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        if (GetTickCount64() >= deadline) break;
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    } while (true);
}
double cpu(HANDLE process) {
    FILETIME c,e,k,u;
    if (!process || !GetProcessTimes(process,&c,&e,&k,&u)) return -1;
    ULARGE_INTEGER kernel, user;
    kernel.LowPart=k.dwLowDateTime;kernel.HighPart=k.dwHighDateTime;
    user.LowPart=u.dwLowDateTime;user.HighPart=u.dwHighDateTime;
    return static_cast<double>(kernel.QuadPart+user.QuadPart)/10000.0;
}
void waitChain(DWORD thread, const char *app, const char *operation, double lag) {
    HWCT session=OpenThreadWaitChainSession(0,nullptr);
    if (!session) { std::fprintf(waits,"%s,%s,%.3f,error,%lu\n",app,operation,lag,GetLastError());return; }
    WAITCHAIN_NODE_INFO nodes[WCT_MAX_NODE_COUNT]={};DWORD count=WCT_MAX_NODE_COUNT;BOOL cycle=FALSE;
    if (GetThreadWaitChain(session,0,WCTP_GETINFO_ALL_FLAGS,thread,&count,nodes,&cycle)) {
        for (DWORD i=0;i<(std::min)(count,static_cast<DWORD>(WCT_MAX_NODE_COUNT));++i)
            std::fprintf(waits,"%s,%s,%.3f,node,%u,%u,%d\n",app,operation,lag,
                static_cast<unsigned>(nodes[i].ObjectType),static_cast<unsigned>(nodes[i].ObjectStatus),cycle);
    } else std::fprintf(waits,"%s,%s,%.3f,error,%lu\n",app,operation,lag,GetLastError());
    CloseThreadWaitChainSession(session);std::fflush(waits);
}
struct Sink : IDispatch {
    LONG references=1; unsigned enumerated=0, changed=0;
    STDMETHODIMP QueryInterface(REFIID id,void **out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=IID_IUnknown && id!=IID_IDispatch && id!=DIID_DShellFolderViewEvents)return E_NOINTERFACE;
        *out=static_cast<IDispatch*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override {return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release() override {ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP GetTypeInfoCount(UINT *n) override {if(!n)return E_POINTER;*n=0;return S_OK;}
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo**) override {return E_NOTIMPL;}
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*) override {return E_NOTIMPL;}
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*,VARIANT *result,EXCEPINFO*,UINT*) override {
        if(result)VariantInit(result);
        if(id==DISPID_FILELISTENUMDONE)++enumerated;
        if(id==DISPID_ORDERCHANGED || id==DISPID_VIEWMODECHANGED || id==DISPID_SELECTIONCHANGED)++changed;
        return S_OK;
    }
};
struct Target {
    HWND window=nullptr;DWORD processId=0,threadId=0;
    CComPtr<IShellView> shell;
    CComPtr<IFolderView2> view;
    CComPtr<IConnectionPoint> connection;DWORD cookie=0;
    CComPtr<Sink> sink;
    void disconnect(){if(connection)connection->Unadvise(cookie);connection.Release();sink.Release();view.Release();shell.Release();}
    ~Target(){disconnect();if(window && IsWindow(window))PostMessageW(window,WM_CLOSE,0,0);}
    void subscribe(){
        CComPtr<IDispatch> automation;
        if(FAILED(shell->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&automation))))return;
        CComQIPtr<IConnectionPointContainer> container(automation);
        if(!container || FAILED(container->FindConnectionPoint(DIID_DShellFolderViewEvents,&connection)))return;
        sink.Attach(new Sink);
        if(FAILED(connection->Advise(sink,&cookie))){connection.Release();sink.Release();}
    }
};
bool findView(IShellWindows *windows,const std::wstring &path,bool fileSpacer,Target &target){
    long count=0;windows->get_Count(&count);
    for(long i=0;i<count;++i){
        CComPtr<IDispatch> dispatch;
        if(FAILED(windows->Item(CComVariant(i),&dispatch)))continue;
        CComQIPtr<IWebBrowserApp> browser(dispatch);SHANDLE_PTR raw=0;
        if(!browser || FAILED(browser->get_HWND(&raw)))continue;
        HWND frame=reinterpret_cast<HWND>(raw);wchar_t name[80]={};GetClassNameW(frame,name,80);
        bool isFileSpacer=wcscmp(name,L"FileSpacer Item Window")==0;
        if(isFileSpacer!=fileSpacer)continue;
        CComQIPtr<IServiceProvider> services(browser);CComPtr<IShellBrowser> host;
        if(!services || FAILED(services->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&host))))continue;
        CComPtr<IShellView> shell;CComPtr<IFolderView2> view;CComPtr<IPersistFolder2> folder;
        if(FAILED(host->QueryActiveShellView(&shell)) || FAILED(shell->QueryInterface(IID_PPV_ARGS(&view)))
            || FAILED(view->GetFolder(IID_PPV_ARGS(&folder))))continue;
        CComHeapPtr<ITEMIDLIST> id;CComHeapPtr<wchar_t> shown;
        if(FAILED(folder->GetCurFolder(&id)) || FAILED(SHGetNameFromIDList(id,SIGDN_FILESYSPATH,&shown)))continue;
        if(_wcsicmp(shown,path.c_str())!=0)continue;
        target.window=frame;target.threadId=GetWindowThreadProcessId(frame,&target.processId);
        target.shell=shell;target.view=view;return true;
    }
    return false;
}
void row(const char *app,int n,int repetition,const char *operation,HRESULT hr,double call,double lag,
         unsigned timeouts,double cpuMs,unsigned enumEvents,unsigned changeEvents,int count,int selected){
    std::fprintf(csv,"%s,%d,%d,%s,0x%08lX,%.3f,%.3f,%u,%.3f,%u,%u,%d,%d\n",
        app,n,repetition,operation,static_cast<unsigned long>(hr),call,lag,timeouts,cpuMs,enumEvents,changeEvents,count,selected);
    std::fflush(csv);
}
std::wstring observationRoot;
bool captureEvents = true;
bool captureSnapshots = true;
std::atomic<unsigned> snapshotCount{0};
void captureImages(HANDLE process){
    const std::wstring directory=observationRoot+L"\\images";CreateDirectoryW(directory.c_str(),nullptr);
    HMODULE modules[1024]={};DWORD bytes=0;
    if(!EnumProcessModulesEx(process,modules,sizeof(modules),&bytes,LIST_MODULES_64BIT)||bytes>sizeof(modules)){std::fprintf(stderr,"Module enumeration unavailable\n");return;}
    for(DWORD i=0;i<bytes/sizeof(HMODULE);++i){
        wchar_t file[32768]={};if(!GetModuleFileNameExW(process,modules[i],file,32768))continue;
        const wchar_t *name=wcsrchr(file,L'\\');if(!name)continue;++name;
        for(const wchar_t *wanted:{L"explorerframe.dll",L"windows.storage.dll",L"comctl32.dll",L"shell32.dll",L"user32.dll",L"win32u.dll",L"ntdll.dll",L"kernelbase.dll",L"kernel32.dll",L"rpcrt4.dll",L"combase.dll",L"shcore.dll",L"ucrtbase.dll",L"FileSpacer.exe",L"FileSpacerFolder.dll",L"DesktopPlacementTests.exe",L"ExplorerObservationTests.exe",L"propsys.dll",L"gdi32full.dll",L"gdi32.dll",L"explorer.exe",L"uxtheme.dll"}){
            if(_wcsicmp(name,wanted)!=0)continue;
            const std::wstring target=directory+L"\\"+name;
            if(!CopyFileW(file,target.c_str(),TRUE)&&GetLastError()!=ERROR_FILE_EXISTS)std::fprintf(stderr,"Image copy failed: %ls error=%lu\n",name,GetLastError());
        }
    }
}
void snapshot(DWORD pid,DWORD thread,const std::wstring &file){
    Handle process(OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_DUP_HANDLE|PROCESS_CREATE_PROCESS,FALSE,pid),&CloseHandle);
    if(!process){std::fprintf(stderr,"SNAPSHOT OpenProcess %lu\n",GetLastError());return;}
    HPSS capture=nullptr;const double began=now();
    const DWORD error=PssCaptureSnapshot(process.get(),static_cast<PSS_CAPTURE_FLAGS>(PSS_CAPTURE_VA_CLONE|PSS_CAPTURE_THREADS|PSS_CAPTURE_THREAD_CONTEXT|PSS_CAPTURE_VA_SPACE|PSS_CAPTURE_VA_SPACE_SECTION_INFORMATION),CONTEXT_ALL,&capture);
    if(error){std::fprintf(stderr,"SNAPSHOT capture %lu\n",error);return;}
    const double captured=now();
    Handle output(CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr),&CloseHandle);
    MINIDUMP_CALLBACK_INFORMATION callback={};
    callback.CallbackRoutine=[](PVOID,const PMINIDUMP_CALLBACK_INPUT in,PMINIDUMP_CALLBACK_OUTPUT out)->BOOL{
        if(in->CallbackType==IsProcessSnapshotCallback)out->Status=S_FALSE;
        return TRUE;
    };
    const BOOL wrote=output.get()!=INVALID_HANDLE_VALUE && MiniDumpWriteDump(reinterpret_cast<HANDLE>(capture),pid,output.get(),static_cast<MINIDUMP_TYPE>(MiniDumpNormal|MiniDumpWithThreadInfo|MiniDumpWithFullMemoryInfo|MiniDumpWithUnloadedModules),nullptr,nullptr,&callback);
    const DWORD dumpError=wrote?ERROR_SUCCESS:GetLastError();
    PssFreeSnapshot(GetCurrentProcess(),capture);
    std::printf("SNAPSHOT file=%ls pid=%lu ui_thread=%lu capture_ms=%.3f total_ms=%.3f error=%lu\n",file.c_str(),pid,thread,captured-began,now()-began,dumpError);std::fflush(stdout);
    if(wrote)++snapshotCount;
    captureImages(process.get());
}
std::atomic<unsigned> receivedEvents[8];
void CALLBACK eventObserved(HWINEVENTHOOK,DWORD event,HWND,LONG,LONG,DWORD,DWORD){
    if(event>=EVENT_OBJECT_REORDER&&event<=EVENT_OBJECT_LOCATIONCHANGE)++receivedEvents[event-EVENT_OBJECT_REORDER];
}
BOOL CALLBACK windowObserved(HWND window,LPARAM context){
    FILE *out=reinterpret_cast<FILE *>(context);wchar_t name[128]={};GetClassNameW(window,name,128);
    std::fwprintf(out,L"%p\t%ls\tstyle=%08llX\n",window,name,static_cast<unsigned long long>(GetWindowLongPtrW(window,GWL_STYLE)));return TRUE;
}
HRESULT measure(Target &t,const char *app,int n,int repetition,const char *operation,const std::function<HRESULT()> &action){
#ifdef FILESPACER_VISUAL_TIMING
    HWND body=nullptr;require(SUCCEEDED(t.shell->GetWindow(&body))&&body,"Visible view unavailable");
    const std::string visualLabel=std::string(app)+"-"+operation+"-"+std::to_string(repetition);
    std::unique_ptr<presentation::Observation> visible;
    if(presentation::pointer)presentation::pointer->park();
    const RECT actualRegion=presentation::measuredRegion(body);presentation::requireUncovered(body,actualRegion);
    static UINT referenceDpi=0;const UINT dpi=GetDpiForWindow(body);if(!referenceDpi)referenceDpi=dpi;require(dpi&&dpi==referenceDpi,"Comparison DPI differs between applications");
    FILE *geometry=nullptr;require(_wfopen_s(&geometry,(presentation::outputRoot+L"\\viewport.csv").c_str(),L"ab")==0,"Viewport log unavailable");
    std::unique_ptr<FILE,decltype(&std::fclose)> geometryFile(geometry,&std::fclose);require(std::fseek(geometry,0,SEEK_END)==0,"Viewport log seek failed");
    if(std::ftell(geometry)==0)std::fputs("app,operation,repetition,left,top,width,height,view_width,view_height,dpi,desktop_region,cursor_x,cursor_y\n",geometry);
    RECT viewBounds={};POINT cursor={};GetWindowRect(body,&viewBounds);GetCursorPos(&cursor);
    std::fprintf(geometry,"%s,%s,%d,%ld,%ld,%ld,%ld,%ld,%ld,%u,%d,%ld,%ld\n",app,operation,repetition,actualRegion.left,actualRegion.top,actualRegion.right-actualRegion.left,actualRegion.bottom-actualRegion.top,viewBounds.right-viewBounds.left,viewBounds.bottom-viewBounds.top,dpi,presentation::desktopRegion?1:0,cursor.x,cursor.y);require(std::fclose(geometryFile.release())==0,"Viewport log close failed");
    if(presentation::enabled)visible=std::make_unique<presentation::Observation>(body,presentation::outputRoot+L"\\"+std::wstring(visualLabel.begin(),visualLabel.end()));
#endif
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,t.processId),&CloseHandle);
    const double before=cpu(process.get());
    std::atomic<bool> stop{false},monitorDone{false},completed{false};double lag=0;unsigned timeouts=0;bool probed=false;
    HWINEVENTHOOK hook=nullptr;std::thread sampler;
    unsigned enumeration=t.sink?t.sink->enumerated:0, changed=t.sink?t.sink->changed:0;
    std::thread monitor([&](){
        while(!stop.load()){
            DWORD_PTR response=0;const double began=now();
            const bool ok=SendMessageTimeoutW(t.window,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&response)!=0;
            const double elapsed=now()-began;lag=(std::max)(lag,elapsed);if(!ok)++timeouts;
            if(elapsed>30 && !probed){probed=true;waitChain(t.threadId,app,operation,elapsed);}
            Sleep(5);
        }
        monitorDone.store(true);
    });
    struct Threads {
        std::atomic<bool> &stop,&done,&completed;std::thread &monitor,&sampler;HWINEVENTHOOK &hook;
        ~Threads(){completed.store(true);stop.store(true);while(monitor.joinable()&&!done.load())pump(5);if(monitor.joinable())monitor.join();if(sampler.joinable())sampler.join();if(hook)UnhookWinEvent(hook);}
    }threads{stop,monitorDone,completed,monitor,sampler,hook};
    for(auto &count:receivedEvents)count.store(0);
    hook=captureEvents?SetWinEventHook(EVENT_OBJECT_REORDER,EVENT_OBJECT_LOCATIONCHANGE,nullptr,eventObserved,t.processId,0,WINEVENT_OUTOFCONTEXT):nullptr;
    require(!captureEvents||hook,"Accessibility observer unavailable");
    const bool observe=captureSnapshots&&n==5000&&repetition==0&&(std::string(operation)=="select_batch"||std::string(operation)=="clear_selection"||std::string(operation)=="position_16");
    if(observe){
        const std::string label=std::string(app)+"-"+operation;
        const std::wstring file=observationRoot+L"\\"+std::wstring(label.begin(),label.end())+L".dmp";
        sampler=std::thread([&,file](){try{Sleep(35);if(!completed.load())snapshot(t.processId,t.threadId,file);}catch(const std::exception &error){std::fprintf(stderr,"Snapshot failed: %s\n",error.what());}});
        FILE *tree=nullptr;const std::wstring treeFile=observationRoot+L"\\"+std::wstring(label.begin(),label.end())+L"-windows.txt";
        if(_wfopen_s(&tree,treeFile.c_str(),L"wb")==0){windowObserved(t.window,reinterpret_cast<LPARAM>(tree));EnumChildWindows(t.window,windowObserved,reinterpret_cast<LPARAM>(tree));std::fclose(tree);}
    }
    const double began=now();
#ifdef FILESPACER_VISUAL_TIMING
    if(visible)visible->begin();
#endif
    HRESULT result=action();const double duration=now()-began;
#ifdef FILESPACER_VISUAL_TIMING
    if(visible)visible->apiReturned();
#endif
    completed.store(true);if(sampler.joinable())sampler.join();
    pump(500);stop.store(true);
    // A controlled native host uses this STA too: serve its last WM_NULL before
    // joining the monitor, rather than creating an artificial 100 ms timeout.
    while(!monitorDone.load())pump(5);
    monitor.join();
    if(hook){UnhookWinEvent(hook);hook=nullptr;}
    std::printf("%s %s %s",captureEvents?"EVENTS":"EVENTS_DISABLED",app,operation);for(auto &count:receivedEvents)std::printf(",%u",count.load());std::printf("\n");std::fflush(stdout);
    int count=-1,selected=-1;t.view->ItemCount(SVGIO_ALLVIEW,&count);t.view->ItemCount(SVGIO_SELECTION,&selected);
    row(app,n,repetition,operation,result,duration,lag,timeouts,cpu(process.get())-before,
        (t.sink?t.sink->enumerated:0)-enumeration,(t.sink?t.sink->changed:0)-changed,count,selected);
#ifdef FILESPACER_VISUAL_TIMING
    if(visible)visible->finish();else if(presentation::pointer)presentation::pointer->verify();
#endif
    return result;
}
void extract(int id,const std::wstring &path){
    HRSRC resource=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);require(!!resource,"Embedded application missing");
    HGLOBAL loaded=LoadResource(nullptr,resource);const void *bytes=LockResource(loaded);DWORD size=SizeofResource(nullptr,resource),written=0;
    Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr),&CloseHandle);
    require(file.get()!=INVALID_HANDLE_VALUE && WriteFile(file.get(),bytes,size,&written,nullptr) && written==size,"Application extraction failed");
}
void launch(const std::wstring &executable,const std::wstring &arguments,const std::wstring &log){
    SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE};
    Handle output(CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr),&CloseHandle);
    Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr),&CloseHandle);
    require(output.get()!=INVALID_HANDLE_VALUE,"Log unavailable");
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> attributes(bytes);
    STARTUPINFOEXW start={};start.StartupInfo.cb=sizeof(start);start.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    start.StartupInfo.hStdInput=input.get();start.StartupInfo.hStdOutput=output.get();start.StartupInfo.hStdError=output.get();
    start.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    require(!!InitializeProcThreadAttributeList(start.lpAttributeList,1,0,&bytes),"Handle list unavailable");
    HANDLE handles[]={input.get(),output.get()};
    require(!!UpdateProcThreadAttribute(start.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr),"Handle list rejected");
    PROCESS_INFORMATION p={};std::wstring command=L"\""+executable+L"\" "+arguments;
    const BOOL launched=CreateProcessW(executable.c_str(),&command[0],nullptr,nullptr,TRUE,
        CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&start.StartupInfo,&p);
    DeleteProcThreadAttributeList(start.lpAttributeList);require(!!launched,"Application launch failed");
    CloseHandle(p.hThread);CloseHandle(p.hProcess);
}
void run(const std::wstring &root,const std::wstring &fixture){
    const std::wstring bin=root+L"\\optimized";require(!!CreateDirectoryW(bin.c_str(),nullptr),"Binary directory unavailable");
    const std::wstring executable=bin+L"\\FileSpacer.exe";
    extract(101,executable);extract(102,bin+L"\\FileSpacerFolder.dll");
    require(_wfopen_s(&csv,(root+L"\\comparison.csv").c_str(),L"wb")==0,"CSV unavailable");
    require(_wfopen_s(&waits,(root+L"\\wait-chains.csv").c_str(),L"wb")==0,"Wait-chain log unavailable");
    std::fputs("app,items,repetition,operation,hresult,call_ms,max_ping_ms,ping_timeouts,cpu_ms,enum_events,change_events,observed_items,observed_selection\n",csv);
    CComPtr<IShellWindows> windows;require(SUCCEEDED(windows.CoCreateInstance(CLSID_ShellWindows)),"ShellWindows unavailable");
    wchar_t system[MAX_PATH]={};GetWindowsDirectoryW(system,MAX_PATH);const std::wstring explorer=std::wstring(system)+L"\\explorer.exe";
    observationRoot=root; for(int n:{5000}){
        const std::wstring path=fixture;
        for(int repetition=0;repetition<3;++repetition)for(int order=0;order<2;++order){
            const bool fs=(repetition%2==0)?order==0:order!=0;const char *app=fs?"FileSpacer-O2-test":"Explorer";
            Target t;double began=now();
            const std::wstring log=root+L"\\launch-"+std::to_wstring(n)+L"-"+std::to_wstring(repetition)+L"-"+(fs?L"fs":L"explorer")+L".log";
            launch(fs?executable:explorer,std::wstring(fs?L"/test ":L"")+L"\""+path+L"\"",log);
            const ULONGLONG deadline=GetTickCount64()+45000;
            while(!t.window && GetTickCount64()<deadline){findView(windows,path,fs,t);pump(10);}
            require(!!t.window,"Folder window did not expose a public Shell view");
            int count=-1;
            while(GetTickCount64()<deadline){t.view->ItemCount(SVGIO_ALLVIEW,&count);if(count==n)break;pump(10);}
            require(count==n,"Expected items not observable before watchdog");
            row(app,n,repetition,"open_to_observable_count",S_OK,now()-began,0,0,0,0,0,count,0);
            t.subscribe();pump(500);
            measure(t,app,n,repetition,"refresh",[&](){return t.shell->Refresh();});
            measure(t,app,n,repetition,"sort_name_desc",[&](){SORTCOLUMN sort={PKEY_ItemNameDisplay,SORT_DESCENDING};return t.view->SetSortColumns(&sort,1);});
            measure(t,app,n,repetition,"sort_size_desc",[&](){SORTCOLUMN sort={PKEY_Size,SORT_DESCENDING};return t.view->SetSortColumns(&sort,1);});
            measure(t,app,n,repetition,"icons_48",[&](){return t.view->SetViewModeAndIconSize(FVM_ICON,48);});
            measure(t,app,n,repetition,"details",[&](){return t.view->SetViewModeAndIconSize(FVM_DETAILS,16);});
            std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;
            std::vector<PCUITEMID_CHILD> ids;std::vector<POINT> points(static_cast<size_t>(n));
            CComPtr<IEnumIDList> items;require(SUCCEEDED(t.view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"Items enumeration unavailable");
            for(int i=0;i<n;++i){CComHeapPtr<ITEMID_CHILD> id;require(items->Next(1,&id,nullptr)==S_OK,"Unexpected short enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
            measure(t,app,n,repetition,"select_batch",[&](){return t.view->SelectAndPositionItems(static_cast<UINT>(ids.size()),ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS);});
            measure(t,app,n,repetition,"clear_selection",[&](){return t.shell->SelectItem(nullptr,SVSI_DESELECTOTHERS);});
            measure(t,app,n,repetition,"background_menu",[&](){
                CComPtr<IContextMenu> menu;HRESULT hr=t.shell->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&menu));
                if(FAILED(hr))return hr;HMENU popup=CreatePopupMenu();if(!popup)return E_OUTOFMEMORY;
                hr=menu->QueryContextMenu(popup,0,1,32767,CMF_NORMAL);DestroyMenu(popup);return hr;
            });
            measure(t,app,n,repetition,"resize",[&](){return SetWindowPos(t.window,nullptr,50,50,850,600,SWP_NOZORDER|SWP_NOACTIVATE)?S_OK:HRESULT_FROM_WIN32(GetLastError());});
            HWND frame=t.window;t.disconnect();began=now();PostMessageW(frame,WM_CLOSE,0,0);
            const ULONGLONG closeDeadline=GetTickCount64()+20000;while(IsWindow(frame)&&GetTickCount64()<closeDeadline)pump(10);
            require(!IsWindow(frame),"Test window did not close");t.window=nullptr;
            row(app,n,repetition,"close_frame",S_OK,now()-began,0,0,0,0,0,0,0);pump(500);
            std::printf("MEASURED %s: %d items, repetition %d\n",app,n,repetition);std::fflush(stdout);
        }
    }
    std::fclose(csv);csv=nullptr;std::fclose(waits);waits=nullptr;
}
struct ViewSpec{const wchar_t *name;FOLDERVIEWMODE mode;int iconSize;constexpr ViewSpec(const wchar_t *label,FOLDERVIEWMODE representation,int size)noexcept:name(label),mode(representation),iconSize(size){}};
constexpr ViewSpec viewSpecs[]={
    {L"extra-large",FVM_ICON,256},{L"large",FVM_ICON,96},{L"medium",FVM_ICON,48},{L"small",FVM_SMALLICON,16},
    {L"list",FVM_LIST,16},{L"details",FVM_DETAILS,16},{L"tiles",FVM_TILE,48},{L"content",FVM_CONTENT,32}};
const ViewSpec *findSpec(const wchar_t *name){for(const auto &spec:viewSpecs)if(std::wcscmp(spec.name,name)==0)return &spec;return nullptr;}
void runSelectionComparison(const std::wstring &root,const std::wstring &fixture,const ViewSpec &spec){
    captureEvents=false;captureSnapshots=false;observationRoot=root;
    require(_wfopen_s(&csv,(root+L"\\"+spec.name+L".csv").c_str(),L"wb")==0,"Selection CSV unavailable");
    require(_wfopen_s(&waits,(root+L"\\wait-chains.csv").c_str(),L"wb")==0,"Wait log unavailable");
    FILE *capabilities=nullptr;require(_wfopen_s(&capabilities,(root+L"\\capabilities.csv").c_str(),L"wb")==0,"Capabilities CSV unavailable");
    std::unique_ptr<FILE,decltype(&std::fclose)> capabilityFile(capabilities,&std::fclose);
    std::fputs("app,repetition,view,status,hresult,requested_mode,actual_mode,requested_size,actual_size,width,height,dpi\n",capabilities);
    std::fputs("app,items,repetition,operation,hresult,call_ms,max_ping_ms,ping_timeouts,cpu_ms,enum_events,change_events,observed_items,observed_selection\n",csv);
    const std::wstring oldDirectory=root+L"\\baseline",newDirectory=root+L"\\prototype";
    require(!!CreateDirectoryW(oldDirectory.c_str(),nullptr)&&!!CreateDirectoryW(newDirectory.c_str(),nullptr),"Binary directories unavailable");
    extract(101,oldDirectory+L"\\FileSpacer.exe");extract(102,oldDirectory+L"\\FileSpacerFolder.dll");
    extract(103,newDirectory+L"\\FileSpacer.exe");extract(104,newDirectory+L"\\FileSpacerFolder.dll");
    wchar_t system[MAX_PATH]={};GetWindowsDirectoryW(system,MAX_PATH);
    const std::wstring executables[]={oldDirectory+L"\\FileSpacer.exe",newDirectory+L"\\FileSpacer.exe",std::wstring(system)+L"\\explorer.exe"};
    const char *labels[]={"FileSpacer-baseline","FileSpacer-hybrid","Explorer"};
    CComPtr<IShellWindows> windows;require(SUCCEEDED(windows.CoCreateInstance(CLSID_ShellWindows)),"ShellWindows unavailable");
    int repetitions=3;
#ifdef FILESPACER_VISUAL_TIMING
    if(presentation::protocolOnly)repetitions=1;
#endif
    for(int repetition=0;repetition<repetitions;++repetition)for(int order=0;order<3;++order){
        const int app=(order+repetition)%3;Target t;
        launch(executables[app],std::wstring(app<2?L"/test ":L"")+L"\""+fixture+L"\"",root+L"\\launch-"+std::to_wstring(app)+L"-"+std::to_wstring(repetition)+L".log");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while(!t.window&&GetTickCount64()<deadline){findView(windows,fixture,app<2,t);pump(10);}
        require(!!t.window,"No registered view");
        int count=0;while(GetTickCount64()<deadline){t.view->ItemCount(SVGIO_ALLVIEW,&count);if(count==5000)break;pump(10);}
        require(count==5000,"Incomplete fixture");
        const HRESULT requested=t.view->SetViewModeAndIconSize(spec.mode,spec.iconSize);pump(500);
        t.disconnect();require(findView(windows,fixture,app<2,t),"Current comparison view unavailable");
        FOLDERVIEWMODE mode=FVM_AUTO;int iconSize=0;const HRESULT readMode=t.view->GetViewModeAndIconSize(&mode,&iconSize);
        const bool supported=SUCCEEDED(requested)&&SUCCEEDED(readMode)&&mode==spec.mode&&iconSize==spec.iconSize;
        if(!supported){
            std::fprintf(capabilities,"%s,%d,%ls,UNSUPPORTED,0x%08lX,%d,%d,%d,%d,0,0,0\n",labels[app],repetition,spec.name,static_cast<unsigned long>(FAILED(requested)?requested:readMode),static_cast<int>(spec.mode),static_cast<int>(mode),spec.iconSize,iconSize);std::fflush(capabilities);
            std::printf("UNSUPPORTED %ls %s mode=%d size=%d requested=%d/%d\n",spec.name,labels[app],static_cast<int>(mode),iconSize,static_cast<int>(spec.mode),spec.iconSize);
            HWND frame=t.window;t.disconnect();PostMessageW(frame,WM_CLOSE,0,0);const ULONGLONG close=GetTickCount64()+20000;while(IsWindow(frame)&&GetTickCount64()<close)pump(10);require(!IsWindow(frame),"Unsupported-view window did not close");t.window=nullptr;continue;
        }
#ifdef FILESPACER_VISUAL_TIMING
        presentation::fitView(t.shell,t.window);
        SetForegroundWindow(t.window);if(presentation::pointer)presentation::pointer->park();
#endif
        SORTCOLUMN sort={PKEY_ItemNameDisplay,SORT_ASCENDING};require(SUCCEEDED(t.view->SetSortColumns(&sort,1)),"Sort rejected");pump(500);
        require(SUCCEEDED(t.view->GetViewModeAndIconSize(&mode,&iconSize))&&mode==spec.mode&&iconSize==spec.iconSize,"Wrong representation");
        HWND body=nullptr;RECT bounds={};require(SUCCEEDED(t.shell->GetWindow(&body))&&GetWindowRect(body,&bounds),"View bounds unavailable");
        std::fprintf(capabilities,"%s,%d,%ls,SUPPORTED,0x00000000,%d,%d,%d,%d,%ld,%ld,%u\n",labels[app],repetition,spec.name,static_cast<int>(spec.mode),static_cast<int>(mode),spec.iconSize,iconSize,bounds.right-bounds.left,bounds.bottom-bounds.top,GetDpiForWindow(body));std::fflush(capabilities);
        std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;
        CComPtr<IEnumIDList> items;require(SUCCEEDED(t.view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"No items");
        for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;require(items->Next(1,&id,nullptr)==S_OK,"Short enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
        t.subscribe();require(SUCCEEDED(t.shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Initial clear failed");pump(500);
        FILE *tree=nullptr;const std::wstring treeFile=root+L"\\windows-"+std::to_wstring(app)+L"-"+std::to_wstring(repetition)+L".txt";
        if(_wfopen_s(&tree,treeFile.c_str(),L"wb")==0){EnumChildWindows(t.window,windowObserved,reinterpret_cast<LPARAM>(tree));std::fclose(tree);}
        std::vector<int> counts={100,500,1000,2500,5000};
#ifdef FILESPACER_VISUAL_TIMING
        if(presentation::protocolOnly)counts={2};
#endif
        for(const int selected:counts){
            const std::string select="select_"+std::to_string(selected),clear="clear_"+std::to_string(selected);
            require(SUCCEEDED(measure(t,labels[app],5000,repetition,select.c_str(),[&](){return t.view->SelectAndPositionItems(selected,ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS);})),"Selection failed");
            int actual=-1;require(SUCCEEDED(t.view->ItemCount(SVGIO_SELECTION,&actual))&&actual==selected,"Wrong selected count");
            require(SUCCEEDED(measure(t,labels[app],5000,repetition,clear.c_str(),[&](){return t.shell->SelectItem(nullptr,SVSI_DESELECTOTHERS);})),"Clear failed");
            require(SUCCEEDED(t.view->ItemCount(SVGIO_SELECTION,&actual))&&actual==0,"Selection not cleared");
            require(SUCCEEDED(t.view->GetViewModeAndIconSize(&mode,&iconSize))&&mode==spec.mode&&iconSize==spec.iconSize,"Representation changed during measurement");
        }
        HWND frame=t.window;t.disconnect();PostMessageW(frame,WM_CLOSE,0,0);
        const ULONGLONG closeDeadline=GetTickCount64()+20000;while(IsWindow(frame)&&GetTickCount64()<closeDeadline)pump(10);
        require(!IsWindow(frame),"Window not closed");t.window=nullptr;pump(500);
        std::printf("VIEW %ls %s repetition %d mode=%d size=%d verified\n",spec.name,labels[app],repetition,static_cast<int>(mode),iconSize);std::fflush(stdout);
    }
    std::fclose(csv);csv=nullptr;std::fclose(waits);waits=nullptr;
}
}
int wmain(int argc,wchar_t **argv){
    if((argc!=3&&argc!=4) || FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    captureEvents=argc==3||std::wcscmp(argv[3],L"no-events")!=0;
    QueryPerformanceFrequency(&frequency);int result=0;
    try{const ViewSpec *spec=argc==4?findSpec(argv[3]):nullptr;if(spec)runSelectionComparison(argv[1],argv[2],*spec);else{run(argv[1],argv[2]);require(snapshotCount.load()>=2,"Insufficient process snapshots");}}
    catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());result=1;}
    if(csv)std::fclose(csv);if(waits)std::fclose(waits);CoUninitialize();return result;
}
