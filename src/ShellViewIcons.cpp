#include "ShellViewIcons.h"
#include "MixedFolder.h"
#include "DPI.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <unordered_set>

namespace filespacer {
namespace {
struct IconPosition {
    CComHeapPtr<ITEMID_CHILD> id;
    POINT point = {};
    IconPosition() = default;
    IconPosition(IconPosition &&other) noexcept : id(other.id.Detach()), point(other.point) {}
};

HRESULT decodePositions(const std::vector<uint8_t> &bytes, std::vector<IconPosition> &positions) {
    if (bytes.empty()) return S_OK;
    if (bytes.size() > INT32_MAX) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    if (!stream) return E_OUTOFMEMORY;
    LARGE_INTEGER zero = {};
    ULARGE_INTEGER offset = {};
    for (;;) {
        HRESULT result = stream->Seek(zero, STREAM_SEEK_CUR, &offset);
        if (FAILED(result)) return result;
        if (offset.QuadPart == bytes.size()) return S_OK;
        positions.emplace_back();
        auto &position = positions.back();
        result = IStream_ReadPidl(stream, &position.id);
        if (FAILED(result)) return result;
        if (!position.id) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        result = IStream_Read(stream, &position.point, sizeof(position.point));
        if (FAILED(result)) return result;
    }
}
}

HRESULT captureIconPositions(IFolderView *view, const std::function<bool()> &current,
        bool enumerationComplete, const std::vector<uint8_t> &previous, std::vector<uint8_t> &bytes,
        bool filtered) {
    bytes.clear();
    if (filtered) return S_FALSE;
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    if (!stream) return E_OUTOFMEMORY;
    int count = 0;
    HRESULT result = view->ItemCount(SVGIO_ALLVIEW, &count);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return result;
    if (count < 0) return E_UNEXPECTED;
    std::unordered_set<std::string> visible;
    if (count) {
        CComPtr<IEnumIDList> enumeration;
        result = view->Items(SVGIO_ALLVIEW, IID_PPV_ARGS(&enumeration));
        if (!current()) return E_ABORT;
        if (FAILED(result)) return result;
        if (!enumeration) return E_UNEXPECTED;
        for (;;) {
            CComHeapPtr<ITEMID_CHILD> id;
            result = enumeration->Next(1, &id, nullptr);
            if (!current()) return E_ABORT;
            if (result == S_FALSE) break;
            if (result != S_OK) return FAILED(result) ? result : E_UNEXPECTED;
            if (!id) return E_UNEXPECTED;
            POINT position = {};
            result = view->GetItemPosition(id, &position);
            if (!current()) return E_ABORT;
            if (FAILED(result)) return result;
            position = invScaleDPI(position);
            CComHeapPtr<ITEMID_CHILD> native;
            result = nativeViewChild(view, id, &native);
            if (!current()) return E_ABORT;
            if (FAILED(result)) return result;
            result = IStream_WritePidl(stream, native);
            if (SUCCEEDED(result)) result = IStream_Write(stream, &position, sizeof(position));
            if (FAILED(result)) return result;
            visible.emplace(reinterpret_cast<const char *>(native.m_pData), ILGetSize(native));
        }
    }
    if (!enumerationComplete && !previous.empty()) {
        std::vector<IconPosition> positions;
        result = decodePositions(previous, positions);
        if (FAILED(result)) return result;
        for (const auto &position : positions) {
            const std::string key(reinterpret_cast<const char *>(position.id.m_pData), ILGetSize(position.id));
            if (visible.insert(key).second) {
                result = IStream_WritePidl(stream, position.id);
                if (SUCCEEDED(result)) result = IStream_Write(stream, &position.point, sizeof(position.point));
                if (FAILED(result)) return result;
            }
        }
    }
    STATSTG size = {};
    result = stream->Stat(&size, STATFLAG_NONAME);
    if (FAILED(result)) return result;
    if (size.cbSize.QuadPart > INT32_MAX) return E_OUTOFMEMORY;
    LARGE_INTEGER beginning = {};
    result = stream->Seek(beginning, STREAM_SEEK_SET, nullptr);
    if (FAILED(result)) return result;
    std::vector<uint8_t> captured(static_cast<size_t>(size.cbSize.QuadPart));
    if (!captured.empty()) result = IStream_Read(stream, captured.data(), static_cast<ULONG>(captured.size()));
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result)) bytes.swap(captured);
    return result;
}

HRESULT restoreIconPositions(IFolderView *view, const std::vector<uint8_t> &bytes,
        const std::function<bool()> &current) {
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    std::vector<IconPosition> positions;
    HRESULT result = decodePositions(bytes, positions);
    if (FAILED(result)) return result;
    if (!current()) return E_ABORT;
    for (auto &position : positions) {
        CComHeapPtr<ITEMID_CHILD> shown;
        result = mixedViewChild(view, position.id, &shown);
        if (!current()) return E_ABORT;
        if (FAILED(result)) return result;
        position.id.Free(); position.id.Attach(shown.Detach());
    }
    std::vector<PCUITEMID_CHILD> items;
    std::vector<POINT> points;
    items.reserve(positions.size());
    points.reserve(positions.size());
    for (const auto &position : positions) {
        const POINT point = scaleDPI(position.point);
        POINT actual = {};
        result = view->GetItemPosition(position.id, &actual);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && actual.x == point.x && actual.y == point.y) continue;
        items.push_back(position.id);
        points.push_back(point);
    }
    if (items.empty()) return S_OK;
    result = view->SelectAndPositionItems(static_cast<UINT>(items.size()), items.data(), points.data(),
        SVSI_POSITIONITEM | SVSI_NOSTATECHANGE);
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result)) return result;
    // Keep individual-item recovery for providers that reject a batch or an
    // item removed since capture, preserving the previous partial restore.
    HRESULT restored = S_OK;
    for (size_t i = 0; i < items.size(); ++i) {
        result = view->SelectAndPositionItems(1, &items[i], &points[i], SVSI_POSITIONITEM | SVSI_NOSTATECHANGE);
        if (!current()) return E_ABORT;
        if (FAILED(result) && SUCCEEDED(restored)) restored = result;
    }
    return restored;
}
} // namespace filespacer
