#include "ShellViewIcons.h"
#include "DPI.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <unordered_set>
#include <map>
#include <set>
#include <memory>
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

namespace filespacer {
namespace {
using IconPosition = PreparedIconPosition;

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

// The ListView postpones layout work while redraw is disabled:
// https://devblogs.microsoft.com/oldnewthing/20110124-00/?p=11683
// Some Shell implementations resume it inside SelectAndPositionItems. Retain
// those resumes only for this batch, on an unambiguous native child. Discovery
// is an optional implementation dependency: unsupported views stay native.
//
// Documented second exception to the public-interfaces rule
// (docs/en/native-shell-view.md): 5,000 saved positions were restored in
// about 3.7 s with this guard against about 162 s under
// IFolderView2::SetRedraw alone. Invariant: during the batch the control has
// no WS_VISIBLE, the documented effect of the WM_SETREDRAW(FALSE) that
// SetRedraw(FALSE) passes to DefWindowProc. The guard never changes that
// state itself: it only withholds the Shell's internal WM_SETREDRAW(TRUE)
// resumes, then lets IFolderView2::SetRedraw(TRUE) restore the style. That
// message only re-adds WS_VISIBLE, so the repaint withheld with the resumes
// is requested afterwards on the public view window (RedrawWindow with
// RDW_ALLCHILDREN, the repaint the WM_SETREDRAW contract prescribes). If
// SetRedraw(TRUE) fails, the subclass is removed and nothing is sent to the
// control.
class IconRedrawGuard {
    struct State {
        HWND window = nullptr;
        HWND view = nullptr;
        bool holding = true;
        bool detachedOwner = false;
    };
    struct Search { HWND window = nullptr; unsigned count = 0; };
    static BOOL CALLBACK findList(HWND window, LPARAM data) {
        wchar_t name[32] = {};
        DWORD process = 0;
        if (GetClassNameW(window, name, _countof(name)) && wcscmp(name, WC_LISTVIEWW) == 0
                && GetWindowThreadProcessId(window, &process) == GetCurrentThreadId()
                && process == GetCurrentProcessId()) {
            auto &search = *reinterpret_cast<Search *>(data);
            search.window = window;
            ++search.count;
        }
        return TRUE;
    }
    static LRESULT CALLBACK subclass(HWND window, UINT message, WPARAM wParam,
            LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
        auto state = reinterpret_cast<State *>(data);
        if (message == WM_SETREDRAW && wParam && state->holding) return 0;
        if (message == WM_NCDESTROY) {
            state->window = nullptr;
            RemoveWindowSubclass(window, subclass, id);
            if (state->detachedOwner) delete state;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }
    CComPtr<IFolderView2> redraw;
    std::unique_ptr<State> state;
public:
    IconRedrawGuard(IFolderView *view, const std::function<bool()> &current) {
        CComQIPtr<IFolderView2> candidate(view);
        if (!current() || !candidate) return;
        CComQIPtr<IShellView> source(view);
        if (!current() || !source) return;
        HWND parent = nullptr;
        const HRESULT result = source->GetWindow(&parent);
        if (!current() || FAILED(result) || !parent || !IsWindowVisible(parent)) return;
        DWORD process = 0;
        if (GetWindowThreadProcessId(parent, &process) != GetCurrentThreadId()
                || process != GetCurrentProcessId()) return;
        Search search;
        EnumChildWindows(parent, findList, reinterpret_cast<LPARAM>(&search));
        DWORD_PTR existing = 0;
        if (search.count != 1 || !IsWindowVisible(search.window)
                || GetPropW(search.window, L"SysSetRedraw")
                || GetWindowSubclass(search.window, subclass, 1, &existing)) return;
        state.reset(new State);
        state->window = search.window;
        state->view = parent;
        if (!SetWindowSubclass(search.window, subclass, 1, reinterpret_cast<DWORD_PTR>(state.get()))) {
            state.reset();
            return;
        }
        redraw = candidate;
        const HRESULT suspended = redraw->SetRedraw(FALSE);
        if (FAILED(suspended) || !current()) finish();
    }
    ~IconRedrawGuard() { finish(); }
    IconRedrawGuard(const IconRedrawGuard &) = delete;
    IconRedrawGuard &operator=(const IconRedrawGuard &) = delete;
    HRESULT finish() {
        if (!state) return S_OK;
        state->holding = false;
        HRESULT result = S_OK;
        // WM_NCDESTROY clears window before a handle can be reused. Resume our
        // original control even when the caller's active generation changed.
        // Only the Shell resumes its control; a failure leaves it untouched.
        if (state->window) {
            result = redraw->SetRedraw(TRUE);
            if (state->window) {
                if (RemoveWindowSubclass(state->window, subclass, 1)) state->window = nullptr;
                if (SUCCEEDED(result))
                    RedrawWindow(state->view, nullptr, nullptr,
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
            }
        }
        if (state->window) {
            // If removal fails, keep a disarmed heap object until NCDESTROY.
            // It must never point into a completed restoration's stack frame.
            state->detachedOwner = true;
            (void)state.release();
        } else state.reset();
        redraw.Release();
        return result;
    }
};

struct IdentityFailure { HRESULT result; };
struct PositionOrder {
    CComPtr<IShellFolder> folder;
    std::function<bool()> current;
    bool operator()(PCUIDLIST_RELATIVE a, PCUIDLIST_RELATIVE b) const {
        if (current && !current()) throw IdentityFailure{E_ABORT};
        const HRESULT result = folder->CompareIDs(SHCIDS_CANONICALONLY, a, b);
        if (current && !current()) throw IdentityFailure{E_ABORT};
        if (FAILED(result)) throw IdentityFailure{result};
        return static_cast<short>(HRESULT_CODE(result)) < 0;
    }
};

HRESULT encodePositions(const std::vector<IconPosition> &positions, std::vector<uint8_t> &bytes) {
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    if (!stream) return E_OUTOFMEMORY;
    for (const auto &position : positions) {
        HRESULT result = IStream_WritePidl(stream, position.id);
        if (SUCCEEDED(result)) result = IStream_Write(stream, &position.point, sizeof(position.point));
        if (FAILED(result)) return result;
    }
    STATSTG size = {};
    HRESULT result = stream->Stat(&size, STATFLAG_NONAME);
    if (FAILED(result)) return result;
    if (size.cbSize.QuadPart > INT32_MAX) return E_OUTOFMEMORY;
    LARGE_INTEGER beginning = {};
    result = stream->Seek(beginning, STREAM_SEEK_SET, nullptr);
    if (FAILED(result)) return result;
    std::vector<uint8_t> encoded(static_cast<size_t>(size.cbSize.QuadPart));
    if (!encoded.empty()) result = IStream_Read(stream, encoded.data(), static_cast<ULONG>(encoded.size()));
    if (SUCCEEDED(result)) bytes.swap(encoded);
    return result;
}
}

HRESULT extendIconBaseline(const std::vector<uint8_t> &baseline,
        const std::vector<uint8_t> &observed, std::vector<uint8_t> &bytes,
        IShellFolder *identityFolder, const std::function<bool()> &current) {
    if (!identityFolder) return E_POINTER;
    if (current && !current()) return E_ABORT;
    std::vector<IconPosition> positions, additions;
    HRESULT result = decodePositions(baseline, positions);
    if (SUCCEEDED(result)) result = decodePositions(observed, additions);
    if (FAILED(result)) return result;
    PositionOrder order{identityFolder, current};
    try {
        std::set<PCUIDLIST_RELATIVE, PositionOrder> known(order);
        for (const auto &position : positions) known.insert(position.id);
        for (auto &position : additions)
            if (known.insert(position.id).second) positions.push_back(std::move(position));
    } catch (const IdentityFailure &failure) { return failure.result; }
    return encodePositions(positions, bytes);
}

HRESULT mergeIconChanges(const std::vector<uint8_t> &reference,
        const std::vector<uint8_t> &baseline, const std::vector<uint8_t> &current,
        bool complete, std::vector<uint8_t> &bytes, IShellFolder *identityFolder,
        const std::function<bool()> &valid) {
    if (!identityFolder) return E_POINTER;
    if (valid && !valid()) return E_ABORT;
    std::vector<IconPosition> saved, before, now;
    HRESULT result = decodePositions(reference, saved);
    if (SUCCEEDED(result)) result = decodePositions(baseline, before);
    if (SUCCEEDED(result)) result = decodePositions(current, now);
    if (FAILED(result)) return result;
    std::vector<IconPosition> merged;
    PositionOrder order{identityFolder, valid};
    try {
        std::map<PCUIDLIST_RELATIVE, POINT, PositionOrder> initial(order);
        std::map<PCUIDLIST_RELATIVE, const IconPosition *, PositionOrder> visible(order);
        for (const auto &position : before) initial.emplace(position.id, position.point);
        for (const auto &position : now) visible.emplace(position.id, &position);
        std::set<PCUIDLIST_RELATIVE, PositionOrder> retained(order);
        // Legacy partial captures may contain binary-distinct PIDLs for the
        // same Shell item. The visible observation was encoded first.
        std::vector<bool> keep(saved.size(), false);
        {
            std::map<PCUIDLIST_RELATIVE, size_t, PositionOrder> first(order);
            for (size_t i = 0; i < saved.size(); ++i) first.emplace(saved[i].id, i);
            for (const auto &entry : first) keep[entry.second] = true;
        }
        for (size_t i = 0; i < saved.size(); ++i) {
            if (!keep[i]) continue;
            auto &position = saved[i];
            const auto present = visible.find(position.id);
            const auto first = initial.find(position.id);
            if (complete && present == visible.end()) continue;
            if (present != visible.end()) {
                const auto &observed = *present->second;
                if (first != initial.end() && (observed.point.x != first->second.x || observed.point.y != first->second.y))
                    position.point = observed.point;
                // Persist the current opaque PIDL, retaining the chosen saved
                // coordinate when only its metadata changed.
                auto refreshed = reinterpret_cast<ITEMID_CHILD *>(ILClone(observed.id));
                if (!refreshed) return E_OUTOFMEMORY;
                position.id.Free(); position.id.Attach(refreshed);
            }
            retained.insert(position.id);
            merged.push_back(std::move(position));
        }
        for (auto &position : now)
            if (visible.find(position.id)->second == &position && retained.insert(position.id).second)
                merged.push_back(std::move(position));
    } catch (const IdentityFailure &failure) { return failure.result; }
    return encodePositions(merged, bytes);
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
            result = IStream_WritePidl(stream, id);
            if (SUCCEEDED(result)) result = IStream_Write(stream, &position, sizeof(position));
            if (FAILED(result)) return result;
            visible.emplace(reinterpret_cast<const char *>(id.m_pData), ILGetSize(id));
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

HRESULT prepareIconPositions(IFolderView *view, const std::vector<uint8_t> &bytes,
        const std::function<bool()> &current, PreparedIconPositions &prepared) {
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    PreparedIconPositions candidate;
    candidate.view = view;
    HRESULT result = decodePositions(bytes, candidate.positions);
    if (FAILED(result)) return result;
    if (!current()) return E_ABORT;
    candidate.items.reserve(candidate.positions.size());
    candidate.points.reserve(candidate.positions.size());
    for (const auto &position : candidate.positions) {
        const POINT point = scaleDPI(position.point);
        POINT actual = {};
        result = view->GetItemPosition(position.id, &actual);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && actual.x == point.x && actual.y == point.y) continue;
        candidate.items.push_back(position.id);
        candidate.points.push_back(point);
    }
    candidate.ready = true;
    prepared = std::move(candidate);
    return S_OK;
}

HRESULT applyIconPositions(IFolderView *view, PreparedIconPositions &prepared,
        const std::function<bool()> &current) {
    if (!view) return E_POINTER;
    if (!prepared.ready || prepared.view != view) return E_INVALIDARG;
    prepared.ready = false; // Consume before native calls can dispatch messages.
    if (!current()) return E_ABORT;
    if (prepared.items.empty()) return S_OK;
    IconRedrawGuard redraw(view, current);
    if (!current()) return E_ABORT;
    HRESULT result = view->SelectAndPositionItems(static_cast<UINT>(prepared.items.size()),
        prepared.items.data(), prepared.points.data(), SVSI_POSITIONITEM | SVSI_NOSTATECHANGE);
    if (!current()) return E_ABORT;
    if (FAILED(result)) {
        // Retain per-item recovery for providers rejecting a batch. A partial
        // failure stays a failure even when later items can be positioned.
        HRESULT restored = S_OK;
        for (size_t i = 0; i < prepared.items.size(); ++i) {
            result = view->SelectAndPositionItems(1, &prepared.items[i], &prepared.points[i],
                SVSI_POSITIONITEM | SVSI_NOSTATECHANGE);
            if (!current()) return E_ABORT;
            if (FAILED(result) && SUCCEEDED(restored)) restored = result;
        }
        result = restored;
    }
    const HRESULT resumed = redraw.finish();
    return FAILED(result) ? result : resumed;
}

HRESULT restoreIconPositions(IFolderView *view, const std::vector<uint8_t> &bytes,
        const std::function<bool()> &current) {
    PreparedIconPositions prepared;
    const HRESULT result = prepareIconPositions(view, bytes, current, prepared);
    return FAILED(result) ? result : applyIconPositions(view, prepared, current);
}
} // namespace filespacer
