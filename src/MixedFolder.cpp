#include "MixedFolder.h"

#include <shlobj.h>
#include <shlwapi.h>

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
}
