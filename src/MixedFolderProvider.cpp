#include <common.h>
#include "MixedFolder.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <atlcom.h>
#include <propkey.h>
#include <propvarutil.h>
#include <strsafe.h>
#include <atomic>
#include <cstring>
#include <limits>
#include <vector>

namespace filespacer {
namespace {
const CLSID providerClass = {0x4fdd0ce1,0x6fd0,0x441c,{0xad,0x2d,0x6d,0xd0,0x56,0x61,0x0d,0xe3}};
const CLSID testProviderClass = {0xe129740c,0x52fd,0x4c7b,{0x98,0xaa,0x9e,0xd6,0xeb,0xf9,0xa6,0x04}};
const GUID itemSignature = {0x3e07595c,0x9b09,0x45ba,{0xb8,0xe3,0x20,0x68,0x9d,0x72,0x91,0x2a}};
HINSTANCE moduleInstance = nullptr;
std::atomic<long> objects{0}, serverLocks{0};
constexpr USHORT absoluteItem = 1, relativeItem = 2;
constexpr LONG changeEvents = SHCNE_CREATE | SHCNE_DELETE | SHCNE_MKDIR | SHCNE_RMDIR
    | SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER | SHCNE_ATTRIBUTES | SHCNE_UPDATEITEM | SHCNE_UPDATEDIR;
#pragma pack(push, 1)
struct ItemHeader { USHORT cb; GUID signature; USHORT version; USHORT kind; };
#pragma pack(pop)

// Only our own item identifiers are interpreted. The native PIDL payload is
// opaque; its documented SHITEMID lengths are validated before delegation.
HRESULT wrapItem(PCUIDLIST_RELATIVE native, USHORT kind, PITEMID_CHILD *out) {
    if (!native || !out) return E_POINTER;
    *out = nullptr;
    const UINT size = ILGetSize(native);
    if (size > USHRT_MAX - sizeof(ItemHeader)) return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    const size_t component = sizeof(ItemHeader) + size;
    auto *memory = static_cast<BYTE *>(CoTaskMemAlloc(component + sizeof(USHORT)));
    if (!memory) return E_OUTOFMEMORY;
    const ItemHeader header = {static_cast<USHORT>(component), itemSignature, 1, kind};
    memcpy(memory, &header, sizeof(header));
    memcpy(memory + sizeof(header), native, size);
    const USHORT end = 0;
    memcpy(memory + component, &end, sizeof(end));
    *out = reinterpret_cast<PITEMID_CHILD>(memory);
    return S_OK;
}

HRESULT unwrapItem(PCUIDLIST_RELATIVE item, USHORT kind, PCUIDLIST_RELATIVE *native) {
    if (!item || !native) return E_POINTER;
    *native = nullptr;
    const UINT size = item->mkid.cb;
    if (size < sizeof(ItemHeader) + sizeof(USHORT)) return E_INVALIDARG;
    ItemHeader header = {};
    memcpy(&header, item, sizeof(header));
    if (header.signature != itemSignature || header.version != 1 || header.kind != kind) return E_INVALIDARG;
    const BYTE *payload = reinterpret_cast<const BYTE *>(item) + sizeof(header);
    const size_t length = size - sizeof(header);
    size_t offset = 0;
    while (offset + sizeof(USHORT) <= length) {
        USHORT cb = 0;
        memcpy(&cb, payload + offset, sizeof(cb));
        if (!cb) {
            if (offset + sizeof(cb) != length) return E_INVALIDARG;
            *native = reinterpret_cast<PCUIDLIST_RELATIVE>(payload);
            return S_OK;
        }
        if (cb < sizeof(USHORT) || cb > length - offset) return E_INVALIDARG;
        offset += cb;
    }
    return E_INVALIDARG;
}

HRESULT unwrapRelative(PCUIDLIST_RELATIVE item, PIDLIST_RELATIVE *out) {
    if (!item || !out) return E_POINTER;
    *out = nullptr;
    size_t size = sizeof(USHORT);
    for (auto current = item; !ILIsEmpty(current); current = ILNext(current)) {
        PCUIDLIST_RELATIVE native = nullptr;
        const HRESULT hr = unwrapItem(current, relativeItem, &native);
        if (FAILED(hr)) return hr;
        size += ILGetSize(native) - sizeof(USHORT);
        if (size > UINT_MAX) return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    }
    CComHeapPtr<BYTE> bytes;
    if (!bytes.Allocate(size)) return E_OUTOFMEMORY;
    size_t offset = 0;
    for (auto current = item; !ILIsEmpty(current); current = ILNext(current)) {
        PCUIDLIST_RELATIVE native = nullptr;
        const HRESULT hr = unwrapItem(current, relativeItem, &native);
        if (FAILED(hr)) return hr;
        const UINT length = ILGetSize(native) - sizeof(USHORT);
        memcpy(bytes.m_pData + offset, native, length); offset += length;
    }
    const USHORT end = 0;
    memcpy(bytes.m_pData + offset, &end, sizeof(end));
    *out = reinterpret_cast<PIDLIST_RELATIVE>(bytes.Detach()); return S_OK;
}
HRESULT translatedItems(UINT count, PCUITEMID_CHILD_ARRAY items,
        std::vector<CComHeapPtr<ITEMID_CHILD>> &owned, std::vector<PCUITEMID_CHILD> &native) {
    if (count && !items) return E_POINTER;
    try { owned.resize(count); native.resize(count); }
    catch (const std::bad_alloc &) { return E_OUTOFMEMORY; }
    for (UINT i = 0; i < count; ++i) {
        PCUIDLIST_RELATIVE payload = nullptr;
        const HRESULT hr = unwrapItem(items[i], relativeItem, &payload);
        if (FAILED(hr)) return hr;
        owned[i].Attach(ILCloneChild(reinterpret_cast<PCUITEMID_CHILD>(payload)));
        if (!owned[i]) return E_OUTOFMEMORY;
        native[i] = owned[i];
    }
    return S_OK;
}

class Enumeration : public IEnumIDList {
    LONG references = 1;
    CComPtr<IEnumIDList> source;
public:
    explicit Enumeration(IEnumIDList *value) : source(value) { ++objects; }
    ~Enumeration() { --objects; }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IEnumIDList) return E_NOINTERFACE;
        *out = static_cast<IEnumIDList *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if (!n) delete this; return n; }
    STDMETHODIMP Next(ULONG count, PITEMID_CHILD *out, ULONG *fetched) override {
        if (!out || (!fetched && count != 1)) return E_POINTER;
        if (fetched) *fetched = 0;
        ULONG done = 0;
        while (done < count) {
            CComHeapPtr<ITEMID_CHILD> native;
            ULONG received = 0;
            HRESULT hr = source->Next(1, &native, &received);
            if (FAILED(hr)) { if (fetched) *fetched = done; return hr; }
            if (!received) break;
            hr = wrapItem(native, relativeItem, &out[done]);
            if (FAILED(hr)) { if (fetched) *fetched = done; return hr; }
            ++done;
        }
        if (fetched) *fetched = done;
        return done == count ? S_OK : S_FALSE;
    }
    STDMETHODIMP Skip(ULONG count) override { return source->Skip(count); }
    STDMETHODIMP Reset() override { return source->Reset(); }
    STDMETHODIMP Clone(IEnumIDList **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComPtr<IEnumIDList> copy;
        const HRESULT hr = source->Clone(&copy);
        if (FAILED(hr)) return hr;
        *out = new(std::nothrow) Enumeration(copy);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
};

class Categorizer : public ICategorizer {
    LONG references = 1;
    CComPtr<ICategorizer> source;
public:
    explicit Categorizer(ICategorizer *value) : source(value) { ++objects; }
    ~Categorizer() { --objects; }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ICategorizer) return E_NOINTERFACE;
        *out = static_cast<ICategorizer *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if (!n) delete this; return n; }
    STDMETHODIMP GetDescription(LPWSTR text, UINT count) override { return source->GetDescription(text, count); }
    STDMETHODIMP GetCategory(UINT count, PCUITEMID_CHILD_ARRAY items, DWORD *categories) override {
        std::vector<CComHeapPtr<ITEMID_CHILD>> owned; std::vector<PCUITEMID_CHILD> native;
        const HRESULT hr = translatedItems(count, items, owned, native);
        return SUCCEEDED(hr) ? source->GetCategory(count, native.data(), categories) : hr;
    }
    STDMETHODIMP GetCategoryInfo(DWORD id, CATEGORY_INFO *info) override { return source->GetCategoryInfo(id, info); }
    STDMETHODIMP CompareCategory(CATSORT_FLAGS flags, DWORD a, DWORD b) override { return source->CompareCategory(flags, a, b); }
};

class Categories : public ICategoryProvider {
    LONG references = 1;
    CComPtr<ICategoryProvider> source;
public:
    explicit Categories(ICategoryProvider *value) : source(value) { ++objects; }
    ~Categories() { --objects; }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ICategoryProvider) return E_NOINTERFACE;
        *out = static_cast<ICategoryProvider *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if (!n) delete this; return n; }
    STDMETHODIMP CanCategorizeOnSCID(const SHCOLUMNID *key) override { return source->CanCategorizeOnSCID(key); }
    STDMETHODIMP GetDefaultCategory(GUID *id, SHCOLUMNID *key) override { return source->GetDefaultCategory(id, key); }
    STDMETHODIMP GetCategoryForSCID(const SHCOLUMNID *key, GUID *id) override { return source->GetCategoryForSCID(key, id); }
    STDMETHODIMP EnumCategories(IEnumGUID **out) override { return source->EnumCategories(out); }
    STDMETHODIMP GetCategoryName(const GUID *id, LPWSTR name, UINT count) override { return source->GetCategoryName(id, name, count); }
    STDMETHODIMP CreateCategory(const GUID *id, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComPtr<ICategorizer> native;
        HRESULT hr = source->CreateCategory(id, IID_PPV_ARGS(&native));
        if (FAILED(hr)) return hr;
        auto *category = new(std::nothrow) Categorizer(native);
        if (!category) return E_OUTOFMEMORY;
        hr = category->QueryInterface(iid, out); category->Release(); return hr;
    }
};

class ViewCallback : public IShellFolderViewCB {
    LONG references = 1;
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> nativeRoot, viewRoot;
    HWND window = nullptr;
    ULONG subscription = 0;
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto *self = reinterpret_cast<ViewCallback *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<ViewCallback *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (message == WM_APP + 1 && self) { self->relay(wParam, lParam); return 0; }
        if (message == WM_NCDESTROY) SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    PIDLIST_ABSOLUTE mapped(PCIDLIST_ABSOLUTE id) {
        if (!id) return nullptr;
        if (ILIsEqual(nativeRoot, id)) return ILCloneFull(viewRoot);
        if (!ILIsParent(nativeRoot, id, TRUE)) return nullptr;
        CComHeapPtr<ITEMID_CHILD> wrapped;
        if (FAILED(wrapItem(ILFindChild(nativeRoot, id), relativeItem, &wrapped))) return nullptr;
        return ILCombine(viewRoot, wrapped);
    }
    void relay(WPARAM wParam, LPARAM lParam) {
        PIDLIST_ABSOLUTE *ids = nullptr;
        LONG event = 0;
        HANDLE lock = SHChangeNotification_Lock(reinterpret_cast<HANDLE>(wParam), static_cast<DWORD>(lParam), &ids, &event);
        if (!lock) return;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> first, second;
        if (ids) {
            first.Attach(mapped(ids[0]));
            if (event & (SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER)) second.Attach(mapped(ids[1]));
        }
        SHChangeNotification_Unlock(lock);
        if (event & (SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER)) {
            if (!first && second) { first.Attach(second.Detach()); event = event & SHCNE_RENAMEFOLDER ? SHCNE_MKDIR : SHCNE_CREATE; }
            else if (first && !second) event = event & SHCNE_RENAMEFOLDER ? SHCNE_RMDIR : SHCNE_DELETE;
        }
        if (first) SHChangeNotify(event, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, first, second);
    }
public:
    ViewCallback() { ++objects; }
    ~ViewCallback() {
        if (subscription) SHChangeNotifyDeregister(subscription);
        if (window) DestroyWindow(window);
        --objects;
    }
    HRESULT initialize(PCIDLIST_ABSOLUTE native, PCIDLIST_ABSOLUTE shown) {
        nativeRoot.Attach(ILCloneFull(native)); viewRoot.Attach(ILCloneFull(shown));
        if (!nativeRoot || !viewRoot) return E_OUTOFMEMORY;
        WNDCLASSW wc = {}; wc.lpfnWndProc = windowProc; wc.hInstance = moduleInstance; wc.lpszClassName = L"FileSpacer.MixedFolderNotifications";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return HRESULT_FROM_WIN32(GetLastError());
        window = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, moduleInstance, this);
        if (!window) return HRESULT_FROM_WIN32(GetLastError());
        const SHChangeNotifyEntry entry = {nativeRoot, FALSE};
        subscription = SHChangeNotifyRegister(window, SHCNRF_ShellLevel | SHCNRF_InterruptLevel | SHCNRF_NewDelivery, changeEvents, WM_APP + 1, 1, &entry);
        return subscription ? S_OK : E_FAIL;
    }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IShellFolderViewCB) return E_NOINTERFACE;
        *out = static_cast<IShellFolderViewCB *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if (!n) delete this; return n; }
    STDMETHODIMP MessageSFVCB(UINT message, WPARAM wParam, LPARAM lParam) noexcept override {
        if (message == SFVM_GETNOTIFY) {
            if (!wParam || !lParam) return E_POINTER;
            *reinterpret_cast<PCIDLIST_ABSOLUTE *>(wParam) = viewRoot;
            *reinterpret_cast<LONG *>(lParam) = changeEvents;
            return S_OK;
        }
        return E_NOTIMPL;
    }
};

struct PropertyValue {
    PROPVARIANT value = {};
    ~PropertyValue() { PropVariantClear(&value); }
};

class Folder : public IShellFolder2, public IPersistFolder2, public IMixedFolder, public IShellIcon {
    LONG references = 1;
    CLSID clsid;
    CComPtr<IShellFolder2> source;
    CComPtr<IShellIcon> sourceIcons;
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> nativeRoot, viewRoot;
    HRESULT nativeAbsolute(PCUIDLIST_RELATIVE id, PIDLIST_ABSOLUTE *out) {
        if (!id || !out) return E_POINTER; *out = nullptr;
        if (source) {
            CComHeapPtr<ITEMIDLIST_RELATIVE> child;
            const HRESULT hr = unwrapRelative(id, &child);
            if (FAILED(hr)) return hr;
            *out = ILCombine(nativeRoot, child);
        } else {
            PCUIDLIST_RELATIVE native = nullptr;
            HRESULT hr = unwrapItem(id, absoluteItem, &native);
            if (FAILED(hr)) return hr;
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> first; first.Attach(ILCloneFull(reinterpret_cast<PCUIDLIST_ABSOLUTE>(native)));
            if (!first) return E_OUTOFMEMORY;
            if (!ILIsEmpty(ILNext(id))) {
                CComHeapPtr<ITEMIDLIST_RELATIVE> rest;
                hr = unwrapRelative(ILNext(id), &rest);
                if (FAILED(hr)) return hr;
                *out = ILCombine(first, rest);
            } else *out = first.Detach();
        }
        return *out ? S_OK : E_OUTOFMEMORY;
    }
public:
    explicit Folder(REFCLSID value) : clsid(value) { ++objects; }
    ~Folder() { --objects; }
    HRESULT bind(PCIDLIST_ABSOLUTE native, PCIDLIST_ABSOLUTE shown) {
        nativeRoot.Attach(ILCloneFull(native)); viewRoot.Attach(ILCloneFull(shown));
        if (!nativeRoot || !viewRoot) return E_OUTOFMEMORY;
        const HRESULT hr = SHBindToObject(nullptr, nativeRoot, nullptr, IID_PPV_ARGS(&source));
        if (SUCCEEDED(hr)) source->QueryInterface(IID_PPV_ARGS(&sourceIcons));
        return hr;
    }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IShellFolder || iid == IID_IShellFolder2) *out = static_cast<IShellFolder2 *>(this);
        else if (iid == IID_IPersist || iid == IID_IPersistFolder || iid == IID_IPersistFolder2) *out = static_cast<IPersistFolder2 *>(this);
        else if (iid == IID_IShellIcon) *out = static_cast<IShellIcon *>(this);
        else if (iid == __uuidof(IMixedFolder)) *out = static_cast<IMixedFolder *>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if (!n) delete this; return n; }
    STDMETHODIMP GetClassID(CLSID *out) override { if (!out) return E_POINTER; *out = clsid; return S_OK; }
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE id) override {
        if (!id) return E_POINTER;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> copy; copy.Attach(ILCloneFull(id));
        if (!copy) return E_OUTOFMEMORY;
        viewRoot.Free(); viewRoot.Attach(copy.Detach()); return S_OK;
    }
    STDMETHODIMP GetCurFolder(PIDLIST_ABSOLUTE *out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (!viewRoot) return E_UNEXPECTED;
        *out = ILCloneFull(viewRoot); return *out ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute;
        const HRESULT hr = nativeAbsolute(child, &absolute);
        return SUCCEEDED(hr) ? SHCreateItemFromIDList(absolute, iid, out) : hr;
    }
    STDMETHODIMP NativeChild(PCUITEMID_CHILD child, PITEMID_CHILD *out) override {
        if (!out) return E_POINTER; *out = nullptr;
        PCUIDLIST_RELATIVE native = nullptr;
        const HRESULT hr = unwrapItem(child, relativeItem, &native);
        if (FAILED(hr)) return hr;
        *out = ILCloneChild(reinterpret_cast<PCUITEMID_CHILD>(native)); return *out ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetIconOf(PCUITEMID_CHILD child, UINT flags, int *index) override {
        if (!child || !index) return E_POINTER;
        *index = -1;
        if (!sourceIcons) return S_FALSE;
        PCUIDLIST_RELATIVE native = nullptr;
        const HRESULT hr = unwrapItem(child, relativeItem, &native);
        return SUCCEEDED(hr) ? sourceIcons->GetIconOf(reinterpret_cast<PCUITEMID_CHILD>(native), flags, index) : hr;
    }
    STDMETHODIMP ViewChild(PCUITEMID_CHILD child, PITEMID_CHILD *out) override { return wrapItem(child, relativeItem, out); }
    STDMETHODIMP ParseDisplayName(HWND hwnd, IBindCtx *context, LPWSTR name, ULONG *eaten, PIDLIST_RELATIVE *out, ULONG *attributes) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComHeapPtr<ITEMIDLIST_RELATIVE> native;
        HRESULT hr = source ? source->ParseDisplayName(hwnd, context, name, eaten, &native, attributes)
            : SHParseDisplayName(name, context, reinterpret_cast<PIDLIST_ABSOLUTE *>(&native), attributes ? *attributes : 0, attributes);
        if (FAILED(hr)) return hr;
        return wrapItem(native, source ? relativeItem : absoluteItem, reinterpret_cast<PITEMID_CHILD *>(out));
    }
    STDMETHODIMP EnumObjects(HWND hwnd, SHCONTF flags, IEnumIDList **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (!source) return S_FALSE;
        CComPtr<IEnumIDList> native;
        const HRESULT hr = source->EnumObjects(hwnd, flags, &native);
        if (FAILED(hr) || !native) return hr;
        *out = new(std::nothrow) Enumeration(native); return *out ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP BindToObject(PCUIDLIST_RELATIVE id, IBindCtx *, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> native, shown;
        HRESULT hr = nativeAbsolute(id, &native);
        if (FAILED(hr)) return hr;
        shown.Attach(ILCombine(viewRoot, id)); if (!shown) return E_OUTOFMEMORY;
        auto *folder = new(std::nothrow) Folder(clsid); if (!folder) return E_OUTOFMEMORY;
        hr = folder->bind(native, shown);
        if (SUCCEEDED(hr)) hr = folder->QueryInterface(iid, out);
        folder->Release(); return hr;
    }
    STDMETHODIMP BindToStorage(PCUIDLIST_RELATIVE id, IBindCtx *context, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (!source) return E_NOTIMPL;
        CComHeapPtr<ITEMIDLIST_RELATIVE> native;
        const HRESULT hr = unwrapRelative(id, &native);
        return SUCCEEDED(hr) ? source->BindToStorage(native, context, iid, out) : hr;
    }
    STDMETHODIMP CompareIDs(LPARAM flags, PCUIDLIST_RELATIVE left, PCUIDLIST_RELATIVE right) override {
        if (!source) return E_NOTIMPL;
        CComHeapPtr<ITEMIDLIST_RELATIVE> a, b;
        HRESULT hr = unwrapRelative(left, &a); if (FAILED(hr)) return hr;
        hr = unwrapRelative(right, &b); if (FAILED(hr)) return hr;
        const ULONG_PTR rule = static_cast<ULONG_PTR>(flags);
        if (rule & static_cast<ULONG_PTR>(SHCIDS_CANONICALONLY | SHCIDS_ALLFIELDS)) return source->CompareIDs(flags, a, b);
        SHCOLUMNID key = {};
        hr = source->MapColumnToSCID(static_cast<UINT>(rule & static_cast<ULONG_PTR>(SHCIDS_COLUMNMASK)), &key);
        if (FAILED(hr)) return hr;
        CComVariant first, second;
        hr = source->GetDetailsEx(reinterpret_cast<PCUITEMID_CHILD>(a.m_pData), &key, &first); if (FAILED(hr)) return hr;
        hr = source->GetDetailsEx(reinterpret_cast<PCUITEMID_CHILD>(b.m_pData), &key, &second); if (FAILED(hr)) return hr;
        PropertyValue av, bv;
        hr = VariantToPropVariant(&first, &av.value); if (FAILED(hr)) return hr;
        hr = VariantToPropVariant(&second, &bv.value); if (FAILED(hr)) return hr;
        const int order = PropVariantCompareEx(av.value, bv.value, PVCU_DEFAULT, PVCF_DEFAULT);
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(static_cast<short>(order)));
    }
    STDMETHODIMP CreateViewObject(HWND hwnd, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (!source) return E_NOINTERFACE;
        if (iid == IID_IShellView) {
            CComPtr<ViewCallback> callback; callback.Attach(new(std::nothrow) ViewCallback());
            if (!callback) return E_OUTOFMEMORY;
            HRESULT hr = callback->initialize(nativeRoot, viewRoot); if (FAILED(hr)) return hr;
            SFV_CREATE create = {sizeof(create)}; create.pshf = this; create.psfvcb = callback;
            CComPtr<IShellView> view;
            hr = SHCreateShellFolderView(&create, &view);
            if (FAILED(hr)) return hr;
            // This marker is queried in-process; it has no cross-process proxy.
            CComPtr<IUnknown> nativeView;
            hr = view->QueryInterface(IID_CDefView, reinterpret_cast<void **>(&nativeView));
            return SUCCEEDED(hr) ? view->QueryInterface(iid, out) : hr;
        }
        if (iid == IID_ICategoryProvider) {
            CComPtr<ICategoryProvider> native;
            HRESULT hr = source->CreateViewObject(hwnd, IID_PPV_ARGS(&native)); if (FAILED(hr)) return hr;
            auto *categories = new(std::nothrow) Categories(native); if (!categories) return E_OUTOFMEMORY;
            hr = categories->QueryInterface(iid, out); categories->Release(); return hr;
        }
        if (iid == IID_IContextMenu || iid == IID_IDropTarget) return source->CreateViewObject(hwnd, iid, out);
        return E_NOINTERFACE;
    }
    STDMETHODIMP GetAttributesOf(UINT count, PCUITEMID_CHILD_ARRAY ids, SFGAOF *attributes) override {
        if (!attributes || (count && !ids)) return E_POINTER;
        if (!source) {
            if (!count) { *attributes &= SFGAO_FOLDER | SFGAO_HASSUBFOLDER; return S_OK; }
            SFGAOF value = *attributes;
            for (UINT i = 0; i < count; ++i) {
                CComPtr<IShellItem> item; HRESULT hr = NativeItem(ids[i], IID_PPV_ARGS(&item)); if (FAILED(hr)) return hr;
                SFGAOF one = 0; hr = item->GetAttributes(*attributes, &one); if (FAILED(hr)) return hr; value &= one;
            }
            *attributes = value; return S_OK;
        }
        std::vector<CComHeapPtr<ITEMID_CHILD>> owned; std::vector<PCUITEMID_CHILD> native;
        const HRESULT hr = translatedItems(count, ids, owned, native);
        return SUCCEEDED(hr) ? source->GetAttributesOf(count, native.data(), attributes) : hr;
    }
    STDMETHODIMP GetUIObjectOf(HWND hwnd, UINT count, PCUITEMID_CHILD_ARRAY ids, REFIID iid, UINT *reserved, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (count && !ids) return E_POINTER;
        if (count == 1 && (iid == IID_IShellItem || iid == IID_IShellItem2)) return NativeItem(ids[0], iid, out);
        if (!source) {
            if (count != 1 || !ids) return E_NOINTERFACE;
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute;
            HRESULT hr = nativeAbsolute(ids[0], &absolute); if (FAILED(hr)) return hr;
            CComPtr<IShellFolder> parent; PCUITEMID_CHILD child = nullptr;
            hr = SHBindToParent(absolute, IID_PPV_ARGS(&parent), &child);
            return SUCCEEDED(hr) ? parent->GetUIObjectOf(hwnd, 1, &child, iid, reserved, out) : hr;
        }
        std::vector<CComHeapPtr<ITEMID_CHILD>> owned; std::vector<PCUITEMID_CHILD> native;
        const HRESULT hr = translatedItems(count, ids, owned, native);
        return SUCCEEDED(hr) ? source->GetUIObjectOf(hwnd, count, native.data(), iid, reserved, out) : hr;
    }
    STDMETHODIMP GetDisplayNameOf(PCUITEMID_CHILD id, SHGDNF flags, STRRET *out) override {
        if (!out) return E_POINTER;
        if (!source) {
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute; HRESULT hr = nativeAbsolute(id, &absolute); if (FAILED(hr)) return hr;
            out->uType = STRRET_WSTR;
            return SHGetNameFromIDList(absolute, static_cast<UINT>(flags) & static_cast<UINT>(SHGDN_FORPARSING) ? SIGDN_DESKTOPABSOLUTEPARSING : SIGDN_NORMALDISPLAY, &out->pOleStr);
        }
        CComHeapPtr<ITEMID_CHILD> native; const HRESULT hr = NativeChild(id, &native);
        return SUCCEEDED(hr) ? source->GetDisplayNameOf(native, flags, out) : hr;
    }
    STDMETHODIMP SetNameOf(HWND hwnd, PCUITEMID_CHILD id, LPCWSTR name, SHGDNF flags, PITEMID_CHILD *out) override {
        if (out) *out = nullptr;
        if (!source) return E_NOTIMPL;
        CComHeapPtr<ITEMID_CHILD> native, renamed;
        HRESULT hr = NativeChild(id, &native);
        if (SUCCEEDED(hr)) hr = source->SetNameOf(hwnd, native, name, flags, out ? &renamed : nullptr);
        if (SUCCEEDED(hr) && out) hr = wrapItem(renamed, relativeItem, out);
        return hr;
    }
    STDMETHODIMP GetDefaultSearchGUID(GUID *out) override { return source ? source->GetDefaultSearchGUID(out) : E_NOTIMPL; }
    STDMETHODIMP EnumSearches(IEnumExtraSearch **out) override { if (!source && out) *out = nullptr; return source ? source->EnumSearches(out) : E_NOTIMPL; }
    STDMETHODIMP GetDefaultColumn(DWORD reserved, ULONG *sort, ULONG *display) override { return source ? source->GetDefaultColumn(reserved, sort, display) : E_NOTIMPL; }
    STDMETHODIMP GetDefaultColumnState(UINT column, SHCOLSTATEF *out) override {
        if (!source) return E_NOTIMPL;
        const HRESULT hr = source->GetDefaultColumnState(column, out);
        if (SUCCEEDED(hr)) {
            *out |= static_cast<SHCOLSTATEF>(SHCOLSTATE_NOSORTBYFOLDERNESS);
            *out &= ~static_cast<SHCOLSTATEF>(SHCOLSTATE_PREFER_VARCMP | SHCOLSTATE_PREFER_FMTCMP);
        }
        return hr;
    }
    STDMETHODIMP GetDetailsEx(PCUITEMID_CHILD id, const SHCOLUMNID *key, VARIANT *out) override {
        if (!source) return E_NOTIMPL;
        CComHeapPtr<ITEMID_CHILD> native; const HRESULT hr = NativeChild(id, &native);
        return SUCCEEDED(hr) ? source->GetDetailsEx(native, key, out) : hr;
    }
    STDMETHODIMP GetDetailsOf(PCUITEMID_CHILD id, UINT column, SHELLDETAILS *out) override {
        if (!source) return E_NOTIMPL;
        if (!id) return source->GetDetailsOf(nullptr, column, out);
        CComHeapPtr<ITEMID_CHILD> native; const HRESULT hr = NativeChild(id, &native);
        return SUCCEEDED(hr) ? source->GetDetailsOf(native, column, out) : hr;
    }
    STDMETHODIMP MapColumnToSCID(UINT column, SHCOLUMNID *out) override { return source ? source->MapColumnToSCID(column, out) : E_NOTIMPL; }
};

class Factory : public IClassFactory {
    LONG references = 1;
    CLSID clsid;
public:
    explicit Factory(REFCLSID value) : clsid(value) { ++objects; }
    ~Factory() { --objects; }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IClassFactory) return E_NOINTERFACE;
        *out = static_cast<IClassFactory *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if (!n) delete this; return n; }
    STDMETHODIMP CreateInstance(IUnknown *outer, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto *folder = new(std::nothrow) Folder(clsid); if (!folder) return E_OUTOFMEMORY;
        const HRESULT hr = folder->QueryInterface(iid, out); folder->Release(); return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override { if (lock) ++serverLocks; else --serverLocks; return S_OK; }
};
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out) {
    if (!out) return E_POINTER; *out = nullptr;
    if (clsid != providerClass && clsid != testProviderClass) return CLASS_E_CLASSNOTAVAILABLE;
    auto *factory = new(std::nothrow) Factory(clsid); if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(iid, out); factory->Release(); return hr;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() { return !objects.load() && !serverLocks.load() ? S_OK : S_FALSE; }

extern "C" HRESULT WINAPI CreateMixedFolderItem(BOOL isolated, IShellItem *item, IShellItem **out) {
    if (!item || !out) return E_POINTER; *out = nullptr;
    const CLSID &clsid = isolated ? testProviderClass : providerClass;
    wchar_t id[40], key[100], path[MAX_PATH], parsing[MAX_PATH];
    if (!StringFromGUID2(clsid, id, ARRAYSIZE(id))) return E_FAIL;
    const DWORD length = GetModuleFileNameW(moduleInstance, path, ARRAYSIZE(path));
    if (!length || length == ARRAYSIZE(path)) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    swprintf_s(key, L"Software\\Classes\\CLSID\\%s", id);
    CRegKey registration, server, shell;
    LSTATUS status = registration.Create(HKEY_CURRENT_USER, key);
    if (status == ERROR_SUCCESS) status = registration.SetStringValue(nullptr, L"FileSpacer");
    if (status == ERROR_SUCCESS) status = server.Create(registration, L"InprocServer32");
    if (status == ERROR_SUCCESS) status = server.SetStringValue(nullptr, path);
    if (status == ERROR_SUCCESS) status = server.SetStringValue(L"ThreadingModel", L"Apartment");
    if (status == ERROR_SUCCESS) status = shell.Create(registration, L"ShellFolder");
    if (status == ERROR_SUCCESS) status = shell.SetDWORDValue(L"Attributes", SFGAO_FOLDER | SFGAO_HASSUBFOLDER);
    if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
    // A documented filesystem junction keeps the namespace root internal to the
    // application's data directory, without a Desktop/This PC namespace entry.
    CComHeapPtr<wchar_t> localData;
    HRESULT location = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localData);
    if (FAILED(location)) return location;
    location = StringCchPrintfW(parsing, ARRAYSIZE(parsing), L"%s\\FileSpacer\\%s.%s", localData.m_pData,
        isolated ? L"TestShellView" : L"ShellView", id);
    if (FAILED(location)) return location;
    const int created = SHCreateDirectoryExW(nullptr, parsing, nullptr);
    if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS) return HRESULT_FROM_WIN32(created);
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> root, native, shown;
    HRESULT hr = SHParseDisplayName(parsing, nullptr, &root, 0, nullptr);
    if (SUCCEEDED(hr)) hr = SHGetIDListFromObject(item, &native);
    CComHeapPtr<ITEMID_CHILD> child;
    if (SUCCEEDED(hr)) hr = wrapItem(native, absoluteItem, &child);
    if (FAILED(hr)) return hr;
    shown.Attach(ILCombine(root, child));
    return shown ? SHCreateItemFromIDList(shown, IID_PPV_ARGS(out)) : E_OUTOFMEMORY;
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) filespacer::moduleInstance = instance;
    return TRUE;
}
