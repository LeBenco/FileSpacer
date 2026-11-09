#pragma once
#include <common.h>
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace filespacer {
// Full-width Details columns for the classic Shell view. Only the public IShellView
// window is subclassed. Notifications identify their senders; child controls
// are neither searched for nor subclassed. All width changes use IColumnManager.
class ShellViewColumns : public std::enable_shared_from_this<ShellViewColumns> {
    struct Column {
        PROPERTYKEY key = {};
        int width = 0, minimum = 1;
    };
    CComPtr<IShellView> view;
    HWND window = nullptr, list = nullptr, target = nullptr, headerWindow = nullptr;
    DWORD originalFullDrag = 0;
    UINT message = 0;
    ULONG_PTR generation = 0;
    bool posted = false, applying = false, suspended = false;
    bool deferred = false, tracking = false;
    int changedColumn = -1, requestedWidth = -1;
    LONG observedWidth = 0;
    std::vector<Column> columns, weights;

    bool current() const { return window && IsWindow(window); }
    static int adjacentWidth(const std::vector<Column> &items, size_t index, int proposed) {
        const int total = items[index].width + items[index + 1].width;
        return (std::max)(items[index].minimum,
            (std::min)(proposed, total - items[index + 1].minimum));
    }
    static void restoreTrackingStyle(HWND parent, HWND header, DWORD fullDrag) {
        if (header && IsWindow(header) && IsChild(parent, header)) {
            const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(header, GWL_STYLE));
            SetWindowLongPtrW(header, GWL_STYLE, static_cast<LONG_PTR>((style & ~static_cast<DWORD>(HDS_FULLDRAG)) | fullDrag));
        }
    }
    void configureHeaderTracking() {
        if (tracking || !current() || !list) return;
        const HWND header = ListView_GetHeader(list);
        if (!header) return;
        if (headerWindow && headerWindow != header) {
            restoreTrackingStyle(window, headerWindow, originalFullDrag);
            headerWindow = nullptr;
        }
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(header, GWL_STYLE));
        if (!headerWindow) {
            headerWindow = header;
            originalFullDrag = style & static_cast<DWORD>(HDS_FULLDRAG);
        }
        const DWORD desired = (style & ~static_cast<DWORD>(HDS_FULLDRAG)) | (deferred ? 0 : originalFullDrag);
        if (desired != style) SetWindowLongPtrW(header, GWL_STYLE, static_cast<LONG_PTR>(desired));
    }
    static bool sameKeys(const std::vector<Column> &left, const std::vector<Column> &right) {
        if (left.size() != right.size()) return false;
        for (size_t i = 0; i < left.size(); ++i)
            if (!IsEqualPropertyKey(left[i].key, right[i].key)) return false;
        return true;
    }
    bool capture(std::vector<Column> &result) {
        CComQIPtr<IFolderView2> folder(view);
        UINT mode = 0, count = 0;
        if (!current() || !folder || FAILED(folder->GetCurrentViewMode(&mode))
                || !current() || mode != FVM_DETAILS) return false;
        CComQIPtr<IColumnManager> manager(view);
        if (!manager || !current() || FAILED(manager->GetColumnCount(CM_ENUM_VISIBLE, &count))
                || !current() || !count || count > 64) return false;
        std::vector<PROPERTYKEY> keys(count);
        if (FAILED(manager->GetColumns(CM_ENUM_VISIBLE, keys.data(), count)) || !current()) return false;
        std::vector<int> order(count);
        const bool ordered = list && IsWindow(list)
            && ListView_GetColumnOrderArray(list, static_cast<int>(count), order.data());
        for (UINT i = 0; i < count; ++i) {
            CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
            if (FAILED(manager->GetColumnInfo(keys[i], &info)) || !current() || info.uWidth > 32768) return false;
            Column column;
            column.key = keys[i]; column.width = static_cast<int>(info.uWidth);
            LVCOLUMNW native = {}; native.mask = LVCF_MINWIDTH;
            if (ordered && ListView_GetColumn(list, order[i], &native))
                column.minimum = (std::max)(1, native.cxMin);
            result.push_back(column);
        }
        return true;
    }
    bool apply(const std::vector<Column> &desired) {
        CComQIPtr<IColumnManager> manager(view);
        if (!manager || !current()) return false;
        applying = true;
        struct Guard { bool &flag; ~Guard() { flag = false; } } guard{applying};
        // Shrink before growing, to avoid transient horizontal overflow.
        for (int pass = 0; pass < 2; ++pass) for (size_t i = 0; i < desired.size(); ++i) {
            if (desired[i].width == columns[i].width
                    || (desired[i].width < columns[i].width) != (pass == 0)) continue;
            CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
            info.uWidth = static_cast<UINT>(desired[i].width);
            if (FAILED(manager->SetColumnInfo(desired[i].key, &info)) || !current()) return false;
        }
        std::vector<Column> actual;
        if (!capture(actual) || !sameKeys(desired, actual)) return false;
        for (size_t i = 0; i < desired.size(); ++i)
            if (desired[i].width != actual[i].width) return false;
        columns.swap(actual);
        return true;
    }
    void request() {
        if (!posted && target) posted = PostMessageW(target, message, generation, 0) != FALSE;
    }
    static LRESULT CALLBACK procedure(HWND source, UINT msg, WPARAM wParam,
            LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
        auto self = reinterpret_cast<ShellViewColumns *>(data)->shared_from_this();
        if (msg == WM_NCDESTROY) {
            RemoveWindowSubclass(source, procedure, id);
            self->window = self->list = self->target = nullptr;
            self->headerWindow = nullptr;
        } else if (msg == WM_NOTIFY && lParam && !self->applying && !self->suspended) {
            const auto notice = reinterpret_cast<const NMHDR *>(lParam);
            wchar_t name[32] = {};
            if (GetClassNameW(notice->hwndFrom, name, _countof(name)) && IsChild(source, notice->hwndFrom)) {
                if (notice->code == NM_CUSTOMDRAW && lstrcmpW(name, WC_LISTVIEWW) == 0) {
                    RECT bounds = {};
                    if (GetClientRect(notice->hwndFrom, &bounds)
                            && (self->list != notice->hwndFrom || self->observedWidth != bounds.right)) {
                        self->list = notice->hwndFrom; self->observedWidth = bounds.right; self->request();
                        self->configureHeaderTracking();
                    }
                } else if (lstrcmpW(name, WC_HEADERW) == 0) {
                    const bool width = notice->code == HDN_ITEMCHANGINGW || notice->code == HDN_ITEMCHANGINGA;
                    const auto change = reinterpret_cast<const NMHEADERW *>(lParam);
                    const bool begin = notice->code == HDN_BEGINTRACKW || notice->code == HDN_BEGINTRACKA;
                    const bool end = notice->code == HDN_ENDTRACKW || notice->code == HDN_ENDTRACKA;
                    const bool track = notice->code == HDN_TRACKW || notice->code == HDN_TRACKA;
                    if (begin && self->deferred) {
                        HDITEMW item = {}; item.mask = HDI_ORDER;
                        if (Header_GetItem(notice->hwndFrom, change->iItem, &item)) {
                            if (!self->columns.empty() && item.iOrder == static_cast<int>(self->columns.size()) - 1)
                                return TRUE;
                            self->tracking = true;
                            self->changedColumn = item.iOrder;
                            self->requestedWidth = -1;
                        }
                    } else if (track && self->tracking && change->pitem && (change->pitem->mask & HDI_WIDTH)) {
                        self->requestedWidth = change->pitem->cxy;
                    } else if (end && self->tracking) {
                        self->tracking = false;
                        // A pause does not finish the gesture. Escape/capture cancellation does not commit it.
                        if (GetKeyState(VK_LBUTTON) < 0) {
                            self->changedColumn = self->requestedWidth = -1;
                        } else {
                            if (self->requestedWidth < 0 && change->pitem && (change->pitem->mask & HDI_WIDTH))
                                self->requestedWidth = change->pitem->cxy;
                            self->request();
                        }
                    } else if (width && change->pitem && (change->pitem->mask & HDI_WIDTH)) {
                        HDITEMW item = {}; item.mask = HDI_ORDER;
                        if (Header_GetItem(notice->hwndFrom, change->iItem, &item)) {
                            if (!self->columns.empty() && item.iOrder == static_cast<int>(self->columns.size()) - 1)
                                return TRUE; // Fixed right edge; the final column has no right neighbour.
                            if (self->tracking && item.iOrder == self->changedColumn) {
                                self->requestedWidth = change->pitem->cxy;
                                return TRUE; // Keep both native widths unchanged until the button is released.
                            }
                            self->changedColumn = item.iOrder;
                            self->request();
                        }
                    } else if (notice->code == HDN_ENDDRAG) {
                        self->columns.clear(); self->weights.clear(); self->request();
                    }
                }
            }
        }
        return DefSubclassProc(source, msg, wParam, lParam);
    }
public:
    ~ShellViewColumns() { stop(); }
    void stop() {
        const HWND oldWindow = window, oldHeader = headerWindow;
        const DWORD fullDrag = originalFullDrag;
        if (window) RemoveWindowSubclass(window, procedure, 0);
        window = list = target = headerWindow = nullptr;
        tracking = false;
        changedColumn = requestedWidth = -1;
        restoreTrackingStyle(oldWindow, oldHeader, fullDrag);
    }
    static std::shared_ptr<ShellViewColumns> attach(IShellView *source, HWND target,
            UINT message, ULONG_PTR generation) {
        auto result = std::make_shared<ShellViewColumns>();
        result->view = source; result->target = target; result->message = message; result->generation = generation;
        if (!source || FAILED(source->GetWindow(&result->window)) || !result->window
                || GetWindowThreadProcessId(result->window, nullptr) != GetCurrentThreadId()
                || !SetWindowSubclass(result->window, procedure, 0, reinterpret_cast<DWORD_PTR>(result.get()))) return {};
        result->request();
        RedrawWindow(result->window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        return result;
    }
    void suspend(bool value) {
        const bool resume = suspended && !value;
        suspended = value;
        if (resume) request();
    }
    void resize() { request(); }
    void setDeferred(bool value) { deferred = value; configureHeaderTracking(); }
    bool synchronize() {
        auto keepAlive = shared_from_this();
        posted = false;
        if (suspended || tracking || applying || !current() || !list || !IsWindow(list)) return false;
        std::vector<Column> actual;
        if (!capture(actual)) return false;
        if (changedColumn >= 0 && sameKeys(columns, actual)) {
            const size_t index = static_cast<size_t>(changedColumn);
            changedColumn = -1;
            const int proposed = requestedWidth;
            requestedWidth = -1;
            if (index + 1 < columns.size()) {
                auto desired = actual;
                const int total = columns[index].width + columns[index + 1].width;
                desired[index].width = adjacentWidth(columns, index, proposed >= 0 ? proposed : actual[index].width);
                desired[index + 1].width = total - desired[index].width;
                columns = actual;
                if (!apply(desired)) return false;
                weights = columns;
            }
        } else {
            changedColumn = -1;
            requestedWidth = -1;
            if (!sameKeys(columns, actual)) weights = actual;
            columns = actual;
        }
        if (weights.empty()) weights = columns;
        RECT bounds = {};
        if (!GetClientRect(list, &bounds) || bounds.right <= 0) return false;
        const auto desired = fit(columns, weights, bounds.right);
        return apply(desired);
    }
private:
    static std::vector<Column> fit(const std::vector<Column> &actual,
            const std::vector<Column> &ratios, int available) {
        auto result = actual;
        std::vector<bool> fixed(result.size(), false);
        int remaining = available;
        for (;;) {
            uint64_t total = 0;
            for (size_t i = 0; i < result.size(); ++i)
                if (!fixed[i]) total += static_cast<unsigned>((std::max)(1, ratios[i].width));
            if (!total) return result;
            bool clamped = false;
            for (size_t i = 0; i < result.size(); ++i) if (!fixed[i]
                    && static_cast<int64_t>(remaining) * (std::max)(1, ratios[i].width)
                        < static_cast<int64_t>(result[i].minimum) * static_cast<int64_t>(total)) {
                result[i].width = result[i].minimum;
                remaining -= result[i].minimum; fixed[i] = true; clamped = true;
            }
            if (clamped) continue;
            uint64_t cumulative = 0;
            int allocated = 0;
            for (size_t i = 0; i < result.size(); ++i) if (!fixed[i]) {
                cumulative += static_cast<unsigned>((std::max)(1, ratios[i].width));
                const int edge = static_cast<int>(static_cast<uint64_t>(remaining) * cumulative / total);
                result[i].width = edge - allocated; allocated = edge;
            }
            return result;
        }
    }
};
} // namespace filespacer
