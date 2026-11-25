// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "FolderWindow.h"
#include "Settings.h"
#include "UIStrings.h"
#include <commctrl.h>

namespace filespacer {
namespace {
// Saved spacing uses thousandths of the Windows pitch. Each slider step is 10%.
constexpr int spacingStep = 100;
int spacingStepForRatio(int ratio) { return (ratio + spacingStep / 2) / spacingStep; }

int pixels(HWND window, int dip) {
    return MulDiv(dip, static_cast<int>(GetDpiForWindow(window)), USER_DEFAULT_SCREEN_DPI);
}

constexpr COLORREF transparentColor = RGB(1, 2, 3);

void fillSpacingRect(HDC dc, const RECT &rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void shadeSpacingRect(HDC dc, const RECT &rect, COLORREF color, bool vertical, bool highContrast) {
    if (IsRectEmpty(&rect)) return;
    if (!highContrast) {
        const auto vertex = [color](LONG x, LONG y, bool light) {
            const auto channel = [light](BYTE value) -> COLOR16 {
                return static_cast<COLOR16>((light ? (value + 255) / 2 : value) * 257);
            };
            return TRIVERTEX{x, y, channel(GetRValue(color)), channel(GetGValue(color)), channel(GetBValue(color)), 0};
        };
        TRIVERTEX ends[] = {vertex(rect.left, rect.top, true), vertex(rect.right, rect.bottom, false)};
        GRADIENT_RECT indices = {0, 1};
        if (GdiGradientFill(dc, ends, 2, &indices, 1, vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H)) return;
    }
    fillSpacingRect(dc, rect, color);
}

LRESULT drawSpacingTrack(const NMCUSTOMDRAW &draw, bool vertical) {
    if (draw.dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
    const HWND track = draw.hdr.hwndFrom;
    RECT client = {}, thumb = {}, channel = {};
    GetClientRect(track, &client);
    SendMessageW(track, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    SendMessageW(track, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    fillSpacingRect(draw.hdc, client, transparentColor);

    // Some common-controls versions return the vertical channel in horizontal coordinates.
    const bool wide = channel.right - channel.left > channel.bottom - channel.top;
    const LONG first = wide ? channel.left : channel.top, last = wide ? channel.right : channel.bottom;
    const LONG center = vertical ? (thumb.left + thumb.right) / 2 : (thumb.top + thumb.bottom) / 2;
    const LONG pos = vertical ? (thumb.top + thumb.bottom) / 2 : (thumb.left + thumb.right) / 2;
    const auto axisRect = [vertical](LONG start, LONG nearSide, LONG end, LONG farSide) -> RECT {
        return vertical ? RECT{nearSide, start, farSide, end} : RECT{start, nearSide, end, farSide};
    };
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON);
    const COLORREF accent = GetSysColor(COLOR_HIGHLIGHT);
    const LONG half = pixels(track, 3);
    RECT rail = axisRect(first, center - half, last, center + half);
    fillSpacingRect(draw.hdc, rail, GetSysColor(highContrast ? COLOR_WINDOWTEXT : COLOR_3DSHADOW));
    InflateRect(&rail, -1, -1);
    fillSpacingRect(draw.hdc, rail, GetSysColor(COLOR_WINDOW));
    if (vertical) rail.bottom = pos; else rail.right = pos;
    shadeSpacingRect(draw.hdc, rail, accent, vertical, highContrast);

    // Four major ticks at 0, 100, 200 and 300 percent. Keep them outside the thumb.
    const LONG tickTop = (vertical ? thumb.right : thumb.bottom) + pixels(track, 2);
    const LONG thumbRadius = (vertical ? thumb.bottom - thumb.top : thumb.right - thumb.left) / 2;
    for (int tick = 0; tick <= 3; ++tick) {
        const LONG at = first + thumbRadius + MulDiv(last - first - 2 * thumbRadius, tick, 3);
        fillSpacingRect(draw.hdc, axisRect(at, tickTop, at + 1, tickTop + half), GetSysColor(COLOR_GRAYTEXT));
    }

    POINT mouse = {};
    const bool hot = GetCursorPos(&mouse) && ScreenToClient(track, &mouse) && PtInRect(&thumb, mouse);
    const auto cues = static_cast<UINT>(SendMessageW(track, WM_QUERYUISTATE, 0, 0));
    const bool focused = GetFocus() == track && !(cues & UISF_HIDEFOCUS);
    fillSpacingRect(draw.hdc, thumb, GetSysColor(focused || highContrast ? COLOR_WINDOWTEXT : COLOR_3DSHADOW));
    InflateRect(&thumb, -1, -1);
    shadeSpacingRect(draw.hdc, thumb, hot || GetCapture() == track ? GetSysColor(COLOR_HOTLIGHT) : accent,
        !vertical, highContrast);
    return CDRF_SKIPDEFAULT;
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
        if ((previous.standard.x > 0 && previous.standard.y > 0) != (next.standard.x > 0 && next.standard.y > 0))
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
        EnableWindow(spacingButtons[axis], available);
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
    if (spacingMetrics.standard.x <= 0 || spacingMetrics.standard.y <= 0) return;
    const HWND button = spacingButtons[axis];
    RECT anchor = {};
    MONITORINFO monitor = {sizeof(monitor)};
    if (!GetWindowRect(button, &anchor) || !GetMonitorInfoW(MonitorFromWindow(button, MONITOR_DEFAULTTONEAREST),
            &monitor)) return;
    spacingAxis = axis;
    spacingPreview = getSavedFolderState().view.iconGap[axis];
    spacingEdited = false;
    spacingDeferred = settings::getIconSpacingDeferred();
    const int width = pixels(button, axis ? 40 : 230), height = pixels(button, axis ? 230 : 40);
    const int preferredX = axis ? (anchor.left + anchor.right - width) / 2 : anchor.right - width;
    const int x = std::max<int>(monitor.rcWork.left, std::min<int>(preferredX, monitor.rcWork.right - width));
    const int y = anchor.top >= monitor.rcWork.top + height ? anchor.top - height :
        std::min<int>(anchor.bottom, monitor.rcWork.bottom - height);
    const auto sliderName = getString(axis ? IDS_ICON_GAP_VERTICAL : IDS_ICON_GAP_HORIZONTAL);
    spacingPopup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED, WC_STATICW, sliderName, WS_POPUP | WS_CLIPCHILDREN,
        x, y, width, height, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingPopup) return;
    if (!SetLayeredWindowAttributes(spacingPopup, transparentColor, 0, LWA_COLORKEY) ||
            !SetWindowSubclass(spacingPopup, spacingPopupProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
        closeSpacingSlider();
        return;
    }
    RECT client = {};
    GetClientRect(spacingPopup, &client);
    const int pad = pixels(button, 4);
    spacingTrack = CreateWindowExW(0, TRACKBAR_CLASSW, sliderName, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        (axis ? TBS_VERT : TBS_HORZ) | TBS_AUTOTICKS, pad, pad, client.right - 2 * pad,
        client.bottom - 2 * pad, spacingPopup, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingTrack) { closeSpacingSlider(); return; }
    SendMessageW(spacingTrack, TBM_SETRANGEMAX, FALSE, 3000 / spacingStep);
    SendMessageW(spacingTrack, TBM_SETTICFREQ, 1000 / spacingStep, 0);
    SendMessageW(spacingTrack, TBM_SETPAGESIZE, 0, 1);
    SendMessageW(spacingTrack, TBM_SETPOS, TRUE, spacingStepForRatio(spacingPreview));
    RECT thumb = {};
    SendMessageW(spacingTrack, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    const int cross = axis ? (width - thumb.left - thumb.right) / 2 : (height - thumb.top - thumb.bottom) / 2;
    SetWindowPos(spacingTrack, nullptr, axis ? cross : pad, axis ? pad : cross, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(spacingPopup, SW_SHOW);
    SetFocus(spacingTrack);
}

void FolderWindow::closeSpacingSlider() {
    const HWND popup = spacingPopup;
    spacingPopup = spacingTrack = nullptr;
    spacingFramePending = false;
    if (popup) DestroyWindow(popup);
}

void FolderWindow::readSpacingSlider(bool commit, bool frame) {
    if (!spacingTrack || !isWindowOperational() || (!commit && spacingDeferred)) return;
    const int position = static_cast<int>(SendMessageW(spacingTrack, TBM_GETPOS, 0, 0));
    // Merely opening and closing the popup must preserve older, finer settings.
    if (!spacingEdited && position == spacingStepForRatio(spacingPreview)) return;
    const int ratio = position * spacingStep;
    spacingEdited = true;
    if (!commit && !frame && spacingFramePending) return;
    if (spacingFramePending) {
        KillTimer(spacingPopup, 1);
        spacingFramePending = false;
    }
    if (ratio != spacingPreview) {
        const HWND popup = spacingPopup;
        const int previous = spacingPreview;
        spacingPreview = ratio;
        if (!updateExplorerIconSpacing()) {
            spacingPreview = previous;
            return;
        }
        // Show the first change immediately. During a continuous drag, sample
        // the latest value once per interval and stop when the thumb is idle.
        if (!commit && popup && spacingPopup == popup)
            spacingFramePending = SetTimer(popup, 1, 16, nullptr) != 0;
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
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        fillSpacingRect(dc, paint.rcPaint, transparentColor);
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_NOTIFY && lParam) {
        const auto header = reinterpret_cast<const NMHDR *>(lParam);
        if (header->code == NM_CUSTOMDRAW && header->hwndFrom == owner.spacingTrack)
            return drawSpacingTrack(*reinterpret_cast<const NMCUSTOMDRAW *>(header), owner.spacingAxis != 0);
    }
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
        InvalidateRect(owner.spacingTrack, nullptr, FALSE);
        owner.readSpacingSlider(LOWORD(wParam) != TB_THUMBTRACK);
        return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, spacingPopupProc, id);
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace filespacer
