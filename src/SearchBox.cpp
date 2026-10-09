#include "SearchBox.h"
#include "DPI.h"
#include "UIStrings.h"
#include "GeomUtils.h"
#include <uxtheme.h>
#include <vssym32.h>
#include <algorithm>

namespace filespacer {

static const wchar_t SEARCH_BOX_CLASS[] = L"FileSpacer Search Box";
static constexpr UINT_PTR SEARCH_TIMER = 1;
static constexpr UINT SEARCH_DELAY_MS = 150;

void SearchBox::init() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = SEARCH_BOX_CLASS;
    checkLE(RegisterClass(&wc));
}

void SearchBox::create(HWND parent, HWND ownerWindow) {
    owner = ownerWindow;
    if (!checkLE(CreateWindowEx(0, SEARCH_BOX_CLASS, getString(IDS_SEARCH_NAME),
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0, parent, nullptr, GetModuleHandle(nullptr), this)))
        return;
    edit = checkLE(CreateWindowEx(0, L"EDIT", getString(IDS_SEARCH_NAME),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd, nullptr, GetModuleHandle(nullptr), nullptr));
    button = checkLE(CreateWindowEx(0, L"BUTTON", getString(IDS_SEARCH_APPLY),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, hwnd, nullptr, GetModuleHandle(nullptr), nullptr));
    if (!edit || !button) {
        destroy();
        return;
    }
    // Start with an empty edit; the cue is supplied from the folder's display name.
    SetWindowText(edit, L"");
    SendMessage(edit, EM_SETLIMITTEXT, 1024, 0);
    updateFonts();
    splitter = checkLE(CreateWindowEx(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 0, 0, GetParent(parent), nullptr, GetModuleHandle(nullptr), nullptr));
    if (splitter)
        checkLE(SetWindowSubclass(splitter, splitterProc, 0, (DWORD_PTR)this));
}

void SearchBox::destroy() {
    if (splitter) {
        DestroyWindow(splitter);
        splitter = nullptr;
    }
    if (hwnd)
        KillTimer(hwnd, SEARCH_TIMER);
    if (hwnd)
        DestroyWindow(hwnd);
    if (textFont)
        DeleteObject(textFont);
    if (iconFont)
        DeleteObject(iconFont);
    textFont = iconFont = nullptr;
}

void SearchBox::updateFonts() {
    NONCLIENTMETRICS metrics = {sizeof(metrics)};
    if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
        return;
    HFONT font = CreateFontIndirect(&metrics.lfMessageFont);
    metrics.lfMessageFont.lfHeight = -scaleDPI(12);
    lstrcpy(metrics.lfMessageFont.lfFaceName, L"Segoe MDL2 Assets");
    HFONT glyphFont = CreateFontIndirect(&metrics.lfMessageFont);
    if (font) {
        SendMessage(edit, WM_SETFONT, (WPARAM)font, TRUE);
        if (textFont)
            DeleteObject(textFont);
        textFont = font;
    }
    if (glyphFont) {
        if (iconFont)
            DeleteObject(iconFont);
        iconFont = glyphFont;
    }
    layout();
}

void SearchBox::place(const RECT &rect, bool resizable) {
    if (!hwnd)
        return;
    const bool changed = resizeEnabled != resizable;
    resizeEnabled = resizable;
    // Cover the complete placeholder, including the toolbar's separator drawing.
    SetWindowPos(hwnd, HWND_TOP, rect.left, rect.top,
        (std::max)(0L, rect.right - rect.left), rect.bottom - rect.top, SWP_NOACTIVATE);
    if (splitter) {
        const HWND toolbar = GetParent(hwnd);
        RECT splitterRect = rect;
        MapWindowRect(toolbar, GetParent(toolbar), &splitterRect);
        SetWindowPos(splitter, HWND_TOP, splitterRect.left, splitterRect.top,
            (std::min)(scaleDPI(8), rectWidth(rect)), rectHeight(rect),
            SWP_NOACTIVATE | (resizable ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
    if (changed) {
        layout();
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

void SearchBox::layout() {
    if (!edit || !button)
        return;
    RECT rect = clientRect(hwnd);
    rect.left += scaleDPI(resizeEnabled ? 12 : 4); // Optional grip, then frame margin
    rect.right = (std::max)(rect.left, rect.right - scaleDPI(4));
    const int buttonWidth = scaleDPI(24);
    HDC dc = GetDC(edit);
    HGDIOBJ oldFont = SelectObject(dc, textFont ? textFont : GetStockObject(DEFAULT_GUI_FONT));
    TEXTMETRIC metrics = {};
    GetTextMetrics(dc, &metrics);
    SelectObject(dc, oldFont);
    ReleaseDC(edit, dc);
    const int height = (std::min)((int)rect.bottom - 2, (int)metrics.tmHeight + scaleDPI(2));
    SetWindowPos(edit, nullptr, rect.left + scaleDPI(5), (rect.bottom - height) / 2,
        (std::max)(0L, rect.right - rect.left - buttonWidth - scaleDPI(8)),
        (std::max)(0, height), SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(button, nullptr, (std::max)(1L, rect.right - buttonWidth - 1), 1,
        buttonWidth, (std::max)(0L, rect.bottom - 2), SWP_NOZORDER | SWP_NOACTIVATE);
}

void SearchBox::setFolderName(const wchar_t *name) {
    auto cue = formatString(IDS_SEARCH_CUE, name);
    SendMessage(edit, EM_SETCUEBANNER, TRUE, (LPARAM)cue.get());
}

void SearchBox::setLiveSearch(bool enabled) {
    if (liveSearch == enabled)
        return;
    liveSearch = enabled;
    searchPending = false;
    if (hwnd)
        KillTimer(hwnd, SEARCH_TIMER);
    if (enabled && edit)
        submit();
}

bool SearchBox::hasFocus() const {
    HWND focused = GetFocus();
    return hwnd && (focused == hwnd || IsChild(hwnd, focused));
}

bool SearchBox::focus() {
    if (!hwnd || !IsWindowVisible(hwnd))
        return false;
    SetFocus(edit);
    SendMessage(edit, EM_SETSEL, 0, -1);
    return true;
}

void SearchBox::submit() {
    KillTimer(hwnd, SEARCH_TIMER);
    searchPending = false;
    const int length = GetWindowTextLength(edit);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowText(edit, &text[0], length + 1);
    Notification notification = {{hwnd, 0, APPLY}, text.c_str()};
    SendMessage(owner, WM_NOTIFY, 0, (LPARAM)&notification);
}

LRESULT CALLBACK SearchBox::splitterProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData) {
    auto self = reinterpret_cast<SearchBox *>(refData);
    switch (message) {
        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return TRUE;
        case WM_LBUTTONDOWN: {
            POINT cursor = {};
            GetCursorPos(&cursor);
            self->dragStartX = static_cast<int>(cursor.x);
            self->dragStartWidth = static_cast<int>(clientSize(self->hwnd).cx);
            SetCapture(window);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (GetCapture() == window) {
                POINT cursor = {};
                GetCursorPos(&cursor);
                Notification notification = {{self->hwnd, 0, RESIZE}, nullptr,
                    self->dragStartWidth + self->dragStartX - static_cast<int>(cursor.x)};
                SendMessage(self->owner, WM_NOTIFY, 0, (LPARAM)&notification);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
        case WM_CANCELMODE:
            if (GetCapture() == window)
                ReleaseCapture();
            return 0;
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, splitterProc, subclassID);
            self->splitter = nullptr;
            break;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

bool SearchBox::handleTopLevelMessage(MSG *msg) {
    if (!hasFocus() || msg->message != WM_KEYDOWN)
        return false;
    if (msg->wParam == VK_RETURN) {
        submit();
        return true;
    }
    if (msg->wParam == VK_ESCAPE) {
        SetWindowText(edit, L"");
        submit();
        SetFocus(edit);
        return true;
    }
    if (msg->wParam == L'A' && GetKeyState(VK_CONTROL) < 0) {
        SendMessage(edit, EM_SETSEL, 0, -1);
        return true;
    }
    if (msg->wParam == VK_TAB) {
        const bool backward = GetKeyState(VK_SHIFT) < 0;
        if ((GetFocus() == edit && !backward) || (GetFocus() == button && backward)) {
            SetFocus(GetFocus() == edit ? button : edit);
        } else {
            Notification notification = {{hwnd, 0, FOCUS_VIEW}, nullptr};
            SendMessage(owner, WM_NOTIFY, 0, (LPARAM)&notification);
        }
        return true;
    }
    return false;
}

LRESULT SearchBox::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SETFOCUS:
        case WM_LBUTTONDOWN:
            SetFocus(edit);
            return 0;
        case WM_SIZE:
            layout();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_COMMAND:
            if ((HWND)lParam == button && HIWORD(wParam) == BN_CLICKED) {
                submit();
                return 0;
            }
            if ((HWND)lParam == edit && HIWORD(wParam) == EN_CHANGE) {
                if (liveSearch)
                    searchPending = !!checkLE(SetTimer(hwnd, SEARCH_TIMER, SEARCH_DELAY_MS, nullptr));
                return 0;
            }
            if ((HWND)lParam == edit && (HIWORD(wParam) == EN_SETFOCUS
                    || HIWORD(wParam) == EN_KILLFOCUS))
                InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case WM_TIMER:
            if (wParam == SEARCH_TIMER) {
                if (liveSearch && searchPending)
                    submit();
                return 0;
            }
            break;
        case WM_DRAWITEM: {
            auto draw = reinterpret_cast<DRAWITEMSTRUCT *>(lParam);
            if (draw->hwndItem != button)
                break;
            FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(COLOR_WINDOW));
            const int saved = SaveDC(draw->hDC);
            if (iconFont)
                SelectObject(draw->hDC, iconFont);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, GetSysColor(COLOR_WINDOWTEXT));
            DrawText(draw->hDC, L"\uE721", 1, &draw->rcItem,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (draw->itemState & ODS_FOCUS)
                DrawFocusRect(draw->hDC, &draw->rcItem);
            RestoreDC(draw->hDC, saved);
            return TRUE;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT rect = clientRect(hwnd);
            FillRect(dc, &rect, GetSysColorBrush(COLOR_WINDOW));
            rect.left += scaleDPI(resizeEnabled ? 12 : 4); // match the edit margin
            rect.right = (std::max)(rect.left, rect.right - scaleDPI(4));
            COLORREF borderColor = GetSysColor(hasFocus() ? COLOR_HIGHLIGHT : COLOR_3DLIGHT);
            if (!hasFocus()) {
                // Match the existing breadcrumb's themed gray frame.
                if (HTHEME theme = OpenThemeData(hwnd, L"Rebar")) {
                    COLORREF color;
                    if (SUCCEEDED(GetThemeColor(theme, RP_BAND, 0,
                            TMT_BORDERCOLORHINT, &color)))
                        borderColor = color;
                    CloseThemeData(theme);
                }
            }
            HBRUSH brush = CreateSolidBrush(borderColor);
            FrameRect(dc, &rect, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            updateFonts();
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case WM_SYSCOLORCHANGE:
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
            break;
        case WM_NCDESTROY: {
            LRESULT result = DefWindowProc(hwnd, message, wParam, lParam);
            hwnd = edit = button = nullptr;
            return result;
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace
