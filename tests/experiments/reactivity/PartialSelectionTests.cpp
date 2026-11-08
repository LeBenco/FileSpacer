// Real-Shell validation of the public large partial-selection reader.
#include <common.h>
#define RESTORE_PATH_DRIVER
#include "RestoreAllTests.cpp"
#include "MixedFolder.h"

namespace {
std::vector<std::wstring> partialPaths(IShellItemArray *array) {
    DWORD count=0;need(array&&SUCCEEDED(array->GetCount(&count)),"No array count");
    std::vector<std::wstring> result;
    for(DWORD i=0;i<count;++i){CComPtr<IShellItem> item;need(SUCCEEDED(array->GetItemAt(i,&item))&&item,"No array item");result.push_back(itemPath(item));}
    return result;
}
void partialContract(IShellItemArray *candidate,IShellItemArray *native) {
    const auto wanted=partialPaths(native);need(partialPaths(candidate)==wanted,"Native ordered identities differ");
    SFGAOF actual=0,expected=0;const SFGAOF mask=SFGAO_FILESYSTEM|SFGAO_CANCOPY|SFGAO_CANMOVE|SFGAO_CANDELETE;
    need(SUCCEEDED(candidate->GetAttributes(SIATTRIBFLAGS_AND,mask,&actual))
        &&SUCCEEDED(native->GetAttributes(SIATTRIBFLAGS_AND,mask,&expected))&&actual==expected,"Native attributes differ");
    CComPtr<IDataObject> data;CComPtr<IShellItemArray> roundtrip;
    need(SUCCEEDED(candidate->BindToHandler(nullptr,BHID_DataObject,IID_PPV_ARGS(&data)))&&data
        &&SUCCEEDED(SHCreateShellItemArrayFromDataObject(data,IID_PPV_ARGS(&roundtrip)))
        &&partialPaths(roundtrip)==wanted,"Native data-object round trip differs");
}
void partialSelections(const wchar_t *fixture) {
    wchar_t flag[2]={};
    const bool measurements=GetEnvironmentVariableW(L"FS_PARTIAL_MEASURE_ONLY",flag,_countof(flag))==1&&flag[0]==L'1';
    struct Mode {const char *name;FOLDERVIEWMODE mode;int size;};
    const Mode modes[]={{"medium",FVM_ICON,48},{"small",FVM_SMALLICON,16},{"large",FVM_ICON,96},{"extra-large",FVM_ICON,256}};
    for(const auto &mode:modes){
        RestoreHost owner;owner.createProfile(fixture,FVM_DETAILS,static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING));
        ShowWindow(owner.frame,SW_HIDE);
        CComPtr<IShellView> shell=owner.events->shell;CComQIPtr<IFolderView2> view(shell);CComQIPtr<IShellFolderView> bulk(shell);
        need(view&&bulk&&SUCCEEDED(view->SetViewModeAndIconSize(mode.mode,mode.size))
            &&SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0)),"Cannot set free view");
        restoreEnumeration(shell,owner.events);need(view->GetAutoArrange()==S_FALSE,"Fixture is not free");
        int all=0;need(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW,&all))&&all==5000,"Incomplete selection fixture");
        const auto current=[&]{return !cancelled&&IsWindow(owner.frame)!=FALSE&&owner.events->shell==shell;};
        filespacer::ShellSelectionClear binding;binding.bind(shell,current);need(binding.acceleratesSelectAll(),"No qualified selection setup path");
        std::vector<OwnedId> owned;std::vector<PCUITEMID_CHILD> ids;
        for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(SUCCEEDED(view->Item(i,&id))&&id,"No fixture identity");owned.emplace_back(id.Detach(),&CoTaskMemFree);ids.push_back(owned.back().get());}
        PCUITEMID_CHILD moved[]={ids[0],ids[17],ids[4000],ids[4999]};POINT locations[]={{1900,1800},{5,5},{1200,8300},{6000,9000}};
        need(SUCCEEDED(view->SelectAndPositionItems(4,moved,locations,SVSI_POSITIONITEM|SVSI_NOSTATECHANGE)),"Cannot seed geometry");
        std::vector<POINT> positions;
        for(const auto id:ids){POINT p={};need(SUCCEEDED(view->GetItemPosition(id,&p)),"No original position");positions.push_back(p);}
        const auto prepare=[&](int pattern,int focus){
            need(SUCCEEDED(binding.clear(shell,current)),"Cannot clear setup selection");
            if(pattern==6)need(SUCCEEDED(view->SelectAndPositionItems(1023,ids.data()+3977,nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot seed small subset");
            else need(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Cannot seed full selection");
            std::vector<PCUITEMID_CHILD> excluded;
            for(int i=0;i<5000;++i)if((pattern==1&&i==4999)||(pattern==2&&i>=2500)||(pattern==3&&i%5!=0)
                ||(pattern==4&&i<4488)||(pattern==5&&i%2==0))excluded.push_back(ids[static_cast<size_t>(i)]);
            if(!excluded.empty())need(SUCCEEDED(view->SelectAndPositionItems(static_cast<UINT>(excluded.size()),excluded.data(),nullptr,SVSI_DESELECT|SVSI_NOTAKEFOCUS)),"Cannot seed subset");
            DWORD selected=0;need(SUCCEEDED(view->GetSelectionState(ids[static_cast<size_t>(focus)],&selected))
                &&SUCCEEDED(shell->SelectItem(ids[static_cast<size_t>(focus)],(selected&SVSI_SELECT)|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed focus");
            const int expected[]={5000,4999,2500,1000,512,2500,1023};int actual=0;
            need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&actual))&&actual==expected[pattern],"Incomplete setup selection");
        };
        for(int pattern=0;pattern<6;++pattern)for(const int focus:{7,4000,4999}){
            if(measurements&&(pattern>1||focus!=7))continue;
            const int repetitions=measurements||(pattern==1&&focus==7)?3:1;
            for(int repetition=0;repetition<repetitions;++repetition){
                CComPtr<IShellItemArray> native,candidate;double nativeMs=0,candidateMs=0;
                for(int arm=0;arm<2;++arm){
                    prepare(pattern,focus);const bool reference=(arm==0)==(repetition%2==0);
                    const HWND keyboard=restoreKeyboardFocus(owner.frame);const double began=clockMs();
                    if(reference){need(view->GetSelection(FALSE,&native)==S_OK&&native,"No native array");nativeMs=clockMs()-began;}
                    else{need(filespacer::nativeViewSelection(view,FALSE,&candidate,current)==S_OK&&candidate,"No integrated array");candidateMs=clockMs()-began;}
                    int actual=-1;need(SUCCEEDED(view->GetFocusedItem(&actual))&&actual==focus
                        &&restoreKeyboardFocus(owner.frame)==keyboard,"Capture changed focus");
                }
                CComPtr<IShellItemArray> qualified;
                const HRESULT expected=pattern==3||pattern==4?S_FALSE:S_OK;
                need(filespacer::captureSpatialSelection(view,current,&qualified)==expected
                    &&static_cast<bool>(qualified)==(expected==S_OK),"Unexpected fast-path qualification");
                partialContract(candidate,native);const auto wanted=partialPaths(native);
                need(SUCCEEDED(binding.clear(shell,current))&&partialPaths(candidate)==wanted&&partialPaths(native)==wanted,"Clear changed retained arrays");
                std::printf("PARTIAL mode=%s pattern=%d focus=%d rep=%d count=%zu native_ms=%.3f integrated_ms=%.3f order=1 attributes=1 data_object=1 retained=1 focus=1\n",
                    mode.name,pattern,focus,repetition,wanted.size(),nativeMs,candidateMs);std::fflush(stdout);
            }
        }
        prepare(6,4999);CComPtr<IShellItemArray> rejected,native,candidate;
        need(filespacer::captureSpatialSelection(view,current,&rejected)==S_FALSE&&!rejected
            &&view->GetSelection(FALSE,&native)==S_OK&&filespacer::nativeViewSelection(view,FALSE,&candidate,current)==S_OK,"Small subset fallback failed");
        partialContract(candidate,native);
        prepare(1,7);unsigned checks=0;
        CComPtr<IShellItemArray> calibrated;
        need(filespacer::captureSpatialSelection(view,[&]{++checks;return current();},&calibrated)==S_OK
            &&calibrated&&checks>2,"Cannot calibrate capture boundaries");
        calibrated.Release();unsigned calls=0;
        // Calibrate from the real capture rather than hard-coding its COM call count.
        // Change focus just before the last check; replace membership halfway
        // between the initial and final selected-index enumerations.
        const auto changeFocus=[&]{
            if(++calls==checks-1)need(SUCCEEDED(shell->SelectItem(ids[8],SVSI_SELECT|SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot change verification focus");
            return current();
        };
        int changedFocus=-1;
        need(filespacer::captureSpatialSelection(view,changeFocus,&rejected)==S_FALSE&&!rejected
            &&calls>=checks-1&&SUCCEEDED(view->GetFocusedItem(&changedFocus))&&changedFocus==8,"Late focus change escaped validation");
        candidate.Release();native.Release();
        prepare(1,7);calls=0;
        need(filespacer::nativeViewSelection(view,FALSE,&rejected,[&]{return current()&&++calls<64;})==E_ABORT&&!rejected,"Cancellation fell back to a stale view");
        // Replace a selected identity after enumeration, keeping both count and focus.
        calls=0;const auto mutate=[&]{
            if(++calls==checks/2)need(SUCCEEDED(shell->SelectItem(ids[8],SVSI_DESELECT|SVSI_NOTAKEFOCUS))
                &&SUCCEEDED(shell->SelectItem(ids[4999],SVSI_SELECT|SVSI_NOTAKEFOCUS)),"Cannot replace selected identity");
            return current();
        };
        need(filespacer::captureSpatialSelection(view,mutate,&rejected)==S_FALSE&&!rejected
            &&calls>=checks/2,"Same-count mutation escaped validation");
        prepare(1,7);calls=0;
        need(filespacer::nativeViewSelection(view,FALSE,&candidate,mutate)==S_OK&&view->GetSelection(FALSE,&native)==S_OK,"Mutation fallback failed");
        partialContract(candidate,native);
        prepare(1,7);PCUITEMID_CHILD duplicate=ids[1];POINT at=positions[0];
        need(SUCCEEDED(view->SelectAndPositionItems(1,&duplicate,&at,SVSI_POSITIONITEM|SVSI_NOSTATECHANGE))
            &&filespacer::captureSpatialSelection(view,current,&rejected)==S_FALSE&&!rejected,"Tied coordinates did not fall back");
        at=positions[1];need(SUCCEEDED(view->SelectAndPositionItems(1,&duplicate,&at,SVSI_POSITIONITEM|SVSI_NOSTATECHANGE)),"Cannot restore tied position");
        for(size_t i=0;i<ids.size();++i){POINT p={};need(SUCCEEDED(view->GetItemPosition(ids[i],&p))
            &&p.x==positions[i].x&&p.y==positions[i].y,"Capture changed geometry");}
        need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE,FWF_AUTOARRANGE))
            &&filespacer::captureSpatialSelection(view,current,&rejected)==S_FALSE&&!rejected,"Arranged view did not fall back");
        std::printf("SAFETY mode=%s small_subset=1 cancellation=1 same_count_mutation=1 late_focus_change=1 tied_positions=1 arranged=1 positions=1\n",mode.name);std::fflush(stdout);
    }
}
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=4||FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();headless=true;
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=testWindowProc;type.lpszClassName=L"FileSpacer Placement Prototype";
    if(!RegisterClassW(&type)){CoUninitialize();return 2;}
    int result=0;try{
        filespacer::initializeShellSelectionClear(argv[3],false);
        const ULONGLONG deadline=GetTickCount64()+10000;
        while(!selectionDiscovery::resolved()&&GetTickCount64()<deadline)pump(1);
        need(selectionDiscovery::resolved()&&selectionDiscovery::image().selectAllSupported,"No qualified setup cache; prepare it for the current Shell first");
        partialSelections(argv[2]);
    }
    catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
