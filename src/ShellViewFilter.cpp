#include "ShellViewFilter.h"
#include <shlobj.h>
#include <shlwapi.h>

namespace filespacer {
FolderNameFilter::FolderNameFilter(const wchar_t *text,
        const CComHeapPtr<ITEMID_CHILD> *hidden, size_t count)
    : query(std::make_shared<const Query>(text)), hiddenItemIDs(hidden), hiddenCount(count) {}

void FolderNameFilter::setQuery(const wchar_t *text) {
    std::atomic_store(&query, std::make_shared<const Query>(text));
}

HRESULT FolderNameFilter::initialize() {
    return CoCreateFreeThreadedMarshaler(this, &marshaler);
}

STDMETHODIMP FolderNameFilter::QueryInterface(REFIID id, void **object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (id == IID_IUnknown || id == __uuidof(IFolderFilter)) {
        *object = static_cast<IFolderFilter *>(this);
        AddRef();
        return S_OK;
    }
    if (id == IID_IMarshal && marshaler)
        return marshaler->QueryInterface(id, object);
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) FolderNameFilter::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&references));
}

STDMETHODIMP_(ULONG) FolderNameFilter::Release() {
    const LONG count = InterlockedDecrement(&references);
    if (!count) delete this;
    return static_cast<ULONG>(count);
}

STDMETHODIMP FolderNameFilter::GetEnumFlags(IShellFolder *, PCIDLIST_ABSOLUTE, HWND *, DWORD *flags) {
    if (!flags) return E_POINTER;
    SHELLSTATE state = {};
    SHGetSetSettings(&state, SSF_SHOWALLOBJECTS | SSF_SHOWSUPERHIDDEN, FALSE);
    *flags = SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_ENABLE_ASYNC;
    if (state.fShowAllObjects) {
        *flags |= SHCONTF_INCLUDEHIDDEN;
        if (state.fShowSuperHidden) *flags |= SHCONTF_INCLUDESUPERHIDDEN;
    }
    return S_OK;
}

STDMETHODIMP FolderNameFilter::ShouldShow(IShellFolder *folder, PCIDLIST_ABSOLUTE folderID,
        PCUITEMID_CHILD childID) {
    if (!folder || !childID) return E_INVALIDARG;
    // SetFilter replaces IncludeObject; retain its Desktop exclusions.
    if (folderID && ILIsEmpty(folderID)) {
        for (size_t i = 0; i < hiddenCount; ++i) {
            if (hiddenItemIDs[i] && ILIsEqual(childID, hiddenItemIDs[i])) return S_FALSE;
        }
    }
    const auto current = std::atomic_load(&query);
    if (current->text.empty()) return S_OK;
    SFGAOF attributes = SFGAO_FILESYSTEM;
    const bool filesystem = SUCCEEDED(folder->GetAttributesOf(1, &childID, &attributes))
        && (attributes & SFGAO_FILESYSTEM);
    STRRET result = {};
    const SHGDNF flags = filesystem
        ? static_cast<SHGDNF>(SHGDN_INFOLDER | SHGDN_FORPARSING) : SHGDN_INFOLDER;
    HRESULT hr = folder->GetDisplayNameOf(childID, flags, &result);
    if (FAILED(hr)) return hr;
    CComHeapPtr<wchar_t> name;
    hr = StrRetToStr(&result, childID, &name);
    if (FAILED(hr)) return hr;
    // Filesystem names include extensions even when the view hides them.
    const wchar_t *fullName = name;
    const wchar_t *filename = filesystem ? PathFindFileName(fullName) : fullName;
    if (current->wildcard)
        hr = PathMatchSpecEx(filename, current->text.c_str(), PMSF_NORMAL | PMSF_DONT_STRIP_SPACES);
    else
        hr = StrStrIW(filename, current->text.c_str()) ? S_OK : S_FALSE;
    return hr;
}
} // namespace filespacer
