// Diagnostic only: child-control messages and command IDs are not product APIs.
#define FILESPACER_SELECTION_PROBE_MAIN selectionStrategiesProbeMain
#include "SelectionStrategiesTests.cpp"
#undef FILESPACER_SELECTION_PROBE_MAIN
#include <commctrl.h>

namespace {
// Deliberately private, process-local diagnosis. The driver pins the guest DLL hash.
// On that x64 image the entry begins 40 53 (neutral REX prefix + push rbx).
// Breaking on the prefix leaves the equivalent 53 instruction to execute.
struct ComparisonCounter {
    static ComparisonCounter *instance;
    unsigned char *entry=nullptr;PVOID handler=nullptr;DWORD protection=0,thread=0;
    bool patched=false,measuring=false;unsigned long long comparisons=0;
    static LONG WINAPI exception(EXCEPTION_POINTERS *information){
        auto *self=instance;
        if(self&&information->ExceptionRecord->ExceptionCode==EXCEPTION_BREAKPOINT
                &&information->ExceptionRecord->ExceptionAddress==self->entry){
            if(GetCurrentThreadId()==self->thread&&self->measuring)++self->comparisons;
            information->ContextRecord->Rip=reinterpret_cast<DWORD64>(self->entry+1);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    void install(){
        wchar_t computer[MAX_COMPUTERNAME_LENGTH+1]={};DWORD length=MAX_COMPUTERNAME_LENGTH+1;
        need(GetComputerNameW(computer,&length)&&_wcsicmp(computer,L"AGENTWIN10")==0,"Private comparison instrumentation requires AgentWin10");
        const auto module=GetModuleHandleW(L"shell32.dll");need(module!=nullptr,"No diagnostic Shell module");
        entry=reinterpret_cast<unsigned char *>(module)+0x450ad0;
        const unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0x05};
        need(std::memcmp(entry,expected,sizeof(expected))==0,"Private diagnostic entry does not match validated image");
        thread=GetCurrentThreadId();instance=this;handler=AddVectoredExceptionHandler(1,exception);need(handler!=nullptr,"No comparison breakpoint handler");
        need(VirtualProtect(entry,1,PAGE_EXECUTE_READWRITE,&protection)!=FALSE,"Cannot install process-local breakpoint");
        *entry=0xcc;patched=true;need(FlushInstructionCache(GetCurrentProcess(),entry,1)!=FALSE,"Cannot flush diagnostic breakpoint");
    }
    ~ComparisonCounter(){
        if(patched){*entry=0x40;FlushInstructionCache(GetCurrentProcess(),entry,1);DWORD ignored=0;VirtualProtect(entry,1,protection,&ignored);}
        if(handler)RemoveVectoredExceptionHandler(handler);if(instance==this)instance=nullptr;
    }
};
ComparisonCounter *ComparisonCounter::instance=nullptr;
struct DiagnosticCodePatch {
    unsigned char *entry=nullptr;unsigned char original[8]={};size_t size=0;DWORD protection=0;
    void apply(size_t offset,const unsigned char *expected,size_t expectedSize,const unsigned char *replacement,size_t replacementSize){
        wchar_t computer[MAX_COMPUTERNAME_LENGTH+1]={};DWORD length=MAX_COMPUTERNAME_LENGTH+1;
        need(GetComputerNameW(computer,&length)&&_wcsicmp(computer,L"AGENTWIN10")==0,"Private causal probe requires AgentWin10");
        const auto module=GetModuleHandleW(L"shell32.dll");need(module!=nullptr,"No private causal Shell module");
        auto *address=reinterpret_cast<unsigned char *>(module)+offset;
        need(replacementSize<=sizeof(original)&&std::memcmp(address,expected,expectedSize)==0,"Private causal entry does not match validated image");
        need(VirtualProtect(address,replacementSize,PAGE_EXECUTE_READWRITE,&protection)!=FALSE,"Cannot install private causal probe");
        std::memcpy(original,address,replacementSize);entry=address;size=replacementSize;
        std::memcpy(entry,replacement,size);need(FlushInstructionCache(GetCurrentProcess(),entry,size)!=FALSE,"Cannot flush private causal probe");
    }
    void restore(){if(entry){std::memcpy(entry,original,size);FlushInstructionCache(GetCurrentProcess(),entry,size);DWORD ignored=0;VirtualProtect(entry,size,protection,&ignored);entry=nullptr;}}
    ~DiagnosticCodePatch(){restore();}
};
struct NotificationProbe {
    HWND window=nullptr;bool measuring=false;unsigned changed=0,changing=0,ranges=0,draws=0;
    double notificationMs=0;
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR,DWORD_PTR data){
        auto &self=*reinterpret_cast<NotificationProbe *>(data);
        bool selection=false;
        if(self.measuring&&message==WM_NOTIFY&&lParam){
            const auto &notice=*reinterpret_cast<NMHDR *>(lParam);
            if(notice.code==LVN_ITEMCHANGED){++self.changed;selection=true;}
            else if(notice.code==LVN_ITEMCHANGING)++self.changing;
            else if(notice.code==LVN_ODSTATECHANGED){++self.ranges;selection=true;}
            else if(notice.code==NM_CUSTOMDRAW)++self.draws;
        }
        const double began=selection?clockMs():0;
        const LRESULT result=DefSubclassProc(window,message,wParam,lParam);
        if(selection)self.notificationMs+=clockMs()-began;
        return result;
    }
    explicit NotificationProbe(HWND target):window(target){need(SetWindowSubclass(window,procedure,1,reinterpret_cast<DWORD_PTR>(this))!=FALSE,"Cannot install diagnostic notification probe");}
    ~NotificationProbe(){RemoveWindowSubclass(window,procedure,1);}
    void begin(){changed=changing=ranges=draws=0;notificationMs=0;measuring=true;}
};
BOOL CALLBACK findList(HWND window,LPARAM data){
    wchar_t name[128]={};GetClassNameW(window,name,128);
    if(std::wcscmp(name,WC_LISTVIEWW)==0){*reinterpret_cast<HWND *>(data)=window;return FALSE;}return TRUE;
}
void implementation(IUnknown *object,unsigned slot,const char *label){
    const auto address=(*reinterpret_cast<void ***>(object))[slot];HMODULE module=nullptr;wchar_t path[32768]={};
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&module)&&GetModuleFileNameW(module,path,32768))
        std::printf("IMPLEMENTATION %s module=%ls rva=0x%llX\n",label,path,static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address)-reinterpret_cast<uintptr_t>(module)));
}
void investigate(const wchar_t *path,const wchar_t *modeName,const wchar_t *strategy,unsigned options,int repetition){
    const bool countComparisons=std::wcscmp(strategy,L"comparison-counts")==0;
    const int selected=countComparisons?repetition:5000;need(selected>0&&selected<=5000,"Invalid diagnostic cardinality");
    const FOLDERVIEWMODE mode=std::wcscmp(modeName,L"content")==0?FVM_CONTENT:std::wcscmp(modeName,L"tiles")==0?FVM_TILE:std::wcscmp(modeName,L"small")==0?FVM_SMALLICON:FVM_ICON;
    const int size=std::wcscmp(modeName,L"content")==0?32:std::wcscmp(modeName,L"small")==0?16:std::wcscmp(modeName,L"medium")==0||std::wcscmp(modeName,L"tiles")==0?48:std::wcscmp(modeName,L"large")==0?96:256;
    SelectionHost owner;owner.create(path,static_cast<FOLDERVIEWOPTIONS>(options),false);auto view=owner.view();
    need(SUCCEEDED(view->SetViewModeAndIconSize(mode,size)),"Requested mode unavailable");pump(200);
    FOLDERVIEWMODE actualMode=FVM_AUTO;int actualSize=0;need(SUCCEEDED(view->GetViewModeAndIconSize(&actualMode,&actualSize))&&actualMode==mode&&actualSize==size,"Representation mismatch");
    HWND body=nullptr,list=nullptr;need(SUCCEEDED(owner.events->shell->GetWindow(&body)),"No Shell window");EnumChildWindows(body,findList,reinterpret_cast<LPARAM>(&list));
    implementation(view,16,"IFolderView::SelectAndPositionItems");implementation(owner.events->shell,14,"IShellView::SelectItem");
    std::printf("INTERNAL view=%ls strategy=%ls profile=%u list=%p style=0x%08lX ownerdata=%d\n",modeName,strategy,options,list,list?static_cast<unsigned long>(GetWindowLongPtrW(list,GWL_STYLE)):0,list&&(GetWindowLongPtrW(list,GWL_STYLE)&LVS_OWNERDATA)?1:0);
    if((std::wcscmp(strategy,L"raw-list")==0||std::wcscmp(strategy,L"raw-items")==0)&&!list){std::puts("UNSUPPORTED raw-list: no ListView; no substitute protocol attempted");return;}
    CComQIPtr<IShellFolderView> bulk(owner.events->shell);
    if(std::wcscmp(strategy,L"view-bulk")==0&&!bulk){std::puts("UNSUPPORTED IShellFolderView");return;}
    CComPtr<IAccessible> accessible;
    if(std::wcscmp(strategy,L"accessible-items")==0||std::wcscmp(strategy,L"accessible-range")==0){
        if(!list){std::puts("UNSUPPORTED MSAA ListView diagnostic");return;}
        need(SUCCEEDED(AccessibleObjectFromWindow(list,static_cast<DWORD>(OBJID_CLIENT),IID_PPV_ARGS(&accessible))),"No diagnostic ListView accessibility object");
        LONG children=0;need(SUCCEEDED(accessible->get_accChildCount(&children))&&children>=5000,"Missing accessible items");
    }
    std::vector<std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>> owned;std::vector<PCUITEMID_CHILD> ids;
    CComPtr<IEnumIDList> items;need(SUCCEEDED(view->Items(SVGIO_ALLVIEW,IID_PPV_ARGS(&items))),"No diagnostic identities");
    for(int i=0;i<5000;++i){CComHeapPtr<ITEMID_CHILD> id;need(items->Next(1,&id,nullptr)==S_OK,"Short diagnostic enumeration");ids.push_back(id);owned.emplace_back(id.Detach(),&CoTaskMemFree);}
    std::vector<POINT> positions;
    const bool spatial=(options&FVO_CUSTOMPOSITION)!=0&&(mode==FVM_ICON||mode==FVM_SMALLICON)&&std::wcscmp(strategy,L"auto-batch")!=0;
    if(spatial){need(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0))&&view->GetAutoArrange()==S_FALSE,"Cannot enable free positions");
        for(size_t i=0;i<ids.size();++i)positions.push_back(POINT{30+static_cast<LONG>(i%50)*300,30+static_cast<LONG>(i/50)*300});
        need(SUCCEEDED(filespacer::restoreIconPositions(view,positionBlob(ids,positions),[](){return true;})),"Cannot seed diagnostic positions");}
    need(SUCCEEDED(owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot clear diagnostic input");
    POINT firstPosition={};const HRESULT positioned=view->GetItemPosition(ids[0],&firstPosition);
    const HRESULT autoArrange=view->GetAutoArrange();
    std::printf("ARRANGEMENT auto_hr=0x%08lX position_hr=0x%08lX seeded_free=%d\n",static_cast<unsigned long>(autoArrange),static_cast<unsigned long>(positioned),spatial?1:0);
    if(std::wcscmp(strategy,L"auto-batch")==0)need(autoArrange==S_OK,"Automatic arrangement was not retained");
    need(SUCCEEDED(owner.events->shell->SelectItem(ids[7],SVSI_FOCUSED|SVSI_NOTAKEFOCUS)),"Cannot seed diagnostic focus");pump(100);
    const HWND keyboard=GetFocus();NotificationProbe probe(body);ComparisonCounter counter;if(countComparisons)counter.install();
    for(int operation=0;operation<2;++operation){
        // Causal ablation only. New-item membership checks are redundant from empty;
        // the native global clear still performs its final ClearSelection afterwards.
        DiagnosticCodePatch causal;
        if(std::wcscmp(strategy,L"private-bypass")==0){
            const unsigned char selectedEntry[]={0x48,0x83,0xec,0x28},deselectEntry[]={0x48,0x89,0x5c,0x24,0x08};
            const unsigned char returnFalse[]={0x33,0xc0,0xc3},returnVoid[]={0xc3};
            causal.apply(operation?0x44cc90:0x44e4ac,operation?deselectEntry:selectedEntry,operation?sizeof(deselectEntry):sizeof(selectedEntry),operation?returnVoid:returnFalse,operation?sizeof(returnVoid):sizeof(returnFalse));
        }
        probe.begin();counter.comparisons=0;counter.measuring=countComparisons;const double began=clockMs();HRESULT result=S_OK;
        if(std::wcscmp(strategy,L"raw-items")==0){LVITEMW state={};state.stateMask=LVIS_SELECTED;state.state=operation?0:LVIS_SELECTED;
            for(int i=0;i<5000&&SUCCEEDED(result);++i)result=SendMessageW(list,LVM_SETITEMSTATE,i,reinterpret_cast<LPARAM>(&state))?S_OK:E_FAIL;
        }
        else if(std::wcscmp(strategy,L"accessible-items")==0){VARIANT child={};child.vt=VT_I4;
            for(LONG i=1;i<=5000&&SUCCEEDED(result);++i){child.lVal=i;result=accessible->accSelect(operation?SELFLAG_REMOVESELECTION:SELFLAG_ADDSELECTION,child);}
        }
        else if(std::wcscmp(strategy,L"accessible-range")==0){VARIANT child={};child.vt=VT_I4;child.lVal=1;
            result=accessible->accSelect(SELFLAG_TAKEFOCUS,child);
            if(SUCCEEDED(result)){child.lVal=5000;result=accessible->accSelect(SELFLAG_EXTENDSELECTION|(operation?SELFLAG_REMOVESELECTION:SELFLAG_ADDSELECTION),child);}
        }
        else if(std::wcscmp(strategy,L"raw-list")==0){LVITEMW state={};state.stateMask=LVIS_SELECTED;state.state=operation?0:LVIS_SELECTED;result=SendMessageW(list,LVM_SETITEMSTATE,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(&state))?S_OK:E_FAIL;}
        // Observed legacy DefView command IDs; deliberately outside the product.
        else if(std::wcscmp(strategy,L"native-command")==0)SendMessageW(body,WM_COMMAND,operation?0x7022:0x7021,0);
        else if(std::wcscmp(strategy,L"view-bulk")==0)result=bulk->Select(operation?SFVS_SELECT_NONE:SFVS_SELECT_ALLITEMS);
        else if(std::wcscmp(strategy,L"index-sentinel")==0)result=view->SelectItem(-1,operation?SVSI_DESELECTOTHERS:SVSI_SELECT|SVSI_NOTAKEFOCUS);
        else if(operation&&std::wcscmp(strategy,L"batch-deselect")==0)result=view->SelectAndPositionItems(5000,ids.data(),nullptr,SVSI_NOTAKEFOCUS);
        else if(operation&&(std::wcscmp(strategy,L"selection-array")==0||std::wcscmp(strategy,L"selection-array-reverse")==0)){
            CComPtr<IShellItemArray> selection;result=view->GetSelection(FALSE,&selection);
            for(unsigned i=0;SUCCEEDED(result)&&i<5000;++i){CComPtr<IShellItem> item;CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute;
                result=selection->GetItemAt(std::wcscmp(strategy,L"selection-array-reverse")==0?4999-i:i,&item);
                if(SUCCEEDED(result))result=SHGetIDListFromObject(item,&absolute);
                if(SUCCEEDED(result))result=owner.events->shell->SelectItem(ILFindLastID(absolute),SVSI_NOTAKEFOCUS);
            }
        }
        else if(operation)result=owner.events->shell->SelectItem(nullptr,SVSI_DESELECTOTHERS);
        else result=view->SelectAndPositionItems(static_cast<UINT>(selected),ids.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS);
        const double duration=clockMs()-began;counter.measuring=false;probe.measuring=false;causal.restore();
        if(countComparisons)std::printf("COMPARISONS profile=%u selected=%d operation=%s calls=%llu; breakpoint timings are not benchmark results\n",options,selected,operation?"clear":"select",counter.comparisons);
        std::printf("DIAGNOSTIC view=%ls strategy=%ls profile=%u repetition=%d operation=%s ms=%.3f hr=0x%08lX changed=%u changing=%u ranges=%u draw=%u notify_ms=%.3f\n",modeName,strategy,options,repetition,operation?"clear":"select",duration,static_cast<unsigned long>(result),probe.changed,probe.changing,probe.ranges,probe.draws,probe.notificationMs);std::fflush(stdout);
        if(FAILED(result)){std::puts("UNSUPPORTED candidate HRESULT; no product change inferred");return;}
        pump(0);int count=-1;need(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count)),"No diagnostic selection count");
        if(count!=(operation?0:selected)){std::printf("REJECTED inconsistent selection count=%d expected=%d\n",count,operation?0:selected);return;}
        verifySelection(view,ids,operation?0:selected);
        CComPtr<IShellItemArray> selectedArray;DWORD arrayCount=0;const HRESULT selectionResult=view->GetSelection(FALSE,&selectedArray);
        const HRESULT counted=selectedArray?selectedArray->GetCount(&arrayCount):S_OK;
        std::printf("SELECTION_ARRAY hr=0x%08lX present=%d count=%lu visible=%d\n",static_cast<unsigned long>(selectionResult),selectedArray?1:0,arrayCount,count);std::fflush(stdout);
        const bool emptyMissing=selectionResult==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)&&!selectedArray&&count==0;
        need((SUCCEEDED(selectionResult)||emptyMissing)&&SUCCEEDED(counted)&&arrayCount==static_cast<DWORD>(count)
            &&(selectedArray||((selectionResult==S_FALSE||emptyMissing)&&count==0)),"Shell selection array and visible selection differ");
        for(size_t i=0;i<positions.size();++i){POINT actual={};need(SUCCEEDED(view->GetItemPosition(ids[i],&actual))&&actual.x==positions[i].x&&actual.y==positions[i].y,"Diagnostic changed free coordinates");}
        int focused=-1;CComHeapPtr<ITEMID_CHILD> focus;const bool sameFocus=SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focus))&&ILIsEqual(focus,ids[7]);
        std::printf("INVARIANTS selected=%d identities=PASS positions=%zu focus=%d keyboard=%d\n",count,positions.size(),sameFocus?1:0,GetFocus()==keyboard?1:0);std::fflush(stdout);
    }
}
}
int wmain(int argc,wchar_t **argv){
    if(argc!=7||FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    QueryPerformanceFrequency(&frequency);filespacer::initDPI();WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"FileSpacer Placement Prototype";
    if(!RegisterClassW(&type)){CoUninitialize();return 2;}int result=0;
    try{investigate(argv[2],argv[3],argv[4],std::wcstoul(argv[5],nullptr,10),std::wcstol(argv[6],nullptr,10));}catch(const std::exception &error){std::fprintf(stderr,"FAIL diagnostic: %s\n",error.what());result=1;}
    UnregisterClassW(type.lpszClassName,type.hInstance);CoUninitialize();return result;
}
