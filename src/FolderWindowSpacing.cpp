// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "FolderWindow.h"
#include "Settings.h"
#include "UIStrings.h"
#include <commctrl.h>
#include <objidl.h>
#pragma warning(push)
#pragma warning(disable: 4458) // Shadowed members in Windows SDK GDI+ headers.
#include <gdiplus.h>
#pragma warning(pop)

namespace filespacer {
namespace {
// Saved spacing uses thousandths of the Windows pitch. Each slider step is 10%.
constexpr int spacingStep = 100;
int spacingStepForRatio(int ratio) { return (ratio + spacingStep / 2) / spacingStep; }

int pixels(HWND window, int dip) {
    return MulDiv(dip, static_cast<int>(GetDpiForWindow(window)), USER_DEFAULT_SCREEN_DPI);
}

void fillSpacingRect(HDC dc, const RECT &rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void shadeSpacingThumb(HDC dc, const RECT &rect, bool vertical, bool pressed) {
    // A narrow highlight and shadow at the ends give the reference thumb its relief.
    const BYTE light = pressed ? 214 : 248, dark = pressed ? 248 : 214;
    const COLOR16 from = static_cast<COLOR16>(light * 257), to = static_cast<COLOR16>(dark * 257);
    RECT blend = rect, highlight = rect;
    const LONG cap = (std::max)(1L, (vertical ? rect.bottom - rect.top : rect.right - rect.left) / 6);
    if (vertical) {
        blend.top += cap;
        blend.bottom -= 2 * cap;
        highlight.bottom = blend.top;
    } else {
        blend.left += cap;
        blend.right -= 2 * cap;
        highlight.right = blend.left;
    }
    fillSpacingRect(dc, rect, RGB(dark, dark, dark));
    fillSpacingRect(dc, highlight, RGB(light, light, light));
    TRIVERTEX ends[] = {{blend.left, blend.top, from, from, from, 0}, {blend.right, blend.bottom, to, to, to, 0}};
    GRADIENT_RECT indices = {0, 1};
    if (!IsRectEmpty(&blend))
        GdiGradientFill(dc, ends, 2, &indices, 1, vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
}

LRESULT drawSpacingTrack(const NMCUSTOMDRAW &draw, bool vertical) {
    if (draw.dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
    const HWND track = draw.hdr.hwndFrom;
    RECT client = {}, thumb = {}, channel = {};
    GetClientRect(track, &client);
    SendMessageW(track, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    SendMessageW(track, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    FillRect(draw.hdc, &client, GetSysColorBrush(COLOR_WINDOW));

    // Some common-controls versions return the vertical channel in horizontal coordinates.
    const bool wide = channel.right - channel.left > channel.bottom - channel.top;
    const LONG first = wide ? channel.left : channel.top, last = wide ? channel.right : channel.bottom;
    const LONG center = vertical ? (thumb.left + thumb.right) / 2 : (thumb.top + thumb.bottom) / 2;
    const auto axisRect = [vertical](LONG start, LONG nearSide, LONG end, LONG farSide) -> RECT {
        return vertical ? RECT{nearSide, start, farSide, end} : RECT{start, nearSide, end, farSide};
    };
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON);
    const LONG railStart = center - pixels(track, 2), railEnd = railStart + pixels(track, 4);
    fillSpacingRect(draw.hdc, axisRect(first, railStart, last, railEnd),
        highContrast ? GetSysColor(COLOR_WINDOWTEXT) : RGB(176, 176, 176));
    if (!highContrast) {
        fillSpacingRect(draw.hdc, axisRect(first + 1, railStart + 1, last, railEnd), RGB(253, 253, 253));
        fillSpacingRect(draw.hdc, axisRect(first + 1, railStart + 1, last - 1, railEnd - 1), RGB(231, 234, 234));
    }

    const LONG gap = pixels(track, 1);
    const LONG nearTick = (vertical ? thumb.left : thumb.top) - gap;
    const LONG farTick = (vertical ? thumb.right : thumb.bottom) + gap;
    const LONG thumbRadius = (vertical ? thumb.bottom - thumb.top : thumb.right - thumb.left) / 2;
    const COLORREF tickColor = highContrast ? GetSysColor(COLOR_WINDOWTEXT) : RGB(196, 196, 196);
    for (int tick = 0; tick <= 6; ++tick) {
        const LONG at = first + thumbRadius + MulDiv(last - first - 2 * thumbRadius, tick, 6);
        const LONG length = pixels(track, tick == 0 || tick == 6 ? 4 : 3);
        fillSpacingRect(draw.hdc, axisRect(at, nearTick - length, at + 1, nearTick), tickColor);
        fillSpacingRect(draw.hdc, axisRect(at, farTick, at + 1, farTick + length), tickColor);
    }

    POINT mouse = {};
    const bool hot = GetCursorPos(&mouse) && ScreenToClient(track, &mouse) && PtInRect(&thumb, mouse);
    const auto cues = static_cast<UINT>(SendMessageW(track, WM_QUERYUISTATE, 0, 0));
    const bool focused = GetFocus() == track && !(cues & UISF_HIDEFOCUS);
    const bool pressed = GetCapture() == track;
    const COLORREF edge = highContrast ? GetSysColor(COLOR_WINDOWTEXT) :
        focused || hot || pressed ? RGB(105, 105, 105) : RGB(146, 146, 146);
    fillSpacingRect(draw.hdc, thumb, edge);
    InflateRect(&thumb, -1, -1);
    if (highContrast) {
        FillRect(draw.hdc, &thumb, GetSysColorBrush(focused || pressed ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
    } else {
        fillSpacingRect(draw.hdc, thumb, RGB(255, 255, 255));
        InflateRect(&thumb, -1, -1);
        shadeSpacingThumb(draw.hdc, thumb, vertical, pressed);
    }
    return CDRF_SKIPDEFAULT;
}

void drawSpacingGlyph(const DRAWITEMSTRUCT &item, bool vertical) {
    const RECT &rect = item.rcItem;
    const int inset = pixels(item.hwndItem, 2);
    const int size = (std::min<int>)(pixels(item.hwndItem, 20),
        (std::min)(rect.right - rect.left, rect.bottom - rect.top) - 2 * inset);
    if (size <= 0) return;
    const float scale = static_cast<float>(size) / 20.f;
    const float down = (item.itemState & ODS_SELECTED) ? 1.f : 0.f;
    const COLORREF color = GetSysColor((item.itemState & ODS_DISABLED) ? COLOR_GRAYTEXT : COLOR_BTNTEXT);
    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), 1.06f);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::Graphics graphics(item.hDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.TranslateTransform(static_cast<float>(rect.left + rect.right) / 2.f + down,
        static_cast<float>(rect.top + rect.bottom) / 2.f + down);
    if (vertical) graphics.RotateTransform(90.f);
    graphics.ScaleTransform(scale, scale);
    graphics.TranslateTransform(-12.f, -12.f);
    graphics.DrawLine(&pen, 2.6f, 4.f, 2.6f, 20.f);
    graphics.DrawLine(&pen, 21.4f, 4.f, 21.4f, 20.f);

    // Radix Icons "Width" (MIT), expressed as strokes on its original 15-unit grid.
    // Both buttons use the same arrow and end bars, rotated together above.
    graphics.TranslateTransform(2.6f, 2.7f);
    graphics.ScaleTransform(1.25f, 1.25f);
    pen.SetWidth(0.9f);
    const Gdiplus::PointF left[] = {{4.5f, 5.f}, {2.f, 7.5f}, {4.5f, 10.f}};
    const Gdiplus::PointF right[] = {{10.5f, 5.f}, {13.f, 7.5f}, {10.5f, 10.f}};
    graphics.DrawLine(&pen, 2.f, 7.5f, 13.f, 7.5f);
    graphics.DrawLines(&pen, left, ARRAYSIZE(left));
    graphics.DrawLines(&pen, right, ARRAYSIZE(right));
}

void drawSpacingButton(const DRAWITEMSTRUCT &item, bool vertical) {
    RECT rect = item.rcItem;
    const bool down = (item.itemState & ODS_SELECTED) != 0;
    DrawFrameControl(item.hDC, &rect, DFC_BUTTON, DFCS_BUTTONPUSH | (down ? DFCS_PUSHED : DFCS_FLAT));
    drawSpacingGlyph(item, vertical);
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
    spacingPopup = CreateWindowExW(WS_EX_TOOLWINDOW, WC_STATICW, sliderName, WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
        x, y, width, height, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingPopup) return;
    if (!SetWindowSubclass(spacingPopup, spacingPopupProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
        closeSpacingSlider();
        return;
    }
    RECT client = {};
    GetClientRect(spacingPopup, &client);
    const int pad = pixels(button, 4);
    spacingTrack = CreateWindowExW(0, TRACKBAR_CLASSW, sliderName, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        (axis ? TBS_VERT : TBS_HORZ) | TBS_BOTH | TBS_FIXEDLENGTH | TBS_AUTOTICKS,
        axis ? 0 : pad, axis ? pad : 0, axis ? client.right : client.right - 2 * pad,
        axis ? client.bottom - 2 * pad : client.bottom, spacingPopup, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingTrack) { closeSpacingSlider(); return; }
    SendMessageW(spacingTrack, TBM_SETTHUMBLENGTH, pixels(button, 22), 0);
    SendMessageW(spacingTrack, TBM_SETRANGEMAX, FALSE, 3000 / spacingStep);
    SendMessageW(spacingTrack, TBM_SETTICFREQ, 500 / spacingStep, 0);
    SendMessageW(spacingTrack, TBM_SETPAGESIZE, 0, 1);
    SendMessageW(spacingTrack, TBM_SETPOS, TRUE, spacingStepForRatio(spacingPreview));
    RECT thumb = {};
    SendMessageW(spacingTrack, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    const int cross = axis ? (client.right - thumb.left - thumb.right) / 2 : (client.bottom - thumb.top - thumb.bottom) / 2;
    SetWindowPos(spacingTrack, nullptr, axis ? cross : pad, axis ? pad : cross, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(spacingPopup, WM_CHANGEUISTATE, MAKEWPARAM(UIS_INITIALIZE, 0), 0);
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
    // The default static control lets clicks pass through its blank margins.
    if (message == WM_NCHITTEST) return HTCLIENT;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        FillRect(dc, &paint.rcPaint, GetSysColorBrush(COLOR_WINDOW));
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
        owner.readSpacingSlider(LOWORD(wParam) != TB_THUMBTRACK);
        return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, spacingPopupProc, id);
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace filespacer
