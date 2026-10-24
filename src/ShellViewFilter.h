#pragma once
#include <common.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <memory>
#include <string>

namespace filespacer {
// Install before creating the first view. Publish immutable query snapshots:
// Shell workers never access the host window or its COM interfaces.
class FolderNameFilter final : public IFolderFilter {
public:
    // Desktop exclusions are immutable and must outlive the filter.
    explicit FolderNameFilter(const wchar_t *text,
        const CComHeapPtr<ITEMID_CHILD> *hidden = nullptr, size_t count = 0);
    void setQuery(const wchar_t *text);
    HRESULT initialize();
    STDMETHODIMP QueryInterface(REFIID id, void **object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP GetEnumFlags(IShellFolder *, PCIDLIST_ABSOLUTE, HWND *, DWORD *flags) override;
    STDMETHODIMP ShouldShow(IShellFolder *folder, PCIDLIST_ABSOLUTE folderID,
        PCUITEMID_CHILD childID) override;

private:
    struct Query {
        explicit Query(const wchar_t *value) : text(value),
            wildcard(text.find_first_of(L"*?") != std::wstring::npos) {}
        const std::wstring text;
        const bool wildcard;
    };
    LONG references = 1;
    std::shared_ptr<const Query> query;
    CComPtr<IUnknown> marshaler;
    const CComHeapPtr<ITEMID_CHILD> *const hiddenItemIDs;
    const size_t hiddenCount;
};
} // namespace filespacer
