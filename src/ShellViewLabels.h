#pragma once
#include <common.h>
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <limits>
#include <memory>
#include <type_traits>

namespace filespacer {

// Optional adaptation of the Shell's native labels. The view window returned
// by IShellView::GetWindow (owned by the Shell) is subclassed; no child control
// is discovered or subclassed. Only the ListView notifications delivered to that
// window and documented LVM_* reads of their sender are used. The ListView
// itself follows from FVO_CUSTOMPOSITION selecting the classic view: should a
// Windows release change that, the adaptation stops silently (test set 23).
// Missing notifications or failed prerequisites leave native painting intact.
inline LRESULT CALLBACK singleSelectionLabelProc(HWND window, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR) {
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, singleSelectionLabelProc, id);
    if (message != WM_NOTIFY || !lParam)
        return DefSubclassProc(window, message, wParam, lParam);
    const auto header = reinterpret_cast<const NMHDR *>(lParam);
    if (header->code != NM_CUSTOMDRAW)
        return DefSubclassProc(window, message, wParam, lParam);
    const HWND control = header->hwndFrom;
    wchar_t className[32] = {};
    if (!GetClassNameW(control, className, _countof(className))
            || lstrcmpW(className, WC_LISTVIEWW) != 0 || !IsChild(window, control))
        return DefSubclassProc(window, message, wParam, lParam);
    const auto draw = reinterpret_cast<const NMLVCUSTOMDRAW *>(lParam);
    const DWORD stage = draw->nmcd.dwDrawStage;
    const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
    if ((stage != CDDS_PREPAINT && stage != CDDS_ITEMPREPAINT)
            || (result & CDRF_SKIPDEFAULT) || !IsWindow(window) || !IsWindow(control)
            || ListView_GetView(control) != LV_VIEW_ICON
            || ListView_GetSelectedCount(control) != 1)
        return result;
    if (stage == CDDS_PREPAINT)
        return result | CDRF_NOTIFYITEMDRAW;
    if (draw->dwItemType != LVCDI_ITEM
            || draw->nmcd.dwItemSpec > static_cast<DWORD_PTR>((std::numeric_limits<int>::max)()))
        return result;
    const int item = static_cast<int>(draw->nmcd.dwItemSpec);
    // The Shell also reports CDIS_SELECTED for some unselected items.
    if (!(ListView_GetItemState(control, item, LVIS_SELECTED) & LVIS_SELECTED))
        return result;
    RECT label = {}, client = {}, visible = {};
    if (!draw->nmcd.hdc || !ListView_GetItemRect(control, item, &label, LVIR_LABEL)
            || !GetClientRect(control, &client) || !IntersectRect(&visible, &label, &client))
        return result;
    COLORREF background = ListView_GetBkColor(control);
    if (background == CLR_NONE)
        background = GetSysColor(COLOR_WINDOW);
    std::unique_ptr<std::remove_pointer<HBRUSH>::type, decltype(&DeleteObject)> brush(
        CreateSolidBrush(background), DeleteObject);
    if (brush)
        FillRect(draw->nmcd.hdc, &visible, brush.get());
    return result;
}

inline void setSingleSelectionLabels(IShellView *view, bool enabled) {
    HWND window = nullptr;
    if (!view || FAILED(view->GetWindow(&window)) || !window
            || GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId())
        return;
    // There is no host pointer or retained COM reference in the callback.
    if (enabled) {
        if (!SetWindowSubclass(window, singleSelectionLabelProc, 0, 0))
            return;
    } else {
        RemoveWindowSubclass(window, singleSelectionLabelProc, 0);
    }
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

} // namespace filespacer
