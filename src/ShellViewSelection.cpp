#include "ShellViewSelection.h"
#include "MixedFolder.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <oleacc.h>
#include <UIAutomation.h>
#include <cmath>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <commctrl.h>
#pragma comment(lib, "Oleacc.lib")

namespace filespacer {
namespace {
struct ReopenViewHeader {
    uint32_t version = 1, selected = 0, flags = 0;
    POINT offset = {};
};
// Existing blob layout: header, optional anchor (first displayed item), optional focus, selection.
constexpr uint32_t REOPEN_ANCHOR = 2, REOPEN_FOCUS = 4;
constexpr uint32_t REOPEN_SCROLL = 8;
constexpr uint32_t REOPEN_VIEWPORT = 16;
// A partial selection in a free icon view is captured by index from this count.
constexpr int SPATIAL_PARTIAL_THRESHOLD = 1024;
struct ViewScrollState {
    uint32_t version = 1, kind = 0, mode = 0;
    int32_t iconSize = 0, x = 0, y = 0;
    uint32_t dpi = 0, reserved = 0;
    double horizontal = UIA_ScrollPatternNoScroll, vertical = UIA_ScrollPatternNoScroll;
};
static_assert(sizeof(ViewScrollState) == 48, "Viewport storage format");
struct ReopenScroll {
    double horizontal = UIA_ScrollPatternNoScroll, vertical = UIA_ScrollPatternNoScroll;
    UINT mode = 0, reserved = 0;
};
bool validScrollPercent(double value) {
    return std::isfinite(value) && (value == UIA_ScrollPatternNoScroll || (value >= 0 && value <= 100));
}
bool validViewScroll(const ViewScrollState &state) {
    return state.version == 1 && !state.reserved && state.kind >= 1 && state.kind <= 2
        && state.mode >= FVM_FIRST && state.mode <= FVM_LAST && state.iconSize > 0
        && (state.kind != 1 || (state.dpi && state.dpi <= INT32_MAX))
        && (state.kind != 2 || (validScrollPercent(state.horizontal) && validScrollPercent(state.vertical)));
}

// Microsoft documents this provider-access route for clients already in-process.
// It avoids using the UI Automation client engine against our own GUI thread.
HRESULT queryScrollPattern(IAccessible *accessible, LONG child,
        const std::function<bool()> &current, IScrollProvider **out) {
    *out = nullptr;
    CComQIPtr<IServiceProvider> services(accessible);
    if (!current()) return E_ABORT;
    if (!services) return E_NOINTERFACE;
    CComPtr<IAccessibleEx> extended;
    HRESULT result = services->QueryService(__uuidof(IAccessibleEx), IID_PPV_ARGS(&extended));
    if (!current()) return E_ABORT;
    if (FAILED(result) || !extended) return FAILED(result) ? result : E_NOINTERFACE;
    if (child != CHILDID_SELF) {
        CComPtr<IAccessibleEx> item;
        result = extended->GetObjectForChild(child, &item);
        if (!current()) return E_ABORT;
        if (FAILED(result) || !item) return FAILED(result) ? result : E_NOINTERFACE;
        extended = item;
    }
    CComQIPtr<IRawElementProviderSimple> provider(extended);
    if (!current()) return E_ABORT;
    if (!provider) return E_NOINTERFACE;
    CComPtr<IUnknown> pattern;
    result = provider->GetPatternProvider(UIA_ScrollPatternId, &pattern);
    if (!current()) return E_ABORT;
    if (FAILED(result) || !pattern) return FAILED(result) ? result : E_NOINTERFACE;
    result = pattern->QueryInterface(IID_PPV_ARGS(out));
    return current() ? result : E_ABORT;
}

HRESULT findScrollPattern(IAccessible *accessible, const std::function<bool()> &current,
        unsigned depth, unsigned &visited, IScrollProvider **out) {
    // Bound optional capability discovery; never enumerate thousands of file items.
    if (depth > 8 || visited >= 128) return E_NOINTERFACE;
    ++visited;
    HRESULT result = queryScrollPattern(accessible, CHILDID_SELF, current, out);
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result) && *out) return result;
    LONG count = 0;
    result = accessible->get_accChildCount(&count);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return result;
    for (LONG start = 0; start < count && visited < 128;) {
        struct Variants {
            VARIANT items[16] = {};
            ~Variants() { for (auto &item : items) VariantClear(&item); }
        } values;
        LONG fetched = 0;
        result = AccessibleChildren(accessible, start, count - start < 16 ? count - start : 16,
            values.items, &fetched);
        if (!current()) return E_ABORT;
        if (FAILED(result) || fetched <= 0) break;
        for (LONG i = 0; i < fetched && visited < 128; ++i) {
            const auto &item = values.items[i];
            if (item.vt == VT_DISPATCH && item.pdispVal) {
                CComQIPtr<IAccessible> nested(item.pdispVal);
                if (!current()) return E_ABORT;
                if (nested) result = findScrollPattern(nested, current, depth + 1, visited, out);
            } else if (item.vt == VT_I4) {
                ++visited;
                result = queryScrollPattern(accessible, item.lVal, current, out);
            }
            if (!current()) return E_ABORT;
            if (SUCCEEDED(result) && *out) return result;
        }
        start += fetched;
    }
    return E_NOINTERFACE;
}

HRESULT getScrollPattern(HWND window, const std::function<bool()> &current, IScrollProvider **out) {
    *out = nullptr;
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId()) return E_NOINTERFACE;
    CComPtr<IAccessible> accessible;
    HRESULT result = AccessibleObjectFromWindow(window, static_cast<DWORD>(OBJID_CLIENT),
        IID_PPV_ARGS(&accessible));
    if (!current()) return E_ABORT;
    if (FAILED(result) || !accessible) return FAILED(result) ? result : E_NOINTERFACE;
    unsigned visited = 0;
    return findScrollPattern(accessible, current, 0, visited, out);
}
}

static bool isFreeIconView(IFolderView *view, const std::function<bool()> &current) {
    UINT mode = 0;
    const HRESULT result = view->GetCurrentViewMode(&mode);
    if (!current() || FAILED(result) || (mode != FVM_ICON && mode != FVM_SMALLICON)) return false;
    const HRESULT arranged = view->GetAutoArrange();
    return current() && arranged == S_FALSE;
}

HRESULT captureSpatialSelection(IFolderView2 *view, const std::function<bool()> &current,
        IShellItemArray **out) try {
    if (!view || !out) return E_POINTER;
    CComPtr<IFolderView2> keepView(view);
    *out = nullptr;
    if (!current()) return E_ABORT;
    UINT mode = 0;
    HRESULT result = view->GetCurrentViewMode(&mode);
    if (!current()) return E_ABORT;
    if (FAILED(result) || (mode != FVM_ICON && mode != FVM_SMALLICON)) return S_FALSE;
    result = view->GetAutoArrange();
    if (!current()) return E_ABORT;
    if (result != S_FALSE) return S_FALSE;
    PROPERTYKEY group = {}; BOOL ascending = FALSE;
    result = view->GetGroupBy(&group, &ascending);
    if (!current()) return E_ABORT;
    if (result != S_FALSE) return S_FALSE;
    int count = 0, selected = 0, focused = -1;
    result = view->ItemCount(SVGIO_ALLVIEW, &count);
    if (!current()) return E_ABORT;
    if (FAILED(result) || count < SELECTION_BULK_THRESHOLD) return S_FALSE;
    result = view->ItemCount(SVGIO_SELECTION, &selected);
    if (!current()) return E_ABORT;
    if (FAILED(result) || selected < SELECTION_BULK_THRESHOLD || selected > count) return S_FALSE;
    if (selected != count && selected < SPATIAL_PARTIAL_THRESHOLD) return S_FALSE;
    result = view->GetFocusedItem(&focused);
    if (!current()) return E_ABORT;
    if (FAILED(result) || focused < 0 || focused >= count) return S_FALSE;
    const auto selectedIndices = [&](std::vector<int> &indices) -> HRESULT {
        indices.reserve(static_cast<size_t>(selected));
        CComHeapPtr<ITEMID_CHILD> zero;
        HRESULT hr = view->Item(0, &zero);
        if (!current()) return E_ABORT;
        if (FAILED(hr) || !zero) return S_FALSE;
        DWORD state = 0;
        hr = view->GetSelectionState(zero, &state);
        if (!current()) return E_ABORT;
        if (FAILED(hr)) return S_FALSE;
        if (state & SVSI_SELECT) indices.push_back(0);
        int start = 0;
        while (indices.size() < static_cast<size_t>(selected)) {
            int index = -1;
            hr = view->GetSelectedItem(start, &index);
            if (!current()) return E_ABORT;
            if (hr != S_OK) return S_FALSE;
            // The API describes both "at or after" and "after". Accept either
            // forward-only interpretation without relying on a negative sentinel.
            if (!indices.empty() && index == indices.back()) {
                if (index >= count - 1) return S_FALSE;
                start = index + 1;
                hr = view->GetSelectedItem(start, &index);
                if (!current()) return E_ABORT;
                if (hr != S_OK) return S_FALSE;
            }
            if (index < start || index >= count || (!indices.empty() && index <= indices.back())) return S_FALSE;
            indices.push_back(index);
            start = index;
        }
        return S_OK;
    };
    std::vector<int> indices;
    if (selected != count) {
        result = selectedIndices(indices);
        if (result != S_OK) return result;
    }
    using OwnedId = std::unique_ptr<ITEMID_CHILD, decltype(&CoTaskMemFree)>;
    struct Entry { PCUITEMID_CHILD id; POINT position; int index; };
    std::vector<OwnedId> owned;
    std::vector<Entry> entries;
    std::unordered_set<uint64_t> positions;
    positions.reserve(static_cast<size_t>(selected));
    owned.reserve(static_cast<size_t>(selected)); entries.reserve(static_cast<size_t>(selected));
    for (int slot = 0; slot < selected; ++slot) {
        const int i = selected == count ? slot : indices[static_cast<size_t>(slot)];
        CComHeapPtr<ITEMID_CHILD> id;
        result = view->Item(i, &id);
        if (!current()) return E_ABORT;
        if (FAILED(result) || !id) return S_FALSE;
        POINT position = {};
        result = view->GetItemPosition(id, &position);
        if (!current()) return E_ABORT;
        if (FAILED(result)) return S_FALSE;
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(position.x)) << 32)
            | static_cast<uint32_t>(position.y);
        if (!positions.emplace(key).second) return S_FALSE;
        owned.emplace_back(id.Detach(), &CoTaskMemFree);
        entries.push_back({owned.back().get(), position, i});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.position.x != b.position.x ? a.position.x < b.position.x : a.position.y < b.position.y;
    });
    const auto first = std::find_if(entries.begin(), entries.end(), [&](const Entry &entry) {
        return entry.index == focused;
    });
    std::rotate(entries.begin(), first, entries.end());
    std::vector<PCUITEMID_CHILD> ids;
    ids.reserve(entries.size());
    for (const auto &entry : entries) ids.push_back(entry.id);
    CComPtr<IShellFolder> folder;
    result = view->GetFolder(IID_PPV_ARGS(&folder));
    if (!current()) return E_ABORT;
    if (FAILED(result) || !folder) return S_FALSE;
    CComPtr<IShellItemArray> array;
    result = SHCreateShellItemArray(nullptr, folder, static_cast<UINT>(ids.size()), ids.data(), &array);
    if (!current()) return E_ABORT;
    if (FAILED(result) || !array) return S_FALSE;
    if (selected != count) {
        // Counts alone do not detect replacing one selected item with another.
        std::vector<int> after;
        result = selectedIndices(after);
        if (result != S_OK) return result;
        if (after != indices) return S_FALSE;
    }
    int allAfter = 0, selectedAfter = 0, focusAfter = -1;
    result = view->ItemCount(SVGIO_ALLVIEW, &allAfter);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return S_FALSE;
    result = view->ItemCount(SVGIO_SELECTION, &selectedAfter);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return S_FALSE;
    result = view->GetFocusedItem(&focusAfter);
    if (!current()) return E_ABORT;
    if (FAILED(result) || allAfter != count || selectedAfter != selected || focusAfter != focused) return S_FALSE;
    return array.CopyTo(out);
} catch (const std::bad_alloc &) {
    return E_OUTOFMEMORY;
}

HRESULT nativeViewSelection(IFolderView2 *view, BOOL noneMeansFolder, IShellItemArray **out,
        const std::function<bool()> &current) {
    if (!view || !out) return E_POINTER;
    *out = nullptr;
    CComPtr<IFolderView2> keepView(view);
    const auto valid = [&] { return !current || current(); };
    if (!valid()) return E_ABORT;
    CComPtr<IShellItemArray> selected;
    HRESULT hr = current ? captureSpatialSelection(view, current, &selected) : S_FALSE;
    if (hr == E_ABORT || !valid()) return E_ABORT;
    if (hr != S_OK) {
        hr = view->GetSelection(noneMeansFolder, &selected);
        if (!valid()) return E_ABORT;
    }
    if (FAILED(hr) || !selected) return hr;
    CComPtr<IShellFolder> folder;
    CComPtr<IMixedFolder> mixed;
    hr = view->GetFolder(IID_PPV_ARGS(&folder));
    if (!valid()) return E_ABORT;
    if (SUCCEEDED(hr) && folder) hr = folder->QueryInterface(IID_PPV_ARGS(&mixed));
    if (!valid()) return E_ABORT;
    if (FAILED(hr) || !mixed)
        return selected.CopyTo(out);
    DWORD count = 0;
    hr = selected->GetCount(&count);
    if (!valid()) return E_ABORT;
    if (FAILED(hr)) return hr;
    std::vector<CComHeapPtr<ITEMIDLIST_ABSOLUTE>> owned(count);
    std::vector<PCIDLIST_ABSOLUTE> ids(count);
    for (DWORD i = 0; i < count; ++i) {
        CComPtr<IShellItem> item, native;
        hr = selected->GetItemAt(i, &item);
        if (!valid()) return E_ABORT;
        if (SUCCEEDED(hr)) hr = nativeShellItem(item, &native);
        if (!valid()) return E_ABORT;
        if (SUCCEEDED(hr)) hr = SHGetIDListFromObject(native, &owned[i]);
        if (!valid()) return E_ABORT;
        if (FAILED(hr)) return hr;
        ids[i] = owned[i];
    }
    CComPtr<IShellItemArray> converted;
    hr = SHCreateShellItemArrayFromIDLists(count, ids.data(), &converted);
    if (!valid()) return E_ABORT;
    return FAILED(hr) ? hr : converted.CopyTo(out);
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
    std::vector<uint8_t> viewport;
    result = captureViewScroll(view, current, viewport);
    if (!current()) return E_ABORT;
    if (result == S_OK) {
        header.version = 3;
        header.flags |= REOPEN_VIEWPORT;
    }
    // Legacy fallback: the first item whose reported position lies inside the view's
    // client area. Report and list views answer in client coordinates, so this is
    // the first displayed row. Views answering in unscrolled layout coordinates
    // (icons) yield the first item of the folder, which restoring leaves at the
    // top: the standard behavior, by construction.
    CComHeapPtr<ITEMID_CHILD> anchor;
    bool positionUnavailable = false;
    ReopenScroll scroll;
    HWND viewWindow = nullptr;
    RECT client = {};
    result = view->GetWindow(&viewWindow);
    if (!current()) return E_ABORT;
    if (viewport.empty() && SUCCEEDED(result) && viewWindow && GetClientRect(viewWindow, &client)
            && client.right > 0 && client.bottom > 0) {
        // A column header covers the top of the client area; the rows under it
        // report non-negative positions but are not displayed. Its height comes
        // from its public MSAA role, as for the Alt gestures; no header gives 0.
        LONG topInset = 0;
        POINT probe = {2, 2};
        // An overlapping window would answer for the probe: only the view itself is asked.
        const HWND probed = ClientToScreen(viewWindow, &probe) ? WindowFromPoint(probe) : nullptr;
        if (probed && (probed == viewWindow || IsChild(viewWindow, probed))) {
            CComPtr<IAccessible> accessible;
            CComVariant child, role;
            result = AccessibleObjectFromPoint(probe, &accessible, &child);
            if (!current()) return E_ABORT;
            if (result == S_OK && accessible) {
                result = accessible->get_accRole(child, &role);
                if (!current()) return E_ABORT;
                if (result == S_OK && role.vt == VT_I4 && role.lVal == ROLE_SYSTEM_COLUMNHEADER) {
                    long left = 0, top = 0, width = 0, height = 0;
                    result = accessible->accLocation(&left, &top, &width, &height, child);
                    if (!current()) return E_ABORT;
                    POINT origin = {};
                    if (result == S_OK && ClientToScreen(viewWindow, &origin) && top + height > origin.y)
                        topInset = top + height - origin.y;
                }
            }
        }
        CComPtr<IEnumIDList> items;
        result = folderView->Items(SVGIO_ALLVIEW, IID_PPV_ARGS(&items));
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && items) {
            for (;;) {
                CComHeapPtr<ITEMID_CHILD> id;
                result = items->Next(1, &id, nullptr);
                if (!current()) return E_ABORT;
                if (result != S_OK || !id) break;
                POINT position = {};
                result = folderView->GetItemPosition(id, &position);
                if (!current()) return E_ABORT;
                positionUnavailable = result == E_NOTIMPL || result == E_NOINTERFACE;
                if (FAILED(result)) break;
                if (position.x >= 0 && position.x < client.right && position.y >= topInset
                        && position.y < client.bottom) {
                    anchor.Attach(id.Detach());
                    break;
                }
            }
        }
    }
    if (anchor) header.flags |= REOPEN_ANCHOR;
    if (!anchor && positionUnavailable && viewWindow) {
        CComPtr<IScrollProvider> pattern;
        result = getScrollPattern(viewWindow, current, &pattern);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) result = folderView->GetCurrentViewMode(&scroll.mode);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) result = pattern->get_HorizontalScrollPercent(&scroll.horizontal);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) result = pattern->get_VerticalScrollPercent(&scroll.vertical);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && scroll.mode >= FVM_ICON && scroll.mode <= FVM_CONTENT
                && validScrollPercent(scroll.horizontal) && validScrollPercent(scroll.vertical)
                && (scroll.horizontal != UIA_ScrollPatternNoScroll || scroll.vertical != UIA_ScrollPatternNoScroll)) {
            header.version = 2;
            header.flags |= REOPEN_SCROLL;
        }
    }
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    if (!stream) return E_OUTOFMEMORY;
    result = IStream_Write(stream, &header, sizeof(header));
    if (SUCCEEDED(result) && (header.flags & REOPEN_VIEWPORT))
        result = IStream_Write(stream, viewport.data(), static_cast<ULONG>(viewport.size()));
    if (SUCCEEDED(result) && (header.flags & REOPEN_SCROLL))
        result = IStream_Write(stream, &scroll, sizeof(scroll));
    if (SUCCEEDED(result) && (header.flags & REOPEN_ANCHOR))
        result = IStream_WritePidl(stream, anchor);
    if (SUCCEEDED(result) && (header.flags & REOPEN_FOCUS))
        result = IStream_WritePidl(stream, focused);
    if (FAILED(result)) return result;
    if (count) {
        // A complete selection is an unordered set in this snapshot. Avoid the
        // native spatial sort; providers without usable indices keep Items.
        std::vector<CComHeapPtr<ITEMID_CHILD>> complete;
        const bool indexed = count >= SELECTION_BULK_THRESHOLD && isFreeIconView(folderView, current);
        if (!current()) return E_ABORT;
        if (indexed) {
            int allCount = 0;
            result = folderView->ItemCount(SVGIO_ALLVIEW, &allCount);
            if (!current()) return E_ABORT;
            if (SUCCEEDED(result) && allCount == count) {
                complete.resize(static_cast<size_t>(count));
                for (int i = 0; i < count; ++i) {
                    result = folderView->Item(i, &complete[static_cast<size_t>(i)]);
                    if (!current()) return E_ABORT;
                    if (FAILED(result) || !complete[static_cast<size_t>(i)]) { complete.clear(); break; }
                }
                int selectedCount = 0;
                result = folderView->ItemCount(SVGIO_SELECTION, &selectedCount);
                if (!current()) return E_ABORT;
                if (FAILED(result) || selectedCount != count) complete.clear();
                result = folderView->ItemCount(SVGIO_ALLVIEW, &allCount);
                if (!current()) return E_ABORT;
                if (FAILED(result) || allCount != count) complete.clear();
            }
        }
        CComPtr<IEnumIDList> selectedItems;
        if (complete.empty()) {
            result = folderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selectedItems));
            if (!current()) return E_ABORT;
            if (FAILED(result)) return result;
            if (!selectedItems) return E_UNEXPECTED;
        }
        for (int i = 0; i < count; ++i) {
            CComHeapPtr<ITEMID_CHILD> selectedItem;
            if (complete.empty()) result = selectedItems->Next(1, &selectedItem, nullptr);
            else { selectedItem.Attach(complete[static_cast<size_t>(i)].Detach()); result = S_OK; }
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

static HRESULT matchesViewItems(IFolderView2 *view, UINT flags,
        const std::vector<CComHeapPtr<ITEMID_CHILD>> &wanted,
        const std::function<bool()> &current) try {
    int count = 0;
    HRESULT result = view->ItemCount(flags, &count);
    if (!current()) return E_ABORT;
    if (FAILED(result) || count < 0 || static_cast<size_t>(count) != wanted.size()) return S_FALSE;
    int allCount = 0;
    result = view->ItemCount(SVGIO_ALLVIEW, &allCount);
    if (!current()) return E_ABORT;
    if (FAILED(result) || allCount != count) return S_FALSE;
    const bool indexed = isFreeIconView(view, current);
    if (!current()) return E_ABORT;
    struct Identity { PCUIDLIST_RELATIVE item; UINT size; bool matched; };
    std::vector<Identity> sorted;
    sorted.reserve(wanted.size());
    for (const auto &item : wanted) sorted.push_back({item, ILGetSize(item), false});
    const auto less = [](const Identity &a, const Identity &b) {
        return a.size != b.size ? a.size < b.size : std::memcmp(a.item, b.item, a.size) < 0;
    };
    std::sort(sorted.begin(), sorted.end(), less);
    for (size_t i = 1; i < sorted.size(); ++i)
        if (!less(sorted[i - 1], sorted[i])) return S_FALSE;
    // This proof only accepts complete sets. Free icons can use indices to
    // avoid the Shell's spatial sort. Other renderers retain their fast Items.
    // For SVGIO_SELECTION, equal selected/all counts establish membership;
    // the exact view identities still have to match before and after Select All.
    CComPtr<IEnumIDList> items;
    if (!indexed) {
        result = view->Items(flags, IID_PPV_ARGS(&items));
        if (!current()) return E_ABORT;
        if (FAILED(result) || !items) return S_FALSE;
    }
    for (int i = 0; i < count; ++i) {
        CComHeapPtr<ITEMID_CHILD> item;
        result = indexed ? view->Item(i, &item) : items->Next(1, &item, nullptr);
        if (!current()) return E_ABORT;
        if (result != S_OK || !item) return S_FALSE;
        const Identity key = {item, ILGetSize(item), false};
        const auto found = std::lower_bound(sorted.begin(), sorted.end(), key, less);
        if (found == sorted.end() || less(key, *found) || found->matched) return S_FALSE;
        found->matched = true;
    }
    if (!indexed) {
        CComHeapPtr<ITEMID_CHILD> extra;
        result = items->Next(1, &extra, nullptr);
        if (!current()) return E_ABORT;
        if (result != S_FALSE) return S_FALSE;
    }
    int finalCount = 0;
    result = view->ItemCount(flags, &finalCount);
    if (!current()) return E_ABORT;
    if (FAILED(result) || finalCount != count) return S_FALSE;
    result = view->ItemCount(SVGIO_ALLVIEW, &finalCount);
    if (!current()) return E_ABORT;
    return SUCCEEDED(result) && finalCount == count ? S_OK : S_FALSE;
} catch (const std::bad_alloc &) { return S_FALSE; }

HRESULT restoreReopenSelection(IShellView *view, std::vector<uint8_t> &pending,
        const std::function<bool()> &current, const ShellSelectionClear *clear) {
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
    if ((header.version != 1 && header.version != 2 && header.version != 3)
            || (header.flags & ~(header.version == 1 ? 7u : header.version == 2 ? 15u : REOPEN_FOCUS | REOPEN_VIEWPORT))
            || (header.version == 3 && !(header.flags & REOPEN_VIEWPORT))
            || ((header.flags & REOPEN_SCROLL) && (header.flags & REOPEN_ANCHOR))
            || header.selected > bytes.size() / 2)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    std::vector<uint8_t> viewport;
    if (header.flags & REOPEN_VIEWPORT) {
        ViewScrollState state;
        result = IStream_Read(stream, &state, sizeof(state));
        if (FAILED(result)) return result;
        if (!validViewScroll(state)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        viewport.resize(sizeof(state));
        std::memcpy(viewport.data(), &state, sizeof(state));
    }
    ReopenScroll scroll;
    if (header.flags & REOPEN_SCROLL) {
        result = IStream_Read(stream, &scroll, sizeof(scroll));
        if (FAILED(result)) return result;
        if (scroll.reserved || scroll.mode < FVM_ICON || scroll.mode > FVM_CONTENT
                || !validScrollPercent(scroll.horizontal) || !validScrollPercent(scroll.vertical))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
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
        // Scrolled to the end first: revealing the anchor from below aligns it at
        // the top, whatever the row height or the scrollbars. Redraw is paused so
        // the two moves paint once. Selection and focus below must not scroll away.
        int count = 0;
        CComHeapPtr<ITEMID_CHILD> last;
        result = folderView->ItemCount(SVGIO_ALLVIEW, &count);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && count > 0) {
            result = folderView->Item(count - 1, &last);
            if (!current()) return E_ABORT;
        }
        // Redraw returns on every exit, including an abandoned restoration on a live view.
        struct RedrawGuard {
            IFolderView2 *view;
            bool held;
            ~RedrawGuard() { if (held) view->SetRedraw(TRUE); }
        } redraw{folderView, true};
        folderView->SetRedraw(FALSE);
        if (!current()) return E_ABORT;
        // Revealing an item must retain its selection bit: otherwise scrolling
        // alone would force the complete selection to be rebuilt below.
        auto reveal = [&](PCUITEMID_CHILD item) {
            DWORD selection = 0;
            const HRESULT queried = folderView->GetSelectionState(item, &selection);
            if (!current()) return E_ABORT;
            if (FAILED(queried)) return queried;
            return view->SelectItem(item, (selection & SVSI_SELECT)
                | SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS);
        };
        if (last) {
            record(reveal(last));
            if (!current()) return E_ABORT;
        }
        record(reveal(anchor));
        if (!current()) return E_ABORT;
        redraw.held = false;
        folderView->SetRedraw(TRUE);
        if (!current()) return E_ABORT;
    }
    ShellSelectionClear fallback;
    const ShellSelectionClear &operation = clear ? *clear : fallback;
    // Mode changes normally retain selection. Avoid generating a deselection
    // and one selection notification per item when the exact set is unchanged.
    int selectedCount = 0;
    result = folderView->ItemCount(SVGIO_SELECTION, &selectedCount);
    if (!current()) return E_ABORT;
    bool selectionCountKnown = SUCCEEDED(result) && selectedCount >= 0;
    bool sameSelection = selectionCountKnown
        && static_cast<uint32_t>(selectedCount) == header.selected;
    if (sameSelection) {
        for (const auto &selectedItem : selectedItems) {
            DWORD selection = 0;
            result = folderView->GetSelectionState(selectedItem, &selection);
            if (!current()) return E_ABORT;
            if (FAILED(result) || !(selection & SVSI_SELECT)) {
                sameSelection = false;
                break;
            }
        }
    }
    if (!sameSelection && selectedItems.size() >= static_cast<size_t>(SELECTION_BULK_THRESHOLD)) {
        result = matchesViewItems(folderView, SVGIO_ALLVIEW, selectedItems, current);
        if (!current()) return E_ABORT;
        if (result == S_OK) {
            // Backend selection and native complete-set support belong to the
            // operation adapter. Identity proofs remain independent of it.
            selectionCountKnown = false;
            result = operation.selectAll(view, current);
            if (!current()) return E_ABORT;
            if (SUCCEEDED(result)) {
                result = matchesViewItems(folderView, SVGIO_SELECTION, selectedItems, current);
                if (!current()) return E_ABORT;
                sameSelection = result == S_OK;
            }
        }
    }
    if (!sameSelection) {
        using OwnedIdentity = std::unique_ptr<ITEMID_CHILD, decltype(&CoTaskMemFree)>;
        std::vector<OwnedIdentity> deselect;
        std::vector<PCUITEMID_CHILD> select;
        bool planned = selectionCountKnown && selectedCount == 0;
        if (selectionCountKnown && selectedCount > 0 && !selectedItems.empty()) {
            auto plan = [&]() -> HRESULT {
                // Equal opaque PIDLs in this view can be retained directly.
                // A different encoding is harmless: deselect/reselect it through
                // the Shell, which remains responsible for resolving its identity.
                auto key = [](PCUIDLIST_RELATIVE item) {
                    return std::string(reinterpret_cast<const char *>(item), ILGetSize(item));
                };
                std::unordered_map<std::string, PCUIDLIST_RELATIVE> wanted;
                wanted.reserve(selectedItems.size());
                for (const auto &item : selectedItems)
                    if (!wanted.emplace(key(item), item).second) return E_INVALIDARG;
                if (selectedItems.size() >= static_cast<size_t>(selectedCount)) {
                    int retainedCount = 0;
                    bool complete = true;
                    for (const auto &item : selectedItems) {
                        DWORD state = 0;
                        const HRESULT probed = folderView->GetSelectionState(item, &state);
                        if (!current()) return E_ABORT;
                        if (FAILED(probed)) { complete = false; break; }
                        if (state & SVSI_SELECT) ++retainedCount;
                        else select.push_back(item);
                    }
                    // Pure additions need no enumeration of the existing selection.
                    if (complete && retainedCount == selectedCount) return S_OK;
                    if (complete && retainedCount == 0) return S_FALSE;
                    select.clear();
                }
                CComPtr<IEnumIDList> selected;
                HRESULT queried = folderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selected));
                if (!current()) return E_ABORT;
                if (FAILED(queried) || !selected) return FAILED(queried) ? queried : E_UNEXPECTED;
                std::unordered_set<PCUIDLIST_RELATIVE> retained;
                retained.reserve(selectedItems.size());
                for (int i = 0; i < selectedCount; ++i) {
                    CComHeapPtr<ITEMID_CHILD> item;
                    queried = selected->Next(1, &item, nullptr);
                    if (!current()) return E_ABORT;
                    if (queried != S_OK || !item)
                        return FAILED(queried) ? queried : E_UNEXPECTED;
                    const auto found = wanted.find(key(item));
                    if (found == wanted.end()) {
                        OwnedIdentity owned(item.Detach(), &CoTaskMemFree);
                        deselect.push_back(std::move(owned));
                    } else retained.insert(found->second);
                }
                CComHeapPtr<ITEMID_CHILD> extra;
                queried = selected->Next(1, &extra, nullptr);
                if (!current()) return E_ABORT;
                if (queried != S_FALSE) return FAILED(queried) ? queried : E_UNEXPECTED;
                // Keep the snapshot order; reversing it can make later native
                // selection operations substantially slower.
                for (const auto &item : selectedItems)
                    if (!retained.count(item)) select.push_back(item);
                return retained.empty() ? S_FALSE : S_OK;
            };
            result = plan();
            if (!current()) return E_ABORT;
            planned = result == S_OK;
            if (!planned) { deselect.clear(); select.clear(); }
        }
        // Selection, focus and scroll are distinct parts. A failed deselection
        // abandons the selection only: it is never completed over a stale one,
        // and focus and scroll are still restored below.
        bool deselected = true;
        if (!planned) {
            // Providers without a usable selection enumeration keep the original path.
            result = operation.clear(view, current);
            if (!current()) return E_ABORT;
            // A clear refused during a reentrant global selection (E_PENDING) is a
            // failed deselection like any other: recorded, never replayed later.
            if (FAILED(result)) { record(result); deselected = false; }
        } else {
            for (const auto &item : deselect) {
                result = view->SelectItem(item.get(), SVSI_NOTAKEFOCUS);
                if (!current()) return E_ABORT;
                if (FAILED(result)) { record(result); deselected = false; break; }
            }
        }
        if (!deselected) select.clear();
        else if (!planned || selectedCount == 0)
            for (const auto &item : selectedItems) select.push_back(item);
        for (const auto item : select) {
            record(view->SelectItem(item, SVSI_SELECT | SVSI_NOTAKEFOCUS));
            if (!current()) return E_ABORT;
        }
    }
    CComPtr<IScrollProvider> scrollPattern;
    if (header.flags & REOPEN_SCROLL) {
        UINT mode = 0;
        result = folderView->GetCurrentViewMode(&mode);
        if (!current()) return E_ABORT;
        // Percentages only describe the captured presentation, not a different mode.
        if (SUCCEEDED(result) && mode == scroll.mode) {
            HWND window = nullptr;
            result = view->GetWindow(&window);
            if (!current()) return E_ABORT;
            if (SUCCEEDED(result) && window) result = getScrollPattern(window, current, &scrollPattern);
            if (!current()) return E_ABORT;
            // A different provider/profile may not expose this optional pattern.
            if (result != E_NOINTERFACE) record(result);
        }
    }
    if (focused) {
        DWORD selection = 0;
        result = folderView->GetSelectionState(focused, &selection);
        if (!current()) return E_ABORT;
        record(result);
        if (SUCCEEDED(result)) {
            // A focus-only SelectItem otherwise includes SVSI_DESELECT (zero).
            record(view->SelectItem(focused, (selection & SVSI_SELECT) | SVSI_FOCUSED
                | (anchor || scrollPattern || !viewport.empty() ? 0 : SVSI_ENSUREVISIBLE) | SVSI_NOTAKEFOCUS));
            if (!current()) return E_ABORT;
        }
    }
    if (scrollPattern) {
        BOOL horizontal = FALSE, vertical = FALSE;
        result = scrollPattern->get_HorizontallyScrollable(&horizontal);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) result = scrollPattern->get_VerticallyScrollable(&vertical);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result) && (horizontal || vertical))
            result = scrollPattern->SetScrollPercent(horizontal ? scroll.horizontal : UIA_ScrollPatternNoScroll,
                vertical ? scroll.vertical : UIA_ScrollPatternNoScroll);
        if (!current()) return E_ABORT;
        record(result);
    }
    if (!viewport.empty()) {
        record(restoreViewScroll(view, viewport, current));
        if (!current()) return E_ABORT;
    }
    return restored;
}
namespace {
struct ScrollListSearch { HWND window = nullptr; unsigned count = 0; };
BOOL CALLBACK findScrollList(HWND window, LPARAM data) {
    wchar_t name[32] = {}; DWORD process = 0;
    if (GetClassNameW(window, name, _countof(name)) && wcscmp(name, WC_LISTVIEWW) == 0
            && GetWindowThreadProcessId(window, &process) == GetCurrentThreadId()
            && process == GetCurrentProcessId()) {
        auto &search = *reinterpret_cast<ScrollListSearch *>(data);
        search.window = window; ++search.count;
    }
    return TRUE;
}
HWND scrollList(IFolderView2 *view, HWND parent, UINT mode, const std::function<bool()> &current) {
    DWORD process = 0;
    if (GetWindowThreadProcessId(parent, &process) != GetCurrentThreadId()
            || process != GetCurrentProcessId()) return nullptr;
    ScrollListSearch search;
    EnumChildWindows(parent, findScrollList, reinterpret_cast<LPARAM>(&search));
    if (!current() || search.count != 1 || !IsWindowVisible(search.window)) return nullptr;
    int expected = -1;
    switch (mode) {
        case FVM_ICON: expected = LV_VIEW_ICON; break;
        case FVM_SMALLICON: expected = LV_VIEW_SMALLICON; break;
        case FVM_LIST: expected = LV_VIEW_LIST; break;
        case FVM_DETAILS: expected = LV_VIEW_DETAILS; break;
        case FVM_TILE: case FVM_CONTENT: expected = LV_VIEW_TILE; break;
        default: return nullptr;
    }
    const int actual = static_cast<int>(ListView_GetView(search.window));
    if (!current() || actual != expected) return nullptr;
    int count = 0;
    const HRESULT result = view->ItemCount(SVGIO_ALLVIEW, &count);
    if (!current() || FAILED(result) || count <= 0) return nullptr;
    const int nativeCount = ListView_GetItemCount(search.window);
    return current() && nativeCount == count ? search.window : nullptr;
}
}

HRESULT captureViewScroll(IShellView *view, const std::function<bool()> &current, std::vector<uint8_t> &bytes) {
    bytes.clear();
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    CComQIPtr<IFolderView2> folder(view);
    if (!current()) return E_ABORT;
    if (!folder) return S_FALSE;
    ViewScrollState state;
    FOLDERVIEWMODE mode = FVM_AUTO;
    int iconSize = 0;
    HRESULT result = folder->GetViewModeAndIconSize(&mode, &iconSize);
    if (!current()) return E_ABORT;
    if (FAILED(result) || mode < FVM_FIRST || mode > FVM_LAST || iconSize <= 0) return S_FALSE;
    state.mode = static_cast<uint32_t>(mode); state.iconSize = iconSize;
    HWND window = nullptr;
    result = view->GetWindow(&window);
    if (!current()) return E_ABORT;
    if (FAILED(result) || !window) return S_FALSE;
    const HWND list = scrollList(folder, window, state.mode, current);
    if (!current()) return E_ABORT;
    if (list) {
        RECT first = {};
        const BOOL located = ListView_GetItemRect(list, 0, &first, LVIR_BOUNDS);
        if (!current()) return E_ABORT;
        state.dpi = GetDpiForWindow(list);
        if (!located || first.right <= first.left || first.bottom <= first.top
                || !state.dpi || state.dpi > INT32_MAX) return S_FALSE;
        state.kind = 1; state.x = first.left; state.y = first.top;
    } else {
        CComPtr<IScrollProvider> pattern;
        result = getScrollPattern(window, current, &pattern);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) result = pattern->get_HorizontalScrollPercent(&state.horizontal);
        if (!current()) return E_ABORT;
        if (SUCCEEDED(result)) result = pattern->get_VerticalScrollPercent(&state.vertical);
        if (!current()) return E_ABORT;
        if (FAILED(result) || !validScrollPercent(state.horizontal) || !validScrollPercent(state.vertical)) return S_FALSE;
        state.kind = 2;
    }
    bytes.resize(sizeof(state));
    std::memcpy(bytes.data(), &state, sizeof(state));
    return S_OK;
}

HRESULT restoreViewScroll(IShellView *view, const std::vector<uint8_t> &bytes, const std::function<bool()> &current) {
    if (bytes.empty()) return S_FALSE;
    if (!view) return E_POINTER;
    if (!current()) return E_ABORT;
    if (bytes.size() != sizeof(ViewScrollState)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    ViewScrollState state;
    std::memcpy(&state, bytes.data(), sizeof(state));
    if (!validViewScroll(state))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    CComQIPtr<IFolderView2> folder(view);
    if (!current()) return E_ABORT;
    if (!folder) return S_FALSE;
    FOLDERVIEWMODE mode = FVM_AUTO; int iconSize = 0;
    HRESULT result = folder->GetViewModeAndIconSize(&mode, &iconSize);
    if (!current()) return E_ABORT;
    if (FAILED(result) || static_cast<uint32_t>(mode) != state.mode || iconSize != state.iconSize) return S_FALSE;
    HWND window = nullptr;
    result = view->GetWindow(&window);
    if (!current()) return E_ABORT;
    if (FAILED(result) || !window) return S_FALSE;
    if (state.kind == 1) {
        HWND list = scrollList(folder, window, state.mode, current);
        if (!current()) return E_ABORT;
        if (!list) return S_FALSE;
        RECT first = {};
        BOOL located = ListView_GetItemRect(list, 0, &first, LVIR_BOUNDS);
        if (!current()) return E_ABORT;
        UINT dpi = GetDpiForWindow(list);
        if (located && dpi && dpi <= INT32_MAX
                && static_cast<int64_t>(first.left) == static_cast<int64_t>(state.x) * dpi / state.dpi
                && static_cast<int64_t>(first.top) == static_cast<int64_t>(state.y) * dpi / state.dpi)
            return S_OK;
        // Re-enabling redraw completes a newly enumerated view's pending
        // layout. Measure and scroll only after its native range is available.
        result = folder->SetRedraw(TRUE);
        if (!current()) return E_ABORT;
        if (FAILED(result)) return result;
        list = scrollList(folder, window, state.mode, current);
        if (!current()) return E_ABORT;
        if (!list) return S_FALSE;
        located = ListView_GetItemRect(list, 0, &first, LVIR_BOUNDS);
        if (!current()) return E_ABORT;
        dpi = GetDpiForWindow(list);
        if (!located || !dpi || dpi > INT32_MAX) return S_FALSE;
        const int64_t x = static_cast<int64_t>(first.left) - static_cast<int64_t>(state.x) * dpi / state.dpi;
        const int64_t y = static_cast<int64_t>(first.top) - static_cast<int64_t>(state.y) * dpi / state.dpi;
        if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX) return S_FALSE;
        if (!x && !y) return S_OK;
        // A visible native ListView animates LVM_SCROLL for roughly 200 ms.
        // One explicit pixel delta needs no intermediate frame or layout query.
        struct Resume {
            IFolderView2 *view;
            bool held = false;
            ~Resume() { if (held) view->SetRedraw(TRUE); }
        } redraw{folder};
        redraw.held = SUCCEEDED(folder->SetRedraw(FALSE));
        if (!current()) return E_ABORT;
        const BOOL scrolled = ListView_Scroll(list, static_cast<int>(x), static_cast<int>(y));
        if (!current()) return E_ABORT;
        HRESULT resumed = S_OK;
        if (redraw.held) {
            redraw.held = false;
            resumed = folder->SetRedraw(TRUE);
            if (!current()) return E_ABORT;
            if (SUCCEEDED(resumed)) RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        return !current() ? E_ABORT : FAILED(resumed) ? resumed : scrolled ? S_OK : S_FALSE;
    }
    CComPtr<IScrollProvider> pattern;
    result = getScrollPattern(window, current, &pattern);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return S_FALSE;
    BOOL horizontal = FALSE, vertical = FALSE;
    result = pattern->get_HorizontallyScrollable(&horizontal);
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result)) result = pattern->get_VerticallyScrollable(&vertical);
    if (!current()) return E_ABORT;
    if (FAILED(result)) return S_FALSE;
    if (horizontal || vertical) result = pattern->SetScrollPercent(horizontal ? state.horizontal : UIA_ScrollPatternNoScroll,
        vertical ? state.vertical : UIA_ScrollPatternNoScroll);
    return !current() ? E_ABORT : result;
}
} // namespace filespacer
