// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "FolderWindow.h"
#include "Settings.h"
#include "UIStrings.h"
#include <commctrl.h>

namespace filespacer {
namespace {
int pixels(HWND window, int dip) {
    return MulDiv(dip, static_cast<int>(GetDpiForWindow(window)), USER_DEFAULT_SCREEN_DPI);
}

void drawSpacingButton(const DRAWITEMSTRUCT &item, bool vertical) {
    RECT rect = item.rcItem;
    const bool down = (item.itemState & ODS_SELECTED) != 0;
    DrawFrameControl(item.hDC, &rect, DFC_BUTTON, DFCS_BUTTONPUSH | (down ? DFCS_PUSHED : DFCS_FLAT));
    const int x = (rect.left + rect.right) / 2 + down, y = (rect.top + rect.bottom) / 2 + down;
    const int tip = pixels(item.hwndItem, 5), bar = pixels(item.hwndItem, 8), wing = pixels(item.hwndItem, 3);
    const auto point = [&](int along, int across) -> POINT {
        return {x + (vertical ? across : along), y + (vertical ? along : across)};
    };
    const auto line = [&](int a, int b, int c, int d) {
        const POINT path[] = {point(a,b), point(c,d)};
        Polyline(item.hDC, path, 2);
    };
    const HGDIOBJ old = SelectObject(item.hDC, GetStockObject(DC_PEN));
    const COLORREF color = SetDCPenColor(item.hDC,
        GetSysColor((item.itemState & ODS_DISABLED) ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
    line(-tip, 0, tip, 0);
    for (int sign : {-1, 1}) {
        line(sign * bar, -wing, sign * bar, wing + 1);
        line(sign * (tip-wing), -wing, sign * tip, 0);
        line(sign * tip, 0, sign * (tip-wing), wing);
    }
    SetDCPenColor(item.hDC, color);
    SelectObject(item.hDC, old);
    if (item.itemState & ODS_FOCUS) {
        InflateRect(&rect, -2, -2);
        DrawFocusRect(item.hDC, &rect);
    }
}
}

ExplorerIconSpacingResult FolderWindow::applyIconSpacing(IShellView *source, const std::function<bool()> &current,
        const std::function<bool(HWND, POINT)> &beforeChange) {
    auto ratios = getSavedFolderState().view.iconGap;
    if (spacingPopup) ratios[spacingAxis] = spacingPreview;
    const auto previous = spacingMetrics;
    auto next = previous;
    const auto result = useExplorerIconSpacing(source, ratios, next, current, beforeChange);
    if (current()) {
        spacingMetrics = next;
        if (result == ExplorerIconSpacingResult::Pending) {
            // Reference enumeration can finish after the real folder, especially
            // during a cold launch. Retry only a pending measurement, with backoff.
            if (spacingReferenceRetries < 7) {
                SetTimer(hwnd, TIMER_SPACING_REFERENCE, 32u << spacingReferenceRetries, nullptr);
                ++spacingReferenceRetries;
            }
        } else if (spacingReferenceRetries) {
            KillTimer(hwnd, TIMER_SPACING_REFERENCE);
            spacingReferenceRetries = 0;
        }
        if ((previous.standard.x > previous.cell.x) != (next.standard.x > next.cell.x)
                || (previous.standard.y > previous.cell.y) != (next.standard.y > next.cell.y))
            layoutSpacingButtons();
    }
    return result;
}

void FolderWindow::createSpacingButtons() {
    if (!bottomStatusBar || !SetWindowSubclass(bottomStatusBar, spacingBarProc, 1, reinterpret_cast<DWORD_PTR>(this)))
        return;
    for (int axis = 0; axis < 2; ++axis) {
        spacingButtons[axis] = CreateWindowExW(0, WC_BUTTONW,
            getString(axis ? IDS_ICON_GAP_VERTICAL : IDS_ICON_GAP_HORIZONTAL),
            WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, bottomStatusBar, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
}

void FolderWindow::layoutSpacingButtons() {
    if (!bottomStatusBar || !spacingButtons[0] || !spacingButtons[1]) return;
    RECT bounds = {};
    GetClientRect(bottomStatusBar, &bounds);
    const bool available = spacingMetrics.standard.x > 0 && spacingMetrics.standard.y > 0;
    const int width = pixels(bottomStatusBar, 28);
    const int parts[] = {available ? std::max<LONG>(0, bounds.right - 2 * width) : -1, -1};
    SendMessageW(bottomStatusBar, SB_SETPARTS, available ? 2 : 1, reinterpret_cast<LPARAM>(parts));
    for (int axis = 0; axis < 2; ++axis) {
        const bool enabled = axis ? spacingMetrics.standard.y > spacingMetrics.cell.y
            : spacingMetrics.standard.x > spacingMetrics.cell.x;
        EnableWindow(spacingButtons[axis], enabled);
        SetWindowPos(spacingButtons[axis], HWND_TOP, parts[0] + axis * width, 0, width, bounds.bottom,
            SWP_NOACTIVATE | (available ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
}

LRESULT CALLBACK FolderWindow::spacingBarProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR data) {
    auto &owner = *reinterpret_cast<FolderWindow *>(data);
    if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED) {
        const auto button = reinterpret_cast<HWND>(lParam);
        if (button && (button == owner.spacingButtons[0] || button == owner.spacingButtons[1])) {
            CComPtr<ItemWindow> keepAlive(&owner);
            owner.openSpacingSlider(button == owner.spacingButtons[1]);
            return 0;
        }
    } else if (message == WM_DRAWITEM && lParam) {
        const auto &item = *reinterpret_cast<const DRAWITEMSTRUCT *>(lParam);
        if (item.hwndItem == owner.spacingButtons[0] || item.hwndItem == owner.spacingButtons[1]) {
            drawSpacingButton(item, item.hwndItem == owner.spacingButtons[1]);
            return TRUE;
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, spacingBarProc, id);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

void FolderWindow::openSpacingSlider(int axis) {
    closeSpacingSlider();
    if (!viewReady || !initialStateApplied || provisionalBaseline || !isWindowOperational()) return;
    const int gap = axis ? spacingMetrics.standard.y - spacingMetrics.cell.y
        : spacingMetrics.standard.x - spacingMetrics.cell.x;
    if (gap <= 0) return;
    const HWND button = spacingButtons[axis];
    RECT anchor = {};
    MONITORINFO monitor = {sizeof(monitor)};
    if (!GetWindowRect(button, &anchor) || !GetMonitorInfoW(MonitorFromWindow(button, MONITOR_DEFAULTTONEAREST),
            &monitor)) return;
    spacingAxis = axis;
    spacingStandardGap = gap;
    spacingPreview = getSavedFolderState().view.iconGap[axis];
    spacingEdited = false;
    spacingDeferred = settings::getIconSpacingDeferred();
    const int width = pixels(button, axis ? 68 : 230), height = pixels(button, axis ? 240 : 62);
    const int x = std::max<int>(monitor.rcWork.left, std::min<int>(anchor.right - width, monitor.rcWork.right - width));
    const int y = anchor.top >= monitor.rcWork.top + height ? anchor.top - height :
        std::min<int>(anchor.bottom, monitor.rcWork.bottom - height);
    const auto sliderName = getString(axis ? IDS_ICON_GAP_VERTICAL : IDS_ICON_GAP_HORIZONTAL);
    spacingPopup = CreateWindowExW(WS_EX_TOOLWINDOW, WC_STATICW, sliderName, WS_POPUP | WS_BORDER | SS_WHITERECT,
        x, y, width, height, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingPopup) return;
    if (!SetWindowSubclass(spacingPopup, spacingPopupProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
        closeSpacingSlider();
        return;
    }
    RECT client = {};
    GetClientRect(spacingPopup, &client);
    const int pad = pixels(button, 4), labelHeight = pixels(button, 20);
    spacingTrack = CreateWindowExW(0, TRACKBAR_CLASSW, sliderName, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        (axis ? TBS_VERT : TBS_HORZ) | TBS_AUTOTICKS, pad, pad, client.right - 2 * pad,
        client.bottom - 2 * pad - labelHeight, spacingPopup, nullptr, GetModuleHandleW(nullptr), nullptr);
    spacingLabel = CreateWindowExW(0, WC_STATICW, nullptr, WS_VISIBLE | WS_CHILD | SS_CENTER,
        0, client.bottom - labelHeight, client.right, labelHeight, spacingPopup, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingLabel || !spacingTrack) { closeSpacingSlider(); return; }
    SendMessageW(spacingLabel, WM_SETFONT, SendMessageW(bottomStatusBar, WM_GETFONT, 0, 0), FALSE);
    SendMessageW(spacingTrack, TBM_SETRANGEMAX, FALSE, 3 * static_cast<LPARAM>(gap));
    SendMessageW(spacingTrack, TBM_SETTICFREQ, gap, 0);
    SendMessageW(spacingTrack, TBM_SETPAGESIZE, 0, (std::max)(1, gap / 4));
    const int position = MulDiv(gap, spacingPreview, 1000);
    spacingDisplayed = position;
    SendMessageW(spacingTrack, TBM_SETPOS, TRUE, position);
    SetWindowTextW(spacingLabel, format(L"%1!d! px", position).get());
    ShowWindow(spacingPopup, SW_SHOW);
    SetFocus(spacingTrack);
}

void FolderWindow::closeSpacingSlider() {
    const HWND popup = spacingPopup;
    spacingPopup = spacingTrack = spacingLabel = nullptr;
    spacingFramePending = false;
    if (popup) DestroyWindow(popup);
}

void FolderWindow::readSpacingSlider(bool commit, bool frame) {
    if (!spacingTrack || !isWindowOperational()) return;
    const int position = static_cast<int>(SendMessageW(spacingTrack, TBM_GETPOS, 0, 0));
    if (spacingDisplayed != position) {
        spacingDisplayed = position;
        spacingEdited = true;
        SetWindowTextW(spacingLabel, format(L"%1!d! px", position).get());
    }
    if (!spacingEdited || (!commit && spacingDeferred)) return;
    const int ratio = MulDiv(position, 1000, spacingStandardGap);
    if (!commit && !frame && ratio != spacingPreview) {
        // One pending frame always reads the most recent thumb position.
        if (!spacingFramePending) spacingFramePending = SetTimer(spacingPopup, 1, 32, nullptr) != 0;
        if (spacingFramePending) return;
    }
    if (spacingFramePending) {
        KillTimer(spacingPopup, 1);
        spacingFramePending = false;
    }
    if (ratio != spacingPreview) {
        const int previous = spacingPreview;
        spacingPreview = ratio;
        if (!updateExplorerIconSpacing()) {
            spacingPreview = previous;
            return;
        }
    }
    if (commit && spacingPopup && getSavedFolderState().view.iconGap[spacingAxis] != ratio) {
        getSavedFolderState().view.iconGap[spacingAxis] = ratio;
        viewStateDirty(FolderState::View);
        // The mode and icon size did not change. Running the mode-transition
        // handler here could replay an older arrangement over live positions.
        SetTimer(hwnd, TIMER_SAVE_STATE, 250, nullptr);
    }
}

LRESULT CALLBACK FolderWindow::spacingPopupProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR data) {
    auto &owner = *reinterpret_cast<FolderWindow *>(data);
    CComPtr<ItemWindow> keepAlive(&owner);
    if (message == WM_TIMER && wParam == 1 && window == owner.spacingPopup) {
        owner.readSpacingSlider(false, true);
        return 0;
    }
    if (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE && window == owner.spacingPopup) {
        owner.readSpacingSlider(true);
        owner.closeSpacingSlider();
        return 0;
    }
    if ((message == WM_HSCROLL || message == WM_VSCROLL) && reinterpret_cast<HWND>(lParam) == owner.spacingTrack) {
        owner.readSpacingSlider(LOWORD(wParam) != TB_THUMBTRACK);
        return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, spacingPopupProc, id);
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace filespacer
