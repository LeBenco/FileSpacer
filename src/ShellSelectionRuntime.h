#pragma once
#include <common.h>
#include <atlbase.h>
#include <shlobj.h>
#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <type_traits>
#include <limits>
#include <dpa_dsa.h>

// Optional private ABI, discovered and qualified in an isolated process on the
// current machine. No version/RVA catalogue or private ClearSelection call,
// code modification, CFG exemption or cross-process memory access.
namespace filespacer { namespace selectionDetail {
template<class T> bool read(const void *address,T &value) {
    static_assert(std::is_trivially_copyable<T>::value,"Raw values only");
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),address,static_cast<void *>(std::addressof(value)),sizeof(T),&copied)&&copied==sizeof(T);
}
enum Field : unsigned char {OnClear,Select,Purge,Init,QueryService,Notify,Cancel,HostQI,EnumGlobal,DeleteGlobal,
    Callback,NotifyTable,HostTable,HostQueryObject,ViewQueryObject,ManagerTable,ManagerQI,Retain,Release,
    HostOffset,ManagerOffset,ArrayOffset,NotifySlot,CancelSlot,Procedure,ArrayGetter,FieldCount};
inline bool writable(const void *pointer,SIZE_T length) {
    const auto *cursor=static_cast<const unsigned char *>(pointer);
    if(length>(std::numeric_limits<ULONG_PTR>::max)()-reinterpret_cast<ULONG_PTR>(cursor))return false;
    while(length){MEMORY_BASIC_INFORMATION memory={};
        if(!VirtualQuery(cursor,&memory,sizeof(memory))||memory.State!=MEM_COMMIT
                ||memory.Type!=MEM_PRIVATE||(memory.Protect!=PAGE_READWRITE&&memory.Protect!=PAGE_WRITECOPY))return false;
        const SIZE_T offset=reinterpret_cast<ULONG_PTR>(cursor)-reinterpret_cast<ULONG_PTR>(memory.BaseAddress);
        if(offset>=memory.RegionSize)return false;
        const SIZE_T portion=(std::min)(length,memory.RegionSize-offset);cursor+=portion;length-=portion;}
    return true;
}
struct ArrayLayout {DWORD data=0,stride=0;};
struct ArraySnapshot {int count=0;unsigned char *data=nullptr;DWORD stride=0;};
inline bool snapshotArray(const void *array,DWORD dataOffset,DWORD strideOffset,ArraySnapshot &snapshot) {
    std::array<unsigned char,72> bytes={};
    if(dataOffset>64||strideOffset>64)return false;
    const SIZE_T length=(std::max)(static_cast<SIZE_T>(dataOffset)+sizeof(snapshot.data),static_cast<SIZE_T>(strideOffset)+sizeof(snapshot.stride));
    SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),array,bytes.data(),length,&copied)||copied!=length)return false;
    std::memcpy(&snapshot.count,bytes.data(),sizeof(snapshot.count));
    std::memcpy(static_cast<void *>(std::addressof(snapshot.data)),bytes.data()+dataOffset,sizeof(snapshot.data));
    std::memcpy(&snapshot.stride,bytes.data()+strideOffset,sizeof(snapshot.stride));return true;
}
inline bool decodeArrayGetter(const unsigned char *code,SIZE_T length,ArrayLayout &layout) {
    const unsigned char prefix[]={0x48,0x85,0xc9,0x74,0x14,0x85,0xd2,0x78,0x10,0x3b,0x11,0x7d,0x0c};
    if(length<24||std::memcmp(code,prefix,sizeof(prefix))!=0||code[19]!=0x48||code[20]!=0x03||code[21]!=0x41||code[23]!=0xc3)return false;
    ArrayLayout value;value.data=code[22];
    if(code[13]==0x8b&&code[14]==0x41&&code[16]==0x0f&&code[17]==0xaf&&code[18]==0xc2)value.stride=code[15];
    else if(code[13]==0x0f&&code[14]==0xaf&&code[15]==0x51&&code[17]==0x8b&&code[18]==0xc2)value.stride=code[16];
    else return false;
    if(value.data<sizeof(void *)||value.data>64||value.stride<sizeof(void *)||value.stride>64)return false;
    layout=value;return true;
}
inline bool decodeInlineArrayLoop(const unsigned char *code,SIZE_T length,DWORD function,DWORD callback,DWORD member,ArrayLayout &layout) {
    const unsigned char prefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x79};
    if(length<19||std::memcmp(code,prefix,sizeof(prefix))!=0||code[18]!=member)return false;
    bool found=false;ArrayLayout value;
    for(SIZE_T i=19;i+15<length;++i){
        if(code[i]!=0x0f||code[i+1]!=0xaf||code[i+2]!=0x4f||code[i+4]!=0x48||code[i+5]!=0x03
                ||code[i+6]!=0x4f||code[i+8]!=0x33||code[i+9]!=0xd2||code[i+10]!=0xe8)continue;
        LONG displacement=0;std::memcpy(&displacement,code+i+11,4);
        if(found||static_cast<LONGLONG>(function)+static_cast<LONGLONG>(i)+15+displacement!=callback)return false;
        value.stride=code[i+3];value.data=code[i+7];found=true;
    }
    if(!found||value.data<sizeof(void *)||value.data>64||value.stride<sizeof(void *)||value.stride>64)return false;
    layout=value;return true;
}
struct Image {
    unsigned char *base=nullptr;DWORD size=0;std::array<DWORD,FieldCount> fields={};
    void *enumerate=nullptr,*erase=nullptr;
    bool clearViaCount=false;
    bool manualEnumeration=false;DWORD dataOffset=0,strideOffset=0;
    bool supported=false;const char *reason="missing discovery data";
    bool range(DWORD rva,SIZE_T length)const{return rva<size&&length<=size-rva;}
    bool cfg(DWORD rva)const {
        IMAGE_DOS_HEADER dos={};IMAGE_NT_HEADERS64 nt={};
        if(!read(base,dos)||!read(base+dos.e_lfanew,nt))return false;
        const auto &dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
        IMAGE_LOAD_CONFIG_DIRECTORY64 config={};
        if(!range(dir.VirtualAddress,sizeof(config))||dir.Size<148||!read(base+dir.VirtualAddress,config)
                ||!config.GuardCFFunctionTable||config.GuardCFFunctionCount>size/4)return false;
        const SIZE_T stride=4+(config.GuardFlags>>28u);
        const auto address=reinterpret_cast<ULONG_PTR>(base);
        if(config.GuardCFFunctionTable<address||config.GuardCFFunctionTable-address>=size)return false;
        const DWORD table=static_cast<DWORD>(config.GuardCFFunctionTable-address);
        if(!range(table,static_cast<SIZE_T>(config.GuardCFFunctionCount)*stride))return false;
        // Sorted PE GuardCFFunctionTable. Discovery happens once, not per clear.
        SIZE_T low=0,high=static_cast<SIZE_T>(config.GuardCFFunctionCount);
        while(low<high){const SIZE_T middle=low+(high-low)/2;DWORD target=0;
            if(!read(base+table+middle*stride,target))return false;
            if(target<rva)low=middle+1;else high=middle;}
        DWORD target=0;unsigned char flags=0;
        return low<config.GuardCFFunctionCount&&read(base+table+low*stride,target)&&target==rva
            &&(stride==4||(read(base+table+low*stride+4,flags)&&(flags&1u)==0));
    }
    static Image discover(const std::wstring &input) {
        Image result;
        HANDLE rawToken=nullptr;result.reason="process token cannot be qualified";
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&rawToken))return result;
        std::unique_ptr<void,decltype(&CloseHandle)> token(rawToken,&CloseHandle);
        TOKEN_ELEVATION elevation={};DWORD returned=0;
        if(!GetTokenInformation(token.get(),TokenElevation,&elevation,sizeof(elevation),&returned)||elevation.TokenIsElevated)return result;
        result.reason="missing discovery data";
        result.clearViaCount=true;result.manualEnumeration=true;
        const SIZE_T length=input.size();
        if(!length)return result;
        std::ifstream data(input.c_str(),std::ios::binary);char magic[8]={};DWORD count=0,expectedSize=0,age=0;GUID guid={};
        data.read(magic,sizeof(magic));data.read(reinterpret_cast<char *>(&expectedSize),sizeof(expectedSize));
        data.read(reinterpret_cast<char *>(&guid),sizeof(guid));data.read(reinterpret_cast<char *>(&age),sizeof(age));
        data.read(reinterpret_cast<char *>(&count),sizeof(count));
        if(!data||std::memcmp(magic,"FSDISC01",8)!=0||count!=FieldCount){result.reason="invalid discovery schema";return result;}
        data.read(reinterpret_cast<char *>(result.fields.data()),sizeof(result.fields));
        if(!data||data.peek()!=std::char_traits<char>::eof()){result.reason="truncated or trailing discovery data";return result;}
        wchar_t system[MAX_PATH]={},loaded[MAX_PATH]={};
        const UINT systemLength=GetSystemDirectoryW(system,_countof(system));
        if(!systemLength||systemLength>=_countof(system))return result;
        const std::wstring path=std::wstring(system)+L"\\shell32.dll";
        HMODULE module=GetModuleHandleW(L"shell32.dll");
        const DWORD loadedLength=GetModuleFileNameW(module,loaded,_countof(loaded));
        if(!module||!loadedLength||loadedLength>=_countof(loaded)||_wcsicmp(loaded,path.c_str())!=0)return result;
        result.base=reinterpret_cast<unsigned char *>(module);
        IMAGE_DOS_HEADER dos={};IMAGE_NT_HEADERS64 nt={};
        result.reason="image identity or CFG mismatch";
        if(!read(result.base,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<64||dos.e_lfanew>4096
                ||!read(result.base+dos.e_lfanew,nt)||nt.Signature!=IMAGE_NT_SIGNATURE
                ||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC
                ||nt.OptionalHeader.SizeOfImage!=expectedSize||expectedSize<sizeof(nt))return result;
        result.size=expectedSize;
        const auto &debug=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];bool identity=false;
        if(!result.range(debug.VirtualAddress,debug.Size)||debug.Size>32*sizeof(IMAGE_DEBUG_DIRECTORY))return result;
        for(DWORD offset=0;offset+sizeof(IMAGE_DEBUG_DIRECTORY)<=debug.Size;offset+=sizeof(IMAGE_DEBUG_DIRECTORY)){
            IMAGE_DEBUG_DIRECTORY entry={};struct CodeView {DWORD signature=0;GUID guid={};DWORD age=0;} code={};
            if(!read(result.base+debug.VirtualAddress+offset,entry))return result;
            if(entry.Type==IMAGE_DEBUG_TYPE_CODEVIEW&&result.range(entry.AddressOfRawData,sizeof(code))
                    &&read(result.base+entry.AddressOfRawData,code)&&code.signature==0x53445352
                    &&code.age==age&&IsEqualGUID(code.guid,guid))identity=true;
        }
        PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY policy={};
        if(!identity||!GetProcessMitigationPolicy(GetCurrentProcess(),ProcessControlFlowGuardPolicy,&policy,sizeof(policy))
                ||!policy.EnableControlFlowGuard)return result;
        PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY known={};known.EnableControlFlowGuard=1;known.EnableExportSuppression=1;known.StrictMode=1;
        if(policy.Flags&~known.Flags)return result; // Unknown/XFG policy: keep public calls.
        result.manualEnumeration=true;result.clearViaCount=true;

        result.clearViaCount=result.clearViaCount||result.manualEnumeration;
        for(int i=0;i<=Release;++i)if(!(result.manualEnumeration&&(i==EnumGlobal||i==DeleteGlobal))&&!result.range(result.fields[i],64))return result;
        if((result.fields[Procedure]!=1&&result.fields[Procedure]!=2)||result.fields[NotifySlot]>127||result.fields[CancelSlot]>127
                ||result.fields[HostOffset]>4096||result.fields[ManagerOffset]>4096||result.fields[ArrayOffset]>4096)return result;
        // Cross-check the discovered field/callback against loaded instructions,
        // not only against an external file. Recognize the inspected helper and
        // inlined compiler forms; unknown forms fail closed.
        for(Field field:{Callback,Notify,Cancel,ManagerQI,Retain,Release})if(!result.cfg(result.fields[field]))return result;
        // These are Shell32's own linked DSA implementations, not necessarily
        // Comctl32 exports. Capture its actual typed native call targets once.
        if(!result.manualEnumeration){
        if(!read(result.base+result.fields[EnumGlobal],result.enumerate)||!read(result.base+result.fields[DeleteGlobal],result.erase))return result;
        for(const void *target:{result.enumerate,result.erase}){
            const ULONG_PTR value=reinterpret_cast<ULONG_PTR>(target),start=reinterpret_cast<ULONG_PTR>(result.base);
            if(value<start||value-start>=result.size||!result.cfg(static_cast<DWORD>(value-start)))return result;
        }
        }
        if(result.fields[Procedure]==1){
        unsigned char purge[62]={};
        const unsigned char prefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8d,0x15};
        if(!read(result.base+result.fields[Purge],purge)||std::memcmp(purge,prefix,sizeof(prefix))!=0)return result;
        LONG displacement=0;std::memcpy(&displacement,purge+12,4);
        const auto relative=[&](DWORD start,DWORD next,LONG delta,DWORD wanted){
            return static_cast<LONGLONG>(start)+next+delta==wanted;};
        if(!relative(result.fields[Purge],16,displacement,result.fields[Callback])
                ||purge[16]!=0x48||purge[17]!=0x8b||purge[18]!=0x49||purge[19]!=result.fields[ArrayOffset]
                ||purge[23]!=0x48||purge[24]!=0xff||purge[25]!=0x15)return result;
        std::memcpy(&displacement,purge+26,4);
        if(!relative(result.fields[Purge],30,displacement,result.fields[EnumGlobal])
                ||purge[35]!=0x48||purge[36]!=0x8b||purge[37]!=0x4b||purge[38]!=result.fields[ArrayOffset]
                ||purge[44]!=0x48||purge[45]!=0xff||purge[46]!=0x15)return result;
        std::memcpy(&displacement,purge+47,4);
        if(!relative(result.fields[Purge],51,displacement,result.fields[DeleteGlobal]))return result;
        }else{
            unsigned char purge[128]={};
            ArrayLayout layout;
            if(!result.range(result.fields[Purge],sizeof(purge))||!read(result.base+result.fields[Purge],purge)
                    ||!decodeInlineArrayLoop(purge,sizeof(purge),result.fields[Purge],result.fields[Callback],result.fields[ArrayOffset],layout))return result;
            result.dataOffset=layout.data;result.strideOffset=layout.stride;
        }
        if(result.clearViaCount&&result.fields[Procedure]==1){
            unsigned char getter[32]={};ArrayLayout layout;
            if(!result.range(result.fields[ArrayGetter],sizeof(getter))||!read(result.base+result.fields[ArrayGetter],getter)
                    ||!decodeArrayGetter(getter,sizeof(getter),layout))return result;
            result.dataOffset=layout.data;result.strideOffset=layout.stride;
        }
        // Native OnClear reads the manager and its selected-array count. Match
        // the register/operand form, never a Windows version or prepared RVA.
        unsigned char clear[256]={};bool operands=false;
        if(!result.range(result.fields[OnClear],sizeof(clear))||!read(result.base+result.fields[OnClear],clear))return result;
        for(size_t i=0;i+18<sizeof(clear);++i)if(clear[i]==0x48&&clear[i+1]==0x8b&&clear[i+2]==0x89){
            DWORD member=0;std::memcpy(&member,clear+i+3,4);
            if(member==result.fields[HostOffset]+result.fields[ManagerOffset]&&clear[i+7]==0x48&&clear[i+8]==0x8b
                    &&clear[i+9]==0x41&&clear[i+10]==result.fields[ArrayOffset]
                    &&clear[i+11]==0x48&&clear[i+12]==0x85&&clear[i+13]==0xc0
                    &&clear[i+14]==0x74&&clear[i+16]==0x8b&&clear[i+17]==0x30)operands=true;
        }
        for(size_t i=0;i+18<sizeof(clear);++i)if(clear[i]==0x48&&clear[i+1]==0x8b&&clear[i+2]==0x81){
            DWORD member=0;std::memcpy(&member,clear+i+3,4);
            if(member==result.fields[HostOffset]+result.fields[ManagerOffset]&&clear[i+7]==0x48&&clear[i+8]==0x8b
                    &&clear[i+9]==0x48&&clear[i+10]==result.fields[ArrayOffset]
                    &&clear[i+11]==0x48&&clear[i+12]==0x85&&clear[i+13]==0xc9
                    &&clear[i+14]==0x74&&clear[i+16]==0x8b&&clear[i+17]==0x31)operands=true;
        }
        if(!operands)return result;

        void *target=nullptr;
        if(!result.range(result.fields[NotifyTable],sizeof(void *)*(result.fields[CancelSlot]+1))
                ||!read(result.base+result.fields[NotifyTable]+sizeof(void *)*result.fields[NotifySlot],target)
                ||target!=result.base+result.fields[Notify]
                ||!read(result.base+result.fields[NotifyTable]+sizeof(void *)*result.fields[CancelSlot],target)
                ||target!=result.base+result.fields[Cancel])return result;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,path.c_str(),&module))return result;
        result.supported=true;result.reason="current-image symbols and loaded operands agree; CFG enabled";
        return result;
    }
};
const Image &image();
UINT readyMessage();
bool resolved();
std::wstring cacheFile(const std::wstring &root);
void initialize(const wchar_t *cacheRoot,bool background);
int runWorker(const wchar_t *cacheRoot=nullptr);
}
}
