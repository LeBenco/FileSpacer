#include <common.h>
#include "SearchBox.h"
#include "DPI.h"
#include "UIStrings.h"
#include "GeomUtils.h"
#include "Everything.h"
#include <uxtheme.h>
#include <vssym32.h>
#include <algorithm>
#include <type_traits>

namespace filespacer {

static const wchar_t SEARCH_BOX_CLASS[] = L"FileSpacer Search Box";
static const wchar_t SEARCH_POPUP_CLASS[] = L"FileSpacer Search Suggestions";
static constexpr UINT_PTR SEARCH_TIMER = 1;
static constexpr UINT SEARCH_DELAY_MS = 150;
static UINT searchIconReadyMessage = 0;

struct SearchBox::IconState {
    SRWLOCK lock = SRWLOCK_INIT;
    HWND owner;
    const int pixels;
    const std::wstring manualPath;
    bool loading = false;
    std::wstring executable;
    WIN32_FILE_ATTRIBUTE_DATA stamp = {};
    std::unique_ptr<std::remove_pointer_t<HICON>, decltype(&DestroyIcon)> icon{nullptr, DestroyIcon};

    IconState(HWND window, int size, const std::wstring &path) : owner(window), pixels(size), manualPath(path) {}

    static DWORD WINAPI load(void *parameter) {
        const std::unique_ptr<std::shared_ptr<IconState>> request(static_cast<std::shared_ptr<IconState> *>(parameter));
        const auto work = *request;
        std::wstring path = work->manualPath.empty() ? findEverything() : work->manualPath;
        WIN32_FILE_ATTRIBUTE_DATA attributes = {};
        const bool found = !path.empty() && GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)
            && !(attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        AcquireSRWLockExclusive(&work->lock);
        const bool unchanged = found && work->icon && path == work->executable
            && CompareFileTime(&attributes.ftLastWriteTime, &work->stamp.ftLastWriteTime) == 0
            && CompareFileTime(&attributes.ftCreationTime, &work->stamp.ftCreationTime) == 0
            && attributes.nFileSizeHigh == work->stamp.nFileSizeHigh && attributes.nFileSizeLow == work->stamp.nFileSizeLow;
        const bool cancelled = !work->owner;
        if (unchanged || cancelled) work->loading = false;
        ReleaseSRWLockExclusive(&work->lock);
        if (unchanged || cancelled) return 0;

        std::unique_ptr<std::remove_pointer_t<HICON>, decltype(&DestroyIcon)> loaded(
            found ? extractEverythingIcon(path, work->pixels) : nullptr, DestroyIcon);
        HWND notify = nullptr;
        AcquireSRWLockExclusive(&work->lock);
        if (work->owner) {
            work->icon.swap(loaded); // Destroy the old icon after releasing the lock.
            work->executable = std::move(path);
            work->stamp = attributes;
            notify = work->owner;
        }
        work->loading = false;
        ReleaseSRWLockExclusive(&work->lock);
        // Notify after unlocking: drawing never waits for the resource worker.
        // This registered message carries no data and only invalidates a live list.
        if (notify) PostMessageW(notify, searchIconReadyMessage, 0, 0);
        return 0;
    }
};

void SearchBox::resetEverythingIcon() {
    if (!everythingIcon) return;
    AcquireSRWLockExclusive(&everythingIcon->lock);
    everythingIcon->owner = nullptr;
    ReleaseSRWLockExclusive(&everythingIcon->lock);
    everythingIcon.reset(); // A running request retains its own state, never this SearchBox.
}

void SearchBox::requestEverythingIcon() {
    if (!searchIconReadyMessage) return;
    const int pixels = scaleDPI(16);
    if (everythingIcon && everythingIcon->pixels != pixels) resetEverythingIcon();
    if (!everythingIcon) everythingIcon = std::make_shared<IconState>(hwnd, pixels, everythingPath);
    const auto work = everythingIcon;
    if (!TryAcquireSRWLockExclusive(&work->lock)) return;
    if (!work->loading) {
        work->loading = true;
        auto request = new std::shared_ptr<IconState>(work);
        if (!QueueUserWorkItem(IconState::load, request, WT_EXECUTEDEFAULT)) {
            delete request;
            work->loading = false;
        }
    }
    ReleaseSRWLockExclusive(&work->lock);
}

HMENU createSearchMenu(bool everything, const std::vector<std::wstring> &history) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return nullptr;
    if (everything) AppendMenuW(menu, MF_STRING, 1, getString(IDS_SEARCH_EVERYTHING));
    bool separator = everything;
    for (size_t index = 0; index < history.size(); ++index) {
        if (history[index].empty()) continue;
        if (separator) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            separator = false;
        }
        std::wstring label;
        for (wchar_t character : history[index]) {
            label += character;
            if (character == L'&') label += L'&';
        }
        AppendMenuW(menu, MF_STRING, 2 + index, label.c_str());
    }
    return menu;
}

void SearchBox::init() {
    searchIconReadyMessage = checkLE(RegisterWindowMessageW(L"FileSpacer.EverythingIconReady"));
    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = SEARCH_BOX_CLASS;
    checkLE(RegisterClass(&wc));
    wc.lpfnWndProc = popupProc;
    wc.lpszClassName = SEARCH_POPUP_CLASS;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
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
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_SEARCH_SUBMIT), GetModuleHandle(nullptr), nullptr));
    if (!edit || !button) {
        destroy();
        return;
    }
    // Start with an empty edit; the cue is supplied from the folder's display name.
    SetWindowText(edit, L"");
    SendMessage(edit, EM_SETLIMITTEXT, 1024, 0);
    checkLE(SetWindowSubclass(edit, editProc, 0, reinterpret_cast<DWORD_PTR>(this)));
    updateFonts();
    splitter = checkLE(CreateWindowEx(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 0, 0, GetParent(parent), nullptr, GetModuleHandle(nullptr), nullptr));
    if (splitter)
        checkLE(SetWindowSubclass(splitter, splitterProc, 0, (DWORD_PTR)this));
}

void SearchBox::destroy() {
    resetEverythingIcon();
    hideDropdown();
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
    hideDropdown();
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

void SearchBox::setDropdownEnabled(bool enabled, const wchar_t *path) {
    hideDropdown();
    resetEverythingIcon();
    everythingPath = path;
    dropdownEnabled = enabled;
}

void SearchBox::requestDropdown() {
    if (!dropdownEnabled || dropdownWindow || !IsWindowVisible(hwnd)) return;
    Notification notification = {{hwnd, 0, DROPDOWN}, nullptr};
    SendMessage(owner, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notification));
}

void SearchBox::hideDropdown() {
    const HWND popup = dropdownWindow;
    dropdownWindow = dropdownList = nullptr;
    if (popup) DestroyWindow(popup);
}

void SearchBox::showDropdown(HMENU menu) {
    if (dropdownWindow || !dropdownEnabled) return;
    suggestions.clear();
    for (int index = 0; index < GetMenuItemCount(menu); ++index) {
        MENUITEMINFOW info = {sizeof(info)};
        info.fMask = MIIM_ID | MIIM_FTYPE;
        if (!GetMenuItemInfoW(menu, index, TRUE, &info)) return;
        std::wstring label(static_cast<size_t>(GetMenuStringW(menu, index, nullptr, 0, MF_BYPOSITION)) + 1, L'\0');
        label.resize(GetMenuStringW(menu, index, &label[0], static_cast<int>(label.size()), MF_BYPOSITION));
        // Menu labels escape ampersands; the list box displays literal strings.
        for (size_t pos = 0; (pos = label.find(L"&&", pos)) != std::wstring::npos; ++pos)
            label.erase(pos, 1);
        suggestions.push_back({(info.fType & MFT_SEPARATOR) ? 0u : info.wID, std::move(label)});
    }
    if (suggestions.empty()) return;
    if (suggestions.front().command == 1) requestEverythingIcon();
    dropdownWindow = checkLE(CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        SEARCH_POPUP_CLASS, getString(IDS_SEARCH_MENU), WS_POPUP | WS_BORDER,
        0, 0, 0, 0, owner, nullptr, GetModuleHandle(nullptr), this));
    if (!dropdownWindow) return;
    dropdownList = checkLE(CreateWindowExW(0, L"LISTBOX", getString(IDS_SEARCH_MENU),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS | LBS_OWNERDRAWVARIABLE,
        0, 0, 0, 0, dropdownWindow, nullptr, GetModuleHandle(nullptr), nullptr));
    if (!dropdownList) { hideDropdown(); return; }
    SendMessageW(dropdownList, WM_SETFONT, reinterpret_cast<WPARAM>(textFont), FALSE);
    checkLE(SetWindowSubclass(dropdownList, listProc, 0, reinterpret_cast<DWORD_PTR>(this)));
    int height = 0;
    for (const auto &entry : suggestions) {
        const LRESULT index = SendMessageW(dropdownList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.text.c_str()));
        if (index < 0) { hideDropdown(); return; }
        height += static_cast<int>(SendMessageW(dropdownList, LB_GETITEMHEIGHT, index, 0));
    }
    SendMessageW(dropdownList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    const RECT anchor = windowRect(hwnd);
    MONITORINFO monitor = {sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
        hideDropdown(); return;
    }
    const int width = (std::min)(rectWidth(monitor.rcWork), (std::max)(rectWidth(anchor), scaleDPI(220)));
    height = (std::min)(height + 2, scaleDPI(240));
    const int below = (std::max)(0L, monitor.rcWork.bottom - anchor.bottom);
    const int above = (std::max)(0L, anchor.top - monitor.rcWork.top);
    const bool upwards = below < height && above > below;
    height = (std::min)(height, upwards ? above : below);
    const int left = (std::max)(monitor.rcWork.left, (std::min)(anchor.left, monitor.rcWork.right - width));
    SetWindowPos(dropdownWindow, HWND_TOP, left, upwards ? anchor.top - height : anchor.bottom,
        width, height, SWP_NOACTIVATE);
    const RECT content = clientRect(dropdownWindow);
    SetWindowPos(dropdownList, nullptr, 0, 0, rectWidth(content), rectHeight(content), SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(dropdownWindow, SW_SHOWNOACTIVATE);
}

void SearchBox::moveDropdownSelection(int direction) {
    if (!dropdownList) return;
    int selected = static_cast<int>(SendMessageW(dropdownList, LB_GETCURSEL, 0, 0));
    if (selected < 0 && direction < 0) selected = static_cast<int>(suggestions.size());
    for (int index = selected + direction; index >= 0 && index < static_cast<int>(suggestions.size()); index += direction) {
        if (!suggestions[index].command) continue;
        SendMessageW(dropdownList, LB_SETCURSEL, index, 0);
        return;
    }
}

void SearchBox::selectDropdown() {
    const int index = static_cast<int>(SendMessageW(dropdownList, LB_GETCURSEL, 0, 0));
    if (index < 0 || index >= static_cast<int>(suggestions.size()) || !suggestions[index].command) return;
    const UINT command = suggestions[index].command;
    const std::wstring query = command == 1 ? text() : suggestions[index].text;
    const HWND notificationOwner = owner;
    Notification notification = {{hwnd, 0, SELECT}, query.c_str(), 0, true, command};
    hideDropdown(); // Destruction can dispatch accessibility callbacks and close the owner.
    SendMessageW(notificationOwner, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notification));
}

LRESULT CALLBACK SearchBox::editProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData) {
    auto self = reinterpret_cast<SearchBox *>(refData);
    if (message == WM_LBUTTONDOWN) {
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        self->requestDropdown();
        return result;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, editProc, subclassID);
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK SearchBox::listProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData) {
    auto self = reinterpret_cast<SearchBox *>(refData);
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE) {
        // A popup appearing under a stationary pointer must not select Everything.
        if (message == WM_MOUSEMOVE && GetCapture() != window) return 0;
        const bool released = message == WM_LBUTTONUP && GetCapture() == window;
        if (message == WM_LBUTTONUP) {
            if (!released) return 0;
            ReleaseCapture();
        }
        const LRESULT hit = SendMessageW(window, LB_ITEMFROMPOINT, 0, lParam);
        if (HIWORD(hit) == 0 && LOWORD(hit) < self->suggestions.size()
                && self->suggestions[LOWORD(hit)].command) {
            SendMessageW(window, LB_SETCURSEL, LOWORD(hit), 0);
            if (message == WM_LBUTTONDOWN) SetCapture(window);
            if (released) self->selectDropdown();
        }
        return 0; // Keep the caret and keyboard focus in the edit.
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, listProc, subclassID);
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK SearchBox::popupProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto self = reinterpret_cast<SearchBox *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<SearchBox *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (self && message == WM_MEASUREITEM) {
        auto measure = reinterpret_cast<MEASUREITEMSTRUCT *>(lParam);
        const bool separator = measure->itemID < self->suggestions.size() && !self->suggestions[measure->itemID].command;
        measure->itemHeight = scaleDPI(separator ? 8 : 24);
        return TRUE;
    }
    if (self && message == WM_DRAWITEM) {
        auto draw = reinterpret_cast<DRAWITEMSTRUCT *>(lParam);
        if (draw->itemID >= self->suggestions.size()) return TRUE;
        const auto &entry = self->suggestions[draw->itemID];
        const bool selected = entry.command && (draw->itemState & ODS_SELECTED);
        FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
        RECT rect = draw->rcItem;
        InflateRect(&rect, -scaleDPI(8), 0);
        if (!entry.command) {
            rect.top = (rect.top + rect.bottom) / 2; rect.bottom = rect.top + 1;
            FillRect(draw->hDC, &rect, GetSysColorBrush(COLOR_3DSHADOW));
        } else {
            const int saved = SaveDC(draw->hDC);
            if (entry.command == 1) {
                const int pixels = scaleDPI(16);
                const auto work = self->everythingIcon;
                if (work && TryAcquireSRWLockShared(&work->lock)) {
                    if (work->icon) DrawIconEx(draw->hDC, rect.left,
                        (rect.top + rect.bottom - pixels) / 2, work->icon.get(), pixels, pixels, 0, nullptr, DI_NORMAL);
                    ReleaseSRWLockShared(&work->lock);
                }
                rect.left += pixels + scaleDPI(6); // Reserve space before async completion; no label jump.
            }
            SelectObject(draw->hDC, self->textFont);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
            DrawTextW(draw->hDC, entry.text.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            RestoreDC(draw->hDC, saved);
        }
        return TRUE;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void SearchBox::setText(const wchar_t *value) {
    SetWindowTextW(edit, value);
}

std::wstring SearchBox::text() const {
    const int length = GetWindowTextLengthW(edit);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, &value[0], length + 1);
    value.resize(length);
    return value;
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
        submit(false);
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
    requestDropdown();
    return true;
}

void SearchBox::submit(bool explicitSubmit) {
    if (explicitSubmit) hideDropdown();
    KillTimer(hwnd, SEARCH_TIMER);
    searchPending = false;
    const std::wstring value = text();
    Notification notification = {{hwnd, 0, APPLY}, value.c_str(), 0, explicitSubmit};
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
        if (dropdownList && SendMessageW(dropdownList, LB_GETCURSEL, 0, 0) != LB_ERR) selectDropdown();
        else submit();
        return true;
    }
    if (msg->wParam == VK_ESCAPE) {
        if (dropdownWindow) { hideDropdown(); return true; }
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
        hideDropdown();
        const bool backward = GetKeyState(VK_SHIFT) < 0;
        const HWND focused = GetFocus();
        if (focused == edit && !backward) SetFocus(button);
        else if (focused == button && backward) SetFocus(edit);
        else {
            Notification notification = {{hwnd, 0, FOCUS_VIEW}, nullptr};
            SendMessage(owner, WM_NOTIFY, 0, (LPARAM)&notification);
        }
        return true;
    }
    if ((msg->wParam == VK_DOWN || msg->wParam == VK_UP) && dropdownEnabled) {
        requestDropdown();
        moveDropdownSelection(msg->wParam == VK_DOWN ? 1 : -1);
        return true;
    }
    return false;
}

LRESULT SearchBox::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (searchIconReadyMessage && message == searchIconReadyMessage) {
        if (dropdownList) InvalidateRect(dropdownList, nullptr, FALSE);
        return 0;
    }
    switch (message) {
        case WM_SETFOCUS:
        case WM_LBUTTONDOWN:
            SetFocus(edit);
            return 0;
        case WM_SIZE:
            layout();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SHOWWINDOW:
            if (!wParam) hideDropdown();
            break;
        case WM_COMMAND:
            if ((HWND)lParam == button && HIWORD(wParam) == BN_CLICKED) {
                submit();
                return 0;
            }
            if ((HWND)lParam == edit && HIWORD(wParam) == EN_CHANGE) {
                if (dropdownList) SendMessageW(dropdownList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                if (liveSearch)
                    searchPending = !!checkLE(SetTimer(hwnd, SEARCH_TIMER, SEARCH_DELAY_MS, nullptr));
                return 0;
            }
            if ((HWND)lParam == edit && (HIWORD(wParam) == EN_SETFOCUS
                    || HIWORD(wParam) == EN_KILLFOCUS)) {
                if (HIWORD(wParam) == EN_SETFOCUS) requestDropdown();
                else hideDropdown();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        case WM_TIMER:
            if (wParam == SEARCH_TIMER) {
                if (liveSearch && searchPending)
                    submit(false);
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
            hideDropdown();
            updateFonts();
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case WM_SYSCOLORCHANGE:
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
            break;
        case WM_NCDESTROY: {
            resetEverythingIcon();
            hideDropdown();
            LRESULT result = DefWindowProc(hwnd, message, wParam, lParam);
            hwnd = edit = button = nullptr;
            return result;
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace
