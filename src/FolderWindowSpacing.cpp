// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "FolderWindow.h"
#include "FolderWindowViewMenuPreview.h"
#include "Settings.h"
#include "UIStrings.h"
#include <commctrl.h>
#include <windowsx.h>

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

}

ExplorerIconSpacingResult FolderWindow::applyIconSpacing(IShellView *source, const std::function<bool()> &current,
        const std::function<bool(HWND, POINT)> &beforeChange) {
    auto &spacing = getSavedFolderState().view.iconSpacing;
    if (spacing.generation < spacingResetGeneration) spacing = {{1000, 1000}, spacingResetGeneration};
    auto ratios = customIconSpacing ? spacing.ratios : std::array<int, 2>{1000, 1000};
    if (spacingPopup) ratios[spacingAxis] = spacingPreview;
    auto next = spacingMetrics;
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
    }
    return result;
}

void FolderWindow::runSpacingCommand(UINT command) {
    if (!canAdjustIconSpacing()) return;
    const int axis = command == IDM_RESET_VERTICAL_GAP;
    const auto generation = viewGeneration;
    auto &spacing = getSavedFolderState().view.iconSpacing;
    const auto previous = spacing;
    auto ratios = previous.ratios;
    if (command == IDM_RESET_BOTH_GAPS) ratios = {1000, 1000};
    else ratios[axis] = 1000;
    if (ratios == previous.ratios) return;
    spacing.ratios = ratios;
    if (!updateExplorerIconSpacing()) {
        if (generation == viewGeneration && spacing.generation == previous.generation && spacing.ratios == ratios)
            spacing.ratios = previous.ratios;
        return;
    }
    viewStateDirty(FolderState::View);
    SetTimer(hwnd, TIMER_SAVE_STATE, 250, nullptr);
}

void FolderWindow::ViewMenuPreview::commitSpacing() {
    owner.readSpacingSlider(true);
    if (!current() || !owner.spacingTrack) return;
    const int axis = owner.spacingAxis;
    const auto &ratios = owner.getSavedFolderState().view.iconSpacing.ratios;
    const UINT reset = axis ? IDM_RESET_VERTICAL_GAP : IDM_RESET_HORIZONTAL_GAP;
    const UINT axisState = ratios[axis] == 1000 ? MF_GRAYED : MF_ENABLED;
    const UINT bothState = ratios[0] == 1000 && ratios[1] == 1000 ? MF_GRAYED : MF_ENABLED;
    EnableMenuItem(menu, reset, axisState);
    EnableMenuItem(menu, IDM_RESET_BOTH_GAPS, bothState);
    if (renderedAxis) EnableMenuItem(renderedAxis, reset, axisState);
    if (renderedRoot && renderedRoot != menu) EnableMenuItem(renderedRoot, IDM_RESET_BOTH_GAPS, bothState);
}

void FolderWindow::ViewMenuPreview::stopSpacingEdit() {
    draggingSpacing = false;
    spacingMenuWindow = nullptr;
    renderedAxis = nullptr;
    if (!editingSpacing) return;
    owner.readSpacingSlider(true);
    owner.closeSpacingSlider();
    editingSpacing = false;
}

void FolderWindow::ViewMenuPreview::moveSpacingThumb(POINT point, bool commit) {
    const HWND track = owner.spacingTrack;
    RECT thumb = {}, channel = {};
    if (!track || !ScreenToClient(track, &point)) return;
    SendMessageW(track, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    SendMessageW(track, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    const bool wide = channel.right - channel.left > channel.bottom - channel.top;
    const LONG radius = (owner.spacingAxis ? thumb.bottom - thumb.top : thumb.right - thumb.left) / 2;
    const LONG start = (wide ? channel.left : channel.top) + radius;
    const LONG end = (wide ? channel.right : channel.bottom) - radius;
    if (end <= start) return;
    const LONG offset = (owner.spacingAxis ? point.y : point.x) - start - thumbOffset;
    const int position = MulDiv((std::max)(0L, (std::min)(offset, end - start)), 3000 / spacingStep, end - start);
    if (position != SendMessageW(track, TBM_GETPOS, 0, 0))
        SendMessageW(track, TBM_SETPOS, TRUE, position);
    if (commit) commitSpacing();
    else owner.readSpacingSlider(false);
}

bool FolderWindow::ViewMenuPreview::measureSpacingItem(HWND window, MEASUREITEMSTRUCT *measure) {
    if (!measure || measure->CtlType != ODT_MENU || (measure->itemID != IDM_SPACING_SLIDER_HORIZONTAL
            && measure->itemID != IDM_SPACING_SLIDER_VERTICAL)) return false;
    const bool vertical = measure->itemID == IDM_SPACING_SLIDER_VERTICAL;
    measure->itemWidth = pixels(window, vertical ? 40 : 230);
    measure->itemHeight = pixels(window, vertical ? 230 : 40);
    return true;
}

LRESULT CALLBACK FolderWindow::ViewMenuPreview::measureAxisMenu(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && active && active->standaloneSpacingAxis) {
        const auto &message = *reinterpret_cast<const CWPSTRUCT *>(lParam);
        if (message.message == WM_INITMENUPOPUP && message.hwnd != active->spacingMeasureOwner) {
            const auto menu = reinterpret_cast<HMENU>(message.wParam);
            const UINT item = GetMenuItemID(menu, 0);
            if (item == IDM_SPACING_SLIDER_HORIZONTAL || item == IDM_SPACING_SLIDER_VERTICAL) {
                // Install on the window receiving the renderer's measurements
                // before Windows requests the size of the first menu row.
                active->releaseSpacingMeasureOwner();
                if (SetWindowSubclass(message.hwnd, measureMenuProc, reinterpret_cast<UINT_PTR>(active), 0))
                    active->spacingMeasureOwner = message.hwnd;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK FolderWindow::ViewMenuPreview::measureMenuProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR id, DWORD_PTR) {
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, measureMenuProc, id);
    const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
    if (message == WM_MEASUREITEM && measureSpacingItem(window, reinterpret_cast<MEASUREITEMSTRUCT *>(lParam))) return TRUE;
    return result;
}

void FolderWindow::ViewMenuPreview::releaseSpacingMeasureOwner() {
    if (spacingMeasureOwner) {
        RemoveWindowSubclass(spacingMeasureOwner, measureMenuProc, reinterpret_cast<UINT_PTR>(this));
        spacingMeasureOwner = nullptr;
    }
}

void CALLBACK FolderWindow::ViewMenuPreview::spacingMenuEvent(HWINEVENTHOOK, DWORD event, HWND popup,
        LONG, LONG, DWORD, DWORD) {
    if (!active || !popup || !active->tracking) return;
    if (event == EVENT_SYSTEM_MENUPOPUPSTART) active->spacingMenuShown(popup);
    else if (popup == active->spacingMenuWindow && !IsWindowVisible(popup)) active->stopSpacingEdit();
}

void FolderWindow::ViewMenuPreview::spacingMenuShown(HWND popup) {
    if (!spacingEnabled || !tracking || busy || !popup || !current()) return;
    if (spacingMenuWindow && !IsWindowVisible(spacingMenuWindow)) stopSpacingEdit();
    MENUBARINFO bar = {sizeof(bar)};
    if (!GetMenuBarInfo(popup, OBJID_CLIENT, 0, &bar) || !bar.hMenu) return;
    if (bar.hMenu == renderedAxis && popup == spacingMenuWindow) return;
    const UINT first = GetMenuItemID(bar.hMenu, 0);
    if (first != IDM_SPACING_SLIDER_HORIZONTAL && first != IDM_SPACING_SLIDER_VERTICAL) {
        MENUITEMINFOW reset = {sizeof(reset)};
        reset.fMask = MIIM_FTYPE;
        if (renderedRoot != bar.hMenu && GetMenuItemInfoW(bar.hMenu, IDM_RESET_BOTH_GAPS, FALSE, &reset)) {
            releaseSpacingMeasureOwner();
            renderedRoot = bar.hMenu;
            if (!context && !standaloneSpacingAxis) {
                RECT bounds = {};
                if (GetWindowRect(popup, &bounds)) {
                    for (int row = 0, count = GetMenuItemCount(bar.hMenu); row < count; ++row) {
                        const UINT command = GetMenuItemID(bar.hMenu, row);
                        RECT entry = {};
                        if ((command == IDM_ICON_GAP_HORIZONTAL || command == IDM_ICON_GAP_VERTICAL)
                                && GetMenuItemRect(popup, bar.hMenu, row, &entry))
                            spacingAxisPoints[command == IDM_ICON_GAP_VERTICAL] = {bounds.right, entry.top};
                    }
                }
                return;
            }
            // The renderer can provide its own owner for the modal menu. Keep its
            // measurements for every item except our two trackbars.
            GUITHREADINFO info = {sizeof(info)};
            if ((reset.fType & MFT_OWNERDRAW) && GetGUIThreadInfo(GetCurrentThreadId(), &info)
                    && info.hwndMenuOwner && GetWindowThreadProcessId(info.hwndMenuOwner, nullptr) == GetCurrentThreadId()
                    && SetWindowSubclass(info.hwndMenuOwner, measureMenuProc, reinterpret_cast<UINT_PTR>(this), 0))
                spacingMeasureOwner = info.hwndMenuOwner;
        }
        return;
    }
    RECT row = {};
    if (!GetMenuItemRect(popup, bar.hMenu, 0, &row) || IsRectEmpty(&row)) return;
    stopSpacingEdit();
    wantedCommand = 0;
    apply(0);
    finishPositions();
    if (currentCommand || !current() || !owner.customIconSpacing) return;
    prepared = capturedIcons = false;
    initial = {};
    initialSpacing = {};
    spacingMenuWindow = popup;
    renderedAxis = bar.hMenu;
    const int axis = first == IDM_SPACING_SLIDER_VERTICAL;
    editingSpacing = true;
    owner.openSpacingSlider(axis, row, popup);
    editingSpacing = owner.spacingTrack != nullptr;
}

bool FolderWindow::ViewMenuPreview::handleSpacingInput(const MSG &message) {
    if (!editingSpacing || !tracking || busy || !current()) return false;
    if (!IsWindowVisible(spacingMenuWindow)) { stopSpacingEdit(); return false; }
    const UINT type = message.message;
    const POINT point = message.pt;
    const bool key = type == WM_KEYDOWN;
    if (type != WM_MOUSEMOVE && type != WM_LBUTTONDOWN && type != WM_LBUTTONUP && !key) return false;
    const HWND track = owner.spacingTrack;
    if (!track) return false;
    if (draggingSpacing && (type == WM_MOUSEMOVE || type == WM_LBUTTONUP)) {
        moveSpacingThumb(point, type == WM_LBUTTONUP);
        if (type == WM_LBUTTONUP) draggingSpacing = false;
        return true;
    }
    if (standaloneSpacingAxis) {
        const UINT state = GetMenuState(renderedAxis, 1, MF_BYPOSITION);
        if (key && message.wParam == VK_RETURN && (state & MF_HILITE) && !(state & MF_GRAYED)) {
            SendMessageW(track, TBM_SETPOS, TRUE, spacingStepForRatio(1000));
            owner.spacingEdited = true;
            commitSpacing();
            return true;
        }
        if (type == WM_LBUTTONUP && pressingOutsideSpacing) {
            pressingOutsideSpacing = false;
            reopenViewMenu = GetForegroundWindow() == owner.hwnd;
            EndMenu();
            return true;
        }
        if (type == WM_LBUTTONDOWN || (type == WM_LBUTTONUP && pressingSpacingReset)) {
            RECT reset = {}, bounds = {};
            const bool overReset = GetMenuItemRect(spacingMenuWindow, renderedAxis, 1, &reset) && PtInRect(&reset, point);
            if (type == WM_LBUTTONUP) {
                pressingSpacingReset = false;
                if (overReset && !(state & MF_GRAYED)) {
                    SendMessageW(track, TBM_SETPOS, TRUE, spacingStepForRatio(1000));
                    owner.spacingEdited = true;
                    commitSpacing();
                }
                return true;
            }
            if (overReset) {
                pressingSpacingReset = true;
                return true;
            }
            if (GetWindowRect(spacingMenuWindow, &bounds) && !PtInRect(&bounds, point)
                    && GetAncestor(WindowFromPoint(point), GA_ROOT) == owner.hwnd) {
                pressingOutsideSpacing = true;
                return true;
            }
        }
    }
    // Let the menu see the pointer enter the slider row and cancel a pending
    // switch to a sibling crossed on the way to a flipped submenu.
    if (type == WM_MOUSEMOVE) return false;
    if (key) {
        if (GetMenuState(renderedAxis, 1, MF_BYPOSITION) & MF_HILITE) return false;
        if ((!owner.spacingAxis && (message.wParam == VK_UP || message.wParam == VK_DOWN))
                || (owner.spacingAxis && message.wParam == VK_LEFT)) return false;
        int position = static_cast<int>(SendMessageW(track, TBM_GETPOS, 0, 0));
        switch (message.wParam) {
            case VK_RETURN: return true; // Keep the active slider open.
            case VK_HOME: position = 0; break;
            case VK_END: position = 3000 / spacingStep; break;
            case VK_UP: case VK_LEFT: case VK_PRIOR: --position; break;
            case VK_DOWN: case VK_RIGHT: case VK_NEXT: ++position; break;
            default: return false;
        }
        SendMessageW(track, TBM_SETPOS, TRUE, position);
        commitSpacing();
        return true;
    }
    RECT popup = {};
    if (GetWindowRect(owner.spacingPopup, &popup) && PtInRect(&popup, point)) {
        RECT bounds = {};
        if (type == WM_LBUTTONDOWN && GetWindowRect(track, &bounds) && PtInRect(&bounds, point)) {
            POINT local = point;
            ScreenToClient(track, &local);
            RECT thumb = {};
            SendMessageW(track, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
            thumbOffset = PtInRect(&thumb, local) ? (owner.spacingAxis ? local.y - (thumb.top + thumb.bottom) / 2
                : local.x - (thumb.left + thumb.right) / 2) : 0;
            draggingSpacing = true;
            moveSpacingThumb(point, false);
        }
        return true;
    }
    // A click on the next axis while its submenu is opening can reselect
    // the old axis. Leave the native hover transition in charge of those rows.
    HWND hit = WindowFromPoint(point);
    MENUBARINFO bar = {sizeof(bar)};
    if (renderedRoot && hit && GetMenuBarInfo(hit, OBJID_CLIENT, 0, &bar) && bar.hMenu == renderedRoot) {
        const int row = MenuItemFromPoint(nullptr, renderedRoot, point);
        MENUITEMINFOW entry = {sizeof(entry)};
        entry.fMask = MIIM_ID;
        if (row >= 0 && GetMenuItemInfoW(renderedRoot, static_cast<UINT>(row), TRUE, &entry)
                && (entry.wID == IDM_ICON_GAP_HORIZONTAL || entry.wID == IDM_ICON_GAP_VERTICAL)) return true;
    }
    return false;
}

void FolderWindow::openSpacingSlider(int axis, const RECT &bounds, HWND menuWindow) {
    closeSpacingSlider();
    spacingAxis = axis;
    spacingPreview = getSavedFolderState().view.iconSpacing.ratios[axis];
    spacingEdited = false;
    spacingDeferred = settings::getIconSpacingDeferred();
    const auto sliderName = getString(axis ? IDS_ICON_GAP_VERTICAL : IDS_ICON_GAP_HORIZONTAL);
    spacingPopup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WC_STATICW, sliderName,
        WS_POPUP | WS_BORDER | WS_CLIPCHILDREN, bounds.left, bounds.top, bounds.right - bounds.left,
        bounds.bottom - bounds.top, menuWindow, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingPopup) return;
    if (!SetWindowSubclass(spacingPopup, spacingPopupProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
        closeSpacingSlider();
        return;
    }
    RECT client = {};
    GetClientRect(spacingPopup, &client);
    const int pad = pixels(hwnd, 4);
    spacingTrack = CreateWindowExW(0, TRACKBAR_CLASSW, sliderName, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        (axis ? TBS_VERT : TBS_HORZ) | TBS_BOTH | TBS_FIXEDLENGTH | TBS_AUTOTICKS,
        axis ? 0 : pad, axis ? pad : 0, axis ? pixels(hwnd, 38) : client.right - 2 * pad,
        axis ? client.bottom - 2 * pad : client.bottom, spacingPopup, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!spacingTrack) { closeSpacingSlider(); return; }
    SendMessageW(spacingTrack, TBM_SETTHUMBLENGTH, pixels(hwnd, 22), 0);
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
    SetWindowPos(spacingPopup, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOMOVE);
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
    if (commit && spacingPopup && getSavedFolderState().view.iconSpacing.ratios[spacingAxis] != ratio) {
        getSavedFolderState().view.iconSpacing.ratios[spacingAxis] = ratio;
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
    if (message == WM_DESTROY && owner.spacingPopup == window) owner.readSpacingSlider(true);
    if (message == WM_NCDESTROY) {
        if (owner.spacingPopup == window) {
            owner.spacingPopup = owner.spacingTrack = nullptr;
            owner.spacingFramePending = false;
        }
        RemoveWindowSubclass(window, spacingPopupProc, id);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace filespacer
