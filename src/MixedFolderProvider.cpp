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

namespace filespacer {
namespace {
const CLSID providerClass = {0x4fdd0ce1,0x6fd0,0x441c,{0xad,0x2d,0x6d,0xd0,0x56,0x61,0x0d,0xe3}};
const CLSID testProviderClass = {0xe129740c,0x52fd,0x4c7b,{0x98,0xaa,0x9e,0xd6,0xeb,0xf9,0xa6,0x04}};
const GUID itemSignature = {0x3e07595c,0x9b09,0x45ba,{0xb8,0xe3,0x20,0x68,0x9d,0x72,0x91,0x2a}};
HINSTANCE moduleInstance = nullptr;
std::atomic<long> objects{0}, serverLocks{0};
constexpr USHORT absoluteItem = 1;
constexpr LONG changeEvents = SHCNE_CREATE | SHCNE_DELETE | SHCNE_MKDIR | SHCNE_RMDIR
    | SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER | SHCNE_ATTRIBUTES | SHCNE_UPDATEITEM | SHCNE_UPDATEDIR;
#pragma pack(push, 1)
struct ItemHeader { USHORT cb; GUID signature; USHORT version; USHORT kind; };
#pragma pack(pop)

// The per-user registration is rewritten only when a value differs, so an
// ordinary navigation no longer writes the Registry once it matches this module.
LSTATUS ensureStringValue(CRegKey &key, const wchar_t *name, const wchar_t *value) {
    wchar_t current[MAX_PATH];
    ULONG length = ARRAYSIZE(current);
    if (key.QueryStringValue(name, current, &length) == ERROR_SUCCESS && wcscmp(current, value) == 0)
        return ERROR_SUCCESS;
    return key.SetStringValue(name, value);
}
LSTATUS ensureDWORDValue(CRegKey &key, const wchar_t *name, DWORD value) {
    DWORD current = 0;
    if (key.QueryDWORDValue(name, current) == ERROR_SUCCESS && current == value) return ERROR_SUCCESS;
    return key.SetDWORDValue(name, value);
}

// Only the root identifier under the junction is our own: it carries the
// absolute native PIDL of the folder shown. The items of that folder are the
// native folder's own child identifiers, passed to the Shell unchanged. The
// native PIDL payload is opaque; its documented SHITEMID lengths are validated
// before delegation.
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

// The view registers for changes of the native folder itself (SFVM_GETNOTIFY
// documents a location other than the one viewed). Its items being the native
// identifiers, the Shell's own notifications apply to them without a relay.
class ViewCallback : public IShellFolderViewCB {
    LONG references = 1;
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> nativeRoot;
public:
    ViewCallback() { ++objects; }
    ~ViewCallback() { --objects; }
    HRESULT initialize(PCIDLIST_ABSOLUTE native) {
        nativeRoot.Attach(ILCloneFull(native));
        return nativeRoot ? S_OK : E_OUTOFMEMORY;
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
            *reinterpret_cast<PCIDLIST_ABSOLUTE *>(wParam) = nativeRoot;
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

class Folder : public IShellFolder2, public IPersistFolder2, public IMixedFolder, public IShellIcon, public IShellIconOverlay {
    LONG references = 1;
    CLSID clsid;
    CComPtr<IShellFolder2> source;
    CComPtr<IShellIcon> sourceIcons;
    CComPtr<IShellIconOverlay> sourceOverlays;
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> nativeRoot, viewRoot;
    // Root (no source): our own first identifier, then native identifiers.
    // Wrapped folder: native identifiers relative to the native root.
    HRESULT nativeAbsolute(PCUIDLIST_RELATIVE id, PIDLIST_ABSOLUTE *out) {
        if (!id || !out) return E_POINTER; *out = nullptr;
        if (source) {
            *out = ILCombine(nativeRoot, id);
        } else {
            PCUIDLIST_RELATIVE native = nullptr;
            const HRESULT hr = unwrapItem(id, absoluteItem, &native);
            if (FAILED(hr)) return hr;
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> first; first.Attach(ILCloneFull(reinterpret_cast<PCUIDLIST_ABSOLUTE>(native)));
            if (!first) return E_OUTOFMEMORY;
            if (!ILIsEmpty(ILNext(id))) *out = ILCombine(first, ILNext(id));
            else *out = first.Detach();
        }
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    static bool folderInterface(REFIID iid) {
        return iid == IID_IUnknown || iid == IID_IShellFolder || iid == IID_IShellFolder2
            || iid == IID_IPersist || iid == IID_IPersistFolder || iid == IID_IPersistFolder2
            || iid == IID_IShellIcon || iid == IID_IShellIconOverlay || iid == __uuidof(IMixedFolder);
    }
public:
    explicit Folder(REFCLSID value) : clsid(value) { ++objects; }
    ~Folder() { --objects; }
    HRESULT bind(PCIDLIST_ABSOLUTE native, PCIDLIST_ABSOLUTE shown) {
        nativeRoot.Attach(ILCloneFull(native)); viewRoot.Attach(ILCloneFull(shown));
        if (!nativeRoot || !viewRoot) return E_OUTOFMEMORY;
        const HRESULT hr = SHBindToObject(nullptr, nativeRoot, nullptr, IID_PPV_ARGS(&source));
        if (SUCCEEDED(hr)) { source->QueryInterface(IID_PPV_ARGS(&sourceIcons)); source->QueryInterface(IID_PPV_ARGS(&sourceOverlays)); }
        return hr;
    }
    STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IShellFolder || iid == IID_IShellFolder2) *out = static_cast<IShellFolder2 *>(this);
        else if (iid == IID_IPersist || iid == IID_IPersistFolder || iid == IID_IPersistFolder2) *out = static_cast<IPersistFolder2 *>(this);
        else if (iid == IID_IShellIcon) *out = static_cast<IShellIcon *>(this);
        else if (iid == IID_IShellIconOverlay && sourceOverlays) *out = static_cast<IShellIconOverlay *>(this);
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
    // An item of the view carries the native identifier; only its parent differs.
    STDMETHODIMP NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute;
        const HRESULT hr = nativeAbsolute(child, &absolute);
        return SUCCEEDED(hr) ? SHCreateItemFromIDList(absolute, iid, out) : hr;
    }
    STDMETHODIMP GetIconOf(PCUITEMID_CHILD child, UINT flags, int *index) override {
        if (!child || !index) return E_POINTER;
        *index = -1;
        return sourceIcons ? sourceIcons->GetIconOf(child, flags, index) : S_FALSE;
    }
    // The Shell asks the folder for overlays before drawing each icon; without
    // this delegation shortcut arrows and sync states vanish from mixed views.
    STDMETHODIMP GetOverlayIndex(PCUITEMID_CHILD child, int *index) override {
        if (!child || !index) return E_POINTER;
        return sourceOverlays ? sourceOverlays->GetOverlayIndex(child, index) : E_NOTIMPL;
    }
    STDMETHODIMP GetOverlayIconIndex(PCUITEMID_CHILD child, int *index) override {
        if (!child || !index) return E_POINTER;
        return sourceOverlays ? sourceOverlays->GetOverlayIconIndex(child, index) : E_NOTIMPL;
    }
    STDMETHODIMP ParseDisplayName(HWND hwnd, IBindCtx *context, LPWSTR name, ULONG *eaten, PIDLIST_RELATIVE *out, ULONG *attributes) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (source) return source->ParseDisplayName(hwnd, context, name, eaten, out, attributes);
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> native;
        const HRESULT hr = SHParseDisplayName(name, context, &native, attributes ? *attributes : 0, attributes);
        if (FAILED(hr)) return hr;
        return wrapItem(native, absoluteItem, reinterpret_cast<PITEMID_CHILD *>(out));
    }
    STDMETHODIMP EnumObjects(HWND hwnd, SHCONTF flags, IEnumIDList **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        return source ? source->EnumObjects(hwnd, flags, out) : S_FALSE;
    }
    STDMETHODIMP BindToObject(PCUIDLIST_RELATIVE id, IBindCtx *context, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> native, shown;
        HRESULT hr = nativeAbsolute(id, &native);
        if (FAILED(hr)) return hr;
        // Only the folder contract is wrapped. Any other handler (stream, property
        // store, thumbnail...) comes from the native folder with the caller's bind context.
        if (!folderInterface(iid)) return SHBindToObject(nullptr, native, context, iid, out);
        shown.Attach(ILCombine(viewRoot, id)); if (!shown) return E_OUTOFMEMORY;
        auto *folder = new(std::nothrow) Folder(clsid); if (!folder) return E_OUTOFMEMORY;
        hr = folder->bind(native, shown);
        if (SUCCEEDED(hr)) hr = folder->QueryInterface(iid, out);
        folder->Release(); return hr;
    }
    STDMETHODIMP BindToStorage(PCUIDLIST_RELATIVE id, IBindCtx *context, REFIID iid, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        return source ? source->BindToStorage(id, context, iid, out) : E_NOTIMPL;
    }
    STDMETHODIMP CompareIDs(LPARAM flags, PCUIDLIST_RELATIVE left, PCUIDLIST_RELATIVE right) override {
        if (!source) return E_NOTIMPL;
        const ULONG_PTR rule = static_cast<ULONG_PTR>(flags);
        // Identity comparisons and multi-level identifiers keep the native order;
        // a column comparison of two children is the value comparison, folders included.
        if ((rule & static_cast<ULONG_PTR>(SHCIDS_CANONICALONLY | SHCIDS_ALLFIELDS))
                || !ILIsChild(left) || !ILIsChild(right))
            return source->CompareIDs(flags, left, right);
        SHCOLUMNID key = {};
        HRESULT hr = source->MapColumnToSCID(static_cast<UINT>(rule & static_cast<ULONG_PTR>(SHCIDS_COLUMNMASK)), &key);
        if (FAILED(hr)) return hr;
        CComVariant first, second;
        hr = source->GetDetailsEx(reinterpret_cast<PCUITEMID_CHILD>(left), &key, &first); if (FAILED(hr)) return hr;
        hr = source->GetDetailsEx(reinterpret_cast<PCUITEMID_CHILD>(right), &key, &second); if (FAILED(hr)) return hr;
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
            HRESULT hr = callback->initialize(nativeRoot); if (FAILED(hr)) return hr;
            SFV_CREATE create = {sizeof(create)}; create.pshf = this; create.psfvcb = callback;
            CComPtr<IShellView> view;
            hr = SHCreateShellFolderView(&create, &view);
            if (FAILED(hr)) return hr;
            // This marker is queried in-process; it has no cross-process proxy.
            CComPtr<IUnknown> nativeView;
            hr = view->QueryInterface(IID_CDefView, reinterpret_cast<void **>(&nativeView));
            return SUCCEEDED(hr) ? view->QueryInterface(iid, out) : hr;
        }
        // Categories, background menu and drop target receive native identifiers:
        // the native folder's own objects serve unchanged.
        if (iid == IID_ICategoryProvider || iid == IID_IContextMenu || iid == IID_IDropTarget)
            return source->CreateViewObject(hwnd, iid, out);
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
        return source->GetAttributesOf(count, ids, attributes);
    }
    STDMETHODIMP GetUIObjectOf(HWND hwnd, UINT count, PCUITEMID_CHILD_ARRAY ids, REFIID iid, UINT *reserved, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (count && !ids) return E_POINTER;
        if (!source) {
            if (count != 1 || !ids) return E_NOINTERFACE;
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute;
            HRESULT hr = nativeAbsolute(ids[0], &absolute); if (FAILED(hr)) return hr;
            CComPtr<IShellFolder> parent; PCUITEMID_CHILD child = nullptr;
            hr = SHBindToParent(absolute, IID_PPV_ARGS(&parent), &child);
            return SUCCEEDED(hr) ? parent->GetUIObjectOf(hwnd, 1, &child, iid, reserved, out) : hr;
        }
        return source->GetUIObjectOf(hwnd, count, ids, iid, reserved, out);
    }
    STDMETHODIMP GetDisplayNameOf(PCUITEMID_CHILD id, SHGDNF flags, STRRET *out) override {
        if (!out) return E_POINTER;
        if (!source) {
            CComHeapPtr<ITEMIDLIST_ABSOLUTE> absolute; HRESULT hr = nativeAbsolute(id, &absolute); if (FAILED(hr)) return hr;
            out->uType = STRRET_WSTR;
            return SHGetNameFromIDList(absolute, static_cast<UINT>(flags) & static_cast<UINT>(SHGDN_FORPARSING) ? SIGDN_DESKTOPABSOLUTEPARSING : SIGDN_NORMALDISPLAY, &out->pOleStr);
        }
        return source->GetDisplayNameOf(id, flags, out);
    }
    STDMETHODIMP SetNameOf(HWND hwnd, PCUITEMID_CHILD id, LPCWSTR name, SHGDNF flags, PITEMID_CHILD *out) override {
        if (out) *out = nullptr;
        return source ? source->SetNameOf(hwnd, id, name, flags, out) : E_NOTIMPL;
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
        return source ? source->GetDetailsEx(id, key, out) : E_NOTIMPL;
    }
    STDMETHODIMP GetDetailsOf(PCUITEMID_CHILD id, UINT column, SHELLDETAILS *out) override {
        return source ? source->GetDetailsOf(id, column, out) : E_NOTIMPL;
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
    if (status == ERROR_SUCCESS) status = ensureStringValue(registration, nullptr, L"FileSpacer");
    if (status == ERROR_SUCCESS) status = server.Create(registration, L"InprocServer32");
    if (status == ERROR_SUCCESS) status = ensureStringValue(server, nullptr, path);
    if (status == ERROR_SUCCESS) status = ensureStringValue(server, L"ThreadingModel", L"Apartment");
    if (status == ERROR_SUCCESS) status = shell.Create(registration, L"ShellFolder");
    if (status == ERROR_SUCCESS) status = ensureDWORDValue(shell, L"Attributes", SFGAO_FOLDER | SFGAO_HASSUBFOLDER);
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
    // The junction is application data, not a user folder: hide it from
    // ordinary listings like other Shell-managed system folders. Only missing
    // attributes are written, and the view does not depend on them.
    constexpr DWORD hidden = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    const DWORD attributes = GetFileAttributesW(parsing);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & hidden) != hidden)
        SetFileAttributesW(parsing, attributes | hidden);
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
