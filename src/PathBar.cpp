#include "PathBar.h"
#include "CreateItemWindow.h"
#include "DPI.h"
#include "GeomUtils.h"
#include "UIStrings.h"
#include <windowsx.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <algorithm>
#include <utility>

namespace filespacer {

static const wchar_t PATH_BAR_CLASS[] = L"FileSpacer Path Bar";
static constexpr int OVERFLOW_COMMAND = 1;
static constexpr int FIRST_SEGMENT_COMMAND = 3;

void PathBar::init() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = PATH_BAR_CLASS;
    checkLE(RegisterClass(&wc));
}

void PathBar::create(HWND parent, HWND ownerWindow, IShellItem *item, HFONT font, HFONT symbolFont) {
    owner = ownerWindow;
    if (!checkLE(CreateWindowEx(0, PATH_BAR_CLASS, getString(IDS_PATH_BAR),
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0, parent, nullptr, GetModuleHandle(nullptr), this)))
        return;
    LOGFONT iconFont = {};
    GetObject(symbolFont, sizeof(iconFont), &iconFont);
    iconFont.lfHeight = scaleDPI(7);
    iconFont.lfWeight = FW_BOLD;
    lstrcpy(iconFont.lfFaceName, L"Segoe MDL2 Assets");
    chevronFont = checkLE(CreateFontIndirect(&iconFont));
    toolbar = checkLE(CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS
            | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 0, 0, hwnd, nullptr, GetModuleHandle(nullptr), nullptr));
    edit = checkLE(CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd, nullptr, GetModuleHandle(nullptr), nullptr));
    if (!toolbar || !edit) {
        destroy();
        return;
    }
    // Flat native buttons provide themed hover and pressed states.
    SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
    SendMessage(toolbar, TB_SETBITMAPSIZE, 0, 0);
    SendMessage(toolbar, TB_SETMAXTEXTROWS, 1, 0);
    SendMessage(toolbar, TB_SETDRAWTEXTFLAGS,
        DT_CENTER | DT_RIGHT | DT_VCENTER | DT_BOTTOM | DT_SINGLELINE,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SendMessage(toolbar, TB_SETPADDING, 0, MAKELPARAM(scaleDPI(8), scaleDPI(2)));
    checkLE(SetWindowSubclass(toolbar, toolbarProc, 0, (DWORD_PTR)this));
    setFont(font);
    setItem(item);
}

void PathBar::destroy() {
    editing = false;
    if (hwnd)
        checkLE(DestroyWindow(hwnd));
    if (chevronFont) {
        DeleteObject(chevronFont);
        chevronFont = nullptr;
    }
}

void PathBar::setItem(IShellItem *item) {
    if (!hwnd)
        return;
    endEdit(true);
    currentItem = item;
    // Remove buttons before releasing the strings they may reference.
    while (SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0) > 0)
        SendMessage(toolbar, TB_DELETEBUTTON, 0, 0);
    segments.clear();
    CComPtr<IShellItem> computer;
    (void)checkHR(SHGetKnownFolderItem(FOLDERID_ComputerFolder, KF_FLAG_DEFAULT,
        nullptr, IID_PPV_ARGS(&computer)));
    CComPtr<IShellItem> current = item;
    while (current) {
        CComHeapPtr<wchar_t> name;
        if (checkHR(current->GetDisplayName(SIGDN_NORMALDISPLAY, &name))) {
            Segment segment;
            segment.item = current;
            segment.name = name;
            segments.push_back(std::move(segment));
        }
        int order = 0;
        if (computer && SUCCEEDED(current->Compare(computer, SICHINT_CANONICAL, &order))
                && order == 0)
            break; // Display This PC as the root of this breadcrumb path.
        CComPtr<IShellItem> parent;
        if (FAILED(current->GetParent(&parent)))
            break;
        current = parent;
    }
    std::reverse(segments.begin(), segments.end());
    CComHeapPtr<wchar_t> path;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        (void)checkHR(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEEDITING, &path));
    address = path ? (const wchar_t *)path : L"";

    TBBUTTON overflow = {I_IMAGENONE, OVERFLOW_COMMAND, TBSTATE_ENABLED,
        BTNS_SHOWTEXT | BTNS_NOPREFIX | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"\u00AB"};
    SendMessage(toolbar, TB_ADDBUTTONS, 1, (LPARAM)&overflow);
    for (size_t i = 0; i < segments.size(); i++) {
        int command = FIRST_SEGMENT_COMMAND + 2 * (int)i;
        TBBUTTON button = {I_IMAGENONE, command, TBSTATE_ENABLED,
            BTNS_SHOWTEXT | BTNS_NOPREFIX | BTNS_AUTOSIZE, {}, 0,
            (INT_PTR)segments[i].name.c_str()};
        SendMessage(toolbar, TB_ADDBUTTONS, 1, (LPARAM)&button);
        if (i + 1 < segments.size()) {
            // A disabled text button is a static separator, not a drop-down.
            TBBUTTON chevron = {I_IMAGENONE, command + 1, 0,
                BTNS_SHOWTEXT | BTNS_NOPREFIX, {}, 0, (INT_PTR)MDL2_CHEVRON_LEFT_MED};
            SendMessage(toolbar, TB_ADDBUTTONS, 1, (LPARAM)&chevron);
            TBBUTTONINFO info = {sizeof(info), TBIF_SIZE};
            info.cx = (WORD)scaleDPI(12);
            SendMessage(toolbar, TB_SETBUTTONINFO, command + 1, (LPARAM)&info);
        }
    }
    layout();
}

void PathBar::setFont(HFONT font) {
    if (font) {
        SendMessage(toolbar, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(edit, WM_SETFONT, (WPARAM)font, TRUE);
    }
    layout();
}

void PathBar::place(int left, int right, int height) {
    if (hwnd)
        SetWindowPos(hwnd, nullptr, left, 0, (std::max)(0, right - left), height,
            SWP_NOZORDER | SWP_NOACTIVATE);
}

void PathBar::layout() {
    if (!toolbar || !edit)
        return;
    // Keep the children inside the one-pixel client-area frame.
    SIZE size = clientSize(hwnd);
    size.cx = (std::max)(0L, size.cx - 2);
    size.cy = (std::max)(0L, size.cy - 2);
    SetWindowPos(toolbar, nullptr, 1, 1, size.cx, size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessage(toolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(0, size.cy));
    HDC dc = GetDC(edit);
    HFONT font = (HFONT)SendMessage(edit, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRIC metrics = {};
    GetTextMetrics(dc, &metrics);
    if (oldFont)
        SelectObject(dc, oldFont);
    ReleaseDC(edit, dc);
    int editHeight = (std::min)((int)size.cy, (int)metrics.tmHeight + scaleDPI(2));
    SetWindowPos(edit, nullptr, 1 + scaleDPI(4), 1 + (size.cy - editHeight) / 2,
        (std::max)(0L, size.cx - scaleDPI(8)), editHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    // Restore the full hierarchy before measuring. Always retain the current folder.
    int total = 0;
    for (size_t i = 0; i < segments.size(); i++) {
        int command = FIRST_SEGMENT_COMMAND + 2 * (int)i;
        TBBUTTONINFO info = {sizeof(info), TBIF_STYLE};
        info.fsStyle = BTNS_SHOWTEXT | BTNS_NOPREFIX | BTNS_AUTOSIZE;
        SendMessage(toolbar, TB_SETBUTTONINFO, command, (LPARAM)&info);
        SendMessage(toolbar, TB_HIDEBUTTON, command, FALSE);
        if (i + 1 < segments.size())
            SendMessage(toolbar, TB_HIDEBUTTON, command + 1, FALSE);
        RECT rect = {};
        SendMessage(toolbar, TB_GETRECT, command, (LPARAM)&rect);
        segments[i].width = rectWidth(rect) + (i + 1 < segments.size() ? scaleDPI(12) : 0);
        total += segments[i].width;
    }
    // Reserve some blank space so the text mode remains reachable with the mouse.
    int available = (std::max)(0L, size.cx - scaleDPI(16));
    firstVisible = 0;
    bool overflow = segments.size() > 1 && total > available;
    SendMessage(toolbar, TB_HIDEBUTTON, OVERFLOW_COMMAND, !overflow);
    if (overflow) {
        RECT rect = {};
        SendMessage(toolbar, TB_GETRECT, OVERFLOW_COMMAND, (LPARAM)&rect);
        available -= rectWidth(rect);
        while (firstVisible + 1 < segments.size() && total > available) {
            int command = FIRST_SEGMENT_COMMAND + 2 * (int)firstVisible;
            SendMessage(toolbar, TB_HIDEBUTTON, command, TRUE);
            SendMessage(toolbar, TB_HIDEBUTTON, command + 1, TRUE);
            total -= segments[firstVisible++].width;
        }
    }
    if (!segments.empty() && total > available) {
        // Limit the final segment instead of losing the blank area for editing.
        TBBUTTONINFO info = {sizeof(info), TBIF_STYLE | TBIF_SIZE};
        info.fsStyle = BTNS_SHOWTEXT | BTNS_NOPREFIX;
        int currentWidth = segments.back().width;
        info.cx = (WORD)(std::min)(65535, (std::max)(1, available - total + currentWidth));
        int command = FIRST_SEGMENT_COMMAND + 2 * (int)(segments.size() - 1);
        SendMessage(toolbar, TB_SETBUTTONINFO, command, (LPARAM)&info);
    }
    InvalidateRect(toolbar, nullptr, TRUE);
}

void PathBar::show(bool visible) {
    if (!hwnd)
        return;
    if (!visible)
        endEdit(true);
    ShowWindow(hwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

bool PathBar::isWindow(HWND window) const { return hwnd && window == hwnd; }
bool PathBar::hasFocus() const { return edit && GetFocus() == edit; }

void PathBar::beginEdit() {
    if (!hwnd || !IsWindowVisible(hwnd))
        return;
    if (!editing) {
        previousFocus = GetFocus();
        SetWindowText(edit, address.c_str());
        editing = true;
        ShowWindow(edit, SW_SHOWNOACTIVATE);
        ShowWindow(toolbar, SW_HIDE);
    }
    SetFocus(edit);
    SendMessage(edit, EM_SETSEL, 0, -1);
}

void PathBar::endEdit(bool restoreFocus) {
    if (!editing)
        return;
    editing = false;
    if (restoreFocus && hasFocus())
        SetFocus(IsWindow(previousFocus) ? previousFocus : owner);
    ShowWindow(edit, SW_HIDE);
    ShowWindow(toolbar, SW_SHOWNOACTIVATE);
}

void PathBar::openItem(IShellItem *item, bool control) {
    OpenItemNotification notification = {{hwnd, 0, OPEN_ITEM}, item, control};
    SendMessage(owner, WM_NOTIFY, 0, (LPARAM)&notification);
}

void PathBar::submit(bool control) {
    std::vector<wchar_t> text((size_t)GetWindowTextLength(edit) + 1);
    GetWindowText(edit, text.data(), (int)text.size());
    PathUnquoteSpaces(text.data());
    if (!text[0])
        return;
    // A virtual folder's editing name need not be a parseable file-system path.
    if (currentItem && address == text.data()) {
        endEdit(true);
        openItem(currentItem, control);
        return;
    }
    DWORD capacity = ExpandEnvironmentStrings(text.data(), nullptr, 0);
    std::vector<wchar_t> expanded(capacity ? capacity : 1);
    DWORD length = capacity ? ExpandEnvironmentStrings(text.data(), expanded.data(), capacity) : 0;
    CComPtr<IShellItem> target;
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
    if (length && length <= capacity) {
        const wchar_t *name = expanded.data();
        if (!PathIsURLW(name) && PathIsRelative(name) && _wcsnicmp(name, L"shell:", 6) != 0
                && wcsncmp(name, L"::", 2) != 0)
            hr = SHCreateItemFromRelativeName(currentItem, name, nullptr, IID_PPV_ARGS(&target));
        else
            hr = SHCreateItemFromParsingName(name, nullptr, IID_PPV_ARGS(&target));
    }
    UINT errorString = IDS_CANT_FIND_ITEM;
    if (SUCCEEDED(hr)) {
        target = resolveLink(target);
        SFGAOF attributes = 0;
        hr = target->GetAttributes(SFGAO_FOLDER, &attributes);
        if (SUCCEEDED(hr) && !(attributes & SFGAO_FOLDER)) {
            errorString = IDS_ADDRESS_NOT_FOLDER;
            hr = E_INVALIDARG;
        }
    }
    if (FAILED(hr)) {
        if (length && length <= capacity) recordFolderPathFailure(expanded.data(), hr);
        MessageBox(owner, formatString(errorString, text.data()).get(),
            getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
        // The modal dialog may have ended edit mode. Retain the failed input.
        beginEdit();
        SetWindowText(edit, text.data());
        SendMessage(edit, EM_SETSEL, 0, -1);
        return;
    }
    endEdit(true);
    openItem(target, control);
}

void PathBar::showHiddenSegments() {
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;
    for (size_t i = 0; i < firstVisible; i++) {
        std::wstring label = segments[i].name;
        for (size_t pos = 0; (pos = label.find(L'&', pos)) != std::wstring::npos; pos += 2)
            label.insert(pos, 1, L'&');
        AppendMenu(menu, MF_STRING, i + 1, label.c_str());
    }
    RECT rect = windowRect(hwnd);
    int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY,
        rect.left, rect.bottom, hwnd, nullptr);
    bool control = GetKeyState(VK_CONTROL) < 0;
    DestroyMenu(menu);
    if (command > 0 && (size_t)command <= firstVisible)
        openItem(segments[(size_t)command - 1].item, control);
}

bool PathBar::handleTopLevelMessage(MSG *msg) {
    if (!hwnd || !IsWindowVisible(hwnd))
        return false;
    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        bool control = GetKeyState(VK_CONTROL) < 0;
        bool alt = GetKeyState(VK_MENU) < 0;
        if ((msg->wParam == 'L' && control && !alt) || (msg->wParam == 'D' && alt && !control)) {
            beginEdit();
            return true;
        }
        if (hasFocus()) {
            if (msg->wParam == 'A' && control && !alt) {
                SendMessage(edit, EM_SETSEL, 0, -1);
                return true;
            }
            if (msg->wParam == VK_RETURN && !alt) {
                submit(control);
                return true;
            }
            if (msg->wParam == VK_ESCAPE || msg->wParam == VK_TAB) {
                endEdit(true);
                return true;
            }
        }
    }
    return false;
}

LRESULT PathBar::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT border = clientRect(hwnd);
            COLORREF borderColor = GetSysColor(COLOR_3DLIGHT);
            if (HTHEME theme = OpenThemeData(hwnd, L"Rebar")) {
                COLORREF themedColor;
                if (SUCCEEDED(GetThemeColor(theme, RP_BAND, 0,
                        TMT_BORDERCOLORHINT, &themedColor)))
                    borderColor = themedColor;
                (void)checkHR(CloseThemeData(theme));
            }
            HBRUSH brush = CreateSolidBrush(borderColor);
            FrameRect(dc, &border, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            break;
        case WM_SIZE:
            layout();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_COMMAND:
            if ((HWND)lParam == edit && HIWORD(wParam) == EN_KILLFOCUS) {
                endEdit(false);
                return 0;
            }
            if ((HWND)lParam == toolbar && HIWORD(wParam) == 0) {
                int command = LOWORD(wParam);
                if (command == OVERFLOW_COMMAND)
                    showHiddenSegments();
                else if (command >= FIRST_SEGMENT_COMMAND && command % 2 == 1) {
                    size_t index = (size_t)(command - FIRST_SEGMENT_COMMAND) / 2;
                    if (index < segments.size())
                        openItem(segments[index].item, GetKeyState(VK_CONTROL) < 0);
                }
                return 0;
            }
            break;
        case WM_NOTIFY: {
            auto header = reinterpret_cast<NMHDR *>(lParam);
            if (header->hwndFrom == toolbar && header->code == NM_CUSTOMDRAW) {
                auto draw = reinterpret_cast<NMTBCUSTOMDRAW *>(header);
                if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                UINT_PTR command = draw->nmcd.dwItemSpec;
                if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT
                        && command > FIRST_SEGMENT_COMMAND && command % 2 == 0) {
                    // Static chevrons need centered text, without the list-button inset.
                    HDC dc = draw->nmcd.hdc;
                    int saved = SaveDC(dc);
                    if (chevronFont)
                        SelectObject(dc, chevronFont);
                    // Use the small right-chevron glyph from the Windows icon font.
                    SetBkMode(dc, TRANSPARENT);
                    SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
                    DrawText(dc, L"\uE970", 1, &draw->nmcd.rc,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    RestoreDC(dc, saved);
                    return CDRF_SKIPDEFAULT;
                }
                return CDRF_DODEFAULT;
            }
            break;
        }
        case WM_CONTEXTMENU:
            // The edit control handles its own native context menu.
            return 0;
        case WM_NCDESTROY: {
            LRESULT result = DefWindowProc(hwnd, message, wParam, lParam);
            hwnd = toolbar = edit = previousFocus = nullptr;
            return result;
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK PathBar::toolbarProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData) {
    PathBar *bar = (PathBar *)refData;
    if (message == WM_LBUTTONDOWN) {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (SendMessage(window, TB_HITTEST, 0, (LPARAM)&point) < 0) {
            bar->beginEdit();
            return 0;
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, toolbarProc, subclassID);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

} // namespace
