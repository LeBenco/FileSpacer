// Real in-process Shell probe; public MSAA/IAccessibleEx provider access only.
#define wmain positioningProbeMain
#include "PlacementTests.cpp"
#undef wmain
#include <UIAutomation.h>
#include <oleacc.h>
#include <cmath>
#include <cstring>
#include <limits>
#pragma comment(lib,"Oleacc.lib")

namespace {
HRESULT scrollPattern(IAccessible *accessible,LONG child,IScrollProvider **out){
    *out=nullptr;
    CComQIPtr<IServiceProvider> services(accessible);if(!services)return E_NOINTERFACE;
    CComPtr<IAccessibleEx> extended;
    HRESULT result=services->QueryService(__uuidof(IAccessibleEx),IID_PPV_ARGS(&extended));
    if(FAILED(result)||!extended)return FAILED(result)?result:E_NOINTERFACE;
    if(child!=CHILDID_SELF){CComPtr<IAccessibleEx> item;result=extended->GetObjectForChild(child,&item);if(FAILED(result)||!item)return FAILED(result)?result:E_NOINTERFACE;extended=item;}
    CComQIPtr<IRawElementProviderSimple> provider(extended);if(!provider)return E_NOINTERFACE;
    CComPtr<IUnknown> pattern;result=provider->GetPatternProvider(UIA_ScrollPatternId,&pattern);
    return FAILED(result)?result:pattern?pattern->QueryInterface(IID_PPV_ARGS(out)):E_NOINTERFACE;
}
HRESULT findScroll(IAccessible *accessible,unsigned depth,unsigned &visited,IScrollProvider **out){
    if(depth>8||visited>=128)return E_NOINTERFACE;
    ++visited;
    HRESULT result=scrollPattern(accessible,CHILDID_SELF,out);if(SUCCEEDED(result)&&*out)return result;
    LONG count=0;if(FAILED(accessible->get_accChildCount(&count)))return E_NOINTERFACE;
    for(LONG start=0;start<count&&visited<128;){
        struct Values{VARIANT items[16]={};~Values(){for(auto &item:items)VariantClear(&item);}}values;
        LONG fetched=0;result=AccessibleChildren(accessible,start,count-start<16?count-start:16,values.items,&fetched);
        if(FAILED(result)||fetched<=0)break;
        for(LONG i=0;i<fetched&&visited<128;++i){
            auto &item=values.items[i];
            if(item.vt==VT_DISPATCH&&item.pdispVal){CComQIPtr<IAccessible> nested(item.pdispVal);if(nested)result=findScroll(nested,depth+1,visited,out);}
            else if(item.vt==VT_I4){++visited;result=scrollPattern(accessible,item.lVal,out);}
            if(SUCCEEDED(result)&&*out)return result;
        }
        start+=fetched;
    }
    return E_NOINTERFACE;
}
void probe(const wchar_t *path,FOLDERVIEWOPTIONS profile,FOLDERVIEWMODE mode){
    PlacementHost owner;
    owner.frame=CreateWindowW(L"FileSpacer Placement Prototype",L"Scroll provider",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,650,450,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    need(owner.frame!=nullptr,"No scroll frame");
    RECT bounds={0,0,600,400};FOLDERSETTINGS settings={static_cast<UINT>(mode),FWF_NOWEBVIEW};owner.events.Attach(new Events);
    need(SUCCEEDED(owner.host.initialize(owner.frame,bounds,settings,nullptr,owner.events,profile)),"No host");
    CComPtr<IShellItem> folder;need(SUCCEEDED(SHCreateItemFromParsingName(path,nullptr,IID_PPV_ARGS(&folder)))&&SUCCEEDED(owner.host.navigate(folder,false,true)),"No fixture navigation");
    const ULONGLONG deadline=GetTickCount64()+45000;
    while((!owner.events->navigation||!owner.events->sink||!owner.events->sink->done)&&GetTickCount64()<deadline)pump(10);
    need(owner.events->navigation&&owner.events->sink&&owner.events->sink->done,"No real enumeration completion");
    auto view=owner.view();int count=0;need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW,&count))&&count==5000,"Incomplete fixture");
    CComHeapPtr<ITEMID_CHILD> selected;need(SUCCEEDED(view->Item(7,&selected)),"No selected identity");
    need(SUCCEEDED(owner.events->shell->SelectItem(selected,SVSI_SELECT|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed selection/focus");
    keyboardView=owner.events->shell;owner.events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS);const HWND keyboard=GetFocus();
    HWND window=nullptr;need(SUCCEEDED(owner.events->shell->GetWindow(&window)),"No public view window");
    for(int repetition=0;repetition<3;++repetition){
        const double start=clockMs();CComPtr<IAccessible> accessible;
        HRESULT result=AccessibleObjectFromWindow(window,static_cast<DWORD>(OBJID_CLIENT),IID_PPV_ARGS(&accessible));
        unsigned visited=0;CComPtr<IScrollProvider> scroll;
        if(SUCCEEDED(result)&&accessible)result=findScroll(accessible,0,visited,&scroll);
        const double discover=clockMs()-start;
        std::printf("PROVIDER profile=%u mode=%d rep=%d hr=0x%08lX visited=%u discover_ms=%.3f\n",static_cast<unsigned>(profile),static_cast<int>(mode),repetition,static_cast<unsigned long>(result),visited,discover);std::fflush(stdout);
        if(FAILED(result)&&profile!=FVO_DEFAULT){std::puts("UNSUPPORTED: optional classic in-process ScrollPattern");return;}
        need(SUCCEEDED(result)&&scroll,"Public in-process ScrollPattern unavailable");
        const bool horizontal=mode==FVM_LIST;
        auto read=[&](double *value){return horizontal?scroll->get_HorizontalScrollPercent(value):scroll->get_VerticalScrollPercent(value);};
        auto set=[&](double value){return scroll->SetScrollPercent(horizontal?value:UIA_ScrollPatternNoScroll,horizontal?UIA_ScrollPatternNoScroll:value);};
        double value=-1;const double reading=clockMs();need(SUCCEEDED(read(&value)),"No scroll percentage");const double readMs=clockMs()-reading;
        const double writing=clockMs();need(SUCCEEDED(set(50.0)),"Cannot restore percentage");const double writeMs=clockMs()-writing;
        need(SUCCEEDED(read(&value))&&std::abs(value-50.0)<1.0,"Wrong restored percentage");
        int focused=-1;CComHeapPtr<ITEMID_CHILD> actual;DWORD state=0;
        need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count))&&count==1&&SUCCEEDED(view->GetSelectionState(selected,&state))&&(state&SVSI_SELECT),"Selection changed");
        need(SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&actual))&&ILIsEqual(actual,selected)&&GetFocus()==keyboard,"Focus changed");
        std::printf("PASS rep=%d read_ms=%.3f set_ms=%.3f percent=%.8f selection/focus retained\n",repetition,readMs,writeMs,value);std::fflush(stdout);
        std::vector<uint8_t> snapshot;const auto current=[](){return true;};
        const double captureStart=clockMs();need(SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,current,snapshot)),"Production scroll capture failed");const double captureMs=clockMs()-captureStart;
        uint32_t version=0;need(snapshot.size()>=sizeof(version),"Missing snapshot");std::memcpy(&version,snapshot.data(),sizeof(version));need(version==3,"Default view did not capture shared viewport format");
        need(SUCCEEDED(set(0)),"Cannot prepare production restore");
        const auto saved=snapshot;double expected=0;need(SUCCEEDED(set(value))&&SUCCEEDED(read(&expected))&&SUCCEEDED(set(0)),"No native round-trip oracle");const double restoreStart=clockMs();need(SUCCEEDED(filespacer::restoreReopenSelection(owner.events->shell,snapshot,current))&&snapshot.empty(),"Production scroll restore failed");const double restoreMs=clockMs()-restoreStart;
        need(SUCCEEDED(read(&value))&&std::abs(value-expected)<0.000001,"Production restore lost scroll");
        need(SUCCEEDED(view->GetSelectionState(selected,&state))&&(state&SVSI_SELECT)&&GetFocus()==keyboard,"Production restore changed selection/focus");
        std::printf("CORE rep=%d capture_ms=%.3f restore_ms=%.3f format=%u percent=%.8f\n",repetition,captureMs,restoreMs,version,value);std::fflush(stdout);
        need(SUCCEEDED(set(0)),"Cannot reset percentage");
        auto cancelled=saved;unsigned checks=0;
        need(filespacer::restoreReopenSelection(owner.events->shell,cancelled,[&](){return ++checks<20;})==E_ABORT&&cancelled.empty(),"Cancellation not propagated");
        need(SUCCEEDED(read(&value))&&value==0&&GetFocus()==keyboard,"Cancelled restore changed view");
        // Version 3 retains the 20-byte header, then the shared 48-byte viewport.
        need(saved.size()>=68,"Incomplete version-3 fixture");
        auto invalid=saved;invalid.resize(28);
        need(FAILED(filespacer::restoreReopenSelection(owner.events->shell,invalid,current))&&invalid.empty(),"Truncated percentage accepted");
        invalid=saved;const double nan=std::numeric_limits<double>::quiet_NaN();std::memcpy(invalid.data()+52,&nan,sizeof(nan));
        need(FAILED(filespacer::restoreReopenSelection(owner.events->shell,invalid,current))&&invalid.empty(),"Nonfinite percentage accepted");
        need(SUCCEEDED(read(&value))&&value==0&&SUCCEEDED(view->GetSelectionState(selected,&state))&&(state&SVSI_SELECT)&&GetFocus()==keyboard,"Malformed snapshot changed view");
    }
    std::vector<uint8_t> closing;need(SUCCEEDED(filespacer::captureReopenSelection(owner.events->shell,[](){return true;},closing)),"No closing snapshot");
    unsigned checks=0;bool destroyed=false;
    const HRESULT closed=filespacer::restoreReopenSelection(owner.events->shell,closing,[&](){if(++checks==20){owner.host.destroy();destroyed=true;}return !destroyed;});
    need(destroyed&&closed==E_ABORT&&closing.empty()&&!IsWindow(window),"Closing generation was not rejected");
    std::puts("PASS production capture/restore, cancellation and destroyed view generation");std::fflush(stdout);
}
}
int wmain(int argc,wchar_t **argv){
    if((argc!=3&&argc!=4)||FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Placement Prototype";int result=0;
    if(!RegisterClassW(&type)){CoUninitialize();return 2;}
    try{probe(argv[2],argc==4&&std::wcscmp(argv[3],L"classic")==0?static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING):FVO_DEFAULT,argc==4&&std::wcscmp(argv[3],L"list")==0?FVM_LIST:FVM_DETAILS);}catch(const std::exception &error){std::fprintf(stderr,"FAIL scroll provider: %s\n",error.what());result=1;}
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
