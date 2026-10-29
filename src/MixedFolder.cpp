#include "MixedFolder.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <vector>

namespace filespacer {
HRESULT mixedFolderItem(IShellItem *item, bool isolated, IShellItem **out) {
    if (!item || !out) return E_POINTER;
    *out = nullptr;
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (!length || length == ARRAYSIZE(path)) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    if (!PathRemoveFileSpecW(path) || !PathAppendW(path, L"FileSpacerFolder.dll")) return E_FAIL;
    // Keep the module loaded while the Shell retains its COM objects.
    HMODULE module = GetModuleHandleW(path);
    if (!module) module = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) return HRESULT_FROM_WIN32(GetLastError());
    using Create = HRESULT (WINAPI *)(BOOL, IShellItem *, IShellItem **);
    const auto create = reinterpret_cast<Create>(GetProcAddress(module, "CreateMixedFolderItem"));
    if (!create) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return create(isolated, item, out);
}

HRESULT nativeShellItem(IShellItem *item, IShellItem **out) {
    if (!item || !out) return E_POINTER;
    *out = nullptr;
    CComQIPtr<IParentAndItem> parent(item);
    CComPtr<IShellFolder> folder;
    CComHeapPtr<ITEMID_CHILD> child;
    if (parent && SUCCEEDED(parent->GetParentAndItem(nullptr, &folder, &child))) {
        CComQIPtr<IMixedFolder> mixed(folder);
        if (mixed) return mixed->NativeItem(child, IID_PPV_ARGS(out));
    }
    return item->QueryInterface(IID_PPV_ARGS(out));
}

HRESULT nativeViewSelection(IFolderView2 *view, BOOL noneMeansFolder, IShellItemArray **out) {
    if (!view || !out) return E_POINTER;
    *out = nullptr;
    CComPtr<IShellItemArray> selected;
    HRESULT hr = view->GetSelection(noneMeansFolder, &selected);
    if (FAILED(hr) || !selected) return hr;
    CComPtr<IMixedFolder> mixed;
    if (FAILED(view->GetFolder(IID_PPV_ARGS(&mixed)))) return selected.CopyTo(out);
    DWORD count = 0;
    hr = selected->GetCount(&count);
    if (FAILED(hr)) return hr;
    std::vector<CComHeapPtr<ITEMIDLIST_ABSOLUTE>> owned(count);
    std::vector<PCIDLIST_ABSOLUTE> ids(count);
    for (DWORD i = 0; i < count; ++i) {
        CComPtr<IShellItem> item, native;
        hr = selected->GetItemAt(i, &item);
        if (SUCCEEDED(hr)) hr = nativeShellItem(item, &native);
        if (SUCCEEDED(hr)) hr = SHGetIDListFromObject(native, &owned[i]);
        if (FAILED(hr)) return hr;
        ids[i] = owned[i];
    }
    return SHCreateShellItemArrayFromIDLists(count, ids.data(), out);
}
}
