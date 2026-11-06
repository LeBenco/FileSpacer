// Optional native-child binding investigation; not a production API contract.
#define wmain orderTransferEntry
#include "OrderTransferTests.cpp"
#undef wmain
#include <commctrl.h>
#include <set>
#include <map>

namespace {
struct ListSearch{HWND window=nullptr;unsigned count=0;};
BOOL CALLBACK findList(HWND window,LPARAM value){wchar_t type[64]={};DWORD process=0;if(GetClassNameW(window,type,_countof(type))&&std::wcscmp(type,WC_LISTVIEWW)==0&&GetWindowThreadProcessId(window,&process)==GetCurrentThreadId()&&process==GetCurrentProcessId()){auto &found=*reinterpret_cast<ListSearch *>(value);found.window=window;++found.count;}return TRUE;}
std::string identityKey(PCUITEMID_CHILD id){return std::string(reinterpret_cast<const char *>(id),ILGetSize(id));}
struct Binding{HWND window=nullptr;std::vector<int> indexes;};
Binding bindIndexes(Host &host,const Snapshot &snapshot,bool forcedMissing=false,bool forcedMismatch=false){
    Binding result;if(forcedMissing)return result;auto view=host.view();FOLDERVIEWMODE mode=FVM_AUTO;int size=0,count=0;HWND body=nullptr;
    if(FAILED(view->GetViewModeAndIconSize(&mode,&size))||mode!=FVM_ICON||view->GetAutoArrange()!=S_FALSE||FAILED(view->ItemCount(SVGIO_ALLVIEW,&count))||count!=static_cast<int>(snapshot.ids.size())||FAILED(host.events->shell->GetWindow(&body)))return result;
    ListSearch found;EnumChildWindows(body,findList,reinterpret_cast<LPARAM>(&found));if(found.count!=1||ListView_GetItemCount(found.window)!=count)return result;
    const LONG_PTR style=GetWindowLongPtrW(found.window,GWL_STYLE);std::printf("NATIVE_STYLE owner_data=%d auto_arrange=%d\n",(style&LVS_OWNERDATA)?1:0,(style&LVS_AUTOARRANGE)?1:0);
    if(style&LVS_OWNERDATA){std::puts("UNAVAILABLE: documented positioning messages do not support owner-data controls");return result;}
    CComPtr<IShellFolder> folder;CComPtr<IAccessible> accessible;if(FAILED(view->GetFolder(IID_PPV_ARGS(&folder)))||FAILED(AccessibleObjectFromWindow(found.window,static_cast<DWORD>(OBJID_CLIENT),IID_PPV_ARGS(&accessible))))return result;
    const unsigned generation=host.events->generation;CComPtr<IShellView> source=host.events->shell;std::map<std::string,int> indexes;std::set<std::wstring> labels;
    for(int i=0;i<count;++i){CComHeapPtr<ITEMID_CHILD> id;STRRET text={};CComHeapPtr<wchar_t> name;CComBSTR accessibleName;CComVariant child(static_cast<LONG>(i+1));if(FAILED(view->Item(i,&id))||FAILED(folder->GetDisplayNameOf(id,SHGDN_NORMAL,&text))||FAILED(StrRetToStrW(&text,id,&name))||!name||FAILED(accessible->get_accName(child,&accessibleName))||!accessibleName||std::wstring(name)!=std::wstring(accessibleName,accessibleName.Length())||!labels.emplace(name).second||!indexes.emplace(identityKey(id),i).second||(forcedMismatch&&i==0))return result;if(generation!=host.events->generation||source!=host.events->shell||!IsWindow(found.window))return result;}
    for(const auto id:snapshot.ids){const auto index=indexes.find(identityKey(id));if(index==indexes.end())return Binding{};result.indexes.push_back(index->second);}if(ListView_GetItemCount(found.window)!=count)return Binding{};result.window=found.window;return result;
}
std::vector<POINT> publicPositions(Host &host,const Snapshot &snapshot){auto view=host.view();std::vector<POINT> positions(snapshot.ids.size());for(size_t i=0;i<snapshot.ids.size();++i)require(SUCCEEDED(view->GetItemPosition(snapshot.ids[i],&positions[i])),"Cannot read canonical Shell position");return positions;}
std::vector<POINT> controlPositions(const Binding &binding){std::vector<POINT> result(binding.indexes.size());for(size_t i=0;i<binding.indexes.size();++i)require(ListView_GetItemPosition(binding.window,binding.indexes[i],&result[i])!=FALSE,"Cannot read native control position");return result;}
void equalPositions(const std::vector<POINT> &a,const std::vector<POINT> &b){require(a.size()==b.size(),"Position count differs");for(size_t i=0;i<a.size();++i)require(a[i].x==b[i].x&&a[i].y==b[i].y,"Canonical and native-control positions differ");}
void place(const Binding &binding,const std::vector<POINT> &positions,size_t moved){SendMessageW(binding.window,WM_SETREDRAW,FALSE,0);for(size_t i=0;i<moved;++i)ListView_SetItemPosition32(binding.window,binding.indexes[i],positions[i].x,positions[i].y);SendMessageW(binding.window,WM_SETREDRAW,TRUE,0);RedrawWindow(binding.window,nullptr,nullptr,RDW_ERASE|RDW_FRAME|RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);pump(0);}
void verifySelection(Host &host,const std::vector<std::wstring> &expected){const auto selected=capture(host.view(),SVGIO_SELECTION);auto a=selected.names,b=expected;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());require(a==b,"Positioning changed selected identities");CComPtr<IShellItemArray> array;DWORD count=0;require(SUCCEEDED(host.view()->GetSelection(FALSE,&array))&&array&&SUCCEEDED(array->GetCount(&count))&&count==expected.size(),"Positioning left a stale selection array");}
void probe(const wchar_t *fixture){
    Host host;host.create(fixture,static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION|FVO_CUSTOMORDERING),false);require(SUCCEEDED(host.view()->SetCurrentFolderFlags(FWF_AUTOARRANGE|FWF_SNAPTOGRID,0)),"Cannot enable free positions");host.refresh();auto snapshot=capture(host.view());require(snapshot.ids.size()==5000,"Wrong probe population");
    const double captured=now();const auto original=publicPositions(host,snapshot);const double publicRead=now()-captured;double began=now();const auto binding=bindIndexes(host,snapshot);const double checked=now()-began;
    if(!binding.window){equalPositions(original,publicPositions(host,snapshot));std::puts("NATIVE_PLACEMENT available=0 fallback=existing_public_api no_mutation=PASS candidate=REJECTED");return;}
    began=now();const auto direct=controlPositions(binding);const double directRead=now()-began;equalPositions(original,direct);
    require(!bindIndexes(host,snapshot,true).window&&!bindIndexes(host,snapshot,false,true).window,"Unsupported mapping did not decline");equalPositions(original,publicPositions(host,snapshot));std::printf("BINDING count=5000 check_ms=%.3f public_capture_ms=%.3f direct_capture_ms=%.3f forced_absence=PASS forced_mismatch=PASS\n",checked,publicRead,directRead);
    auto view=host.view();std::vector<PCUITEMID_CHILD> selected={snapshot.ids[0],snapshot.ids[1]};require(SUCCEEDED(view->SelectAndPositionItems(2,selected.data(),nullptr,SVSI_SELECT|SVSI_NOTAKEFOCUS))&&SUCCEEDED(host.events->shell->SelectItem(selected[0],SVSI_SELECT|SVSI_FOCUSED|SVSI_NOTAKEFOCUS))&&SUCCEEDED(host.events->shell->UIActivate(SVUIA_ACTIVATE_FOCUS)),"Cannot seed position invariants");const HWND focus=GetFocus();const auto selection=capture(view,SVGIO_SELECTION).names;
    for(size_t moved:{size_t{16},snapshot.ids.size()}){auto expected=original;for(size_t i=0;i<moved;++i){expected[i].x+=20;expected[i].y+=10;}began=now();place(binding,expected,moved);const double duration=now()-began;equalPositions(expected,publicPositions(host,snapshot));verifySelection(host,selection);require(GetFocus()==focus,"Positioning stole keyboard focus");std::printf("NATIVE_PLACEMENT moved=%zu paint_ms=%.3f canonical_coordinates=5000 selection=PASS keyboard=PASS\n",moved,duration);std::fflush(stdout);
        host.refresh();equalPositions(expected,publicPositions(host,snapshot));verifySelection(host,selection);std::puts("REFRESH canonical_coordinates=PASS selection=PASS");const auto rebound=bindIndexes(host,snapshot);require(rebound.window!=nullptr,"Mapping unavailable after refresh");place(rebound,original,moved);equalPositions(original,publicPositions(host,snapshot));
    }
}
}
int wmain(int argc,wchar_t **argv){if(argc!=2||FAILED(OleInitialize(nullptr)))return 2;QueryPerformanceFrequency(&frequency);INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&controls);WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;type.lpszClassName=L"Order transfer prototype";if(!RegisterClassW(&type)){OleUninitialize();return 2;}int result=0;try{probe(argv[1]);}catch(const std::exception &error){std::fprintf(stderr,"FAIL optional native placement: %s\n",error.what());result=1;}UnregisterClassW(type.lpszClassName,type.hInstance);OleUninitialize();return result;}
