#include <common.h>
#define FILESPACER_VISUAL_TIMING
#include "DesktopPlacementTests.cpp"

int visualMain(int argc,wchar_t **argv){
    if(argc!=4)return 2;
    presentation::outputRoot=argv[1];
    if(findSpec(argv[3]))return comparisonProbeMain(argc,argv);
    if(std::wcscmp(argv[3],L"api")==0)return desktopProbeMain(argc,argv);
    if(std::wcscmp(argv[3],L"placement-protocol")==0){presentation::enabled=false;wchar_t operation[]=L"api";wchar_t *arguments[]={argv[0],argv[1],argv[2],operation};return desktopProbeMain(4,arguments);}
    if(std::wcscmp(argv[3],L"protocol")==0){
        presentation::enabled=false;presentation::protocolOnly=true;
        for(const auto &spec:viewSpecs){
            std::wstring root=std::wstring(argv[1])+L"\\"+spec.name;require(CreateDirectoryW(root.c_str(),nullptr)!=FALSE,"Protocol output directory unavailable");presentation::outputRoot=root;
            wchar_t *arguments[]={argv[0],&root[0],argv[2],const_cast<wchar_t *>(spec.name)};const int status=comparisonProbeMain(4,arguments);if(status)return status;
        }
        std::puts("PASS eight-view setup protocol: geometry, pointer and selection; no GPU display timing measured");return 0;
    }
    if(std::wcscmp(argv[3],L"self-test")!=0)return 2;
    int result=0;HWND window=nullptr;
    auto deleteBrush=[](HBRUSH value){DeleteObject(value);};
    using Brush=std::unique_ptr<std::remove_pointer<HBRUSH>::type,decltype(deleteBrush)>;
    Brush brush(nullptr,deleteBrush),baseline(nullptr,deleteBrush),changed(nullptr,deleteBrush);
    try{
        std::vector<presentation::Image> history(6);const LONGLONG times[]={10,20,25,30,40,45};const unsigned char colors[]={0,1,2,3,2,2};
        for(size_t i=0;i<history.size();++i){history[i].presented=times[i];history[i].digest[0]=colors[i];}
        auto measured=presentation::boundaries(history,22);require(measured.firstChange==25&&measured.finalImage==40&&!measured.returnedToBaseline,"Frame history did not identify the final continuous image");
        measured=presentation::boundaries(history,41);require(!measured.firstChange&&measured.returnedToBaseline,"Unchanged frames received a false display latency");
        std::puts("PASS frame-history logic (synthetic timestamps; not a GPU validation)");std::fflush(stdout);
        brush.reset(CreateSolidBrush(RGB(0,0,0)));baseline.reset(CreateSolidBrush(RGB(20,50,90)));require(brush!=nullptr&&baseline!=nullptr,"No self-test brush");
        WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Presentation SelfTest";type.hbrBackground=brush.get();require(RegisterClassW(&type)!=0,"No self-test class");
        window=CreateWindowW(type.lpszClassName,L"Presentation timing self-test",WS_POPUP|WS_VISIBLE,50,50,400,250,nullptr,nullptr,type.hInstance,nullptr);require(window!=nullptr,"No self-test window");pump(300);
        presentation::Observation observation(window,presentation::outputRoot+L"\\self-test",[&](){require(SetClassLongPtrW(window,GCLP_HBRBACKGROUND,reinterpret_cast<LONG_PTR>(baseline.get()))!=0&&InvalidateRect(window,nullptr,TRUE),"Initial self-test paint failed");},true);
        changed.reset(CreateSolidBrush(RGB(160,30,80)));require(changed!=nullptr,"No changed brush");
        observation.begin();require(SetClassLongPtrW(window,GCLP_HBRBACKGROUND,reinterpret_cast<LONG_PTR>(changed.get()))!=0&&InvalidateRect(window,nullptr,TRUE),"Self-test paint submission failed");observation.apiReturned();observation.finish();
        require(observation.firstPixels!=observation.lastPixels&&observation.quiet,"Self-test did not observe the new painted image");
        for(size_t i=0;i<observation.lastPixels.size();i+=4)require(observation.lastPixels[i]==80&&observation.lastPixels[i+1]==30&&observation.lastPixels[i+2]==160,"Self-test final pixels do not match the requested paint");
        std::puts("PASS presentation self-test: composed pixels changed and became quiet");
    }catch(const std::exception &error){std::fprintf(stderr,"FAIL presentation self-test: %s\n",error.what());result=1;}
    if(window)DestroyWindow(window);UnregisterClassW(L"FileSpacer Presentation SelfTest",GetModuleHandleW(nullptr));return result;
}
int wmain(int argc,wchar_t **argv){
    try{
        if(!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))throw std::runtime_error("Cannot establish physical-pixel DPI context");
        presentation::PointerParking cursor;presentation::pointer=&cursor;
        struct PointerScope{~PointerScope(){presentation::pointer=nullptr;}}scope;
        return visualMain(argc,argv);
    }catch(const std::exception &error){std::fprintf(stderr,"FAIL visual protocol: %s\n",error.what());return 1;}
}
