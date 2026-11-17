#include <common.h>
#include "ShellViewOperations.h"
#include "ShellSelectionRuntime.h"
#include "ShellViewIcons.h"
#include <shlwapi.h>
#include <new>
#include <utility>

namespace filespacer {
using NotifyFunction=HRESULT(STDMETHODCALLTYPE *)(IUnknown *,UINT,UINT);
using CancelFunction=HRESULT(STDMETHODCALLTYPE *)(IUnknown *);
struct ShellSelectionClear::Binding : std::enable_shared_from_this<ShellSelectionClear::Binding> {
        mutable std::atomic<bool> usable{true};
        CComPtr<IShellView> source;CComPtr<IFolderView2> folder;CComPtr<IUnknown> host,manager,notification;
        selectionDetail::RangeBinding range;
        PFNDAENUMCALLBACK callback=nullptr;NotifyFunction notify=nullptr;CancelFunction cancel=nullptr;
        HWND window=nullptr;
        SUBCLASSPROC nativeProcedure=nullptr;
        SUBCLASSPROC registeredProcedure=nullptr;
        DWORD_PTR nativeData=0;
        decltype(&DSA_InsertItem) insert=nullptr;
        bool selecting=false;
        uint64_t selectAllCount=0;
        void detach(){DWORD_PTR data=0;if(window&&GetWindowSubclass(window,procedure,selectionSubclassId,&data)
            &&data==reinterpret_cast<DWORD_PTR>(this))RemoveWindowSubclass(window,procedure,selectionSubclassId);}
        ~Binding(){detach();}
        static constexpr UINT_PTR selectionSubclassId=0x46535341;
        static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
        bool nativeArray(HDSA expected,selectionDetail::ArraySnapshot &snapshot)const {
            using namespace selectionDetail;const Image &module=image();IUnknown *sameManager=nullptr;HDSA array=nullptr;DWORD_PTR data=0;
            // Detaching our observer must not invalidate an in-flight native
            // construction. The registered native callback also rejects a
            // destroyed/recycled HWND without relying on IsWindow alone.
            return window&&registeredProcedure&&GetWindowSubclass(window,registeredProcedure,0,&data)&&data==nativeData
                &&read(reinterpret_cast<unsigned char *>(host.p)+module.fields[ManagerOffset],sameManager)
                &&sameManager==manager.p&&read(reinterpret_cast<unsigned char *>(manager.p)+module.fields[ArrayOffset],array)
                &&array==expected&&snapshotArray(array,module.dataOffset,module.strideOffset,snapshot);
        }
        bool selectAll(const NMLISTVIEW &notice);
        bool deselectAll(const NMLISTVIEW &notice);
        HRESULT purgeArray(HDSA array,int selected,const std::function<bool()> &current,bool *applied=nullptr)const;
        bool notificationsEnabled()const {
            DWORD state=0;
            return selectionDetail::read(reinterpret_cast<const unsigned char *>(nativeData)+selectionDetail::image().ignoredStateOffset,state)
                &&!(state&0x2000u);
        }
    };

bool ShellSelectionClear::Binding::selectAll(const NMLISTVIEW &notice) {
    using namespace selectionDetail;
    if(!nativeProcedure||selecting||!usable.load()||notice.iItem!=-1||!(notice.uChanged&LVIF_STATE)
            ||(notice.uOldState&LVIS_SELECTED)||!(notice.uNewState&LVIS_SELECTED))return false;
    wchar_t className[32]={};
    if(!IsChild(window,notice.hdr.hwndFrom)||!GetClassNameW(notice.hdr.hwndFrom,className,_countof(className))
            ||lstrcmpW(className,WC_LISTVIEWW)!=0)return false;
    const Image &module=image();HDSA array=nullptr;ArraySnapshot snapshot;
    if(!notificationsEnabled())return false;
    if(!read(reinterpret_cast<unsigned char *>(manager.p)+module.fields[ArrayOffset],array)||!array
            ||!nativeArray(array,snapshot)||snapshot.count<0||snapshot.stride!=16
            ||reinterpret_cast<ULONG_PTR>(array)%alignof(LONG)||!writable(array,sizeof(LONG)))return false;
    int count=0,selected=0;
    if(FAILED(folder->ItemCount(SVGIO_ALLVIEW,&count))||count<SELECTION_BULK_THRESHOLD
            ||FAILED(folder->ItemCount(SVGIO_SELECTION,&selected))||selected!=count
            ||!nativeArray(array,snapshot)||snapshot.count<0||snapshot.count>count)return false;
    struct alignas(void *) Entry {unsigned char bytes[16]={};};
    // Windows creates each entry and subscription. Only their ownership is
    // staged temporarily; no private item constructor or ABI is called.
    struct Construction {
        Binding &owner;HDSA array;std::vector<Entry> entries;size_t filled=0;bool committed=false;
        Construction(Binding &binding,HDSA selectionArray,int itemCount):owner(binding),array(selectionArray),entries(static_cast<size_t>(itemCount)){}
        ~Construction(){
            if(committed)return;
            ArraySnapshot current;Entry pending;
            if(owner.nativeArray(array,current)&&current.count==1&&current.stride==16&&current.data
                    &&read(current.data,pending)) {
                InterlockedExchange(reinterpret_cast<volatile LONG *>(array),0);
                owner.callback(&pending,nullptr);
            }
            for(size_t i=0;i<filled;++i)owner.callback(&entries[i],nullptr);
        }
    };
    selecting=true;
    struct Active {bool &value;~Active(){value=false;}} active{selecting};
    Construction construction(*this,array,count);
    if(snapshot.count) {
        // Reuse the same staging storage for the old selection. The control
        // has already selected every item, so a native fallback can rebuild
        // the complete cache even if allocation subsequently fails.
        const SIZE_T bytes=static_cast<SIZE_T>(snapshot.count)*16;
        if(!snapshot.data||!writable(snapshot.data,bytes))return false;
        std::memcpy(construction.entries.data(),snapshot.data,bytes);
        construction.filled=static_cast<size_t>(snapshot.count);
        InterlockedExchange(reinterpret_cast<volatile LONG *>(array),0);
        while(construction.filled) {
            const size_t index=--construction.filled;
            Entry previous=construction.entries[index];construction.entries[index]={};
            callback(&previous,nullptr);
            // Every staged entry is released on exit: the original notification
            // must reach the native handler, which rebuilds its cache. A closed
            // or replaced view is silenced by the caller.
            if(!nativeArray(array,snapshot)||snapshot.stride!=16){nativeProcedure=nullptr;return false;}
            if(snapshot.count){nativeProcedure=nullptr;return false;}
        }
    }
    const auto selectNativeItem=[&](int index) {
        NMLISTVIEW item=notice;item.iItem=index;item.uOldState=0;item.uNewState=LVIS_SELECTED;item.uChanged=LVIF_STATE;
        nativeProcedure(window,WM_NOTIFY,item.hdr.idFrom,reinterpret_cast<LPARAM>(&item),0,nativeData);
    };
    if(module.nativeReservation) {
        auto *growth=reinterpret_cast<volatile LONG *>(reinterpret_cast<unsigned char *>(array)+module.growthOffset);
        LONG capacity=0,previous=0;
        if(!read(reinterpret_cast<unsigned char *>(array)+module.capacityOffset,capacity)||capacity<0
                ||!read(const_cast<const LONG *>(growth),previous)||previous<=0||!writable(const_cast<LONG *>(growth),sizeof(LONG)))return false;
        if(capacity<count) {
            auto *capacityField=reinterpret_cast<volatile LONG *>(reinterpret_cast<unsigned char *>(array)+module.capacityOffset);
            if(!writable(const_cast<LONG *>(capacityField),sizeof(LONG)))return false;
            struct Growth {
                Binding &owner;HDSA array;volatile LONG *field,*capacityField;LONG previous,capacity;const unsigned char *data;
                Growth(Binding &binding,HDSA selectionArray,const ArraySnapshot &initialArray)
                    :owner(binding),array(selectionArray),field(nullptr),capacityField(nullptr),previous(0),capacity(0),data(initialArray.data){}
                ~Growth(){ArraySnapshot now;if(owner.nativeArray(array,now)){LONG reserved=0;
                    InterlockedExchange(field,previous);
                    if(now.data==data&&read(const_cast<const LONG *>(capacityField),reserved)&&reserved==0)InterlockedExchange(capacityField,capacity);
                }}
            } restore{*this,array,snapshot};
            restore.field=growth;restore.capacityField=capacityField;restore.previous=previous;restore.capacity=capacity;
            // Force the native allocation branch even if a smaller buffer
            // remains. Restore its capacity if allocation fails; only Windows
            // reallocates the buffer and publishes its actual new capacity.
            InterlockedExchange(capacityField,0);
            InterlockedExchange(growth,count);
            selectNativeItem(0);
            if(!nativeArray(array,snapshot)||snapshot.stride!=16||snapshot.count!=1||!snapshot.data
                    ||!read(snapshot.data,construction.entries[0])){nativeProcedure=nullptr;return false;}
            construction.filled=1;
            InterlockedExchange(reinterpret_cast<volatile LONG *>(array),0);
            if(!read(reinterpret_cast<unsigned char *>(array)+module.capacityOffset,capacity)||capacity<count)return false;
        }
        if(!nativeArray(array,snapshot)||snapshot.count!=0||snapshot.stride!=16||!snapshot.data
                ||!writable(snapshot.data,static_cast<SIZE_T>(count)*16))return false;
    }else{
        struct EmptyReservation {HDSA array;~EmptyReservation(){InterlockedExchange(reinterpret_cast<volatile LONG *>(array),0);}} reset{array};
        const Entry empty;
        for(int i=0;i<count;++i)if(insert(array,INT_MAX,&empty)<0)return false;
        if(!nativeArray(array,snapshot)||snapshot.count!=count||snapshot.stride!=16||!snapshot.data
                ||!writable(snapshot.data,static_cast<SIZE_T>(count)*16))return false;
    }
    const unsigned char *reserved=snapshot.data;
    for(int index=static_cast<int>(construction.filled);index<count;++index) {
        selectNativeItem(index);
        if(!nativeArray(array,snapshot)||snapshot.stride!=16||snapshot.data!=reserved){nativeProcedure=nullptr;return false;}
        if(snapshot.count!=1||!read(snapshot.data,construction.entries[construction.filled])){nativeProcedure=nullptr;return false;}
        ++construction.filled;
        InterlockedExchange(reinterpret_cast<volatile LONG *>(array),0);
    }
    // The replayed indexes are only those of the content counted above. If a
    // reentrant handler changed it, release the staged entries and let the
    // original notification rebuild natively. These are external calls: check
    // the native array again before writing to it.
    int finalCount=0,finalSelected=0;
    if(FAILED(folder->ItemCount(SVGIO_ALLVIEW,&finalCount))||finalCount!=count
            ||FAILED(folder->ItemCount(SVGIO_SELECTION,&finalSelected))||finalSelected!=count)return false;
    if(!nativeArray(array,snapshot)||snapshot.stride!=16||snapshot.data!=reserved||snapshot.count!=0){nativeProcedure=nullptr;return false;}
    std::memcpy(snapshot.data,construction.entries.data(),static_cast<SIZE_T>(count)*16);
    InterlockedExchange(reinterpret_cast<volatile LONG *>(array),count);
    construction.committed=true;++selectAllCount;
    // Invalidate a snapshot that a reentrant subscriber could have requested
    // while the native per-item handler was constructing the final entry.
    if(FAILED(notify(notification,0,1))||!window||FAILED(cancel(notification)))nativeProcedure=nullptr;
    return true;
}

HRESULT ShellSelectionClear::Binding::purgeArray(HDSA array,int selected,const std::function<bool()> &current,bool *applied)const {
        using namespace selectionDetail;const Image &module=image();
        if(reinterpret_cast<ULONG_PTR>(array)%alignof(LONG)!=0||!writable(array,sizeof(LONG)))return S_FALSE;
        // Same DSA implementation and callback as Shell32. Finish the purge
        // phase even if callback releases reenter. Never retry after mutation.
        ArraySnapshot snapshot;
        if(!snapshotArray(array,module.dataOffset,module.strideOffset,snapshot)||snapshot.count!=selected||snapshot.stride!=16||!snapshot.data
                ||!writable(snapshot.data,static_cast<SIZE_T>(selected)*snapshot.stride))return S_FALSE;
        const auto owned=[&]{IUnknown *sameManager=nullptr;HDSA sameArray=nullptr;
            return read(reinterpret_cast<unsigned char *>(host.p)+module.fields[ManagerOffset],sameManager)&&sameManager==manager.p
                &&read(reinterpret_cast<unsigned char *>(manager.p)+module.fields[ArrayOffset],sameArray)&&sameArray==array;};
        const unsigned char *verified=snapshot.data;int verifiedCount=selected;
        // From the first released entry on, the continuation no longer decides
        // anything here: the native array must never stay published with
        // released entries. An entry buffer that cannot be followed ends the
        // enumeration, and the count is still reset. Only the loss of the
        // array itself returns at once: nothing is left to write to.
        bool complete=true;
        // A view destroyed by a reentrant release has nothing left to finish
        // here: its teardown is native, and its array may be gone with it.
        const HWND view=window;
        const auto destroyed=[&]{return view&&!IsWindow(view);};
        for(int index=0;index<snapshot.count;++index){
            if(!callback(snapshot.data+static_cast<SIZE_T>(index)*snapshot.stride,nullptr))break;
            if(destroyed())return E_ABORT;
            if(!owned()||!snapshotArray(array,module.dataOffset,module.strideOffset,snapshot))return E_ABORT;
            if(snapshot.count<0||(snapshot.count>0&&(snapshot.stride!=16||!snapshot.data))){complete=false;break;}
            if(snapshot.count>0&&(snapshot.data!=verified||snapshot.count>verifiedCount)){
                if(!writable(snapshot.data,static_cast<SIZE_T>(snapshot.count)*snapshot.stride)){complete=false;break;}
                verified=snapshot.data;verifiedCount=snapshot.count;
            }
        }
        // Qualified internal data write, not a public API contract.
        // Public native clear then reaches its own zero-count purge, using
        // its own allocator and a direct, CFG-compatible internal call.
        const bool live=current();
        if(destroyed()||!owned())return E_ABORT;
        InterlockedExchange(reinterpret_cast<volatile LONG *>(array),0);
        if(!live||!complete)return E_ABORT;
        if(applied)*applied=true;
        return S_OK;
}

bool ShellSelectionClear::Binding::deselectAll(const NMLISTVIEW &notice) {
    using namespace selectionDetail;
    if(!nativeProcedure||selecting||!usable.load()||notice.iItem!=-1||!(notice.uChanged&LVIF_STATE)
            ||!(notice.uOldState&LVIS_SELECTED)||(notice.uNewState&LVIS_SELECTED))return false;
    wchar_t className[32]={};
    if(!IsChild(window,notice.hdr.hwndFrom)||!GetClassNameW(notice.hdr.hwndFrom,className,_countof(className))
            ||lstrcmpW(className,WC_LISTVIEWW)!=0)return false;
    const Image &module=image();HDSA array=nullptr;ArraySnapshot snapshot;
    if(!notificationsEnabled()
            ||!read(reinterpret_cast<unsigned char *>(manager.p)+module.fields[ArrayOffset],array)||!array)return false;
    const HWND expected=window;
    const auto current=[&]{ArraySnapshot now;return window==expected&&nativeArray(array,now);};
    int selected=-1,count=0;
    if(!current()||FAILED(folder->ItemCount(SVGIO_SELECTION,&selected))||selected!=0||!current()
            ||FAILED(folder->ItemCount(SVGIO_ALLVIEW,&count))||!current()
            ||!nativeArray(array,snapshot)||snapshot.count<SELECTION_BULK_THRESHOLD||snapshot.count>count||snapshot.stride!=16||!snapshot.data
            ||!writable(snapshot.data,static_cast<SIZE_T>(snapshot.count)*snapshot.stride))return false;
    // The control has already applied the global deselection. Purge its Shell
    // subscriptions as one batch, then retain the original notification for
    // native cache invalidation, focus, activation and subsequent item selection.
    selecting=true;
    struct Active {bool &value;~Active(){value=false;}} active{selecting};
    bool applied=false;
    if(FAILED(purgeArray(array,snapshot.count,current,&applied)))usable.store(false);
    return applied;
}

LRESULT CALLBACK ShellSelectionClear::Binding::procedure(HWND window,UINT message,WPARAM first,LPARAM second,UINT_PTR id,DWORD_PTR data) {
    auto self=reinterpret_cast<Binding *>(data)->shared_from_this();
    bool cleared=false;
    if(message==WM_NCDESTROY){RemoveWindowSubclass(window,procedure,id);self->window=nullptr;}
    if(message==WM_NOTIFY&&second&&reinterpret_cast<const NMHDR *>(second)->code==LVN_ITEMCHANGED) {
        try {
            const auto &notice=*reinterpret_cast<const NMLISTVIEW *>(second);
            if(self->selectAll(notice))return 0;
            cleared=self->deselectAll(notice);
        }
        catch(const std::bad_alloc &){self->nativeProcedure=nullptr;}
        // A failed construction released every staged native subscription.
        // Let the original native global notification rebuild normally, unless
        // its view was closed or replaced during a reentrant native callback.
        if(self->window!=window)return 0;
    }
    const LRESULT result=DefSubclassProc(window,message,first,second);
    const auto attached=[&]{DWORD_PTR owner=0,native=0;return self->window==window
        &&GetWindowSubclass(window,procedure,id,&owner)&&owner==data
        &&self->registeredProcedure&&GetWindowSubclass(window,self->registeredProcedure,0,&native)&&native==self->nativeData;};
    if(cleared&&attached()) {
        // Native cleanup can see an already-empty manager and skip invalidating
        // a previously returned array. Publish the completed change once, after
        // forwarding the original notification, as explicit clearing does.
        const HRESULT notified=self->notify(self->notification,0,1);
        if(attached()&&FAILED(self->cancel(self->notification)))self->usable.store(false);
        if(FAILED(notified))self->usable.store(false);
    }
    return result;
}

void initializeShellSelectionClear(const wchar_t *cacheRoot,bool background) { selectionDetail::initialize(cacheRoot,background); }
void stopShellSelectionClear() { selectionDetail::shutdown(); }
void ShellSelectionClear::reset(){auto previous=std::move(binding);if(previous)previous->detach();}
bool ShellSelectionClear::accelerated()const {const auto value=binding;return value && value->usable.load();}
bool ShellSelectionClear::acceleratesSelectAll()const {const auto value=binding;return value && value->usable.load() && value->nativeProcedure;}
uint64_t ShellSelectionClear::acceleratedSelectAllCount()const {const auto value=binding;return value?value->selectAllCount:0;}

    bool ShellSelectionClear::backingBufferReleased(bool &known)const {
        using namespace selectionDetail;known=false;const auto value=binding;const Image &module=image();
        if(!value||!module.dataOffset)return false;
        HDSA array=nullptr;void *data=nullptr;
        if(!read(reinterpret_cast<unsigned char *>(value->manager.p)+module.fields[ArrayOffset],array))return false;
        if(!array){known=true;return true;}
        if(!read(reinterpret_cast<unsigned char *>(array)+module.dataOffset,data))return false;
        known=true;return data==nullptr;
    }
    void ShellSelectionClear::bind(IShellView *source,const std::function<bool()> &current) try {
        using namespace selectionDetail;reset();const Image &module=image();
        if(!source||!current()||!module.supported)return;
        auto value=std::make_shared<Binding>();value->source=source;
        CComQIPtr<IServiceProvider> services(source);void *rawHost=nullptr;
        const GUID service={0xc30e04c9,0x7d34,0x4ad0,{0xa1,0xdc,0xaf,0xff,0xae,0xc1,0xb2,0x52}};
        const GUID control={0xc401f7ac,0x9572,0x42cd,{0xae,0x05,0x3b,0xd0,0x84,0x27,0x0c,0xb1}};
        if(!services||!current()||FAILED(services->QueryService(service,control,&rawHost)))return;
        value->host.Attach(static_cast<IUnknown *>(rawHost));
        void *table=nullptr;IUnknown *manager=nullptr;
        if(!current()||!read(value->host.p,table)||table!=module.base+module.fields[HostTable]
                ||!read(reinterpret_cast<unsigned char *>(value->host.p)+module.fields[ManagerOffset],manager)||!manager
                ||!read(manager,table)||table!=module.base+module.fields[ManagerTable])return;
        for(size_t slot=0;slot<3;++slot){void *method=nullptr;
            if(!read(static_cast<unsigned char *>(table)+8*slot,method)||method!=module.base+module.fields[ManagerQI+slot])return;}
        value->manager=manager;
        if(!current()||FAILED(source->QueryInterface(IID_PPV_ARGS(&value->folder))))return;
        const GUID notification={0xca9a60fb,0xec04,0x4b21,{0xae,0x64,0xe8,0x27,0x1f,0x47,0x9b,0xec}};
        void *rawNotification=nullptr;
        if(!current()||FAILED(source->QueryInterface(notification,&rawNotification)))return;
        value->notification.Attach(static_cast<IUnknown *>(rawNotification));
        if(!current()||!read(value->notification.p,table)||table!=module.base+module.fields[NotifyTable])return;
        value->callback=reinterpret_cast<PFNDAENUMCALLBACK>(module.base+module.fields[Callback]);
        value->notify=reinterpret_cast<NotifyFunction>(module.base+module.fields[Notify]);
        value->cancel=reinterpret_cast<CancelFunction>(module.base+module.fields[Cancel]);
        const bool hasWindow=SUCCEEDED(source->GetWindow(&value->window))&&current();
        if(hasWindow)value->range.bind(value->window);
        if(module.selectAllSupported&&hasWindow&&current()) {
            const auto procedure=reinterpret_cast<SUBCLASSPROC>(module.base+module.fields[NativeWindowProcedure]);
            if(GetWindowSubclass(value->window,procedure,0,&value->nativeData)
                    &&value->nativeData==reinterpret_cast<ULONG_PTR>(value->host.p)-module.fields[HostOffset]) {
                value->nativeProcedure=procedure;
                value->registeredProcedure=procedure;
                if(!module.nativeReservation)value->insert=reinterpret_cast<decltype(value->insert)>(module.base+module.fields[NativeInsert]);
                if(!SetWindowSubclass(value->window,Binding::procedure,Binding::selectionSubclassId,reinterpret_cast<DWORD_PTR>(value.get())))
                    value->nativeProcedure=nullptr;
            }
        }
        if(current())binding=std::move(value);
    } catch(const std::bad_alloc &) { binding.reset(); }
    HRESULT ShellSelectionClear::clear(IShellView *source,const std::function<bool()> &current,bool *applied)const {
        using namespace selectionDetail;if(applied)*applied=false;if(!source)return E_POINTER;
        if(!current())return E_ABORT;
        CComPtr<IShellView> keepView(source);
        const auto native=[&](){
            if(!current())return E_ABORT;
            const HRESULT result=source->SelectItem(nullptr,SVSI_DESELECTOTHERS);
            return current()?result:E_ABORT;
        };
        const auto value=binding;
        if(value&&value->selecting)return E_PENDING;
        if(!value||!value->usable.load()||value->source.p!=source)return native();
        const auto reject=[&](){value->usable.store(false);return native();};
        const Image &module=image();IUnknown *manager=nullptr;HDSA array=nullptr;int count=-1,selected=-1;
        const HRESULT counted=value->folder->ItemCount(SVGIO_SELECTION,&selected);
        if(!current())return E_ABORT;
        if(FAILED(counted))return reject();
        // A small selection gains nothing from the private purge.
        if(selected<SELECTION_BULK_THRESHOLD)return native();
        if(!read(reinterpret_cast<unsigned char *>(value->host.p)+module.fields[ManagerOffset],manager)||manager!=value->manager.p
                ||!read(reinterpret_cast<unsigned char *>(manager)+module.fields[ArrayOffset],array)||!array
                ||!read(array,count)||count!=selected)return reject();
        const HRESULT purged=value->purgeArray(array,selected,current,applied);
        if(purged==S_FALSE)return reject();
        // Entries were released before this failure: the binding is not used
        // again for this view, whatever the caller does with the result.
        if(FAILED(purged)){value->usable.store(false);return purged;}
        if(!current())return E_ABORT;
        // The native range owns only selected indexes. Clear it after releasing
        // Shell subscriptions and before public cleanup; otherwise classic free
        // Tiles measures every off-screen label merely to invalidate it. Native
        // failure/missing capability leaves the ordinary cleanup in charge.
        if(value->range.clear(value->window)!=S_OK)value->range=RangeBinding{};
        if(!current())return E_ABORT;
        const HRESULT cleared=native();
        if(!current())return E_ABORT;
        const HRESULT notified=value->notify(value->notification,0,1);
        if(!current())return E_ABORT;
        const HRESULT cancelled=value->cancel(value->notification);
        if(FAILED(cleared)||FAILED(notified)||FAILED(cancelled))value->usable.store(false);
        return !current()?E_ABORT:FAILED(cleared)?cleared:FAILED(notified)?notified:cancelled;
    }


HRESULT ShellSelectionClear::selectAll(IShellView *source, const std::function<bool()> &current) const {
    if (!source) return E_POINTER;
    if (!current()) return E_ABORT;
    CComPtr<IShellView> keepView(source);
    CComQIPtr<IFolderView2> view(source);
    if (!current()) return E_ABORT;
    if (!view) return S_FALSE;
    const auto value = binding;
    bool supported = value && value->usable.load() && value->source.p == source && value->nativeProcedure;
    if (!supported) {
        FOLDERVIEWMODE mode = FVM_AUTO;
        int size = 0;
        const HRESULT result = view->GetViewModeAndIconSize(&mode, &size);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) {
            supported = mode != FVM_ICON && mode != FVM_SMALLICON;
            if (!supported) {
                supported = SUCCEEDED(view->GetAutoArrange());
                if (!current()) return E_ABORT;
            }
        }
    }
    if (!supported) return S_FALSE;
    CComQIPtr<IShellFolderView> bulk(source);
    if (!current()) return E_ABORT;
    if (!bulk) return S_FALSE;
    const HRESULT result = bulk->Select(SFVS_SELECT_ALLITEMS);
    return current() ? result : E_ABORT;
}

UINT shellViewOperationsReadyMessage() { return selectionDetail::readyMessage(); }
int runShellViewOperationsWorker(const wchar_t *cacheRoot) { return selectionDetail::runWorker(cacheRoot); }
HRESULT ShellIconGrid::prepare(IShellView *source, HWND list, const std::function<bool()> &current) {
#ifdef FILESPACER_DEBUG
    const ULONGLONG began = GetTickCount64();
#endif
    ready = qualified = false;
    control.reset();
    font.reset(); imageList.Release();
    folder = CComQIPtr<IFolderView2>(source);
    if (!current()) return E_ABORT;
    if (!folder) return E_NOINTERFACE;
    FOLDERVIEWMODE mode = FVM_AUTO; DWORD flags = 0;
    if (FAILED(folder->GetViewModeAndIconSize(&mode, &size)) || !current() || mode != FVM_ICON
            || FAILED(folder->GetSpacing(&spacing)) || !current() || size <= 0 || spacing.x < size || spacing.y <= 0
            || FAILED(folder->GetCurrentFolderFlags(&flags)) || !current()
            || (flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID | FWF_DESKTOP | FWF_NOSCROLL))) return S_FALSE;
    qualified = GetDpiForWindow(list) == USER_DEFAULT_SCREEN_DPI
        && selectionDetail::gridImage().supported && selectionDetail::gridImage().qualifies();
    if (qualified) {
        ready = true;
        debugPrintf(L"GRID PREPARE path=qualified elapsed_ms=%llu\n", GetTickCount64() - began);
        return S_OK;
    }
    // A one-item public ListView supplies native rounding without touching the
    // actual folder. It shares the image list without owning or drawing it.
    HWND body = nullptr; wchar_t name[32] = {};
    if (FAILED(source->GetWindow(&body)) || !current() || !body || !IsChild(body, list)
            || !GetClassNameW(list, name, _countof(name)) || lstrcmpW(name, WC_LISTVIEWW)) return S_FALSE;
    const HIMAGELIST icons = ListView_GetImageList(list, LVSIL_NORMAL);
    if (!current()) return E_ABORT;
    int width = 0, height = 0;
    if (!icons || !ImageList_GetIconSize(icons, &width, &height) || width <= 0 || height != width || width > spacing.x
            || spacing.x > MAXSHORT || spacing.y > MAXSHORT) return S_FALSE;
    if (FAILED(HIMAGELIST_QueryInterface(icons, IID_PPV_ARGS(&imageList))) || !current()) return E_FAIL;
    RECT bounds = {};
    if (!GetWindowRect(list, &bounds) || !current()) return E_FAIL;
    control.reset(CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WC_LISTVIEWW, L"",
        WS_POPUP | LVS_ICON | LVS_SHAREIMAGELISTS, bounds.left, bounds.top, spacing.x * 2, spacing.y * 2,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    if (!control || !current()) return E_FAIL;
    if (GetClassLongPtrW(control.get(), GCLP_HMODULE) != GetClassLongPtrW(list, GCLP_HMODULE)
            || GetDpiForWindow(control.get()) != GetDpiForWindow(list)) return S_FALSE;
    const HFONT nativeFont = reinterpret_cast<HFONT>(SendMessageW(list, WM_GETFONT, 0, 0));
    if (!current()) return E_ABORT;
    LOGFONTW description = {};
    if (!nativeFont || GetObjectW(nativeFont, sizeof(description), &description) != sizeof(description)) return S_FALSE;
    font.reset(CreateFontIndirectW(&description));
    if (!font) return E_FAIL;
    SendMessageW(control.get(), WM_SETFONT, reinterpret_cast<WPARAM>(font.get()), FALSE);
    ListView_SetImageList(control.get(), icons, LVSIL_NORMAL);
    ListView_SetIconSpacing(control.get(), spacing.x, spacing.y);
    LVITEMW item = {}; item.mask = LVIF_IMAGE; item.iImage = 0;
    if (ListView_InsertItem(control.get(), &item) != 0 || !current()) return E_FAIL;

    ready = true;
    debugPrintf(L"GRID PREPARE path=public elapsed_ms=%llu\n", GetTickCount64() - began);
    return S_OK;
}

bool ShellIconGrid::align(POINT &point) const {
    if (!ready) return false;
    if (qualified) return selectionDetail::snapGridPoint(point, spacing, size);
    ListView_SetItemPosition32(control.get(), 0, point.x, point.y);
    return ListView_Arrange(control.get(), LVA_SNAPTOGRID)
        && ListView_GetItemPosition(control.get(), 0, &point);
}

HRESULT ShellIconGrid::apply(PreparedIconPositions &positions, const std::function<bool()> &current) const {
#ifdef FILESPACER_DEBUG
    const ULONGLONG began = GetTickCount64();
#endif
    if (!ready || positions.view != folder) return E_INVALIDARG;
    if (!current()) return E_ABORT;
    FOLDERVIEWMODE mode = FVM_AUTO; int actualSize = 0; POINT actualSpacing = {}; DWORD flags = 0;
    if (FAILED(folder->GetViewModeAndIconSize(&mode, &actualSize)) || !current()
            || FAILED(folder->GetSpacing(&actualSpacing)) || !current()
            || FAILED(folder->GetCurrentFolderFlags(&flags)) || !current()
            || mode != FVM_ICON || actualSize != size || actualSpacing.x != spacing.x || actualSpacing.y != spacing.y
            || (flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID))
            || (qualified && !selectionDetail::gridImage().qualifies())) return E_ABORT;
    for (auto &point : positions.points) {
        if (!align(point)) return E_INVALIDARG;
        if (!current()) return E_ABORT;
    }
    if (!positions.ready || positions.items.size() != positions.points.size()) return E_INVALIDARG;
    positions.ready = false;
    const HRESULT result = folder->SelectAndPositionItems(static_cast<UINT>(positions.items.size()),
        positions.items.data(), positions.points.data(), SVSI_POSITIONITEM | SVSI_NOSTATECHANGE);
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result)) {
        if (qualified) {
            selectionDetail::gridCalls().fetch_add(1, std::memory_order_relaxed);
#ifdef FILESPACER_DEBUG
            int count = 0;
            if (SUCCEEDED(folder->ItemCount(SVGIO_ALLVIEW, &count)) && current())
                debugPrintf(L"GRID SNAP qualified=1 selected=%zu count=%d\n", positions.items.size(), count);
#endif
        } else debugPrintf(L"GRID SNAP path=public elapsed_ms=%llu\n", GetTickCount64() - began);
    }
    return result;
}

HRESULT alignIconsToGrid(IShellView *source, bool selectedOnly,
        const std::function<bool()> &current) {
#ifdef FILESPACER_DEBUG
    const ULONGLONG snapStarted = GetTickCount64();
#endif
    if (!source) return E_POINTER;
    if (!current()) return E_ABORT;
    CComPtr<IShellView> keepView(source);
    CComQIPtr<IFolderView2> view(source);
    DWORD flags = 0;
    if (!current()) return E_ABORT;
    if (!view) return E_NOINTERFACE;
    const HRESULT queried = view->GetCurrentFolderFlags(&flags);
    if (!current()) return E_ABORT;
    if (FAILED(queried)) return queried;
    if (selectedOnly && (flags & FWF_SNAPTOGRID)) return S_FALSE;
    if (selectedOnly) {
        const HRESULT result = selectionDetail::snapGridSelection(source, current);
        if (!current()) return E_ABORT;
        if (result == S_OK) {
            debugPrintf(L"GRID SNAP path=qualified elapsed_ms=%llu\n", GetTickCount64() - snapStarted);
            return S_OK;
        }
    }
    std::vector<PreparedIconPosition> stationary;
    if (selectedOnly) {
        int count = 0;
        if (FAILED(view->ItemCount(SVGIO_SELECTION, &count)) || !current() || !count) return current() ? S_FALSE : E_ABORT;
        CComPtr<IEnumIDList> items;
        if (FAILED(view->Items(SVGIO_ALLVIEW, IID_PPV_ARGS(&items))) || !current() || !items) return current() ? E_FAIL : E_ABORT;
        for (;;) {
            PreparedIconPosition position;
            const HRESULT next = items->Next(1, &position.id, nullptr);
            if (!current() || next == S_FALSE) break;
            if (next != S_OK || !position.id) return E_FAIL;
            DWORD state = 0;
            if (FAILED(view->GetSelectionState(position.id, &state)) || !current()) return current() ? E_FAIL : E_ABORT;
            if (state & SVSI_SELECT) continue;
            if (FAILED(view->GetItemPosition(position.id, &position.point)) || !current()) return current() ? E_FAIL : E_ABORT;
            stationary.push_back(std::move(position));
        }
    }
    if (!current()) return E_ABORT;
    std::vector<PCUITEMID_CHILD> ids;
    std::vector<POINT> points;
    ids.reserve(stationary.size());
    points.reserve(stationary.size());
    // Suppress intermediate whole-view alignment for a selected-only snap.
    // Always restore the native flag before saves or deferred closure resume.
    const bool redraw = SUCCEEDED(view->SetRedraw(FALSE));
    HRESULT result = current() ? S_OK : E_ABORT;
    if (SUCCEEDED(result) && (flags & FWF_SNAPTOGRID))
        result = view->SetCurrentFolderFlags(FWF_SNAPTOGRID, 0);
    if (SUCCEEDED(result) && current())
        result = view->SetCurrentFolderFlags(FWF_SNAPTOGRID, FWF_SNAPTOGRID);
    if (current()) {
        const HRESULT restored = view->SetCurrentFolderFlags(FWF_SNAPTOGRID, flags & FWF_SNAPTOGRID);
        if (FAILED(restored)) result = restored;
    }
    if (current() && !(flags & FWF_SNAPTOGRID)) {
        for (const auto &position : stationary) {
            POINT actual = {};
            const HRESULT read = view->GetItemPosition(position.id, &actual);
            if (!current()) break;
            if (FAILED(read)) { result = read; break; }
            if (actual.x != position.point.x || actual.y != position.point.y) {
                ids.push_back(position.id);
                points.push_back(position.point);
            }
        }
        if (current() && !ids.empty()) {
            const HRESULT restored = view->SelectAndPositionItems(static_cast<UINT>(ids.size()),
                ids.data(), points.data(), SVSI_POSITIONITEM | SVSI_NOSTATECHANGE);
            if (FAILED(restored)) result = restored;
        }
    }
    if (redraw && current()) checkHR(view->SetRedraw(TRUE));
    if (!current()) return E_ABORT;
    if (selectedOnly) debugPrintf(L"GRID SNAP path=public elapsed_ms=%llu\n", GetTickCount64() - snapStarted);
    return result;
}
} // namespace filespacer
