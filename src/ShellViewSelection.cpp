#include "ShellViewSelection.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <oleacc.h>
#include <UIAutomation.h>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
struct ReopenScroll {
    double horizontal = UIA_ScrollPatternNoScroll, vertical = UIA_ScrollPatternNoScroll;
    UINT mode = 0, reserved = 0;
};
bool validScrollPercent(double value) {
    return std::isfinite(value) && (value == UIA_ScrollPatternNoScroll || (value >= 0 && value <= 100));
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
    // Scroll anchor: the first item whose reported position lies inside the view's
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
    if (SUCCEEDED(result) && viewWindow && GetClientRect(viewWindow, &client)
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
    if (SUCCEEDED(result) && (header.flags & REOPEN_SCROLL))
        result = IStream_Write(stream, &scroll, sizeof(scroll));
    if (SUCCEEDED(result) && (header.flags & REOPEN_ANCHOR))
        result = IStream_WritePidl(stream, anchor);
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
    if ((header.version != 1 && header.version != 2)
            || (header.flags & ~(header.version == 1 ? 7u : 15u))
            || ((header.flags & REOPEN_SCROLL) && (header.flags & REOPEN_ANCHOR))
            || header.selected > bytes.size() / 2)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
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
    // Mode changes normally retain selection. Avoid generating a deselection
    // and one selection notification per item when the exact set is unchanged.
    int selectedCount = 0;
    result = folderView->ItemCount(SVGIO_SELECTION, &selectedCount);
    if (!current()) return E_ABORT;
    const bool selectionCountKnown = SUCCEEDED(result) && selectedCount >= 0;
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
        if (!planned) {
            // Providers without a usable selection enumeration keep the original path.
            result = view->SelectItem(nullptr, SVSI_DESELECTOTHERS);
            if (!current()) return E_ABORT;
            if (FAILED(result)) return result;
        } else {
            for (const auto &item : deselect) {
                result = view->SelectItem(item.get(), SVSI_NOTAKEFOCUS);
                if (!current()) return E_ABORT;
                if (FAILED(result)) return result;
            }
        }
        if (!planned || selectedCount == 0)
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
                | (anchor || scrollPattern ? 0 : SVSI_ENSUREVISIBLE) | SVSI_NOTAKEFOCUS));
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
    return restored;
}
} // namespace filespacer
