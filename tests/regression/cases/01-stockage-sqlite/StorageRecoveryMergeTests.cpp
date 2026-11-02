#include "FolderStateStore.h"
#include "ShellViewIcons.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <fstream>

using namespace filespacer;
namespace {
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
FolderViewSettings::Key key(uint8_t value) { FolderViewSettings::Key result = {}; result[0] = value; return result; }
FolderViewSettings::Column column(uint8_t value, uint32_t width) { return {key(value), width}; }

void viewChanges() {
    FolderViewSettings saved, baseline;
    saved.present = baseline.present = 31;
    saved.mode = 1; saved.iconSize = 96; saved.flags = 0x80000000u;
    saved.columns = {column(1,220),column(2,330),column(4,440)};
    saved.sort = {{key(2),1},{key(4),-1}}; saved.groupBy = key(2);
    baseline.mode = 4; baseline.iconSize = 16; baseline.flags = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
    baseline.columns = {column(1,100),column(2,110)};
    baseline.sort = {{key(1),1}};
    auto current = baseline;
    auto merged = saved.mergeChanges(baseline,current);
    require(merged.mode == 1 && merged.iconSize == 96 && merged.flags == 0x80000000u,
        "unchanged fallback replaced saved mode/size/flags");
    require(merged.columns.size() == 3 && merged.columns[0].width == 220 && merged.columns[2].width == 440,
        "unchanged or unknown saved columns were replaced");
    require(merged.sort.size() == 2 && merged.sort[0].key == key(2) && merged.groupBy == key(2),
        "unchanged sorting/grouping replaced SQL");
    current.flags &= ~static_cast<uint32_t>(FWF_SNAPTOGRID);
    current.columns[0].width = 150;
    merged = saved.mergeChanges(baseline,current);
    require(merged.flags == 0x80000000u, "changing Snap to grid overwrote the saved Auto arrange/unknown bits");
    auto flagBaseline = baseline, flagCurrent = baseline;
    flagBaseline.flags = flagCurrent.flags = 0;
    flagCurrent.flags = FWF_SNAPTOGRID;
    auto toggled = saved.mergeChanges(flagBaseline,flagCurrent);
    require(toggled.flags == (saved.flags | static_cast<uint32_t>(FWF_SNAPTOGRID)), "Snap bit was not acquired");
    auto flaggedSaved = saved; flaggedSaved.flags |= FWF_AUTOARRANGE;
    flagBaseline.flags = FWF_AUTOARRANGE; flagCurrent.flags = 0;
    toggled = flaggedSaved.mergeChanges(flagBaseline,flagCurrent);
    require(toggled.flags == saved.flags, "Auto arrange bit was not cleared");
    require(merged.columns[0].width == 150 && merged.columns[1].width == 330 && merged.columns[2].width == 440,
        "one resized column overwrote untouched widths");
    current.columns = {column(2,110),column(1,150)};
    merged = saved.mergeChanges(baseline,current);
    require(merged.columns.size() == 3 && merged.columns[0].key == key(2) && merged.columns[0].width == 330
        && merged.columns[1].key == key(1) && merged.columns[1].width == 150 && merged.columns[2].key == key(4),
        "explicit column order or saved-only column preservation");
    auto withNewColumn = baseline;
    withNewColumn.columns.push_back(column(4,160));
    merged = saved.mergeChanges(baseline,withNewColumn,{column(4,160)});
    require(merged.columns[2].width == 440,"showing a column without resizing erased its SQL width");
    withNewColumn.columns[2].width = 250;
    merged = saved.mergeChanges(baseline,withNewColumn,{column(4,160)});
    require(merged.columns[2].width == 250,"resizing a newly shown SQL column lost the user's width");
    auto sentinelBaseline = baseline;
    sentinelBaseline.columns[0].width = 0xffffffffu;
    merged = saved.mergeChanges(sentinelBaseline,baseline);
    require(merged.columns[0].width == 220,"automatic/default width sentinel was treated as a measured baseline");
    merged = saved.mergeChanges(baseline,withNewColumn,{column(4,0xffffffffu)});
    require(merged.columns[2].width == 440,"hidden sentinel authorized replacing the saved width");
    current.mode = 3;
    current.sort[0].direction = -1;
    current.groupBy = key(3); current.groupAscending = true;
    merged = saved.mergeChanges(baseline,current);
    require(merged.mode == 3 && merged.iconSize == 96, "mode change overwrote unchanged icon size");
    require(merged.sort.size() == 1 && merged.sort[0].key == key(1) && merged.sort[0].direction == -1,
        "sort change was spliced into another saved sort chain");
    require(merged.groupBy == key(3) && merged.groupAscending, "group choice not preserved");
    auto unavailable = baseline;
    unavailable.present &= ~(FolderViewSettings::Columns | FolderViewSettings::Sort | FolderViewSettings::Group);
    merged = saved.mergeChanges(unavailable,current);
    require(merged.columns[0].key == key(1) && merged.columns[0].width == 220
        && merged.sort.size() == 2 && merged.groupBy == key(2), "missing baseline authorized unknown field replacement");
    current.present = FolderViewSettings::Mode;
    merged = saved.mergeChanges(baseline,current);
    require(merged.flags == saved.flags && merged.columns[1].width == 330 && merged.sort.size() == 2,
        "missing current getters erased SQL fields");
    FolderViewSettings absent;
    merged = absent.mergeChanges({},baseline);
    require(merged.present == baseline.present && merged.columns[0].width == 100,
        "known absence did not accept the observed new configuration");
    std::cout << "PASS three-way view merge, independent flags, column widths/order/unknown columns, unavailable parts\n";
}

struct Entry { USHORT id; POINT point; };
struct Fixture {
    CComPtr<IShellFolder> folder;
    std::array<CComHeapPtr<ITEMIDLIST>,7> ids;
    explicit Fixture(const wchar_t *directory) {
        require(CreateDirectoryW(directory,nullptr) != FALSE,"fresh PIDL fixture directory");
        for (unsigned i = 1; i < ids.size(); ++i) {
            const std::wstring path = std::wstring(directory) + L"\\" + std::to_wstring(i) + L".bin";
            HANDLE file = CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            require(file != INVALID_HANDLE_VALUE,"create PIDL fixture file");
            DWORD written = 0;
            require(WriteFile(file,"old",3,&written,nullptr) && written == 3,"write PIDL fixture file");
            require(CloseHandle(file) != FALSE,"close PIDL fixture file");
        }
        CComPtr<IShellItem> item;
        require(SUCCEEDED(SHCreateItemFromParsingName(directory,nullptr,IID_PPV_ARGS(&item))),"bind fixture item");
        require(SUCCEEDED(item->BindToHandler(nullptr,BHID_SFObject,IID_PPV_ARGS(&folder))),"bind fixture folder");
        for (unsigned i = 1; i < ids.size(); ++i) {
            std::wstring name = std::to_wstring(i) + L".bin";
            ULONG eaten = 0; SFGAOF attributes = 0;
            require(SUCCEEDED(folder->ParseDisplayName(nullptr,nullptr,&name[0],&eaten,&ids[i],&attributes)),"parse fixture child");
        }
    }
};
std::vector<uint8_t> blob(const Fixture &fixture, std::initializer_list<Entry> entries) {
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(nullptr,0));
    require(stream != nullptr, "fixture stream");
    for (const auto &entry : entries) {
        require(SUCCEEDED(IStream_WritePidl(stream,fixture.ids[entry.id]))
            && SUCCEEDED(IStream_Write(stream,&entry.point,sizeof(entry.point))), "fixture PIDL encoding");
    }
    STATSTG stat = {}; require(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME)), "fixture size");
    LARGE_INTEGER zero = {}; require(SUCCEEDED(stream->Seek(zero,STREAM_SEEK_SET,nullptr)), "fixture seek");
    std::vector<uint8_t> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    if (!bytes.empty()) require(SUCCEEDED(IStream_Read(stream,bytes.data(),static_cast<ULONG>(bytes.size()))), "fixture read");
    return bytes;
}
std::map<USHORT,POINT> positions(const Fixture &fixture, const std::vector<uint8_t> &bytes) {
    std::map<USHORT,POINT> result;
    if (bytes.empty()) return result;
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(bytes.data(),static_cast<UINT>(bytes.size())));
    require(stream != nullptr,"decode stream");
    LARGE_INTEGER zero = {};
    for (;;) {
        ULARGE_INTEGER at = {};
        require(SUCCEEDED(stream->Seek(zero,STREAM_SEEK_CUR,&at)),"decode position");
        if (at.QuadPart == bytes.size()) break;
        CComHeapPtr<ITEMIDLIST> id; POINT point = {};
        require(SUCCEEDED(IStream_ReadPidl(stream,&id)) && id
            && SUCCEEDED(IStream_Read(stream,&point,sizeof(point))),"decode fixture PIDL");
        USHORT marker = 0;
        for (USHORT i = 1; i < fixture.ids.size(); ++i) {
            const HRESULT compared = fixture.folder->CompareIDs(SHCIDS_CANONICALONLY,id,fixture.ids[i]);
            require(SUCCEEDED(compared),"compare decoded fixture identity");
            if (static_cast<short>(HRESULT_CODE(compared)) == 0) { marker = i; break; }
        }
        require(marker != 0,"unknown fixture identity");
        require(result.emplace(marker,point).second,"duplicate PIDL");
    }
    return result;
}
void expect(const std::map<USHORT,POINT> &actual, USHORT id, LONG x, LONG y, const char *message) {
    const auto found = actual.find(id);
    require(found != actual.end() && found->second.x == x && found->second.y == y,message);
}
void iconChanges(const Fixture &fixture) {
    const auto saved = blob(fixture,{{1,{700,800}},{2,{900,1000}},{4,{1100,1200}},{5,{1300,1400}}});
    const auto baseline = blob(fixture,{{1,{0,0}},{2,{50,50}},{3,{100,100}}});
    const auto current = blob(fixture,{{1,{20,30}},{2,{50,50}},{3,{100,100}},{5,{200,200}},{6,{250,250}}});
    auto valid = [] { return true; };
    std::vector<uint8_t> bytes;
    require(SUCCEEDED(mergeIconChanges(saved,baseline,current,false,bytes,fixture.folder,valid)),"partial merge");
    auto actual = positions(fixture,bytes);
    expect(actual,1,20,30,"explicitly moved item not preserved");
    expect(actual,2,900,1000,"untouched fallback position overwrote SQL");
    expect(actual,4,1100,1200,"partial snapshot removed an unseen saved item");
    expect(actual,5,1300,1400,"missing baseline authorized replacement of a saved position");
    expect(actual,3,100,100,"new item known absent from SQL missing");
    expect(actual,6,250,250,"new item absent from both SQL and baseline missing");
    require(SUCCEEDED(mergeIconChanges(saved,baseline,current,true,bytes,fixture.folder,valid)),"complete merge");
    actual = positions(fixture,bytes); require(actual.count(4) == 0,"complete snapshot retained a disappeared item");
    require(SUCCEEDED(extendIconBaseline(baseline,current,bytes,fixture.folder,valid)),"extend baseline");
    actual = positions(fixture,bytes);
    expect(actual,1,0,0,"extending baseline consumed a later user movement");
    expect(actual,5,200,200,"newly observable item not added to baseline");
    const auto sentinel = saved;
    const std::vector<uint8_t> invalid{0xff,0xff,0xff};
    bytes = sentinel;
    require(mergeIconChanges(saved,baseline,current,true,bytes,nullptr,valid) == E_POINTER && bytes == sentinel,"missing parent silently accepted binary identity");
    require(FAILED(mergeIconChanges(invalid,baseline,current,true,bytes,fixture.folder,valid)) && bytes == sentinel,"invalid SQL blob altered output");
    require(FAILED(mergeIconChanges(saved,invalid,current,true,bytes,fixture.folder,valid)) && bytes == sentinel,"invalid baseline altered output");
    require(FAILED(extendIconBaseline(baseline,invalid,bytes,fixture.folder,valid)) && bytes == sentinel,"invalid observation altered output");
    require(SUCCEEDED(mergeIconChanges(saved,baseline,{},false,bytes,fixture.folder,valid)) && bytes == saved,"empty partial observation erased SQL");
    require(SUCCEEDED(mergeIconChanges(saved,baseline,{},true,bytes,fixture.folder,valid)) && bytes.empty(),"complete empty folder not represented");
    std::cout << "PASS PIDL/POINT merge, untouched SQL values, new/disappeared IDs, partial/empty/malformed snapshots\n";
}

std::vector<uint8_t> realPosition(PCUIDLIST_RELATIVE id, POINT point) {
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(nullptr,0));
    require(stream && SUCCEEDED(IStream_WritePidl(stream,id))
        && SUCCEEDED(IStream_Write(stream,&point,sizeof(point))),"real PIDL encoding");
    STATSTG stat = {}; require(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME)),"real PIDL size");
    LARGE_INTEGER zero = {}; require(SUCCEEDED(stream->Seek(zero,STREAM_SEEK_SET,nullptr)),"real PIDL seek");
    std::vector<uint8_t> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    require(SUCCEEDED(IStream_Read(stream,bytes.data(),static_cast<ULONG>(bytes.size()))),"real PIDL read");
    return bytes;
}
void realPoint(const std::vector<uint8_t> &bytes, PCUIDLIST_RELATIVE current, LONG x, LONG y) {
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(bytes.data(),static_cast<UINT>(bytes.size())));
    CComHeapPtr<ITEMIDLIST> id; POINT point = {};
    require(stream && SUCCEEDED(IStream_ReadPidl(stream,&id))
        && SUCCEEDED(IStream_Read(stream,&point,sizeof(point))),"real output decode");
    require(point.x == x && point.y == y,"canonical identity lost the chosen coordinate");
    require(ILGetSize(id) == ILGetSize(current) && memcmp(id,current,ILGetSize(current)) == 0,
        "output retained stale PIDL metadata");
    LARGE_INTEGER zero = {}; ULARGE_INTEGER at = {};
    require(SUCCEEDED(stream->Seek(zero,STREAM_SEEK_CUR,&at)) && at.QuadPart == bytes.size(),
        "canonical duplicate would be applied more than once");
}
// This implements the real SDK interface. It injects HRESULT failures at the
// COM boundary; it does not claim to reproduce a partial failure of the Shell.
class IconView : public IFolderView {
    const Fixture &fixture;
    ULONG references = 1;
    unsigned identity(PCUITEMID_CHILD id) const {
        for (unsigned i = 1; i < fixture.ids.size(); ++i)
            if (ILIsEqual(id, fixture.ids[i])) return i;
        throw std::runtime_error("unexpected test PIDL");
    }
public:
    std::map<unsigned,POINT> points;
    unsigned failedItem = 0, applications = 0;
    bool preserveState = true;
    explicit IconView(const Fixture &value) : fixture(value) {
        for (unsigned i = 1; i < fixture.ids.size(); ++i) points[i] = {static_cast<LONG>(i * 10), 10};
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IFolderView) return E_NOINTERFACE;
        *out = static_cast<IFolderView *>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE GetCurrentViewMode(UINT *mode) override { *mode=FVM_ICON; return S_OK; }
    HRESULT STDMETHODCALLTYPE SetCurrentViewMode(UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFolder(REFIID iid, void **out) override { return fixture.folder->QueryInterface(iid,out); }
    HRESULT STDMETHODCALLTYPE Item(int, PITEMID_CHILD *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ItemCount(UINT, int *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Items(UINT, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSelectionMarkedItem(int *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFocusedItem(int *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetItemPosition(PCUITEMID_CHILD id, POINT *point) override { *point=points.at(identity(id)); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetSpacing(POINT *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDefaultSpacing(POINT *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetAutoArrange() override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE SelectItem(int, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SelectAndPositionItems(UINT count, PCUITEMID_CHILD_ARRAY ids, POINT *positions, DWORD flags) override {
        ++applications;
        preserveState = preserveState && (flags & SVSI_NOSTATECHANGE) != 0;
        for (UINT i=0; i<count; ++i) {
            const unsigned item=identity(ids[i]);
            if (item==failedItem) return E_ACCESSDENIED;
            points[item]=positions[i];
        }
        return S_OK;
    }
};

void restorationFailures(const Fixture &fixture) {
    const auto saved=blob(fixture,{{1,{100,50}},{2,{300,50}},{3,{500,50}}});
    IconView view(fixture);
    auto malformed=saved; malformed.push_back(0xff);
    require(FAILED(restoreIconPositions(&view,malformed,[]{return true;})) && view.applications==0,
        "invalid trailing data moved items before complete decoding");
    PreparedIconPositions prepared;
    require(prepareIconPositions(&view,saved,[]{return true;},prepared)==S_OK && view.applications==0,
        "preparation performed a position application");
    view.failedItem=2;
    require(applyIconPositions(&view,prepared,[]{return true;})==E_ACCESSDENIED,
        "partly successful fallback hid the native error");
    require(view.points[1].x==100 && view.points[2].x==20 && view.points[3].x==500 && view.preserveState,
        "partial failure or selection/focus preservation contract");
    const unsigned calls=view.applications;
    require(applyIconPositions(&view,prepared,[]{return true;})==E_INVALIDARG && view.applications==calls,
        "prepared application was not consumed before COM");
    // Observe the partly applied view before the user's next real change.
    const auto baseline=blob(fixture,{{1,view.points[1]},{2,view.points[2]},{3,view.points[3]}});
    std::vector<uint8_t> merged;
    require(mergeIconChanges(saved,baseline,baseline,true,merged,fixture.folder,[]{return true;})==S_OK,
        "unchanged failed view merge");
    auto result=positions(fixture,merged);
    require(result.at(1).x==100 && result.at(2).x==300 && result.at(3).x==500,
        "automatic default/partial coordinates replaced the saved reference");
    const auto changed=blob(fixture,{{1,{120,70}},{2,view.points[2]},{3,view.points[3]}});
    require(mergeIconChanges(saved,baseline,changed,true,merged,fixture.folder,[]{return true;})==S_OK,
        "observed user delta after failed restore");
    result=positions(fixture,merged);
    require(result.at(1).x==120 && result.at(1).y==70 && result.at(2).x==300 && result.at(3).x==500,
        "one user move changed untouched saved positions");
    view.failedItem=0;
    require(restoreIconPositions(&view,merged,[]{return true;})==S_OK && view.points[2].x==300,
        "successful restoration did not use the composed reference");
    PreparedIconPositions cancelled;
    require(prepareIconPositions(&view,saved,[]{return false;},cancelled)==E_ABORT && !cancelled.ready,
        "stale generation prepared an application");
    std::cout << "PASS invalid restore, actual COM HRESULT/partial fallback, known reference, observed delta and recovery (COM double).\n";
}

void verificationFixture(const Fixture &fixture, const wchar_t *directory) {
    const std::wstring database=std::wstring(directory)+L".sqlite3";
    const int length=WideCharToMultiByte(CP_UTF8,0,database.c_str(),-1,nullptr,0,nullptr,nullptr);
    require(length>0,"verification database UTF8 size");
    std::string path(static_cast<size_t>(length),'\0');
    require(WideCharToMultiByte(CP_UTF8,0,database.c_str(),-1,&path[0],length,nullptr,nullptr)>0,"verification database UTF8");
    require(GetFileAttributesW(database.c_str())==INVALID_FILE_ATTRIBUTES,"verification database already exists");
    FolderStateStore store(path.c_str());
    FolderState state;
    state.icons=blob(fixture,{{1,{100,50}},{2,{300,50}},{3,{500,50}}});
    require(store.ready() && store.save("canonical",state,FolderState::Icons)
        && store.saveSearchHistory("canonical",{"alpha","beta"},20),"verification fixture persistence");
    std::ofstream expected(std::wstring(directory)+L".positions");
    expected << "96\n1.bin=100,50;2.bin=300,50;3.bin=500,50";
    require(bool(expected),"verification expected file");
}

void canonicalChanges(const wchar_t *directory) {
    require(CreateDirectoryW(directory,nullptr) != FALSE,"fresh canonical fixture directory");
    const std::wstring path = std::wstring(directory) + L"\\identity.bin";
    HANDLE file = CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(file != INVALID_HANDLE_VALUE,"create canonical fixture");
    DWORD written = 0;
    require(WriteFile(file,"old",3,&written,nullptr) && written == 3,"write old metadata fixture");
    require(CloseHandle(file) != FALSE,"close old metadata fixture");
    CComPtr<IShellItem> item;
    require(SUCCEEDED(SHCreateItemFromParsingName(directory,nullptr,IID_PPV_ARGS(&item))),"bind fixture item");
    CComPtr<IShellFolder> folder;
    require(SUCCEEDED(item->BindToHandler(nullptr,BHID_SFObject,IID_PPV_ARGS(&folder))),"bind real fixture IShellFolder");
    CComHeapPtr<ITEMIDLIST> before, after;
    ULONG eaten = 0; SFGAOF attributes = 0;
    wchar_t name[] = L"identity.bin";
    require(SUCCEEDED(folder->ParseDisplayName(nullptr,nullptr,name,&eaten,&before,&attributes)),"parse old PIDL");
    file = CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(file != INVALID_HANDLE_VALUE,"open changed metadata fixture");
    const std::vector<uint8_t> content(4097,0x5a);
    require(WriteFile(file,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)
        && written == content.size(),"change fixture size");
    require(CloseHandle(file) != FALSE,"close changed metadata fixture");
    require(SUCCEEDED(folder->ParseDisplayName(nullptr,nullptr,name,&eaten,&after,&attributes)),"parse current PIDL");
    const bool different = ILGetSize(before) != ILGetSize(after)
        || memcmp(before,after,(std::min)(ILGetSize(before),ILGetSize(after))) != 0;
    require(different,"metadata mutation did not establish binary-distinct PIDLs");
    const HRESULT compared = folder->CompareIDs(SHCIDS_CANONICALONLY,before,after);
    require(SUCCEEDED(compared) && static_cast<short>(HRESULT_CODE(compared)) == 0,"fixture canonical identities differ");
    const auto saved = realPosition(before,{700,800});
    const auto baseline = realPosition(after,{0,0});
    std::vector<uint8_t> output;
    auto valid = [] { return true; };
    require(SUCCEEDED(mergeIconChanges(saved,baseline,baseline,true,output,folder,valid)),"canonical metadata merge");
    realPoint(output,after,700,800);
    const auto latest = realPosition(after,{900,1000});
    auto duplicates = latest; // Historical capture: visible first, then unmatched old binary PIDL.
    duplicates.insert(duplicates.end(),saved.begin(),saved.end());
    require(SUCCEEDED(mergeIconChanges(duplicates,baseline,baseline,true,output,folder,valid)),"canonical duplicate merge");
    realPoint(output,after,900,1000);
    const auto moved = realPosition(before,{20,30});
    auto duplicateCurrent = moved;
    duplicateCurrent.insert(duplicateCurrent.end(),baseline.begin(),baseline.end());
    require(SUCCEEDED(mergeIconChanges(saved,baseline,duplicateCurrent,true,output,folder,valid)),"visible-first observation merge");
    // Priority follows the legacy encoding contract, not inferred timestamps.
    realPoint(output,before,20,30);
    auto invalidated = saved;
    output = latest;
    unsigned callbacks = 0;
    auto canceled = [&]() {
        if (++callbacks == 3) { invalidated.clear(); return false; }
        return true;
    };
    require(mergeIconChanges(invalidated,baseline,baseline,true,output,folder,canceled) == E_ABORT
        && output == latest,"comparison invalidation changed output or retained borrowed input memory");
    std::cout << "PASS real filesystem IShellFolder: binary PIDLs differ, canonical identity survives metadata changes, visible-first duplicate compatibility, invalidation preserves output\n";
}
}
int wmain(int argc, wchar_t **argv) {
    const HRESULT initialized = CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 1;
    struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
    try {
        require(argc == 2,"isolated fixture directory argument");
        viewChanges();
        Fixture fixture(argv[1]); iconChanges(fixture); restorationFailures(fixture); verificationFixture(fixture,argv[1]);
        canonicalChanges((std::wstring(argv[1]) + L"\\metadata").c_str());
        return 0;
    }
    catch (const std::exception &error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
