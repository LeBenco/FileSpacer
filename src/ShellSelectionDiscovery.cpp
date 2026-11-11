#include <common.h>
#include "ShellSelectionRuntime.h"
#include "ShellViewSelection.h"
#include "ShellViewHost.h"
#include "DPI.h"
#include <dbghelp.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <shldisp.h>
#include <shdispid.h>
#include <propkey.h>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Advapi32.lib")

namespace filespacer { namespace selectionDetail {
namespace {
void closeHandle(HANDLE value) { if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value); }
using Handle = std::unique_ptr<void, decltype(&closeHandle)>;
using Module = std::unique_ptr<HINSTANCE__, decltype(&FreeLibrary)>;
using Internet = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;
Image selectedImage;
const Image publicImage;
RangeImage selectedRange;
const RangeImage publicRange;
std::atomic<bool> rangeReady{false};
std::atomic<bool> rangeFinished{false};
enum class Resolution : unsigned char { Running, Public, Private };
std::atomic<Resolution> resolution{Resolution::Running};
std::once_flag initialized;

struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
void supported(bool condition,const char *reason){if(!condition)throw Unsupported(reason);}
void require(bool condition, const char *reason) {
    if (!condition) throw std::runtime_error(reason);
}
std::wstring systemFile(const wchar_t *name) {
    wchar_t directory[MAX_PATH] = {};
    const UINT size = GetSystemDirectoryW(directory, _countof(directory));
    require(size && size < _countof(directory), "System directory unavailable");
    return std::wstring(directory) + L"\\" + name;
}
bool limitedProcess() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) return false;
    Handle token(raw, &closeHandle);
    TOKEN_ELEVATION elevation = {}; DWORD bytes = 0;
    return GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &bytes)
        && !elevation.TokenIsElevated;
}
bool cfgCompatible() {
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY policy={};
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY known={};
    known.EnableControlFlowGuard=1;known.EnableExportSuppression=1;known.StrictMode=1;
    return GetProcessMitigationPolicy(GetCurrentProcess(),ProcessControlFlowGuardPolicy,&policy,sizeof(policy))
        && policy.EnableControlFlowGuard && !(policy.Flags&~known.Flags);
}
struct Identity {
    unsigned char *base = nullptr;
    DWORD size = 0, age = 0;
    GUID guid = {};
    std::wstring key, cacheKey;
};
Identity identity() {
    Identity result;
    const std::wstring path = systemFile(L"shell32.dll");
    HMODULE module = GetModuleHandleW(L"shell32.dll");
    wchar_t loaded[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, loaded, _countof(loaded));
    require(module && length && length < _countof(loaded) && !_wcsicmp(path.c_str(), loaded), "Unexpected Shell image");
    result.base = reinterpret_cast<unsigned char *>(module);
    IMAGE_DOS_HEADER dos = {}; IMAGE_NT_HEADERS64 nt = {};
    require(read(result.base, dos) && dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= 64
        && dos.e_lfanew <= 4096 && read(result.base + dos.e_lfanew, nt)
        && nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64
        && nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC, "Invalid Shell PE");
    result.size = nt.OptionalHeader.SizeOfImage;
    const auto &debug = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    require(debug.VirtualAddress < result.size && debug.Size <= result.size - debug.VirtualAddress
        && debug.Size <= 32 * sizeof(IMAGE_DEBUG_DIRECTORY), "Invalid debug directory");
    for (DWORD offset = 0; offset + sizeof(IMAGE_DEBUG_DIRECTORY) <= debug.Size; offset += sizeof(IMAGE_DEBUG_DIRECTORY)) {
        IMAGE_DEBUG_DIRECTORY entry = {};
        struct CodeView { DWORD signature; GUID guid; DWORD age; } code = {};
        require(read(result.base + debug.VirtualAddress + offset, entry), "Unreadable debug entry");
        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.AddressOfRawData >= result.size
            || sizeof(code) > result.size - entry.AddressOfRawData) continue;
        if (!read(result.base + entry.AddressOfRawData, code) || code.signature != 0x53445352) continue;
        result.guid = code.guid; result.age = code.age;
        wchar_t text[40] = {}, age[16] = {};
        require(StringFromGUID2(code.guid, text, _countof(text)) != 0, "GUID formatting failed");
        for (wchar_t character : text) if (character && character != L'{' && character != L'}' && character != L'-') result.key += character;
        swprintf_s(age, L"%X", code.age); result.key += age;
        wchar_t size[16]={};swprintf_s(size,L"%X",result.size);
        result.cacheKey=result.key+L"-"+size+L"-s2";
        return result;
    }
    throw std::runtime_error("No Shell RSDS identity");
}
std::wstring rangeCacheFile(const std::wstring &base) {
    RangeData data;if(!controlIdentity(data))return {};
    // Shell identity remains in the stem; control identity is checked inside
    // both positive and negative records. Avoid two GUIDs in a long filename.
    // Change the suffix when the producer/qualification contract changes.
    return base.substr(0,base.size()-4)+L"-r1.range";
}
bool savedRangePublic(const std::wstring &path) {
    std::ifstream file(path.c_str(),std::ios::binary);char magic[10]={};RangeData saved,current;
    file.read(magic,sizeof(magic));file.read(reinterpret_cast<char *>(&saved.guid),sizeof(saved.guid));
    file.read(reinterpret_cast<char *>(&saved.age),sizeof(saved.age));file.read(reinterpret_cast<char *>(&saved.size),sizeof(saved.size));
    return file&&std::memcmp(magic,"FSPUBLIC01",sizeof(magic))==0&&controlIdentity(current)
        &&saved.size==current.size&&saved.age==current.age&&IsEqualGUID(saved.guid,current.guid);
}
void saveRangeRecord(const std::wstring &path,const char *message) {
    RangeData data;if(!controlIdentity(data))return;
    std::ofstream file(path.c_str(),std::ios::binary|std::ios::trunc);file<<"FSPUBLIC01";
    file.write(reinterpret_cast<const char *>(&data.guid),sizeof(data.guid));
    file.write(reinterpret_cast<const char *>(&data.age),sizeof(data.age));
    file.write(reinterpret_cast<const char *>(&data.size),sizeof(data.size));file<<message<<"\n";
}
std::wstring defaultRoot() {
    CComHeapPtr<wchar_t> local;
    require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &local)), "Local cache unavailable");
    // Bump this directory when the producer/qualification contract changes.
    return std::wstring(local) + L"\\FileSpacer\\ShellSelection-v1";
}
void directory(const std::wstring &path) {
    const int status = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    require(status == ERROR_SUCCESS || status == ERROR_ALREADY_EXISTS || status == ERROR_FILE_EXISTS, "Cache directory unavailable");
    const DWORD flags = GetFileAttributesW(path.c_str());
    require(flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_DIRECTORY) && !(flags & FILE_ATTRIBUTE_REPARSE_POINT), "Unsafe cache directory");
}
bool recentAttempt(const std::wstring &path) {
    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) return false;
    FILETIME now = {}; GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER current = {}, previous = {};
    current.LowPart = now.dwLowDateTime; current.HighPart = now.dwHighDateTime;
    previous.LowPart = info.ftLastWriteTime.dwLowDateTime; previous.HighPart = info.ftLastWriteTime.dwHighDateTime;
    return current.QuadPart <= previous.QuadPart || current.QuadPart - previous.QuadPart < 24ull * 60 * 60 * 10000000;
}
template<class T> T api(HMODULE module, const char *name) {
    const FARPROC address = GetProcAddress(module, name);
    require(address != nullptr, "Required system export absent");
    return reinterpret_cast<T>(address);
}
void downloadPdb(const Identity &id, const std::wstring &destination, const wchar_t *pdb=L"shell32.pdb") {
    if (GetFileAttributesW(destination.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    Internet session(WinHttpOpen(L"FileSpacer symbol discovery", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0), &WinHttpCloseHandle);
    require(session && WinHttpSetTimeouts(session.get(), 5000, 5000, 15000, 15000), "Cannot initialize symbol download");
    Internet connection(WinHttpConnect(session.get(), L"msdl.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0), &WinHttpCloseHandle);
    require(connection != nullptr, "Cannot connect to symbol server");
    const std::wstring url = L"/download/symbols/"+std::wstring(pdb)+L"/"+id.key+L"/"+pdb;
    Internet request(WinHttpOpenRequest(connection.get(), L"GET", url.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE), &WinHttpCloseHandle);
    require(request && WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request.get(), nullptr), "Symbol request failed");
    DWORD status = 0, bytes = sizeof(status);
    require(WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX) && status == 200, "Matching Microsoft symbols unavailable");
    const std::wstring temporary = destination + L".partial";
    Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr), &closeHandle);
    require(file.get() != INVALID_HANDLE_VALUE, "Cannot save symbol file");
    const ULONGLONG began = GetTickCount64(); DWORD total = 0;
    std::array<unsigned char, 65536> buffer = {};
    for (;;) {
        DWORD received = 0, written = 0;
        require(WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &received) != FALSE, "Symbol read failed");
        if (!received) break;
        require(total <= 256u * 1024 * 1024 - received && GetTickCount64() - began < 60000, "Symbol download limit exceeded");
        require(WriteFile(file.get(), buffer.data(), received, &written, nullptr) && written == received, "Symbol save failed");
        total += received;
    }
    require(total != 0 && FlushFileBuffers(file.get()), "Empty or incomplete symbol file");
    file.reset();
    require(MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE, "Cannot publish symbol file");
}
struct SymbolTables { DWORD64 base; DWORD size; std::vector<DWORD> tables; const wchar_t *prefix=L"??_7CDefView@@"; };
BOOL CALLBACK collectTables(PSYMBOL_INFOW symbol, ULONG, void *context) {
    auto &result = *static_cast<SymbolTables *>(context);
    if (wcsncmp(symbol->Name,result.prefix,wcslen(result.prefix)) == 0 && symbol->Address >= result.base
        && symbol->Address - result.base < result.size) result.tables.push_back(static_cast<DWORD>(symbol->Address - result.base));
    return TRUE;
}
struct Symbols { std::array<DWORD, 22> methods = {}; std::vector<DWORD> tables; };
Symbols symbols(const Identity &id, const std::wstring &path, bool control=false) {
    Module library(LoadLibraryExW(systemFile(L"dbghelp.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32), &FreeLibrary);
    require(library != nullptr, "System DbgHelp unavailable");
    const auto initialize = api<decltype(&SymInitializeW)>(library.get(), "SymInitializeW");
    const auto options = api<decltype(&SymSetOptions)>(library.get(), "SymSetOptions");
    const auto load = api<decltype(&SymLoadModuleExW)>(library.get(), "SymLoadModuleExW");
    const auto lookup = api<decltype(&SymFromNameW)>(library.get(), "SymFromNameW");
    const auto enumerate = api<decltype(&SymEnumSymbolsW)>(library.get(), "SymEnumSymbolsW");
    const auto moduleInfo = api<decltype(&SymGetModuleInfoW64)>(library.get(), "SymGetModuleInfoW64");
    const auto cleanup = api<decltype(&SymCleanup)>(library.get(), "SymCleanup");
    Handle session(CreateEventW(nullptr, FALSE, FALSE, nullptr), &closeHandle);
    require(session != nullptr, "Cannot create private symbol session");
    options(SYMOPT_DEFERRED_LOADS | SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    require(initialize(session.get(), path.c_str(), FALSE) != FALSE, "Symbol initialization failed");
    struct Cleanup { decltype(cleanup) function; HANDLE session; ~Cleanup(){ function(session); } } finish = {cleanup, session.get()};
    const DWORD64 base = reinterpret_cast<ULONG_PTR>(id.base);
    wchar_t imageFile[32768]={};
    require(GetModuleFileNameW(reinterpret_cast<HMODULE>(id.base),imageFile,_countof(imageFile))!=0,"Symbol image unavailable");
    const wchar_t *moduleName=control?L"comctl32":L"shell32";
    require(load(session.get(), nullptr,imageFile,moduleName, base, id.size, nullptr, 0) == base, "Symbol module mismatch");
    const wchar_t *names[] = {L"CListViewHost::_OnClearSelection",L"CListViewHost::Select",L"CSelectionManager::ClearSelection",
        L"CSelectionManager::Init",L"CDefView::QueryService",L"CDefView::OnItemStateChanged",L"CDefView::CancelDelayActivation",
        L"?QueryInterface@CListViewHost@@UEAAJAEBU_GUID@@PEAPEAX@Z",L"_imp_DSA_EnumCallback",L"_imp_DSA_DeleteAllItems",
        L"CSelectionManager::s_ClearSelectedItemEntry",L"??_7CDefView@@6BIListControlHost@@@",L"??_7CListViewHost@@6BIListControl@@@",
        L"CListViewHost::QueryObject",L"CDefView::QueryObject",L"??_7CSelectionManager@@6B@",L"DSA_GetItemPtr",
        L"CListViewHost::s_ViewSubclassWndProc",L"_imp_DSA_InsertItem",L"DSA_InsertItem",L"CSelectionManager::SelectItem",L"CListViewHost::_OnLVNotify"};
    const wchar_t *controlNames[]={L"?QueryInterface@CListView@@UEAAJAEBU_GUID@@PEAPEAX@Z",L"?AddRef@CListView@@UEAAKXZ",L"?Release@CListView@@UEAAKXZ",
        L"?QueryInterface@CLVRange@@UEAAJAEBU_GUID@@PEAPEAX@Z",L"?AddRef@CLVRange@@UEAAKXZ",L"?Release@CLVRange@@UEAAKXZ",
        L"?GetSelectedCount@CListView@@UEAAJPEAH@Z",L"?Clear@CLVRange@@UEAAJXZ",L"CLVRange::CountIncluded"};
    Symbols result;
    for (size_t i = 0; i < (control?_countof(controlNames):_countof(names)); ++i) {
        const auto method=control?controlNames[i]:names[i];
        struct Symbol { SYMBOL_INFOW info; wchar_t extra[MAX_SYM_NAME]; } symbol = {};
        symbol.info.SizeOfStruct = sizeof(SYMBOL_INFOW); symbol.info.MaxNameLen = MAX_SYM_NAME;
        options(SYMOPT_DEFERRED_LOADS | SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS
            | (method[0] == L'?' ? 0 : SYMOPT_UNDNAME));
        const std::wstring name = std::wstring(moduleName)+L"!"+method;
        if (!lookup(session.get(), name.c_str(), &symbol.info)) {
            IMAGEHLP_MODULEW64 info={};info.SizeOfStruct=sizeof(info);
            require(moduleInfo(session.get(),base,&info) && info.SymType==SymPdb && !info.PdbUnmatched
                && info.PdbAge==id.age && IsEqualGUID(info.PdbSig70,id.guid),"Matching PDB was not loaded");
            if(control || (i != EnumGlobal && i != DeleteGlobal && i < 17)) {
                std::string reason="Required private symbol absent: ";
                for(const wchar_t character:name)reason+=static_cast<char>(character); // Symbol names are ASCII.
                throw Unsupported(reason.c_str());
            }
            continue;
        }
        require(symbol.info.Address >= base && symbol.info.Address - base < id.size, "Symbol outside loaded image");
        result.methods[i] = static_cast<DWORD>(symbol.info.Address - base);
    }
    options(SYMOPT_DEFERRED_LOADS | SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    SymbolTables tables = {base, id.size, {}};
    if(control)tables.prefix=L"??_7CLVRange@@";
    const std::wstring mask=std::wstring(moduleName)+(control?L"!*CLVRange*":L"!*CDefView*");
    require(enumerate(session.get(), base,mask.c_str(), collectTables, &tables) != FALSE, "Cannot enumerate notification tables");
    result.tables = std::move(tables.tables);
    return result;
}
template<size_t N> bool match(const unsigned char *data, size_t length, size_t offset, const unsigned char (&pattern)[N]) {
    return offset <= length && N <= length - offset && std::memcmp(data + offset, pattern, N) == 0;
}
std::array<DWORD, FieldCount> extract(const Identity &id, const Symbols &foundSymbols) {
    Image view; view.base = id.base; view.size = id.size;
    const auto copy = [&](DWORD rva, auto &value) { supported(view.range(rva, sizeof(value)) && read(id.base + rva, value), "Unreadable discovery operand"); };
    const auto relative = [&](DWORD rva, const unsigned char *code, size_t offset, size_t next) {
        LONG displacement = 0; std::memcpy(&displacement, code + offset, sizeof(displacement));
        const LONGLONG target = static_cast<LONGLONG>(rva) + static_cast<LONGLONG>(next) + displacement;
        supported(target >= 0 && target < id.size, "Relative target outside image"); return static_cast<DWORD>(target);
    };
    std::array<DWORD, FieldCount> fields = {};
    std::copy_n(foundSymbols.methods.begin(), 16, fields.begin()); fields[ArrayGetter] = foundSymbols.methods[16];
    std::copy_n(foundSymbols.methods.begin()+17,5,fields.begin()+NativeWindowProcedure);
    unsigned char qi[20] = {}; copy(fields[HostQI], qi);
    const unsigned char qiPrefix[] = {0x4d,0x8b,0xc8,0x4c,0x8b,0xc2,0x48,0x8d,0x15};
    const unsigned char qiJump[] = {0x48,0xff,0x25};
    supported(match(qi,sizeof(qi),0,qiPrefix) && match(qi,sizeof(qi),13,qiJump), "shell32!CListViewHost::QueryInterface: unsupported QISearch stub");
    // A delay-import slot can still contain its resolver thunk. Read the PE
    // import-name metadata instead of comparing that transient function pointer.
    supported(namedImport(view,relative(fields[HostQI],qi,16,20),"QISearch"), "shell32!CListViewHost::QueryInterface: unexpected QISearch import");
    const DWORD qit = relative(fields[HostQI],qi,9,13); unsigned interfaces = 0; bool terminated = false;
    const GUID control = {0xc401f7ac,0x9572,0x42cd,{0xae,0x05,0x3b,0xd0,0x84,0x27,0x0c,0xb1}};
    for (DWORD i = 0; i < 64; ++i) {
        struct Entry { const GUID *id; DWORD offset, padding; } entry = {}; copy(qit + 16*i, entry);
        if (!entry.id) { supported(entry.offset == 0, "Invalid QITAB terminator"); terminated = true; break; }
        const ULONG_PTR address = reinterpret_cast<ULONG_PTR>(entry.id), start = reinterpret_cast<ULONG_PTR>(id.base);
        supported(address >= start && address - start <= id.size - sizeof(GUID), "QITAB GUID outside image");
        GUID guid = {}; supported(read(entry.id, guid), "Unreadable QITAB GUID");
        if (IsEqualGUID(guid,control)) { fields[HostOffset] = entry.offset; ++interfaces; }
    }
    supported(terminated && interfaces == 1, "shell32!CListViewHost::QueryInterface: missing or ambiguous host interface");
    unsigned char clear[256] = {}; copy(fields[OnClear], clear); unsigned chains = 0, calls = 0;
    for (size_t i = 0; i + 18 < sizeof(clear); ++i) {
        const bool first = clear[i]==0x48 && clear[i+1]==0x8b && clear[i+2]==0x89 && clear[i+7]==0x48 && clear[i+8]==0x8b
            && clear[i+9]==0x41 && clear[i+11]==0x48 && clear[i+12]==0x85 && clear[i+13]==0xc0 && clear[i+14]==0x74 && clear[i+16]==0x8b && clear[i+17]==0x30;
        const bool second = clear[i]==0x48 && clear[i+1]==0x8b && clear[i+2]==0x81 && clear[i+7]==0x48 && clear[i+8]==0x8b
            && clear[i+9]==0x48 && clear[i+11]==0x48 && clear[i+12]==0x85 && clear[i+13]==0xc9 && clear[i+14]==0x74 && clear[i+16]==0x8b && clear[i+17]==0x31;
        if (first || second) { DWORD manager=0; std::memcpy(&manager,clear+i+3,4);
            supported(manager > fields[HostOffset] && manager < 4096 && manager%8==0 && clear[i+10]%8==0, "shell32!CListViewHost::_OnClearSelection: invalid manager layout");
            fields[ManagerOffset]=manager-fields[HostOffset];fields[ArrayOffset]=clear[i+10];++chains; }
    }
    for (size_t i=0;i+5<=sizeof(clear);++i) if(clear[i]==0xe8) {
        LONG delta=0;std::memcpy(&delta,clear+i+1,4);
        if(static_cast<LONGLONG>(fields[OnClear])+static_cast<LONGLONG>(i)+5+delta==fields[Purge])++calls;
    }
    supported(chains==1 && calls==1, "shell32!CListViewHost::_OnClearSelection: unsupported clear procedure");
    unsigned char purge[128]={};copy(fields[Purge],purge);
    const unsigned char helper[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8d,0x15};
    ArrayLayout layout;
    if(match(purge,sizeof(purge),0,helper))fields[Procedure]=1;
    else { supported(decodeInlineArrayLoop(purge,sizeof(purge),fields[Purge],fields[Callback],fields[ArrayOffset],layout),"shell32!CSelectionManager::ClearSelection: unsupported purge form");fields[Procedure]=2; }
    unsigned char getter[32]={};copy(fields[ArrayGetter],getter);ArrayLayout getterLayout;
    supported(decodeArrayGetter(getter,sizeof(getter),getterLayout),"shell32!DSA_GetItemPtr: unsupported array getter");
    if(fields[Procedure]==2)supported(layout.data==getterLayout.data && layout.stride==getterLayout.stride,"shell32!CSelectionManager::ClearSelection / DSA_GetItemPtr: array layouts disagree");
    const auto next=std::min_element(foundSymbols.tables.begin(),foundSymbols.tables.end(),[&](DWORD a,DWORD b){
        return (a>fields[NotifyTable]?a:MAXDWORD)<(b>fields[NotifyTable]?b:MAXDWORD);});
    supported(next!=foundSymbols.tables.end() && *next>fields[NotifyTable] && *next-fields[NotifyTable]<=1024 && (*next-fields[NotifyTable])%8==0,"Unbounded notification table");
    for (Field target : {Notify,Cancel}) {
        unsigned found=0;
        for(DWORD slot=0;slot<(*next-fields[NotifyTable])/8;++slot){void *method=nullptr;copy(fields[NotifyTable]+8*slot,method);
            if(method==id.base+fields[target]){fields[target==Notify?NotifySlot:CancelSlot]=slot;++found;}}
        supported(found==1,"Ambiguous notification slot");
    }
    for (DWORD slot=0;slot<3;++slot) {
        void *method=nullptr;copy(fields[ManagerTable]+8*slot,method);
        const ULONG_PTR address=reinterpret_cast<ULONG_PTR>(method),start=reinterpret_cast<ULONG_PTR>(id.base);
        supported(address>=start && address-start<id.size,"Manager method outside image");fields[ManagerQI+slot]=static_cast<DWORD>(address-start);
    }
    return fields;
}
void save(const Identity &id, const std::array<DWORD,FieldCount> &fields, const std::wstring &path) {
    std::ofstream file(path.c_str(),std::ios::binary|std::ios::trunc);const DWORD count=FieldCount;
    file.write("FSDISC02",8);file.write(reinterpret_cast<const char *>(&id.size),4);
    file.write(reinterpret_cast<const char *>(&id.guid),sizeof(id.guid));file.write(reinterpret_cast<const char *>(&id.age),4);
    file.write(reinterpret_cast<const char *>(&count),4);file.write(reinterpret_cast<const char *>(fields.data()),sizeof(fields));file.flush();
    require(file.good(),"Cannot save discovered data");
}
void discoverRange(const std::wstring &root,const std::wstring &path) {
    RangeImage result;require(controlIdentity(result.data),"Control identity unavailable");result.base=controlModule();
    Identity id;id.base=result.base;id.size=result.data.size;id.guid=result.data.guid;id.age=result.data.age;
    wchar_t guid[40]={},age[16]={};require(StringFromGUID2(id.guid,guid,_countof(guid))!=0,"Control GUID unavailable");
    for(wchar_t character:guid)if(character&&character!=L'{'&&character!=L'}'&&character!=L'-')id.key+=character;
    swprintf_s(age,L"%X",id.age);id.key+=age;
    const std::wstring directoryName=root+L"\\symbols\\comctl32.pdb\\"+id.key;directory(directoryName);
    downloadPdb(id,directoryName+L"\\comctl32.pdb",L"comctl32.pdb");
    const auto found=symbols(id,directoryName,true);
    std::copy_n(found.methods.begin(),RangeMethodCount,result.data.methods.begin());
    unsigned char count[96]={};
    supported(read(result.base+result.data.methods[ControlCount],count)
        &&decodeControlCount(count,sizeof(count),result.data.managerOffset,result.data.rangeOffset),"comctl32!CListView::GetSelectedCount: unsupported operand form");
    unsigned matches=0;
    for(const auto table:found.tables)for(DWORD slot=3;slot<13;++slot) {
        void *clear=nullptr,*included=nullptr;
        if(read(result.base+table+sizeof(void *)*slot,clear)&&clear==result.base+result.data.methods[RangeClear]
                &&read(result.base+table+13*sizeof(void *),included)&&included==result.base+result.data.methods[RangeCount]) {
            result.data.clearSlot=slot;result.data.tableRva=table;++matches;
        }
    }
    supported(matches==1,"comctl32!CLVRange::Clear / CountIncluded: virtual table missing or ambiguous");
    const char *reason=nullptr;const bool valid=result.qualifies(&reason);supported(valid,reason);
    std::ofstream file(path.c_str(),std::ios::binary|std::ios::trunc);
    file.write("FSLVR001",8);file.write(reinterpret_cast<const char *>(&result.data),sizeof(result.data));file.flush();
    require(file.good(),"Cannot save range discovery");
}

// Runs only in the isolated background process, with an invisible owned view.
struct QualificationSink : IDispatch {
    LONG references=1;bool ready=false;
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=IID_IUnknown&&id!=IID_IDispatch&&id!=DIID_DShellFolderViewEvents)return E_NOINTERFACE;
        *out=static_cast<IDispatch *>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP GetTypeInfoCount(UINT *count)override{if(!count)return E_POINTER;*count=0;return S_OK;}
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo **)override{return E_NOTIMPL;}
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR *,UINT,LCID,DISPID *)override{return E_NOTIMPL;}
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS *,VARIANT *out,EXCEPINFO *,UINT *)override{
        if(out)VariantInit(out);if(id==DISPID_FILELISTENUMDONE)ready=true;return S_OK;
    }
};
struct QualificationEvents : IExplorerBrowserEvents {
    LONG references=1;bool navigated=false;HRESULT observed=E_PENDING;
    CComPtr<IShellView> shell;CComPtr<QualificationSink> sink;CComPtr<IConnectionPoint> connection;DWORD cookie=0;
    void detach(){if(connection)connection->Unadvise(cookie);connection.Release();sink.Release();shell.Release();}
    STDMETHODIMP QueryInterface(REFIID id,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IExplorerBrowserEvents)return E_NOINTERFACE;
        *out=static_cast<IExplorerBrowserEvents *>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef()override{return InterlockedIncrement(&references);}
    STDMETHODIMP_(ULONG) Release()override{const ULONG left=InterlockedDecrement(&references);if(!left)delete this;return left;}
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE)override{navigated=true;return S_OK;}
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE)override{return S_OK;}
    STDMETHODIMP OnViewCreated(IShellView *view)override{
        detach();shell=view;CComPtr<IDispatch> automation;observed=view->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&automation));
        if(FAILED(observed))return S_OK;
        CComQIPtr<IConnectionPointContainer> container(automation);
        observed=container?container->FindConnectionPoint(DIID_DShellFolderViewEvents,&connection):E_NOINTERFACE;
        if(SUCCEEDED(observed)){sink.Attach(new QualificationSink);observed=connection->Advise(sink,&cookie);}return S_OK;
    }
};

void pump(DWORD duration) {
    const ULONGLONG deadline=GetTickCount64()+duration;
    do { MSG message;
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            TranslateMessage(&message);DispatchMessageW(&message);if(duration&&GetTickCount64()>=deadline)return;
        }
        if(GetTickCount64()>=deadline)return;
        MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);
    }while(true);
}
std::vector<std::wstring> paths(IShellItemArray *array) {
    DWORD count=0;require(array && SUCCEEDED(array->GetCount(&count)),"Cannot read qualification snapshot");
    std::vector<std::wstring> result;
    for(DWORD i=0;i<count;++i){CComPtr<IShellItem> item;CComHeapPtr<wchar_t> path;
        require(SUCCEEDED(array->GetItemAt(i,&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path)),"Cannot read qualification identity");
        result.emplace_back(path);}
    std::sort(result.begin(),result.end());return result;
}
bool selectionSettled(IFolderView2 *view,const std::vector<std::wstring> &wanted) {
    const ULONGLONG deadline=GetTickCount64()+5000;ULONGLONG stable=0;
    do {
        CComPtr<IShellItemArray> selected;int before=-1,after=-1;
        const bool matches=SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&before))&&before==static_cast<int>(wanted.size())
            &&SUCCEEDED(view->GetSelection(FALSE,&selected))&&selected&&paths(selected)==wanted
            &&SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&after))&&after==before;
        if(matches){if(!stable)stable=GetTickCount64();if(GetTickCount64()-stable>=100)return true;}
        else stable=0;
        pump(10);
    }while(GetTickCount64()<deadline);
    return false;
}
// Isolated qualification only: a direct range reset must retain the native
// aggregate accessibility notification as well as its readable selection state.
struct RangeQualificationEvents {
    static RangeQualificationEvents *observer;
    HWND window;HWINEVENTHOOK hook=nullptr;DWORD began=0;unsigned changes=0;bool active=false;
    static void CALLBACK received(HWINEVENTHOOK,DWORD event,HWND source,LONG object,LONG child,DWORD,DWORD time) {
        auto *self=observer;
        if(self&&self->active&&static_cast<DWORD>(time-self->began)<0x80000000u
                &&(source==self->window||IsChild(self->window,source))
                &&event==EVENT_OBJECT_SELECTIONWITHIN&&object==OBJID_CLIENT&&child==CHILDID_SELF)++self->changes;
    }
    explicit RangeQualificationEvents(HWND source):window(source) {
        hook=SetWinEventHook(EVENT_OBJECT_SELECTION,EVENT_OBJECT_SELECTIONWITHIN,nullptr,received,
            GetCurrentProcessId(),GetCurrentThreadId(),WINEVENT_OUTOFCONTEXT);
        require(hook!=nullptr,"Cannot observe range selection notification");observer=this;
    }
    ~RangeQualificationEvents(){if(hook)UnhookWinEvent(hook);observer=nullptr;}
    void begin(){changes=0;began=GetTickCount();active=true;}
    bool finish() {
        const ULONGLONG deadline=GetTickCount64()+5000;
        while(!changes&&GetTickCount64()<deadline)pump(10);
        active=false;return changes!=0;
    }
};
RangeQualificationEvents *RangeQualificationEvents::observer=nullptr;
struct QualificationFixture {
    std::wstring folder;
    std::vector<std::wstring> files;
    ~QualificationFixture(){for(const auto &file:files)DeleteFileW(file.c_str());if(!folder.empty())RemoveDirectoryW(folder.c_str());}
    void create(const std::wstring &root) {
        GUID guid={};wchar_t name[40]={};require(SUCCEEDED(CoCreateGuid(&guid)) && StringFromGUID2(guid,name,_countof(name)),"Cannot name qualification fixture");
        const std::wstring target=root+L"\\qualification-"+name;
        require(CreateDirectoryW(target.c_str(),nullptr)!=FALSE,"Cannot create isolated qualification fixture");folder=target;
        for(unsigned i=0;i<256;++i){wchar_t leaf[32]={};swprintf_s(leaf,L"\\item-%04u.txt",i);
            const std::wstring path=folder+leaf;
            Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr),&closeHandle);
            require(file.get()!=INVALID_HANDLE_VALUE,"Cannot create qualification item");files.push_back(path);
            const char contents='x';DWORD written=0;require(WriteFile(file.get(),&contents,1,&written,nullptr) && written==1,"Cannot write qualification item");}
    }
};
struct QualificationView {
    HWND window=nullptr;ShellViewHost host;CComPtr<QualificationEvents> events;
    ~QualificationView(){if(events)events->detach();host.destroy();if(window)DestroyWindow(window);}
    void create(const std::wstring &path) {
        window=CreateWindowExW(WS_EX_NOACTIVATE,L"FileSpacer Selection Qualification",L"",WS_OVERLAPPEDWINDOW,
            0,0,800,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window!=nullptr && !IsWindowVisible(window),"Cannot create invisible qualification view");
        const RECT bounds={0,0,800,400};const FOLDERSETTINGS settings={FVM_ICON,FWF_AUTOARRANGE};events.Attach(new QualificationEvents);
        require(SUCCEEDED(host.initialize(window,bounds,settings,nullptr,events)),"Cannot initialize qualification Shell host");
        CComPtr<IShellItem> folder;require(SUCCEEDED(SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(&folder)))
            && SUCCEEDED(host.navigate(folder,false,true)),"Cannot navigate qualification fixture");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while((!events->navigated || !events->sink || !events->sink->ready) && GetTickCount64()<deadline)pump(10);
        require(events->navigated && events->sink && events->sink->ready && SUCCEEDED(events->observed),"Qualification enumeration incomplete");
        require(!IsWindowVisible(window),"Qualification unexpectedly became visible");
    }
};
bool qualify(const std::wstring &root) {
    require(SUCCEEDED(OleInitialize(nullptr)),"Qualification COM initialization failed");
    struct Apartment {~Apartment(){OleUninitialize();}} apartment;
    initDPI();QualificationFixture fixture;fixture.create(root);
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=DefWindowProcW;
    type.lpszClassName=L"FileSpacer Selection Qualification";
    require(RegisterClassW(&type)!=0,"Cannot register qualification class");
    struct Class {~Class(){UnregisterClassW(L"FileSpacer Selection Qualification",GetModuleHandleW(nullptr));}} registration;
    struct Mode {FOLDERVIEWMODE mode;int size;bool free;};
    const Mode modes[]={{FVM_ICON,256,false},{FVM_ICON,96,false},{FVM_ICON,48,false},{FVM_SMALLICON,16,false},
        {FVM_TILE,48,false},{FVM_CONTENT,32,false},{FVM_LIST,16,false},{FVM_DETAILS,16,false},{FVM_ICON,48,true},{FVM_TILE,48,true}};
    for(const auto &mode:modes) {
        try {
        const auto beforeRanges=rangeCalls().load(std::memory_order_relaxed);
        QualificationView owner;owner.create(fixture.folder);
        std::unique_ptr<RangeQualificationEvents> selectionEvents;
        if(rangeImage().supported)selectionEvents=std::make_unique<RangeQualificationEvents>(owner.window);
        IShellView *shell=owner.events->shell;CComQIPtr<IFolderView2> view(shell);CComPtr<IShellFolder> folder;
        require(view && SUCCEEDED(view->SetViewModeAndIconSize(mode.mode,mode.size)),"Cannot set qualification mode");pump(100);
        FOLDERVIEWMODE actual=FVM_AUTO;int size=0;
        require(SUCCEEDED(view->GetViewModeAndIconSize(&actual,&size)) && actual==mode.mode && size==mode.size,"Qualification representation differs");
        if(mode.free)require(SUCCEEDED(view->SetCurrentFolderFlags(FWF_AUTOARRANGE,0)),"Cannot qualify free placement");
        require(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&folder))),"Cannot read qualification folder");
        int count=0;require(SUCCEEDED(view->ItemCount(SVGIO_ALLVIEW,&count)) && count==256,"Qualification fixture incomplete");
        using Id=std::unique_ptr<ITEMID_CHILD,decltype(&CoTaskMemFree)>;
        std::vector<Id> owned;std::vector<PCUITEMID_CHILD> ids;std::vector<std::wstring> names;
        for(int i=0;i<count;++i){ITEMID_CHILD *id=nullptr;require(SUCCEEDED(view->Item(i,&id)) && id,"Missing qualification PIDL");
            owned.emplace_back(id,&CoTaskMemFree);ids.push_back(id);
            CComPtr<IShellItem> item;CComHeapPtr<wchar_t> path;
            require(SUCCEEDED(SHCreateItemWithParent(nullptr,folder,id,IID_PPV_ARGS(&item)))
                && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path)),"Missing qualification path");names.emplace_back(path);}
        const DWORD select=static_cast<DWORD>(SVSI_SELECT)|static_cast<DWORD>(SVSI_NOTAKEFOCUS);
        CComQIPtr<IShellFolderView> bulk(shell);
        auto allNames=names;std::sort(allNames.begin(),allNames.end());
        if(image().selectAllSupported) {
            // Select() itself calls the native _FocusOnSomething after its
            // selection notification. Establish that public baseline before
            // comparing keyboard focus; the adaptation does not own activation.
            require(bulk&&SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Native baseline select failed");
            CComPtr<IShellItemArray> all;
            require(SUCCEEDED(view->GetSelection(FALSE,&all))&&paths(all)==allNames,"Native baseline identities differ");
            require(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Native baseline clear failed");
        }
        require(SUCCEEDED(shell->SelectItem(ids[7],static_cast<SVSIF>(SVSI_FOCUSED|SVSI_NOTAKEFOCUS))),"Cannot seed qualification focus");
        const HWND focus=GetFocus();auto current=[&]{return IsWindow(owner.window) && owner.events->shell.p==shell;};
        pump(100);
        ShellSelectionClear candidate;candidate.bind(shell,current);require(candidate.accelerated(),"Qualification did not bind acceleration");
        if(image().selectAllSupported) {
            int before=-1;CComHeapPtr<ITEMID_CHILD> beforeId;
            require(SUCCEEDED(view->GetFocusedItem(&before))&&before>=0&&SUCCEEDED(view->Item(before,&beforeId))
                &&ILIsEqual(beforeId,ids[7]),"Linear selection focus precondition missing");
            require(bulk&&SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS))&&candidate.acceleratedSelectAllCount()==1,
                "Qualification did not use native linear selection");
            CComPtr<IShellItemArray> all;
            require(SUCCEEDED(view->GetSelection(FALSE,&all))&&paths(all)==allNames,"Linear selection identities differ");
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focusedId;
            const bool focusValid=SUCCEEDED(view->GetFocusedItem(&focused))&&focused>=0&&SUCCEEDED(view->Item(focused,&focusedId));
            const bool sameId=focusValid&&ILIsEqual(focusedId,ids[7]);
            const std::string focusError="Linear focus differs: mode="+std::to_string(mode.mode)+" size="+std::to_string(mode.size)
                +" before="+std::to_string(before)+" after="+std::to_string(focused)+" sameId="+std::to_string(sameId)
                +" sameKeyboard="+std::to_string(GetFocus()==focus);
            require(sameId&&GetFocus()==focus,focusError.c_str());
            require(SUCCEEDED(candidate.clear(shell,current)),"Linear selection clear failed");
        }
        for(int selected:{0,1,32,64,128}) {
            require(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Cannot clear qualification setup");
            std::vector<PCUITEMID_CHILD> chosen;std::vector<std::wstring> wanted;
            for(int i=0;i<selected;++i){const size_t index=(static_cast<size_t>(i)*37)%ids.size();chosen.push_back(ids[index]);wanted.push_back(names[index]);}
            std::sort(wanted.begin(),wanted.end());
            if(selected)require(SUCCEEDED(view->SelectAndPositionItems(static_cast<UINT>(chosen.size()),chosen.data(),nullptr,select)),"Cannot select qualification subset");
            pump(100);require(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count)) && count==selected,"Qualification selection setup differs");
            CComPtr<IShellItemArray> retained;
            if(selected){require(SUCCEEDED(view->GetSelection(FALSE,&retained)) && paths(retained)==wanted,"Qualification identities differ");
                CComPtr<IShellItemArray> parent;require(SUCCEEDED(view->GetSelection(TRUE,&parent)) && paths(parent)==wanted,"Qualification parent cache differs");}
            const bool observeRange=selectionEvents&&selected>=128;
            if(observeRange){pump(100);selectionEvents->begin();}
            bool applied=false;require(SUCCEEDED(candidate.clear(shell,current,&applied)) && (!selected || applied),"Qualification did not clear through candidate");
            if(selected){bool known=false;const bool released=candidate.backingBufferReleased(known);
                require(known && released,"Native clear did not release its own backing allocation");}
            pump(100);require(SUCCEEDED(view->ItemCount(SVGIO_SELECTION,&count)) && count==0,"Qualification clear left selection");
            if(observeRange)require(selectionEvents->finish(),"Range clear lost aggregate selection notification");
            CComPtr<IShellItemArray> empty;const HRESULT result=view->GetSelection(FALSE,&empty);
            require((result==HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && !empty) || (SUCCEEDED(result) && (!empty || paths(empty).empty())),"Qualification ordinary cache stale");
            CComPtr<IShellItemArray> parent;require(SUCCEEDED(view->GetSelection(TRUE,&parent)) && paths(parent)==std::vector<std::wstring>{fixture.folder},"Qualification parent cache stale");
            if(retained)require(paths(retained)==wanted,"Qualification corrupted retained snapshot");
            for(auto id:ids){DWORD state=0;require(SUCCEEDED(view->GetSelectionState(id,&state)) && !(state&SVSI_SELECT),"Qualification item remained selected");}
            int focused=-1;CComHeapPtr<ITEMID_CHILD> focusId;
            require(SUCCEEDED(view->GetFocusedItem(&focused)) && focused>=0 && SUCCEEDED(view->Item(focused,&focusId))
                && ILIsEqual(focusId,ids[7]) && GetFocus()==focus,"Qualification clear changed focus");
            require(SUCCEEDED(shell->SelectItem(ids[7],select)),"Qualification subsequent selection failed");
            CComPtr<IShellItemArray> next;require(SUCCEEDED(view->GetSelection(FALSE,&next)) && paths(next)==std::vector<std::wstring>{names[7]},"Qualification subsequent cache failed");
            require(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Qualification subsequent clear failed");pump(100);
        }
        if(image().selectAllSupported)for(int quantity:{1,32,128}) {
            require(SUCCEEDED(view->SelectAndPositionItems(static_cast<UINT>(quantity),ids.data(),nullptr,select)),"Cannot seed partial all-selection qualification");
            std::vector<std::wstring> wanted(names.begin(),names.begin()+quantity);std::sort(wanted.begin(),wanted.end());
            CComPtr<IShellItemArray> retained;
            require(SUCCEEDED(view->GetSelection(FALSE,&retained))&&paths(retained)==wanted,"Partial qualification identities differ");
            const uint64_t before=candidate.acceleratedSelectAllCount();
            require(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS))&&candidate.acceleratedSelectAllCount()==before+1,
                "Partial-to-all qualification did not use acceleration");
            require(selectionSettled(view,allNames)&&paths(retained)==wanted,"Partial-to-all qualification corrupted a snapshot");
            require(SUCCEEDED(candidate.clear(shell,current)),"Partial-to-all qualification clear failed");
        }
        // Keep sorting separate from the focus/clear scenario: the native sort
        // itself can reset focus, even with no optional adaptation installed.
        if(image().selectAllSupported) {
            candidate.reset();
            require(bulk&&SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS)),"Native baseline select failed");
            const SORTCOLUMN descending={PKEY_ItemNameDisplay,SORT_DESCENDING};
            require(SUCCEEDED(view->SetSortColumns(&descending,1)),"Native baseline sort failed");
            require(selectionSettled(view,allNames),"Native baseline selection changed after sort");
            require(SUCCEEDED(shell->SelectItem(nullptr,SVSI_DESELECTOTHERS)),"Native baseline clear failed");
            candidate.bind(shell,current);
            require(SUCCEEDED(bulk->Select(SFVS_SELECT_ALLITEMS))&&candidate.acceleratedSelectAllCount()==1,
                "Linear sort qualification did not use acceleration");
            const SORTCOLUMN ascending={PKEY_ItemNameDisplay,SORT_ASCENDING};
            require(SUCCEEDED(view->SetSortColumns(&ascending,1)),"Linear selection sort failed");
            require(selectionSettled(view,allNames),"Linear selection changed after sort");
            require(SUCCEEDED(candidate.clear(shell,current)),"Linear sort qualification clear failed");
        }
        owner.events->sink->ready=false;require(SUCCEEDED(shell->Refresh()),"Qualification refresh failed");
        const ULONGLONG deadline=GetTickCount64()+45000;
        while(!owner.events->sink->ready && GetTickCount64()<deadline)pump(10);
        require(owner.events->sink->ready,"Qualification refresh incomplete");
        candidate.reset();require(candidate.clear(shell,[]{return false;})==E_ABORT,"Qualification stale continuation accepted");
        if(rangeImage().supported&&mode.free&&mode.mode==FVM_TILE)
            require(rangeCalls().load(std::memory_order_relaxed)>beforeRanges,"Free Tiles range qualification not exercised");
        }catch(const std::exception &error) {
            throw std::runtime_error("Qualification mode="+std::to_string(mode.mode)+" size="+std::to_string(mode.size)
                +" free="+std::to_string(mode.free)+": "+error.what());
        }
    }
    return true;
}
} // namespace

UINT readyMessage() {
    static const UINT message=RegisterWindowMessageW(L"FileSpacer.SelectionCapabilityReady.v1");
    return message;
}
const Image &image() {
    return resolution.load(std::memory_order_acquire)==Resolution::Private?selectedImage:publicImage;
}
const RangeImage &rangeImage() {
    return rangeReady.load(std::memory_order_acquire)?selectedRange:publicRange;
}
bool rangeResolved(){return rangeFinished.load(std::memory_order_acquire);}
bool resolved() { return resolution.load(std::memory_order_acquire)!=Resolution::Running; }
std::wstring cacheFile(const std::wstring &root) { return root+L"\\"+identity().cacheKey+L".bin"; }
namespace {
struct ResolveContext { std::wstring root; bool custom,background; DWORD uiThread; };
BOOL CALLBACK notifyWindow(HWND window,LPARAM message) {
    PostMessageW(window,static_cast<UINT>(message),0,0);return TRUE;
}
void publish(Image candidate,DWORD uiThread) {
    // Exactly one writer, at most one publication. Both images remain alive
    // for the process lifetime; an in-flight call retains its chosen path.
    selectedImage=candidate;
    resolution.store(Resolution::Private,std::memory_order_release);
    if(readyMessage())EnumThreadWindows(uiThread,notifyWindow,readyMessage());
}
void publishRange(RangeImage candidate,DWORD uiThread) {
    selectedRange=candidate;rangeReady.store(true,std::memory_order_release);
    if(readyMessage())EnumThreadWindows(uiThread,notifyWindow,readyMessage());
}
bool savedPublic(const std::wstring &file) {
    std::ifstream data(file.c_str(),std::ios::binary);char magic[10]={};data.read(magic,sizeof(magic));
    return data && std::memcmp(magic,"FSPUBLIC01",sizeof(magic))==0;
}
bool producerActive(const std::wstring &root) {
    Handle lock(CreateFileW((root+L"\\discovery.lock").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr),&closeHandle);
    if(lock.get()!=INVALID_HANDLE_VALUE)return false;
    return GetLastError()==ERROR_SHARING_VIOLATION;
}
Handle startWorker(const ResolveContext &context) {
    std::vector<wchar_t> executable(32768);
    const DWORD size=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!size || size>=executable.size())return Handle(nullptr,&closeHandle);
    std::wstring command=L"\""+std::wstring(executable.data(),size)+L"\" ";
    command+=context.custom?L"/test /selection-discovery \""+context.root+L"\"":L"/selection-discovery";
    STARTUPINFOW startup={};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION child={};
    if(!CreateProcessW(executable.data(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS,
            nullptr,nullptr,&startup,&child))return Handle(nullptr,&closeHandle);
    Handle thread(child.hThread,&closeHandle);return Handle(child.hProcess,&closeHandle);
}
struct DirectoryWatch {
    Handle directoryHandle{nullptr,&closeHandle},event{nullptr,&closeHandle};
    std::array<DWORD,1024> buffer={};OVERLAPPED operation={};bool pending=false;
    ~DirectoryWatch(){if(pending){CancelIoEx(directoryHandle.get(),&operation);DWORD bytes=0;GetOverlappedResult(directoryHandle.get(),&operation,&bytes,TRUE);}}
    bool open(const std::wstring &root) {
        directoryHandle.reset(CreateFileW(root.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr));
        event.reset(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        return directoryHandle.get()!=INVALID_HANDLE_VALUE && event && arm();
    }
    bool arm() {
        ResetEvent(event.get());operation={};operation.hEvent=event.get();
        const BOOL queued=ReadDirectoryChangesW(directoryHandle.get(),buffer.data(),static_cast<DWORD>(sizeof(buffer)),FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE,nullptr,&operation,nullptr);
        pending=queued || GetLastError()==ERROR_IO_PENDING;
        return pending;
    }
    bool next() {
        DWORD bytes=0;const BOOL result=GetOverlappedResult(directoryHandle.get(),&operation,&bytes,FALSE);
        const DWORD error=result?ERROR_SUCCESS:GetLastError();
        if(error==ERROR_IO_INCOMPLETE)return false;
        pending=false;return (result || error==ERROR_NOTIFY_ENUM_DIR) && arm();
    }
};
DWORD WINAPI resolve(void *raw) {
    std::unique_ptr<ResolveContext> context(static_cast<ResolveContext *>(raw));
    struct FinishRange {~FinishRange(){rangeFinished.store(true,std::memory_order_release);}} finish;
    bool published=false;
    try {
        if(!limitedProcess() || !cfgCompatible() || (context->custom && context->root.empty()))throw std::runtime_error("Public path selected");
        if(!context->custom)context->root=defaultRoot();
        const std::wstring final=cacheFile(context->root),negative=final+L".public";
        const std::wstring rangeFinal=rangeCacheFile(final);
        const auto adopt=[&]() {
            const Image candidate=published?image():Image::discover(final);
            if(!candidate.supported)return false;
            const RangeImage range=rangeFinal.empty()?RangeImage{}:RangeImage::discover(rangeFinal);
            if(range.supported)publishRange(range,context->uiThread);
            if(!published){publish(candidate,context->uiThread);published=true;}
            // A missing optional range never takes an existing private path
            // away. Its independent immutable publication can arrive later.
            return range.supported||rangeFinal.empty()||savedRangePublic(rangeFinal+L".public")||!context->background;
        };
        if(adopt())return 0;
        if(!context->background || savedPublic(negative))throw std::runtime_error("No discovery required");
        directory(context->root);DirectoryWatch watch;
        const bool watching=watch.open(context->root);
        // Arm before rechecking, so another process cannot publish between
        // our initial lookup and the notification subscription.
        if(adopt())return 0;
        Handle child(nullptr,&closeHandle);
        if(!producerActive(context->root)) {
            const std::wstring attempt=published&&!rangeFinal.empty()?rangeFinal+L".attempt":context->root+L"\\"+identity().cacheKey+L".attempt";
            if(recentAttempt(attempt)&&(!published||savedRangePublic(attempt)))throw std::runtime_error("Transient failure backoff");
            child=startWorker(*context);
            if(!child)throw std::runtime_error("Cannot start isolated discovery");
        }
        for(;;) {
            if(!watching) {
                if(child)WaitForSingleObject(child.get(),INFINITE);
                if(adopt())return 0;
                break;
            }
            HANDLE handles[2]={watch.event.get(),child.get()};
            const DWORD waited=WaitForMultipleObjects(child?2:1,handles,FALSE,INFINITE);
            if(waited==WAIT_OBJECT_0){if(!watch.next())break;}
            else if(child && waited==WAIT_OBJECT_0+1)child.reset();
            else break;
            if(adopt())return 0;
            if(savedPublic(negative))break;
            if(!child && !producerActive(context->root)) {
                // A competing producer can publish after our read, then drop
                // its lock before this check. Read once more after its exit.
                if(adopt())return 0;
                break;
            }
        }
    }catch(const std::exception &){ /* Public is already callable; no UI wait. */ }
    if(!published)resolution.store(Resolution::Public,std::memory_order_release);return 0;
}
DWORD WINAPI discoveryWatchdog(void *stop) {
    if(WaitForSingleObject(static_cast<HANDLE>(stop),180000)==WAIT_TIMEOUT)
        TerminateProcess(GetCurrentProcess(),ERROR_TIMEOUT); // Only the isolated discovery process.
    return 0;
}
struct Watchdog {
    Handle stop{CreateEventW(nullptr,TRUE,FALSE,nullptr),&closeHandle};
    Handle thread{nullptr,&closeHandle};
    Watchdog(){require(stop!=nullptr,"Cannot create discovery watchdog");
        thread.reset(CreateThread(nullptr,0,discoveryWatchdog,stop.get(),0,nullptr));require(thread!=nullptr,"Cannot start discovery watchdog");}
    ~Watchdog(){SetEvent(stop.get());WaitForSingleObject(thread.get(),INFINITE);}
};
} // namespace
void initialize(const wchar_t *cacheRoot,bool background) {
    std::call_once(initialized,[&] {
        try {
            auto context=std::make_unique<ResolveContext>();context->custom=cacheRoot!=nullptr;
            if(cacheRoot)context->root=cacheRoot;
            context->background=background;context->uiThread=GetCurrentThreadId();
            (void)readyMessage();
            if(QueueUserWorkItem(resolve,context.get(),WT_EXECUTELONGFUNCTION)){(void)context.release();return;}
        }catch(const std::bad_alloc &){ }
        rangeFinished.store(true,std::memory_order_release);
        resolution.store(Resolution::Public,std::memory_order_release);
    });
}
int runWorker(const wchar_t *cacheRoot) {
    if(!limitedProcess() || !cfgCompatible())return ERROR_ACCESS_DENIED;
    const bool background=SetPriorityClass(GetCurrentProcess(),PROCESS_MODE_BACKGROUND_BEGIN)!=FALSE;
    struct Priority {bool changed;~Priority(){if(changed)SetPriorityClass(GetCurrentProcess(),PROCESS_MODE_BACKGROUND_END);}} priority={background};
    std::wstring candidate,attempt,negative;bool rangePhase=false;
    try {
        Watchdog watchdog;
        const std::wstring root=cacheRoot?cacheRoot:defaultRoot();directory(root);
        const Identity id=identity();const std::wstring final=cacheFile(root);negative=final+L".public";
        Handle lock(CreateFileW((root+L"\\discovery.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
            OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr),&closeHandle);
        if(lock.get()==INVALID_HANDLE_VALUE)return ERROR_BUSY;
        if(savedPublic(final+L".public"))return 0;
        const Image previous=Image::discover(final);
        const std::wstring rangeFinal=rangeCacheFile(final);
        if(previous.supported&&(rangeFinal.empty()||RangeImage::discover(rangeFinal).supported||savedRangePublic(rangeFinal+L".public")))return 0;
        if(previous.supported){rangePhase=true;negative=rangeFinal+L".public";}
        attempt=previous.supported&&!rangeFinal.empty()?rangeFinal+L".attempt":root+L"\\"+id.cacheKey+L".attempt";
        if(!cacheRoot&&recentAttempt(attempt)&&(!rangePhase||savedRangePublic(attempt)))return ERROR_RETRY;
        if(rangePhase)saveRangeRecord(attempt,"Discovery running");
        else {std::ofstream mark(attempt.c_str(),std::ios::trunc);mark<<"Discovery running\n";}
        if(!previous.supported) {
            const std::wstring pdbDirectory=root+L"\\symbols\\shell32.pdb\\"+id.key;directory(pdbDirectory);
            downloadPdb(id,pdbDirectory+L"\\shell32.pdb");
            const auto foundSymbols=symbols(id,pdbDirectory);const auto fields=extract(id,foundSymbols);
            candidate=final+L".pending";save(id,fields,candidate);
        }
        std::call_once(initialized,[&]{selectedImage=previous.supported?previous:Image::discover(candidate);
            resolution.store(selectedImage.supported?Resolution::Private:Resolution::Public,std::memory_order_release);});
        supported(selectedImage.supported,"Loaded-image/CFG validation rejected discovery");
        if(!previous.supported) {
            require(qualify(root),"Real Shell qualification failed");
            require(identity().key==id.key,"Shell image changed during qualification");
            require(MoveFileExW(candidate.c_str(),final.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot publish qualified cache");
            candidate.clear();
        }
        if(!rangeFinal.empty()) {
            rangePhase=true;
            negative=rangeFinal+L".public";candidate=rangeFinal+L".pending";
            const std::wstring nextAttempt=rangeFinal+L".attempt";
            if(attempt!=nextAttempt){DeleteFileW(attempt.c_str());attempt=nextAttempt;}
            saveRangeRecord(attempt,"Discovery running");
            discoverRange(root,candidate);
            selectedRange=RangeImage::discover(candidate);supported(selectedRange.supported,"Loaded range qualification rejected discovery");
            rangeReady.store(true,std::memory_order_release);
            // API/qualification failures can be transient (busy Shell, UIA,
            // unavailable desktop). Only structural Unsupported failures get a
            // permanent negative record; this failure uses the scoped backoff.
            require(qualify(root),"Real Shell range qualification failed");
            require(selectedRange.qualifies(),"Control image changed during qualification");
            require(MoveFileExW(candidate.c_str(),rangeFinal.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot publish qualified range cache");
            candidate.clear();
        }
        DeleteFileW(attempt.c_str());return 0;
    }catch(const Unsupported &error){
        if(!candidate.empty())DeleteFileW(candidate.c_str());
        if(rangePhase)saveRangeRecord(negative,error.what());
        else if(!negative.empty()){std::ofstream report(negative.c_str(),std::ios::binary|std::ios::trunc);report<<"FSPUBLIC01"<<error.what()<<"\n";}
        if(!attempt.empty())DeleteFileW(attempt.c_str());
        return ERROR_NOT_SUPPORTED;
    }catch(const std::exception &error){
        if(!candidate.empty())DeleteFileW(candidate.c_str());
        if(rangePhase)saveRangeRecord(attempt,error.what());
        else if(!attempt.empty()){std::ofstream report(attempt.c_str(),std::ios::trunc);report<<error.what()<<"\n";}
        return ERROR_NOT_SUPPORTED;
    }
}
} } // namespace filespacer::selectionDetail
