// Final bounded measurements; no product optimization is introduced here.
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"
#include "MixedFolder.h"

namespace {
void expose(HWND frame) {
    if(GetForegroundWindow()==frame)return;
    if(SetForegroundWindow(frame)){pump(20);if(GetForegroundWindow()==frame)return;}
    restoreExpose(frame);
}
std::vector<std::wstring> orderedPaths(IShellItemArray *array) {
    DWORD count=0;need(array&&SUCCEEDED(array->GetCount(&count)),"No array count");
    std::vector<std::wstring> result;
    for(DWORD index=0;index<count;++index){CComPtr<IShellItem> item;need(SUCCEEDED(array->GetItemAt(index,&item)),"No array item");result.push_back(itemPath(item));}
    return result;
}
bool itemPoint(IAccessible *object,LONG child,unsigned depth,unsigned &visited,POINT &point) {
    if(depth>8||visited>=128)return false;
    ++visited;CComVariant id(child),role;CComBSTR name;
    if(SUCCEEDED(object->get_accRole(id,&role))&&role.vt==VT_I4&&role.lVal==ROLE_SYSTEM_LISTITEM
            &&SUCCEEDED(object->get_accName(id,&name))&&name
            &&(wcscmp(name,L"File00000.txt")==0||wcscmp(name,L"File00000")==0)){
        LONG x=0,y=0,width=0,height=0;
        if(SUCCEEDED(object->accLocation(&x,&y,&width,&height,id))&&width>0&&height>0){point={x+(std::min)(width/2,24L),y+(std::min)(height/2,24L)};return true;}
    }
    if(child!=CHILDID_SELF)return false;
    LONG count=0;if(FAILED(object->get_accChildCount(&count)))return false;
    for(LONG start=0;start<count&&visited<128;){
        struct Children{VARIANT values[16]={};~Children(){for(auto &value:values)VariantClear(&value);}}children;
        LONG fetched=0;if(FAILED(AccessibleChildren(object,start,(std::min)(count-start,16L),children.values,&fetched))||fetched<=0)break;
        for(LONG index=0;index<fetched;++index){const auto &value=children.values[index];
            if(value.vt==VT_I4&&itemPoint(object,value.lVal,depth+1,visited,point))return true;
            if(value.vt==VT_DISPATCH&&value.pdispVal){CComQIPtr<IAccessible> nested(value.pdispVal);if(nested&&itemPoint(nested,CHILDID_SELF,depth+1,visited,point))return true;}
        }
        start+=fetched;
    }
    return false;
}
bool firstItemHit(IAccessible *hit,const VARIANT &child) {
    CComPtr<IAccessible> current=hit;CComVariant id(child);
    for(unsigned depth=0;current&&depth<6;++depth){
        CComBSTR name;CComVariant role;
        if(SUCCEEDED(current->get_accName(id,&name))&&name
            &&(wcscmp(name,L"File00000.txt")==0||wcscmp(name,L"File00000")==0)
            &&SUCCEEDED(current->get_accRole(id,&role))&&role.vt==VT_I4&&role.lVal==ROLE_SYSTEM_LISTITEM)return true;
        CComPtr<IDispatch> parent;
        if(FAILED(current->get_accParent(&parent))||!parent)return false;
        current.Release();parent->QueryInterface(IID_PPV_ARGS(&current));id=CComVariant(CHILDID_SELF);
    }
    return false;
}
HWND measure(const wchar_t *output,const wchar_t *fixture,bool explorer,bool tiles,bool free) {
    RestoreHost owner;RestoreExplorer native;
    if(explorer)native.open(fixture);
    else owner.createProfile(fixture,FVM_DETAILS,static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING));
    CComPtr<IShellView> shell=explorer?native.shell:owner.events->shell;
    CComQIPtr<IFolderView2> view(shell);CComQIPtr<IShellFolderView> bulk(shell);
    const HWND frame=explorer?native.frame:owner.frame;
    const auto current=[&]{return !cancelled&&!locked&&IsWindow(frame)!=FALSE;};
    need(view&&SUCCEEDED(view->SetViewModeAndIconSize(tiles?FVM_TILE:FVM_ICON,48))
        &&SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,free?0:FWF_AUTOARRANGE)),"Cannot set measurement view");
    const SORTCOLUMN sort={PKEY_ItemNameDisplay,SORT_ASCENDING};need(SUCCEEDED(view->SetSortColumns(&sort,1)),"Cannot sort fixture");
    restoreEnumeration(shell,explorer?native.events.p:owner.events.p);
    if(explorer)presentation::fitView(shell,frame);
    else presentation::fitView(shell,frame,[&]{RECT r={};GetClientRect(frame,&r);need(SUCCEEDED(owner.host.setBounds(r)),"Cannot fit host");});
    expose(frame);
    FOLDERVIEWMODE actual=FVM_AUTO;int size=0,count=0;
    need(SUCCEEDED(view->GetViewModeAndIconSize(&actual,&size))&&actual==(tiles?FVM_TILE:FVM_ICON)&&size==48
        &&SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Unexpected measurement presentation/count");
    need(view->GetAutoArrange()==(free?S_FALSE:S_OK),"Unexpected arrangement");
    filespacer::ShellSelectionClear binding;if(!explorer){binding.bind(shell,current);need(binding.acceleratesSelectAll(),"Qualified setup binding unavailable");}
    std::vector<OwnedId> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<std::wstring> expected;
    for(int index=0;index<count;++index){CComHeapPtr<ITEMID_CHILD> id;need(SUCCEEDED(view->Item(index,&id)),"No fixture identity");owned.emplace_back(id.Detach(),&CoTaskMemFree);ids.push_back(owned.back().get());}
    if(free){PCUITEMID_CHILD moved[]={ids[13],ids[4000]};POINT positions[]={{17,29},{3000,8000}};need(SUCCEEDED(view->SelectAndPositionItems(2,moved,positions,SVSI_POSITIONITEM|SVSI_NOSTATECHANGE)),"Cannot seed irregular geometry");}
    std::vector<POINT> positions;
    if(!explorer)for(const auto id:ids){POINT point={};need(SUCCEEDED(view->GetItemPosition(id,&point)),"No fixture position");positions.push_back(point);}
    for(int index=0;index<count-1;++index){CComPtr<IShellItem> item;need(SUCCEEDED(view->GetItem(index,IID_PPV_ARGS(&item))),"No expected item");expected.push_back(itemPath(item));}
    std::sort(expected.begin(),expected.end());
    const char *app=explorer?"Explorer":"FS-host";
    const char *mode=free?(tiles?"free-tiles":"free-icons"):(tiles?"tiles":"medium");
    for(int repetition=0;repetition<3;++repetition){
        CComPtr<IShellItemArray> arrays[2];
        for(int iteration=0;iteration<(explorer?1:2);++iteration){
            const int arm=explorer?0:(iteration+repetition)%2;
            requireSession();need(SUCCEEDED(explorer?shell->SelectItem(nullptr,SVSI_DESELECTOTHERS):binding.clear(shell,current)),"Cannot reset selection");
            need(SUCCEEDED(restoreExplorerAll(shell,view,frame,bulk))
                &&SUCCEEDED(shell->SelectItem(ids[7],SVSI_SELECT|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed partial selection");
            const HWND keyboard=restoreKeyboardFocus(frame);
            const double change=clockMs();need(SUCCEEDED(view->SelectItem(count-1,SVSI_DESELECT)),"Cannot exclude last item");
            const double began=clockMs();
            const HRESULT result=arm?filespacer::nativeViewSelection(view,FALSE,&arrays[arm],current):view->GetSelection(FALSE,&arrays[arm]);
            const double read=clockMs()-began,total=clockMs()-change;
            need(result==S_OK&&arrays[arm],"No first partial array");
            auto paths=orderedPaths(arrays[arm]);std::sort(paths.begin(),paths.end());
            int selected=0,focus=-1;need(paths==expected&&SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&selected))&&selected==4999
                &&SUCCEEDED(view->GetFocusedItem(&focus))&&focus==7&&restoreKeyboardFocus(frame)==keyboard,"Partial-array identity/focus mismatch");
            std::printf("ARRAY app=%s view=%s rep=%d arm=%s read_ms=%.3f change_read_ms=%.3f selected=%d exact=1\n",app,mode,repetition,arm?"integrated":"native",read,total,selected);std::fflush(stdout);
        }
        if(!explorer)need(orderedPaths(arrays[0])==orderedPaths(arrays[1]),"Partial array order differs");
    }
    if(!explorer){for(size_t index=0;index<ids.size();++index){POINT point={};need(SUCCEEDED(view->GetItemPosition(ids[index],&point))&&point.x==positions[index].x&&point.y==positions[index].y,"Array read changed positions");}return nullptr;}
    for(int repetition=0;repetition<3;++repetition){
        requireSession();expose(frame);
        need(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS))&&SUCCEEDED(restoreExplorerAll(shell,view,frame,bulk))
            &&SUCCEEDED(shell->SelectItem(ids[0],SVSI_SELECT|SVSI_FOCUSED|SVSI_ENSUREVISIBLE|SVSI_NOTAKEFOCUS)),"Cannot seed real click");
        pump(50);expose(frame);
        HWND body=nullptr;need(SUCCEEDED(shell->GetWindow(&body)),"No click view");
        CComPtr<IAccessible> accessible;need(SUCCEEDED(AccessibleObjectFromWindow(body,static_cast<DWORD>(OBJID_CLIENT),IID_PPV_ARGS(&accessible))),"No accessible view");
        POINT point={};unsigned visited=0;
        need(itemPoint(accessible,CHILDID_SELF,0,visited,point),"No first visible item point");
        need(GetAncestor(WindowFromPoint(point),GA_ROOT)==frame,"Explorer item covered");
        CComPtr<IAccessible> hit;CComVariant child;
        const HRESULT hitResult=AccessibleObjectFromPoint(point,&hit,&child);
        need(SUCCEEDED(hitResult)&&hit&&firstItemHit(hit,child),"Explorer hit identity differs");
        need(SetCursorPos(point.x,point.y)!=FALSE,"Cannot place click pointer");
        std::wstring prefix=std::wstring(output)+L"/explorer-"+(tiles?L"tiles-":L"medium-")+std::to_wstring(repetition);
        presentation::Observation observation(body,prefix,{},true);
        need(GetForegroundWindow()==frame&&GetAncestor(WindowFromPoint(point),GA_ROOT)==frame,"Explorer no longer foreground");
        for(const int key:{VK_LBUTTON,VK_RBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU})need(GetAsyncKeyState(key)>=0,"Unexpected held input");
        observation.begin();const LONGLONG sent=presentation::tick();
        INPUT input[2]={};input[0].type=input[1].type=INPUT_MOUSE;input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        need(SendInput(2,input,sizeof(INPUT))==2,"Cannot send click");
        const ULONGLONG deadline=GetTickCount64()+30000;int selected=0;
        while(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&selected))&&selected!=1&&GetTickCount64()<deadline)pump(1);
        need(selected==1,"Click did not reduce selection");
        RECT outer={};need(GetWindowRect(frame,&outer)&&SetCursorPos(outer.left+100,outer.top+10),"Cannot park after click");
        observation.apiReturned();observation.finish();
        const auto images=presentation::boundaries(observation.images,observation.began.load());
        need(images.firstChange&&!images.returnedToBaseline&&observation.quiet&&observation.lost==0&&GetForegroundWindow()==frame,"Invalid click observation");
        CComPtr<IShellItemArray> result;need(view->GetSelection(FALSE,&result)==S_OK,"No clicked identity");
        const auto paths=orderedPaths(result);need(paths.size()==1&&paths[0]==std::wstring(fixture)+L"\\File00000.txt","Clicked wrong identity");
        const double elapsed=static_cast<double>(images.finalImage-sent)*1000.0/static_cast<double>(observation.rate);
        std::printf("CLICK app=Explorer view=%s rep=%d visible_ms=%.3f selected=1 exact=1\n",mode,repetition,elapsed);std::fflush(stdout);
    }
    return frame;
}
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=4||FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=testWindowProc;type.lpszClassName=L"FileSpacer Placement Prototype";
    if(!RegisterClassW(&type)){CoUninitialize();return 2;}
    int result=0;
    try{
        filespacer::initializeShellSelectionClear(argv[3],false);
        const ULONGLONG ready=GetTickCount64()+10000;
        while(!selectionDiscovery::resolved()&&GetTickCount64()<ready)pump(1);
        need(selectionDiscovery::resolved()&&selectionDiscovery::image().selectAllSupported,"No qualified setup cache; prepare it for the current Shell first");
        wchar_t option[2]={};const bool tilesOnly=GetEnvironmentVariableW(L"FS_TAIL_EXPLORER_TILES_ONLY",option,_countof(option))==1&&option[0]==L'1';
        if(!tilesOnly)for(const bool free:{false,true})for(const bool tiles:{false,true})measure(argv[1],argv[2],false,tiles,free);
        for(const bool tiles:{false,true}){
            if(tilesOnly&&!tiles)continue;
            const HWND closed=measure(argv[1],argv[2],true,tiles,false);
            // RestoreExplorer posts WM_CLOSE. Do not rediscover that same
            // fixture window while it is still completing asynchronous close.
            const ULONGLONG deadline=GetTickCount64()+10000;
            while(IsWindow(closed)&&GetTickCount64()<deadline)pump(10);
            need(!IsWindow(closed),"Previous Explorer fixture did not close");
        }
    }catch(const std::exception &error){
        const HWND foreground=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(foreground,&pid);
        wchar_t name[256]={};GetClassNameW(foreground,name,_countof(name));
        std::fprintf(stderr,"FAIL: %s; foreground=%p pid=%lu class=%ls\n",error.what(),static_cast<void*>(foreground),pid,name);result=1;
    }
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
