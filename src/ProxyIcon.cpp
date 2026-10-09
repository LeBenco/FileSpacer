#include "ProxyIcon.h"
#include "ItemWindow.h"
#include "GeomUtils.h"
#include "WinUtils.h"
#include "GDIUtils.h"
#include "DPI.h"
#include "resource.h"
#include <windowsx.h>
#include <shlobj.h>
#include <vssym32.h>
#include <strsafe.h>
#include <propkey.h>
#include <VersionHelpers.h>

namespace filespacer {

// dimensions
static SIZE PROXY_PADDING = {7, 3};
static SIZE PROXY_INFLATE = {4, 1};

// this produces the color used in every high-contrast theme
// regular light mode theme uses #999999
const BYTE INACTIVE_CAPTION_ALPHA = 156;

const BYTE PROXY_BUTTON_STYLE = BTNS_WHOLEDROPDOWN | BTNS_NOPREFIX;

static HFONT captionFont = nullptr;

void ProxyIcon::init() {
    PROXY_PADDING = scaleDPI(PROXY_PADDING);
    PROXY_INFLATE = scaleDPI(PROXY_INFLATE);
}

void ProxyIcon::initTheme(HTHEME theme) {
    LOGFONT logFont;
    if (checkHR(GetThemeSysFont(theme, TMT_CAPTIONFONT, &logFont)))
        captionFont = CreateFontIndirect(&logFont);
}

void ProxyIcon::initMetrics(const NONCLIENTMETRICS &metrics) {
    captionFont = CreateFontIndirect(&metrics.lfCaptionFont);
}

void ProxyIcon::uninit() {
    if (captionFont)
        DeleteFont(captionFont);
}

ProxyIcon::ProxyIcon(ItemWindow *const outer) : outer(outer) {}

void ProxyIcon::create(HWND parent, wchar_t *title, int top, int height) {
    const bool layered = IsWindows8OrGreater();
    const DWORD exStyle = layered ? WS_EX_LAYERED : 0;
    caption = checkLE(CreateWindowEx(exStyle, L"STATIC", title,
        SS_OWNERDRAW | SS_NOPREFIX | WS_VISIBLE | WS_CHILD,
        0, 0, 0, 0, parent, nullptr, GetModuleHandle(nullptr), nullptr));
    SetWindowSubclass(caption, captionProc, 0, (DWORD_PTR)this);
    toolbar = checkLE(CreateWindowEx(exStyle, TOOLBARCLASSNAME, nullptr,
        TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS
            | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER | WS_VISIBLE | WS_CHILD,
        0, 0, 0, 0, parent, nullptr, GetModuleHandle(nullptr), nullptr));
    SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
    SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(toolbar, TB_SETBITMAPSIZE, 0, 0);
    if (captionFont) {
        SendMessage(caption, WM_SETFONT, (WPARAM)captionFont, FALSE);
        SendMessage(toolbar, WM_SETFONT, (WPARAM)captionFont, FALSE);
    }
    SendMessage(toolbar, TB_SETPADDING, 0, MAKELPARAM(PROXY_PADDING.cx, PROXY_PADDING.cy));
    TBBUTTON menuButton = {I_IMAGENONE, IDM_PROXY_BUTTON, TBSTATE_ENABLED,
        PROXY_BUTTON_STYLE | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L""};
    SendMessage(toolbar, TB_ADDBUTTONS, 1, (LPARAM)&menuButton);
    SIZE ideal = {};
    SendMessage(toolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);
    if (IsWindows10OrGreater()) {
        const int buttonHeight = GET_Y_LPARAM(SendMessage(toolbar, TB_GETBUTTONSIZE, 0, 0));
        top += max(0, (height - buttonHeight) / 2);
        height = min(height, buttonHeight);
    }
    SetWindowPos(caption, nullptr, 0, top, 0, height, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(toolbar, nullptr, 0, top, ideal.cx, height, SWP_NOZORDER | SWP_NOACTIVATE);
    setActive(false);
}

void ProxyIcon::destroy() {
    caption = toolbar = renameBox = nullptr;
    captionIcon = nullptr; // Borrowed from ItemWindow.
}

bool ProxyIcon::isToolbarWindow(HWND window) const {
    return toolbar && window == toolbar;
}

POINT ProxyIcon::getMenuPoint(HWND parent) {
    if (toolbar) {
        RECT buttonRect = {};
        SendMessage(toolbar, TB_GETRECT, IDM_PROXY_BUTTON, (LPARAM)&buttonRect);
        return clientToScreen(toolbar, {buttonRect.left, buttonRect.bottom});
    }
    return clientToScreen(parent, {0, 0});
}

void ProxyIcon::setTitle(wchar_t *title) {
    if (caption)
        SetWindowText(caption, title);
}

void ProxyIcon::setIcon(HICON icon) {
    captionIcon = icon;
    if (caption)
        InvalidateRect(caption, nullptr, FALSE);
}

// The passive label is part of the native caption hit-test area. Windows handles
// moving, activation and double-click maximization; no simulated dragging is used.
LRESULT CALLBACK ProxyIcon::captionProc(HWND window, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR) {
    if (message == WM_NCHITTEST)
        return HTTRANSPARENT;
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, captionProc, id);
    return DefSubclassProc(window, message, wParam, lParam);
}

bool ProxyIcon::drawTitle(const DRAWITEMSTRUCT *draw) {
    if (!caption || draw->hwndItem != caption || draw->CtlType != ODT_STATIC)
        return false;
    const int saved = SaveDC(draw->hDC);
    FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(
        IsWindows10OrGreater() ? COLOR_WINDOW : COLOR_3DFACE));
    if (captionFont)
        SelectObject(draw->hDC, captionFont);
    SetBkMode(draw->hDC, TRANSPARENT);
    COLORREF color = GetSysColor(COLOR_BTNTEXT);
    HTHEME theme = OpenThemeData(toolbar, L"Toolbar");
    if (theme)
        GetThemeColor(theme, TP_BUTTON, TS_NORMAL, TMT_TEXTCOLOR, &color);
    SetTextColor(draw->hDC, color);
    const int iconSize = GetSystemMetrics(SM_CXSMICON);
    if (captionIcon)
        DrawIconEx(draw->hDC, PROXY_PADDING.cx,
            (rectHeight(draw->rcItem) - iconSize) / 2, captionIcon,
            iconSize, iconSize, 0, nullptr, DI_NORMAL);
    RECT text = draw->rcItem;
    text.left += PROXY_PADDING.cx + iconSize + PROXY_INFLATE.cx;
    text.right = max(text.left, text.right - PROXY_PADDING.cx);
    const DWORD flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
    if (theme) {
        DrawThemeText(theme, draw->hDC, TP_BUTTON, TS_NORMAL, outer->title, -1,
            flags, 0, &text);
        CloseThemeData(theme);
    } else {
        DrawText(draw->hDC, outer->title, -1, &text, flags);
    }
    makeBitmapOpaque(draw->hDC, draw->rcItem);
    RestoreDC(draw->hDC, saved);
    return true;
}

void ProxyIcon::setActive(bool active) {
    if (IsWindows8OrGreater()) {
        if (caption)
            SetLayeredWindowAttributes(caption, 0, active ? 255 : INACTIVE_CAPTION_ALPHA, LWA_ALPHA);
        if (toolbar)
            SetLayeredWindowAttributes(toolbar, 0, active ? 255 : INACTIVE_CAPTION_ALPHA, LWA_ALPHA);
    }
}

void ProxyIcon::setPressedState(bool pressed) {
    if (toolbar) {
        LONG_PTR state = SendMessage(toolbar, TB_GETSTATE, IDM_PROXY_BUTTON, 0);
        state = pressed ? (state | TBSTATE_PRESSED) : (state & ~TBSTATE_PRESSED);
        SendMessage(toolbar, TB_SETSTATE, IDM_PROXY_BUTTON, state);
    }
}

void ProxyIcon::autoSize(LONG parentWidth, LONG captionLeft, LONG captionRight) {
    if (!caption || !toolbar)
        return;
    HDC dc = GetDC(caption);
    HGDIOBJ previous = captionFont ? SelectObject(dc, captionFont) : nullptr;
    SIZE textSize = {};
    GetTextExtentPoint32(dc, outer->title, lstrlen(outer->title), &textSize);
    if (previous)
        SelectObject(dc, previous);
    ReleaseDC(caption, dc);
    SIZE menuSize = {};
    SendMessage(toolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&menuSize);
    const int idealLabelWidth = 2 * PROXY_PADDING.cx + GetSystemMetrics(SM_CXSMICON)
        + PROXY_INFLATE.cx + textSize.cx;
    const int idealWidth = idealLabelWidth + menuSize.cx;
    const int actualLeft = outer->centeredProxy()
        ? max((parentWidth - idealWidth) / 2, captionLeft) : captionLeft;
    const int actualWidth = min(idealWidth, max(0, parentWidth - actualLeft - captionRight));
    const int labelWidth = max(0, actualWidth - menuSize.cx);
    RECT rect = windowRect(toolbar);
    MapWindowRect(nullptr, outer->hwnd, &rect);
    SetWindowPos(caption, nullptr, actualLeft, rect.top, labelWidth, rectHeight(rect),
        SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(toolbar, nullptr, actualLeft + labelWidth, rect.top,
        min(actualWidth, menuSize.cx), rectHeight(rect), SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(caption, nullptr, FALSE);
}

void ProxyIcon::redrawToolbar() {
    if (caption)
        RedrawWindow(caption, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    if (toolbar)
        RedrawWindow(toolbar, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

RECT ProxyIcon::titleRect() {
    RECT rect = clientRect(caption);
    rect.left += PROXY_PADDING.cx + GetSystemMetrics(SM_CXSMICON) + PROXY_INFLATE.cx;
    rect.right = max(rect.left, rect.right - PROXY_PADDING.cx);
    HDC dc = GetDC(caption);
    HGDIOBJ previous = captionFont ? SelectObject(dc, captionFont) : nullptr;
    TEXTMETRIC metrics = {};
    GetTextMetrics(dc, &metrics);
    if (previous)
        SelectObject(dc, previous);
    ReleaseDC(caption, dc);
    rect.top += max(0, (rectHeight(rect) - metrics.tmHeight) / 2);
    rect.bottom = rect.top + metrics.tmHeight;
    MapWindowRect(caption, outer->hwnd, &rect);
    return rect;
}

void ProxyIcon::beginRename() {
    if (!toolbar)
        return;
    CComHeapPtr<wchar_t> editingName;
    if (!checkHR(outer->item->GetDisplayName(SIGDN_PARENTRELATIVEEDITING, &editingName)))
        return;

    if (!renameBox) {
        // create rename box
        renameBox = checkLE(CreateWindow(L"EDIT", nullptr, WS_POPUP | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, outer->hwnd, nullptr, GetModuleHandle(nullptr), nullptr));
        // support ctrl+backspace
        checkHR(SHAutoComplete(renameBox, SHACF_AUTOAPPEND_FORCE_OFF|SHACF_AUTOSUGGEST_FORCE_OFF));
        SetWindowSubclass(renameBox, renameBoxProc, 0, (DWORD_PTR)this);
        if (captionFont)
            SendMessage(renameBox, WM_SETFONT, (WPARAM)captionFont, FALSE);
    }

    // update rename box rect
    int leftMargin = LOWORD(SendMessage(renameBox, EM_GETMARGINS, 0, 0));
    RECT textRect = titleRect();
    int renameHeight = rectHeight(textRect) + 4; // NOT scaled with DPI
    POINT renamePos = {textRect.left - leftMargin - 2,
                       (ItemWindow::CAPTION_HEIGHT - renameHeight) / 2};
    TITLEBARINFOEX titleBar = {sizeof(titleBar)};
    SendMessage(outer->hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&titleBar);
    int renameWidth = clientSize(outer->hwnd).cx - rectWidth(titleBar.rgrect[5]) - renamePos.x;
    renamePos = clientToScreen(outer->hwnd, renamePos);
    MoveWindow(renameBox, renamePos.x, renamePos.y, renameWidth, renameHeight, FALSE);

    SendMessage(renameBox, WM_SETTEXT, 0, (LPARAM)&*editingName);
    Edit_SetSel(renameBox, 0, -1);
    ShowWindow(renameBox, SW_SHOW);
    EnableWindow(toolbar, FALSE);
}

bool ProxyIcon::isRenaming() {
    return renameBox && IsWindowVisible(renameBox);
}

void ProxyIcon::completeRename() {
    wchar_t newName[MAX_PATH];
    SendMessage(renameBox, WM_GETTEXT, _countof(newName), (LPARAM)newName);
    cancelRename();

    CComHeapPtr<wchar_t> editingName;
    if (!checkHR(outer->item->GetDisplayName(SIGDN_PARENTRELATIVEEDITING, &editingName)))
        return;

    if (lstrcmp(newName, editingName) == 0)
        return; // names are identical, which would cause an unnecessary error message
    if (PathCleanupSpec(nullptr, newName) & (PCS_REPLACEDCHAR | PCS_REMOVEDCHAR)) {
        outer->enableTaskbarOwner(false);
        checkHR(TaskDialog(outer->hwnd, GetModuleHandle(nullptr),
            MAKEINTRESOURCE(IDS_ERROR_CAPTION), nullptr, MAKEINTRESOURCE(IDS_INVALID_CHARS),
            TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr));
        outer->enableTaskbarOwner(true);
        return;
    }

    SHELLFLAGSTATE shFlags = {};
    SHGetSettings(&shFlags, SSF_SHOWEXTENSIONS);
    if (!shFlags.fShowExtensions) {
        CComQIPtr<IShellItem2> item2(outer->item);
        CComHeapPtr<wchar_t> display, ext;
        if (item2 && checkHR(item2->GetString(PKEY_ItemNameDisplay, &display)) && display
                && checkHR(item2->GetString(PKEY_FileExtension, &ext)) && ext) {
            if (lstrcmpi(display, editingName) != 0) {
                // extension was probably hidden (TODO: jank!)
                debugPrintf(L"Appending extension %s\n", &*ext);
                if (!checkHR(StringCchCat(newName, _countof(newName), ext)))
                    return;
            }
        }
    }

    outer->proxyRename(newName);
}

void ProxyIcon::cancelRename() {
    ShowWindow(renameBox, SW_HIDE);
    EnableWindow(toolbar, TRUE);
}

LRESULT CALLBACK ProxyIcon::renameBoxProc(HWND hwnd, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    if (message == WM_CHAR && wParam == VK_RETURN) {
        ((ProxyIcon *)refData)->completeRename();
        return 0;
    } else if (message == WM_CHAR && wParam == VK_ESCAPE) {
        ((ProxyIcon *)refData)->cancelRename();
        return 0;
    } else if (message == WM_CLOSE) {
        // prevent user closing rename box (it will still be destroyed when owner is closed)
        return 0;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

bool ProxyIcon::onControlCommand(HWND controlHwnd, WORD notif) {
    if (renameBox && controlHwnd == renameBox && notif == EN_KILLFOCUS) {
        if (isRenaming())
            completeRename();
        return true;
    }
    return false;
}

void ProxyIcon::onThemeChanged() {
    if (caption && captionFont)
        PostMessage(caption, WM_SETFONT, (WPARAM)captionFont, TRUE);
    if (toolbar && captionFont)
        PostMessage(toolbar, WM_SETFONT, (WPARAM)captionFont, TRUE);
    if (renameBox && captionFont)
        PostMessage(renameBox, WM_SETFONT, (WPARAM)captionFont, TRUE);
}

} // namespace
