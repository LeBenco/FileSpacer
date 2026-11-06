#pragma once
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
// Experimental, exact-build private binding. No instruction is patched here.
namespace selectionDispatch {
template<class Source,class Value> bool read(const Source *address,Value &value){
    static_assert(std::is_trivially_copyable<Value>::value,"Only raw ABI values may be copied");
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),static_cast<const void *>(address),
        static_cast<void *>(std::addressof(value)),sizeof(Value),&copied)&&copied==sizeof(Value);
}
inline unsigned char *knownModule(){
    static unsigned char *const result=[]()->unsigned char *{
        auto *base=reinterpret_cast<unsigned char *>(GetModuleHandleW(L"shell32.dll"));if(!base)return nullptr;
        wchar_t loaded[MAX_PATH]={},system[MAX_PATH]={};
        const DWORD loadedLength=GetModuleFileNameW(reinterpret_cast<HMODULE>(base),loaded,_countof(loaded));
        const UINT systemLength=GetSystemDirectoryW(system,_countof(system));
        if(!loadedLength||loadedLength>=_countof(loaded)||!systemLength||systemLength>=_countof(system)
                ||_wcsicmp(loaded,(std::wstring(system)+L"\\shell32.dll").c_str())!=0)return nullptr;
        IMAGE_DOS_HEADER dos={};IMAGE_NT_HEADERS64 nt={};
        if(!read(base,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<64||dos.e_lfanew>4096
                ||!read(base+dos.e_lfanew,nt)||nt.Signature!=IMAGE_NT_SIGNATURE
                ||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.OptionalHeader.SizeOfImage!=0x770000)return nullptr;
        const auto &directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        if(directory.Size>32*sizeof(IMAGE_DEBUG_DIRECTORY)||directory.VirtualAddress>=nt.OptionalHeader.SizeOfImage
                ||directory.Size>nt.OptionalHeader.SizeOfImage-directory.VirtualAddress)return nullptr;
        for(DWORD offset=0;offset+sizeof(IMAGE_DEBUG_DIRECTORY)<=directory.Size;offset+=sizeof(IMAGE_DEBUG_DIRECTORY)){
            IMAGE_DEBUG_DIRECTORY debug={};if(!read(base+directory.VirtualAddress+offset,debug))return nullptr;
            struct CodeView {DWORD signature=0;GUID guid={};DWORD age=0;} code;
            if(debug.Type!=IMAGE_DEBUG_TYPE_CODEVIEW||debug.AddressOfRawData>nt.OptionalHeader.SizeOfImage-sizeof(CodeView))continue;
            const GUID expected={0xfd92fc12,0x9e95,0x7ebe,{0xe6,0xcb,0x4c,0x6d,0x73,0x8d,0x33,0x9f}};
            if(read(base+debug.AddressOfRawData,code)&&code.signature==0x53445352&&code.age==1&&IsEqualGUID(code.guid,expected)){
                unsigned char entry[9]={};const unsigned char expectedEntry[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};
                unsigned char notification[10]={},cancel[6]={};
                const unsigned char expectedNotification[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};
                const unsigned char expectedCancel[]={0x40,0x53,0x48,0x83,0xec,0x20};
                if(read(base+0x319d0,entry)&&std::memcmp(entry,expectedEntry,sizeof(entry))==0
                        &&read(base+0xc29b0,notification)&&std::memcmp(notification,expectedNotification,sizeof(notification))==0
                        &&read(base+0xd7880,cancel)&&std::memcmp(cancel,expectedCancel,sizeof(cancel))==0)return base;
            }
        }
        return nullptr;
    }();return result;
}
inline IUnknown *manager(IShellView *view,unsigned char *base){
    void **viewTable=nullptr;void *select=nullptr,*host=nullptr;void **hostTable=nullptr;void *hostSelect=nullptr;IUnknown *value=nullptr;void **managerTable=nullptr;void *query=nullptr,*retain=nullptr,*release=nullptr;
    if(!base||!read(view,viewTable)||!read(viewTable+14,select)||select!=base+0x105610)return nullptr;
    // Exact-build IShellView CDefView -> IViewHost -> CSelectionManager offsets.
    if(!read(reinterpret_cast<unsigned char *>(view)+0x4c0,host)||!host||!read(host,hostTable)
            ||!read(hostTable+31,hostSelect)||hostSelect!=base+0x44f7a0)return nullptr;
    if(!read(reinterpret_cast<unsigned char *>(host)+0x338,value)||!value||!read(value,managerTable)
            ||!read(managerTable,query)||query!=base+0x44f680||!read(managerTable+1,retain)||retain!=base+0xdc2b0
            ||!read(managerTable+2,release)||release!=base+0x360e0)return nullptr;
    return value;
}
struct Operations {
    // Private ABI pinned to the inspected Microsoft image; investigation only.
    using Notify=HRESULT(STDMETHODCALLTYPE *)(IUnknown *,UINT,UINT);
    using Cancel=HRESULT(STDMETHODCALLTYPE *)(IUnknown *);
    unsigned char *module=nullptr;bool accelerated=false,applied=false;
    CComPtr<IUnknown> notification;
    Notify notify=nullptr;Cancel cancel=nullptr;
    static HRESULT normal(IShellView *view){return view->SelectItem(nullptr,SVSI_DESELECTOTHERS);}
    HRESULT clear(IShellView *view){
        if(!accelerated)return normal(view);
        Operations &self=*this;
        IUnknown *raw=manager(view,self.module);if(!raw)return normal(view);
        CComPtr<IUnknown> keep(raw);
        CComQIPtr<IFolderView2> folder(view);int selected=-1;void *array=nullptr;int cached=-1;
        if(!folder||FAILED(folder->ItemCount(SVGIO_SELECTION,&selected))||selected<0
                ||!read(reinterpret_cast<unsigned char *>(raw)+0x10,array)||!array||!read(array,cached)||cached!=selected)return normal(view);
        using Purge=void(*)(void *);
        reinterpret_cast<Purge>(self.module+0x319d0)(raw);
        const HRESULT cleared=normal(view);
        // Explorer sends this aggregate notification even when clearing selection.
        // It invalidates both CDefView arrays before host state notifications.
        const HRESULT notified=self.notify(self.notification,0,1);
        const HRESULT cancelled=self.cancel(self.notification);
        self.applied=true;
        return FAILED(cleared)?cleared:FAILED(notified)?notified:cancelled;
    }
    static Operations choose(IShellView *view,bool forceMissing){
        Operations result;if(forceMissing)return result;
        result.module=knownModule();if(!manager(view,result.module))return result;
        const GUID hostId={0xca9a60fb,0xec04,0x4b21,{0xae,0x64,0xe8,0x27,0x1f,0x47,0x9b,0xec}};
        void *host=nullptr;
        if(FAILED(view->QueryInterface(hostId,&host)))return result;
        result.notification.Attach(static_cast<IUnknown *>(host)); // Adopt QueryInterface's acquired reference.
        void **table=nullptr;void *notify=nullptr,*cancel=nullptr;
        if(!read(static_cast<IUnknown *>(result.notification),table)||!read(table+7,notify)||!read(table+29,cancel)
                ||notify!=result.module+0xc29b0||cancel!=result.module+0xd7880){result.notification.Release();return result;}
        result.notify=reinterpret_cast<Notify>(notify);result.cancel=reinterpret_cast<Cancel>(cancel);
        result.accelerated=true;return result;
    }
};
}
