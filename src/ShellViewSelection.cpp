#include "ShellViewSelection.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace filespacer {
namespace {
struct ReopenViewHeader {
    uint32_t version = 1, selected = 0, flags = 0;
    POINT offset = {};
};
// Keep the existing blob layout and legacy flags; new captures have no anchor.
constexpr uint32_t REOPEN_ANCHOR = 2, REOPEN_FOCUS = 4;
}

HRESULT captureReopenSelection(IShellView *view, const std::function<bool()> &current,
        std::vector<uint8_t> &bytes) {
    bytes.clear();
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    CComQIPtr<IFolderView> folderView(view);
    if (!current()) return E_ABORT;
    if (!folderView) return E_NOINTERFACE;
    ReopenViewHeader header;
    int count = 0;
    HRESULT result = folderView->ItemCount(SVGIO_SELECTION, &count);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return result;
    if (count < 0) return E_UNEXPECTED;
    header.selected = static_cast<uint32_t>(count);
    CComHeapPtr<ITEMID_CHILD> focused;
    int index = -1;
    result = folderView->GetFocusedItem(&index);
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result) && index >= 0) {
        result = folderView->Item(index, &focused);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && focused) header.flags |= REOPEN_FOCUS;
    }
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    if (!stream) return E_OUTOFMEMORY;
    result = IStream_Write(stream, &header, sizeof(header));
    if (SUCCEEDED(result) && (header.flags & REOPEN_FOCUS))
        result = IStream_WritePidl(stream, focused);
    if (FAILED(result)) return result;
    if (count) {
        CComPtr<IEnumIDList> selectedItems;
        result = folderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selectedItems));
        if (!current()) return E_ABORT;
        if (FAILED(result)) return result;
        if (!selectedItems) return E_UNEXPECTED;
        for (int i = 0; i < count; ++i) {
            CComHeapPtr<ITEMID_CHILD> selectedItem;
            result = selectedItems->Next(1, &selectedItem, nullptr);
            if (!current()) return E_ABORT;
            if (result != S_OK) return FAILED(result) ? result : E_UNEXPECTED;
            result = IStream_WritePidl(stream, selectedItem);
            if (FAILED(result)) return result;
        }
    }
    STATSTG stats = {};
    LARGE_INTEGER beginning = {};
    result = stream->Stat(&stats, STATFLAG_NONAME);
    if (FAILED(result)) return result;
    if (stats.cbSize.QuadPart > INT32_MAX) return E_OUTOFMEMORY;
    result = stream->Seek(beginning, STREAM_SEEK_SET, nullptr);
    if (FAILED(result)) return result;
    std::vector<uint8_t> captured(static_cast<size_t>(stats.cbSize.QuadPart));
    result = IStream_Read(stream, captured.data(), static_cast<ULONG>(captured.size()));
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result)) bytes.swap(captured);
    return result;
}

HRESULT restoreReopenSelection(IShellView *view, std::vector<uint8_t> &pending,
        const std::function<bool()> &current) {
    std::vector<uint8_t> bytes;
    bytes.swap(pending);
    if (bytes.empty()) return S_FALSE;
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    CComQIPtr<IFolderView2> folderView(view);
    if (!current()) return E_ABORT;
    if (!folderView) return E_NOINTERFACE;
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    if (!stream) return E_OUTOFMEMORY;
    ReopenViewHeader header;
    HRESULT result = IStream_Read(stream, &header, sizeof(header));
    if (FAILED(result)) return result;
    if (header.version != 1 || (header.flags & ~7u) || header.selected > bytes.size() / 2)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    CComHeapPtr<ITEMID_CHILD> anchor, focused;
    if (header.flags & REOPEN_ANCHOR) {
        result = IStream_ReadPidl(stream, &anchor);
        if (FAILED(result)) return result;
        if (!anchor) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    if (header.flags & REOPEN_FOCUS) {
        result = IStream_ReadPidl(stream, &focused);
        if (FAILED(result)) return result;
        if (!focused) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    std::vector<CComHeapPtr<ITEMID_CHILD>> selectedItems(header.selected);
    for (auto &selectedItem : selectedItems) {
        result = IStream_ReadPidl(stream, &selectedItem);
        if (FAILED(result)) return result;
        if (!selectedItem) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    // Decode the whole snapshot before changing the native selection.
    if (!current()) return E_ABORT;
    HRESULT restored = S_OK;
    auto record = [&](HRESULT value) {
        if (FAILED(value) && SUCCEEDED(restored)) restored = value;
    };
    if (anchor) {
        record(view->SelectItem(anchor, SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS));
        if (!current()) return E_ABORT;
    }
    // With no target item, request only the native deselect-all operation.
    result = view->SelectItem(nullptr, SVSI_DESELECTOTHERS);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return result;
    for (const auto &selectedItem : selectedItems) {
        record(view->SelectItem(selectedItem, SVSI_SELECT | SVSI_NOTAKEFOCUS));
        if (!current()) return E_ABORT;
    }
    if (focused) {
        DWORD selection = 0;
        result = folderView->GetSelectionState(focused, &selection);
        if (!current()) return E_ABORT;
        record(result);
        if (SUCCEEDED(result)) {
            // A focus-only SelectItem otherwise includes SVSI_DESELECT (zero).
            record(view->SelectItem(focused, (selection & SVSI_SELECT)
                | SVSI_FOCUSED | SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS));
            if (!current()) return E_ABORT;
        }
    }
    return restored;
}
} // namespace filespacer
