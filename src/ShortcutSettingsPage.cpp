#include <common.h>
#include "SettingsDialog.h"
#include "Settings.h"
#include "UIStrings.h"
#include "DPI.h"
#include <windowsx.h>
#include <commctrl.h>
#include <prsht.h>
#include <dlgs.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <cguid.h>
#include <initguid.h>
#include <oleacc.h>
#include <atlbase.h>
#include <array>
#include <memory>
#include <string>
#include <type_traits>

#pragma comment(lib, "Oleacc.lib")

namespace filespacer {
namespace {
struct FontDeleter { void operator()(HFONT font) const { DeleteObject(font); } };
using FontPtr = std::unique_ptr<std::remove_pointer<HFONT>::type, FontDeleter>;
constexpr int headerIds[] = {IDC_SHORTCUT_MOVE, IDC_SHORTCUT_RESIZE};
constexpr int enableIds[] = {IDC_SHORTCUT_ENABLE_MOVE, IDC_SHORTCUT_ENABLE_RESIZE};
constexpr UINT nameIds[] = {IDS_SHORTCUT_MOVE, IDS_SHORTCUT_RESIZE};

class ShortcutPage {
public:
    HWND page = nullptr, editor = nullptr;
    WindowShortcuts saved = settings::getWindowShortcuts(), pending = saved;
    int selected = -1, hot = -1;
    bool recording = false, warning = false;
    UINT captureUp = 0;
    std::array<RECT, 2> frames = {};
    std::array<std::wstring, 2> captions;
    CComPtr<IAccPropServices> accessibility;
    FontPtr icons;

    WindowShortcut &binding(int index) { return index == 0 ? pending.move : pending.resize; }
    const WindowShortcut &previous(int index) const { return index == 0 ? saved.move : saved.resize; }
    HWND child(int id) const { return GetDlgItem(editor, id); }
    RECT units(int x, int y, int width, int height) const {
        RECT rect = {x, y, x + width, y + height};
        MapDialogRect(page, &rect); return rect;
    }
    void place(HWND window, int x, int y, int width, int height) const {
        const RECT rect = units(x, y, width, height);
        SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
    void layout() {
        int y = 24;
        for (int i = 0; i < 2; ++i) {
            const int top = y;
            place(GetDlgItem(page, enableIds[i]), 14, y + 5, 12, 12);
            place(GetDlgItem(page, headerIds[i]), 30, y + 1, 262, 20);
            y += 22;
            if (selected == i) {
                const int height = recording ? 180 : 142;
                place(editor, 15, y, 270, height);
                y += height;
            }
            frames[i] = units(7, top, 286, y - top + 1);
            y += 4;
        }
        place(child(IDC_SHORTCUT_DEFAULT), 0, recording ? 163 : 125, 100, 14);
        const bool swap = selected >= 0 && conflictingShortcuts(pending)
            && !reservedShortcut(binding(selected)) && !reservedShortcut(previous(selected));
        place(child(IDC_SHORTCUT_FEEDBACK), 0, 87, swap ? 205 : 270, 17);
        InvalidateRect(page, nullptr, TRUE);
    }
    std::wstring describe(const WindowShortcut &value) const {
        if (!value.enabled) return getString(IDS_SHORTCUT_DISABLED);
        std::wstring keys;
        if (value.modifiers & GestureCtrl) keys += getString(IDS_SHORTCUT_CTRL);
        if (value.modifiers & GestureAlt) keys += getString(IDS_SHORTCUT_ALT);
        if (value.modifiers & GestureShift) keys += getString(IDS_SHORTCUT_SHIFT);
        const UINT buttons[] = {IDS_SHORTCUT_LEFT, IDS_SHORTCUT_MIDDLE, IDS_SHORTCUT_RIGHT};
        const unsigned button = static_cast<unsigned>(value.button);
        if (button >= _countof(buttons)) return getString(IDS_SHORTCUT_RESERVED);
        return formatString(IDS_SHORTCUT_DESCRIPTION, keys.c_str(),
            getString(buttons[button])).get();
    }
    void update() {
        for (int i = 0; i < 2; ++i) {
            const auto description = describe(binding(i));
            captions[i] = formatString(IDS_SHORTCUT_HEADER, getString(nameIds[i]), description.c_str()).get();
            SetDlgItemTextW(page, headerIds[i], captions[i].c_str());
            CheckDlgButton(page, enableIds[i], binding(i).enabled ? BST_CHECKED : BST_UNCHECKED);
            InvalidateRect(GetDlgItem(page, headerIds[i]), nullptr, TRUE);
        }
        const bool changed = !sameShortcut(pending.move, saved.move) || !sameShortcut(pending.resize, saved.resize);
        EnableWindow(GetDlgItem(page, IDC_SHORTCUT_REVERT), changed);
        // Documented sheet contract only: a changed, valid page registers its
        // change (PSM_CHANGED enables Apply); an invalid or unchanged one
        // withdraws it (PSM_UNCHANGED). Apply enabled by another page is
        // refused in PSN_KILLACTIVE / PSN_APPLY with the reason.
        if (changed && validShortcuts(pending)) PropSheet_Changed(GetParent(page), page);
        else PropSheet_UnChanged(GetParent(page), page);
        if (selected < 0) return;
        const auto &value = binding(selected);
        const unsigned masks[] = {GestureCtrl, GestureAlt, GestureShift};
        for (int i = 0; i < 3; ++i) {
            CheckDlgButton(editor, IDC_SHORTCUT_CTRL + i, (value.modifiers & masks[i]) ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(child(IDC_SHORTCUT_CTRL + i), value.enabled);
            CheckDlgButton(editor, IDC_SHORTCUT_LEFT + i,
                static_cast<unsigned>(value.button) == static_cast<unsigned>(i) ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(child(IDC_SHORTCUT_LEFT + i), value.enabled && (i == 1 || (value.modifiers & GestureAlt)));
        }
        SetDlgItemTextW(editor, IDC_SHORTCUT_LEFT_HINT, value.modifiers & GestureAlt ? L"" : getString(IDS_SHORTCUT_LEFT_HINT));
        SetDlgItemTextW(editor, IDC_SHORTCUT_RIGHT_HINT, value.modifiers & GestureAlt ? L"" : getString(IDS_SHORTCUT_RIGHT_HINT));
        const bool reserved = value.enabled && reservedShortcut(value);
        const bool conflict = conflictingShortcuts(pending);
        warning = reserved || conflict;
        const auto conflictText = formatString(IDS_SHORTCUT_CONFLICT, getString(nameIds[1 - selected]));
        SetDlgItemTextW(editor, IDC_SHORTCUT_FEEDBACK, reserved ? getString(IDS_SHORTCUT_RESERVED)
            : conflict ? conflictText.get() : getString(IDS_SHORTCUT_GENERAL));
        InvalidateRect(child(IDC_SHORTCUT_FEEDBACK), nullptr, TRUE);
        ShowWindow(child(IDC_SHORTCUT_EXCHANGE), conflict && !reserved && !reservedShortcut(previous(selected)) ? SW_SHOWNA : SW_HIDE);
        EnableWindow(child(IDC_SHORTCUT_RECORD), value.enabled);
        EnableWindow(child(IDC_SHORTCUT_CAPTURE), value.enabled);
        ShowWindow(child(IDC_SHORTCUT_CAPTURE), recording ? SW_SHOWNA : SW_HIDE);
        ShowWindow(child(IDC_SHORTCUT_CAPTURE_HINT), recording ? SW_SHOWNA : SW_HIDE);
        SetDlgItemTextW(editor, IDC_SHORTCUT_RECORD, getString(recording ? IDS_SHORTCUT_STOP_RECORD : IDS_SHORTCUT_RECORD));
    }
    // The sheet keeps this page active while its shortcuts are invalid
    // (PSN_KILLACTIVE) and refuses to apply them (PSN_APPLY): the user is
    // told which entry is refused and why, instead of a silent no-op.
    void explainInvalid() {
        const int target = !validShortcut(pending.move) ? 0 : 1;
        // Focus restored to a shared auto-radio button can select it for the
        // other action. Keep the refused choice intact while opening its editor.
        SetFocus(GetDlgItem(page, enableIds[target]));
        if (selected != target) open(target);
        const auto &value = target == 0 ? pending.move : pending.resize;
        const auto title = formatString(IDS_SHORTCUT_APPLY_REFUSED, getString(nameIds[target]));
        const auto conflict = formatString(IDS_SHORTCUT_CONFLICT, getString(nameIds[1 - target]));
        TaskDialog(GetParent(page), GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDS_ERROR_CAPTION), title.get(),
            value.enabled && reservedShortcut(value) ? getString(IDS_SHORTCUT_RESERVED) : conflict.get(),
            TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
    }
    void endCapture() {
        recording = false; captureUp = 0;
        if (GetCapture() == child(IDC_SHORTCUT_CAPTURE)) ReleaseCapture();
        SetFocus(child(IDC_SHORTCUT_RECORD)); update(); layout();
    }
    void open(int index) {
        ShowWindow(editor, SW_HIDE);
        recording = false;
        selected = selected == index ? -1 : index;
        update(); layout();
        if (selected >= 0) {
            SetWindowPos(editor, GetDlgItem(page, headerIds[selected]), 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            BOOL animate = FALSE;
            SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
            if (!IsWindowVisible(page) || !animate || !AnimateWindow(editor, 140, AW_VER_POSITIVE)) ShowWindow(editor, SW_SHOWNA);
        }
    }
    void command(int id) {
        if (id == IDC_SHORTCUT_MOVE || id == IDC_SHORTCUT_RESIZE) { open(id - IDC_SHORTCUT_MOVE); return; }
        if (id == IDC_SHORTCUT_ENABLE_MOVE || id == IDC_SHORTCUT_ENABLE_RESIZE) {
            const int index = id - IDC_SHORTCUT_ENABLE_MOVE;
            binding(index).enabled = IsDlgButtonChecked(page, id) == BST_CHECKED;
        } else if (id == IDC_SHORTCUT_REVERT) pending = saved;
        else if (selected >= 0) {
            auto &value = binding(selected);
            if (id == IDC_SHORTCUT_DEFAULT) value = selected == 0 ? DEFAULT_WINDOW_SHORTCUTS.move : DEFAULT_WINDOW_SHORTCUTS.resize;
            else if (id >= IDC_SHORTCUT_CTRL && id <= IDC_SHORTCUT_SHIFT) {
                value.modifiers = (IsDlgButtonChecked(editor, IDC_SHORTCUT_CTRL) == BST_CHECKED ? GestureCtrl : 0u)
                    | (IsDlgButtonChecked(editor, IDC_SHORTCUT_ALT) == BST_CHECKED ? GestureAlt : 0u)
                    | (IsDlgButtonChecked(editor, IDC_SHORTCUT_SHIFT) == BST_CHECKED ? GestureShift : 0u);
            } else if (id >= IDC_SHORTCUT_LEFT && id <= IDC_SHORTCUT_RIGHT)
                value.button = static_cast<GestureButton>(id - IDC_SHORTCUT_LEFT);
            else if (id == IDC_SHORTCUT_RECORD) recording = !recording;
            else if (id == IDC_SHORTCUT_EXCHANGE && conflictingShortcuts(pending)
                    && !reservedShortcut(value) && !reservedShortcut(previous(selected))) {
                auto &other = binding(1 - selected);
                const bool enabled = other.enabled;
                other = previous(selected); other.enabled = enabled;
            }
        }
        update(); layout();
    }
    void paint(HDC context) {
        RECT client = {}; GetClientRect(page, &client);
        HTHEME theme = OpenThemeData(page, L"Tab");
        if (!theme || FAILED(DrawThemeBackground(theme, context, TABP_BODY, 0, &client, nullptr)))
            FillRect(context, &client, GetSysColorBrush(COLOR_BTNFACE));
        if (theme) CloseThemeData(theme);
        theme = OpenThemeData(page, L"Button");
        for (int i = 0; i < 2; ++i) {
            const RECT &frame = frames[i];
            if (!theme || FAILED(DrawThemeBackground(theme, context, BP_GROUPBOX, GBS_NORMAL, &frame, nullptr)))
                FrameRect(context, &frame, GetSysColorBrush(COLOR_BTNSHADOW));
            if (selected == i) {
                RECT header = frame;
                header.bottom = frame.top + units(0, 0, 0, 22).bottom;
                const RECT edge = {header.left, header.bottom - 2, header.right, header.bottom};
                if (!theme || FAILED(DrawThemeBackground(theme, context, BP_GROUPBOX, GBS_NORMAL, &header, &edge)))
                    FillRect(context, &edge, GetSysColorBrush(COLOR_BTNSHADOW));
            }
        }
        if (theme) CloseThemeData(theme);
    }
    void draw(const DRAWITEMSTRUCT &item) {
        const int index = static_cast<int>(item.CtlID) - IDC_SHORTCUT_MOVE;
        if (index < 0 || index >= 2) return;
        const int savedDC = SaveDC(item.hDC);
        if (!savedDC) return;
        if (FAILED(DrawThemeParentBackground(item.hwndItem, item.hDC, &item.rcItem)))
            FillRect(item.hDC, &item.rcItem, GetSysColorBrush(COLOR_BTNFACE));
        SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, GetSysColor(COLOR_BTNTEXT));
        SelectObject(item.hDC, reinterpret_cast<HFONT>(SendMessageW(page, WM_GETFONT, 0, 0)));
        RECT text = item.rcItem; text.right -= scaleDPI(25);
        DrawTextW(item.hDC, captions[index].c_str(), -1, &text,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(item.hDC, icons.get());
        RECT arrow = item.rcItem; arrow.left = arrow.right - scaleDPI(23); arrow.right -= scaleDPI(5);
        SetTextColor(item.hDC, GetSysColor(hot == index ? COLOR_HOTLIGHT : COLOR_BTNTEXT));
        DrawTextW(item.hDC, selected == index ? L"\uE70E" : L"\uE70D", 1, &arrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT)) DrawFocusRect(item.hDC, &item.rcItem);
        RestoreDC(item.hDC, savedDC);
    }
    void clearNames() {
        if (!accessibility) return;
        const MSAAPROPID name = PROPID_ACC_NAME;
        for (int id : enableIds)
            accessibility->ClearHwndProps(GetDlgItem(page, id), static_cast<DWORD>(OBJID_CLIENT), CHILDID_SELF, &name, 1);
    }
};

LRESULT CALLBACK headerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    auto self = reinterpret_cast<ShortcutPage *>(data);
    if (message == WM_MOUSEMOVE) {
        self->hot = GetDlgCtrlID(window) - IDC_SHORTCUT_MOVE;
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0}; TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, TRUE);
    } else if (message == WM_MOUSELEAVE) { self->hot = -1; InvalidateRect(window, nullptr, TRUE); }
    else if (message == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK captureProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    auto self = reinterpret_cast<ShortcutPage *>(data);
    if (message == WM_GETDLGCODE && wParam != VK_TAB) return DLGC_WANTALLKEYS;
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) { self->endCapture(); return 0; }
    if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN) {
        if (!self->recording || self->selected < 0 || !self->binding(self->selected).enabled || self->captureUp) return 0;
        const GestureButton button = message == WM_LBUTTONDOWN ? GestureButton::Left
            : message == WM_MBUTTONDOWN ? GestureButton::Middle : GestureButton::Right;
        auto &value = self->binding(self->selected);
        value.button = button; value.modifiers = currentGestureModifiers();
        self->captureUp = gestureButtonUp(button);
        SetFocus(window); SetCapture(window); self->update(); return 0;
    }
    if (message == self->captureUp && self->captureUp) { self->endCapture(); return 0; }
    if (message == WM_CAPTURECHANGED && self->captureUp) { self->endCapture(); return 0; }
    if (message == WM_CONTEXTMENU || message == WM_LBUTTONUP || message == WM_MBUTTONUP || message == WM_RBUTTONUP) return 0;
    return DefSubclassProc(window, message, wParam, lParam);
}

INT_PTR CALLBACK editorProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(window, GWLP_USERDATA, lParam);
        checkHR(EnableThemeDialogTexture(window, ETDT_ENABLETAB));
        return TRUE;
    }
    auto self = reinterpret_cast<ShortcutPage *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!self) return FALSE;
    if (message == WM_COMMAND) { self->command(LOWORD(wParam)); return TRUE; }
    if (message == WM_CTLCOLORSTATIC && self->warning
            && GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) == IDC_SHORTCUT_FEEDBACK) {
        const HDC context = reinterpret_cast<HDC>(wParam);
        DrawThemeParentBackground(reinterpret_cast<HWND>(lParam), context, nullptr);
        SetTextColor(context, RGB(190, 0, 0));
        SetBkMode(context, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(GetStockObject(HOLLOW_BRUSH));
    }
    return FALSE;
}
} // namespace

INT_PTR CALLBACK shortcutsProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto self = reinterpret_cast<ShortcutPage *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_INITDIALOG) {
        checkHR(EnableThemeDialogTexture(window, ETDT_ENABLETAB));
        auto state = std::make_unique<ShortcutPage>(); state->page = window;
        state->accessibility.CoCreateInstance(CLSID_AccPropServices);
        LOGFONTW font = {};
        GetObjectW(reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)), sizeof(font), &font);
        font.lfHeight = -scaleDPI(12); wcscpy_s(font.lfFaceName, L"Segoe MDL2 Assets");
        state->icons.reset(CreateFontIndirectW(&font));
        state->editor = CreateDialogParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SHORTCUT_EDITOR), window,
            editorProc, reinterpret_cast<LPARAM>(state.get()));
        if (!state->editor) return FALSE;
        self = state.release(); SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        for (int i = 0; i < 2; ++i) {
            if (self->accessibility) {
                auto label = formatString(IDS_SHORTCUT_ACCESSIBLE, getString(nameIds[i]));
                self->accessibility->SetHwndPropStr(GetDlgItem(window, enableIds[i]), static_cast<DWORD>(OBJID_CLIENT),
                    CHILDID_SELF, PROPID_ACC_NAME, label.get());
            }
            SetWindowSubclass(GetDlgItem(window, headerIds[i]), headerProc, 1, reinterpret_cast<DWORD_PTR>(self));
        }
        SetWindowSubclass(self->child(IDC_SHORTCUT_CAPTURE), captureProc, 1, reinterpret_cast<DWORD_PTR>(self));
        self->update(); self->layout(); return TRUE;
    }
    if (!self) return FALSE;
    if (message == WM_COMMAND) { self->command(LOWORD(wParam)); return TRUE; }
    if (message == WM_NOTIFY) {
        const auto notification = reinterpret_cast<NMHDR *>(lParam);
        if (notification->code == PSN_SETACTIVE) self->update();
        else if (notification->code == PSN_KILLACTIVE) {
            const bool invalid = !validShortcuts(self->pending);
            if (invalid) self->explainInvalid();
            SetWindowLongPtrW(window, DWLP_MSGRESULT, invalid); return TRUE;
        } else if (notification->code == PSN_APPLY) {
            if (!validShortcuts(self->pending)) {
                self->explainInvalid();
                SetWindowLongPtrW(window, DWLP_MSGRESULT, PSNRET_INVALID); return TRUE;
            }
            if (settings::setWindowShortcuts(self->pending) != ERROR_SUCCESS) {
                TaskDialog(GetParent(window), GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDS_ERROR_CAPTION),
                    MAKEINTRESOURCEW(IDS_SHORTCUT_SAVE_FAILED), nullptr, TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
                SetWindowLongPtrW(window, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE); return TRUE;
            }
            self->saved = self->pending; self->update(); notifyWindowSettingsChanged();
            SetWindowLongPtrW(window, DWLP_MSGRESULT, PSNRET_NOERROR); return TRUE;
        }
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint = {}; HDC context = BeginPaint(window, &paint);
        self->paint(context); EndPaint(window, &paint); return TRUE;
    }
    if (message == WM_PRINTCLIENT) { self->paint(reinterpret_cast<HDC>(wParam)); return TRUE; }
    if (message == WM_DRAWITEM) { self->draw(*reinterpret_cast<DRAWITEMSTRUCT *>(lParam)); return TRUE; }
    if (message == WM_THEMECHANGED || message == WM_SYSCOLORCHANGE)
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    if (message == WM_DESTROY) {
        self->recording = false; self->captureUp = 0;
        if (GetCapture() == self->child(IDC_SHORTCUT_CAPTURE)) ReleaseCapture();
        self->clearNames();
    }
    if (message == WM_NCDESTROY) { SetWindowLongPtrW(window, GWLP_USERDATA, 0); delete self; }
    return FALSE;
}
} // namespace filespacer
