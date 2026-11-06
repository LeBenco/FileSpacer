#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <bcrypt.h>
#include <array>

// Test instrumentation only. LastPresentTime timestamps a composed desktop
// image, not physical scan-out. Final means the last observed quiet ROI image.
namespace presentation {
inline LONGLONG tick(){LARGE_INTEGER value={};QueryPerformanceCounter(&value);return value.QuadPart;}
inline void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
using Digest=std::array<unsigned char,32>;
struct Image {LONGLONG presented=0;UINT accumulated=0;Digest digest={};};
struct Boundaries {LONGLONG firstChange=0,finalImage=0;bool returnedToBaseline=false;};
inline Boundaries boundaries(const std::vector<Image> &images,LONGLONG start){
    check(!images.empty(),"No image history");Boundaries result;Digest initial=images.front().digest;
    for(const auto &image:images)if(image.presented<start)initial=image.digest;
    const Digest final=images.back().digest;result.returnedToBaseline=final==initial;
    for(const auto &image:images)if(image.presented>=start&&image.digest!=initial){result.firstChange=image.presented;break;}
    for(auto i=images.rbegin();i!=images.rend()&&i->digest==final;++i)if(i->presented>=start)result.finalImage=i->presented;
    return result;
}
struct Observation {
    RECT region={};std::wstring prefix;std::vector<Image> images;
    std::vector<unsigned char> firstPixels,lastPixels;std::string failure;
    std::atomic<bool> initialized{false},ready{false},done{false},stop{false};
    std::atomic<LONGLONG> began{0},returned{0};std::thread worker;
    LONGLONG rate=0;UINT width=0,height=0;unsigned lost=0;bool quiet=false;
    Observation(HWND body,const std::wstring &path,const std::function<void()> &initialPaint={}):prefix(path){
        check(GetWindowRect(body,&region)!=FALSE,"Cannot locate visible view");
        MONITORINFO monitor={sizeof(monitor)};check(GetMonitorInfoW(MonitorFromRect(&region,MONITOR_DEFAULTTONULL),&monitor)!=FALSE,"No display work area");
        RECT visible={};check(IntersectRect(&visible,&region,&monitor.rcWork)!=FALSE,"View has no visible work area");region=visible;
        const HWND frame=GetAncestor(body,GA_ROOT);
        for(int y=1;y<=3;++y)for(int x=1;x<=3;++x){const POINT point={region.left+(region.right-region.left)*x/4,region.top+(region.bottom-region.top)*y/4};check(GetAncestor(WindowFromPoint(point),GA_ROOT)==frame,"View is covered by another window; leave the measurement desktop free");}
        LARGE_INTEGER frequency={};QueryPerformanceFrequency(&frequency);rate=frequency.QuadPart;
        worker=std::thread([this](){try{capture();}catch(const std::exception &error){failure=error.what();}done.store(true);ready.store(true);});
        const LONGLONG deadline=tick()+rate*10;
        try{
            bool requested=false;
            while(!ready.load()&&tick()<deadline){
                if(initialized.load()&&!requested){
                    // AcquireNextFrame reports updates, not a guaranteed initial
                    // image of a static desktop. Request a public repaint once.
                    if(initialPaint)initialPaint();else RedrawWindow(body,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
                    requested=true;
                }
                MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(1);
            }
            if(!ready.load()||done.load()){
                stop.store(true);if(worker.joinable())worker.join();
                throw std::runtime_error(failure.empty()?"Desktop capture initialization timed out; captured images="+std::to_string(images.size()):failure);
            }
        }catch(...){stop.store(true);if(worker.joinable())worker.join();throw;}
    }
    ~Observation(){stop.store(true);if(worker.joinable())worker.join();}
    Observation(const Observation&)=delete;Observation &operator=(const Observation&)=delete;
    void capture(){
        const HMONITOR monitor=MonitorFromRect(&region,MONITOR_DEFAULTTONULL);check(monitor!=nullptr,"View is outside the display");
        CComPtr<IDXGIFactory1> factory;check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))),"DXGI factory unavailable");
        CComPtr<IDXGIAdapter1> chosen;CComPtr<IDXGIOutput1> output;DXGI_OUTPUT_DESC description={};
        for(UINT a=0;!output;++a){CComPtr<IDXGIAdapter1> adapter;const HRESULT nextAdapter=factory->EnumAdapters1(a,&adapter);if(nextAdapter==DXGI_ERROR_NOT_FOUND)break;check(SUCCEEDED(nextAdapter)&&adapter,"Cannot enumerate display adapters");
            for(UINT o=0;;++o){CComPtr<IDXGIOutput> candidate;const HRESULT nextOutput=adapter->EnumOutputs(o,&candidate);if(nextOutput==DXGI_ERROR_NOT_FOUND)break;check(SUCCEEDED(nextOutput)&&candidate,"Cannot enumerate display outputs");
                DXGI_OUTPUT_DESC current={};if(SUCCEEDED(candidate->GetDesc(&current))&&current.Monitor==monitor){chosen=adapter;candidate->QueryInterface(IID_PPV_ARGS(&output));description=current;break;}}
        }
        check(output!=nullptr,"Display has no Desktop Duplication output");
        DXGI_ADAPTER_DESC1 adapterDescription={};chosen->GetDesc1(&adapterDescription);
        std::printf("CAPTURE adapter=%ls output=%ls region=%ld,%ld,%ld,%ld\n",adapterDescription.Description,description.DeviceName,region.left,region.top,region.right,region.bottom);std::fflush(stdout);
        check(description.Rotation==DXGI_MODE_ROTATION_IDENTITY,"Rotated display is not supported by this probe");
        const RECT desktop=description.DesktopCoordinates;
        check(region.left>=desktop.left&&region.top>=desktop.top&&region.right<=desktop.right&&region.bottom<=desktop.bottom,"View must fit entirely on one display");
        width=static_cast<UINT>(region.right-region.left);height=static_cast<UINT>(region.bottom-region.top);check(width&&height&&width<=8192&&height<=8192,"Unexpected view dimensions");
        CComPtr<ID3D11Device> device;CComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level={};
        check(SUCCEEDED(D3D11CreateDevice(chosen,D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"D3D11 device unavailable");
        CComPtr<IDXGIOutputDuplication> duplication;const HRESULT duplicated=output->DuplicateOutput(device,&duplication);
        if(FAILED(duplicated)){char text[100]={};sprintf_s(text,"Desktop Duplication unavailable, HRESULT=0x%08lX",static_cast<unsigned long>(duplicated));throw std::runtime_error(text);}
        D3D11_TEXTURE2D_DESC texture={};texture.Width=width;texture.Height=height;texture.MipLevels=texture.ArraySize=1;texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM;texture.SampleDesc.Count=1;texture.Usage=D3D11_USAGE_STAGING;texture.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        CComPtr<ID3D11Texture2D> staging;check(SUCCEEDED(device->CreateTexture2D(&texture,nullptr,&staging)),"Readback texture unavailable");
        BCRYPT_ALG_HANDLE hash=nullptr;check(BCryptOpenAlgorithmProvider(&hash,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA-256 provider unavailable");
        auto closeHash=[](void *value){BCryptCloseAlgorithmProvider(value,0);};std::unique_ptr<void,decltype(closeHash)> algorithm(hash,closeHash);
        initialized.store(true);
        std::vector<unsigned char> pixels(static_cast<size_t>(width)*height*4);LONGLONG changed=tick();Digest previous={};bool haveImage=false;
        while(!stop.load()){
            DXGI_OUTDUPL_FRAME_INFO info={};CComPtr<IDXGIResource> resource;
            const HRESULT acquired=duplication->AcquireNextFrame(20,&info,&resource);
            if(acquired!=DXGI_ERROR_WAIT_TIMEOUT){
                check(SUCCEEDED(acquired),"Desktop duplication was lost during capture");
                struct Frame{IDXGIOutputDuplication *value;~Frame(){value->ReleaseFrame();}}frame{duplication};
                check(!info.ProtectedContentMaskedOut,"Protected content was masked in the captured desktop");
                if(info.LastPresentTime.QuadPart||!haveImage){
                    CComQIPtr<ID3D11Texture2D> source(resource);check(source!=nullptr,"Desktop texture unavailable");
                    const UINT left=static_cast<UINT>(region.left-desktop.left),top=static_cast<UINT>(region.top-desktop.top);
                    const D3D11_BOX box={left,top,0,left+width,top+height,1};context->CopySubresourceRegion(staging,0,0,0,0,source,0,&box);
                    D3D11_MAPPED_SUBRESOURCE mapped={};check(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped)),"Desktop readback failed");
                    for(UINT row=0;row<height;++row)std::memcpy(pixels.data()+static_cast<size_t>(row)*width*4,static_cast<const unsigned char *>(mapped.pData)+static_cast<size_t>(row)*mapped.RowPitch,static_cast<size_t>(width)*4);
                    context->Unmap(staging,0);Image image;image.presented=info.LastPresentTime.QuadPart;image.accumulated=info.AccumulatedFrames;
                    check(BCryptHash(hash,nullptr,0,pixels.data(),static_cast<ULONG>(pixels.size()),image.digest.data(),static_cast<ULONG>(image.digest.size()))>=0,"Frame digest failed");
                    if(!haveImage||image.digest!=previous){changed=tick();previous=image.digest;}
                    if(!haveImage||!began.load()||image.presented<began.load())firstPixels=pixels;
                    haveImage=true;
                    lastPixels=pixels;images.push_back(image);if(began.load()&&info.AccumulatedFrames>1)lost+=info.AccumulatedFrames-1;
                }
            }
            const LONGLONG time=tick();
            if(haveImage&&!ready.load()&&time-changed>=rate/5)ready.store(true);
            const LONGLONG end=returned.load();
            if(end&&time-end>=rate*3/2&&time-changed>=rate/2){quiet=true;break;}
            if(end&&time-end>=rate*5)break;
        }
    }
    void begin(){began.store(tick());}
    void apiReturned(){returned.store(tick());}
    void bitmap(const std::wstring &path,const std::vector<unsigned char> &pixels){
        BITMAPFILEHEADER file={};BITMAPINFOHEADER image={};image.biSize=sizeof(image);image.biWidth=static_cast<LONG>(width);image.biHeight=-static_cast<LONG>(height);image.biPlanes=1;image.biBitCount=32;image.biCompression=BI_RGB;image.biSizeImage=static_cast<DWORD>(pixels.size());file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(image);file.bfSize=file.bfOffBits+image.biSizeImage;
        FILE *out=nullptr;check(_wfopen_s(&out,path.c_str(),L"wb")==0,"Cannot save frame proof");
        const bool wrote=std::fwrite(&file,sizeof(file),1,out)==1&&std::fwrite(&image,sizeof(image),1,out)==1&&std::fwrite(pixels.data(),pixels.size(),1,out)==1;const int closed=std::fclose(out);check(wrote&&closed==0,"Frame proof write failed");
    }
    void finish(){
        while(!done.load()){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(1);}
        if(worker.joinable())worker.join();check(failure.empty(),failure.c_str());check(!images.empty(),"No desktop image observed");
        const LONGLONG start=began.load();check(start&&returned.load(),"Missing operation markers");
        const auto result=boundaries(images,start);const LONGLONG firstChange=result.firstChange,finalImage=result.finalImage;
        const char *quality=!quiet?"UNSTABLE":!firstChange?"NO_VISIBLE_CHANGE":result.returnedToBaseline?"RETURNED_TO_BASELINE":lost?"COALESCED_UPPER_BOUND":"OBSERVED";
        FILE *out=nullptr;check(_wfopen_s(&out,(prefix+L"-presentation.csv").c_str(),L"wb")==0,"Presentation journal unavailable");
        std::fputs("quality,api_ms,first_changed_ms,final_observed_ms,frames,coalesced_frames,observation_ms,width,height\n",out);
        const double scale=1000.0/static_cast<double>(rate);
        std::fprintf(out,"%s,%.3f,",quality,static_cast<double>(returned.load()-start)*scale);
        if(firstChange)std::fprintf(out,"%.3f",static_cast<double>(firstChange-start)*scale);std::fputc(',',out);
        if(firstChange&&!result.returnedToBaseline&&quiet&&finalImage)std::fprintf(out,"%.3f",static_cast<double>(finalImage-start)*scale);
        std::fprintf(out,",%zu,%u,%.3f,%u,%u\n",images.size(),lost,static_cast<double>(tick()-start)*scale,width,height);check(std::fclose(out)==0,"Presentation journal close failed");
        bitmap(prefix+L"-before.bmp",firstPixels);bitmap(prefix+L"-final.bmp",lastPixels);
        check(_wfopen_s(&out,(prefix+L"-frames.csv").c_str(),L"wb")==0,"Frame journal unavailable");std::fputs("relative_present_ms,accumulated_frames,sha256\n",out);
        for(const auto &image:images){if(image.presented)std::fprintf(out,"%.3f",static_cast<double>(image.presented-start)*scale);std::fprintf(out,",%u,",image.accumulated);for(unsigned char value:image.digest)std::fprintf(out,"%02x",static_cast<unsigned>(value));std::fputc('\n',out);}check(std::fclose(out)==0,"Frame journal close failed");
        std::printf("PRESENTATION %ls quality=%s coalesced=%u\n",prefix.c_str(),quality,lost);std::fflush(stdout);
    }
};
static std::wstring outputRoot;
}
