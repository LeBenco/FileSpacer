#include <common.h>
#include "Settings.h"
#include "SettingsDialog.h"
#include "UIStrings.h"
#include "DPI.h"
#include "WindowResizeGesture.h"
#include <windowsx.h>
#include <commctrl.h>
#include <prsht.h>
#include <objbase.h>
#include <oleacc.h>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace filespacer;

namespace {
int checks = 0, notifications = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s (check %d)\n", message, checks); std::abort(); }
}
HWND sheet = nullptr, page = nullptr, editor = nullptr;
int openSection = -1;
WindowResizeGesture gesture;
void pump() {
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (!IsDialogMessageW(sheet, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
}
WindowShortcut choice(unsigned index) {
    return {index >= 24, (index / 3) % 8, static_cast<GestureButton>(index % 3)};
}
bool allowed(unsigned modifiers, unsigned button) {
    const unsigned buttons[] = {2, 2, 7, 7, 2, 2, 7, 7};
    return (buttons[modifiers] & (1u << button)) != 0;
}
bool equal(const WindowShortcuts &a, const WindowShortcuts &b) {
    return sameShortcut(a.move, b.move) && sameShortcut(a.resize, b.resize);
}
DWORD registryValue() {
    DWORD value = 0, size = sizeof(value);
    require(RegGetValueW(HKEY_CURRENT_USER, L"Software\\FileSpacer\\Test", L"WindowShortcuts",
        RRF_RT_DWORD, nullptr, &value, &size) == ERROR_SUCCESS, "read actual test registry value");
    return value;
}
void click(HWND parent, int id) {
    HWND button = GetDlgItem(parent, id); require(button != nullptr, "actual dialog control exists");
    SendMessageW(button, BM_CLICK, 0, 0);
}
void setCheck(HWND parent, int id, bool value) {
    if ((IsDlgButtonChecked(parent, id) == BST_CHECKED) != value) click(parent, id);
    require((IsDlgButtonChecked(parent, id) == BST_CHECKED) == value, "native checkbox state");
}
void open(int index) {
    if (openSection != index) { click(page, IDC_SHORTCUT_MOVE + index); openSection = index; }
    require((static_cast<DWORD_PTR>(GetWindowLongPtrW(editor, GWL_STYLE)) & WS_VISIBLE) != 0, "shared editor expanded");
}
void setAction(int index, const WindowShortcut &value) {
    open(index);
    setCheck(page, IDC_SHORTCUT_ENABLE_MOVE + index, true);
    // Select the button while permitted, then exercise incompatible modifier changes too.
    setCheck(editor, IDC_SHORTCUT_ALT, true);
    click(editor, IDC_SHORTCUT_LEFT + static_cast<int>(value.button));
    setCheck(editor, IDC_SHORTCUT_CTRL, (value.modifiers & GestureCtrl) != 0);
    setCheck(editor, IDC_SHORTCUT_SHIFT, (value.modifiers & GestureShift) != 0);
    setCheck(editor, IDC_SHORTCUT_ALT, (value.modifiers & GestureAlt) != 0);
    setCheck(page, IDC_SHORTCUT_ENABLE_MOVE + index, value.enabled);
    require(IsDlgButtonChecked(editor, IDC_SHORTCUT_LEFT + static_cast<int>(value.button)) == BST_CHECKED,
        "invalid selection is retained for correction");
    for (unsigned button = 0; button < 3; ++button)
        require((IsWindowEnabled(GetDlgItem(editor, IDC_SHORTCUT_LEFT + button)) != FALSE)
            == (value.enabled && allowed(value.modifiers, button)), "allowed buttons match independent table");
}
LONG_PTR notify(UINT code) {
    NMHDR notification = {}; notification.code = code; notification.hwndFrom = sheet;
    SendMessageW(page, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notification));
    return GetWindowLongPtrW(page, DWLP_MSGRESULT);
}
void registryAndPageMatrix() {
    unsigned valid = 0, invalid = 0;
    for (unsigned first = 0; first < 48; ++first) for (unsigned second = 0; second < 48; ++second) {
        const WindowShortcuts candidate = {choice(first), choice(second)};
        const bool conflict = first >= 24 && second >= 24 && first % 24 == second % 24;
        const bool expected = (!candidate.move.enabled || allowed(candidate.move.modifiers, first % 3))
            && (!candidate.resize.enabled || allowed(candidate.resize.modifiers, second % 3)) && !conflict;
        require(validShortcuts(candidate) == expected, "all 2304 pairs use the specified combination policy");
        setAction(0, candidate.move); setAction(1, candidate.resize);
        const DWORD before = registryValue();
        require((notify(PSN_KILLACTIVE) == FALSE) == expected, "native property sheet validates before leaving page");
        if (!expected) openSection = !validShortcut(candidate.move) ? 0 : 1;
        require((notify(PSN_APPLY) == PSNRET_NOERROR) == expected, "native page validates Apply for every pair");
        if (expected) {
            ++valid; require(equal(settings::getWindowShortcuts(), candidate), "valid pair persists and reloads atomically");
        } else {
            ++invalid; require(registryValue() == before, "invalid page submission leaves registry unchanged");
            require(settings::setWindowShortcuts(candidate) == ERROR_INVALID_DATA, "storage independently rejects invalid pairs");
            require(registryValue() == before, "rejected storage write is atomic");
            require(IsWindowEnabled(GetDlgItem(sheet, 0x3021)) == FALSE, "Apply disabled while any action is invalid");
        }
    }
    require(valid == 1584 && invalid == 720, "expected exhaustive pair counts");
    std::printf("PASS: 2304 native page/registry pairs: %u valid, %u rejected\n", valid, invalid);
    for (unsigned value = 0; value < 48; ++value) for (unsigned input = 0; input < 24; ++input) {
        const auto binding = choice(value);
        const bool expected = value >= 24 && value % 24 == input && allowed(input / 3, input % 3);
        require(matchesShortcut(binding, static_cast<GestureButton>(input % 3), input / 3) == expected,
            "exact modifier/button matching, including disabled and forbidden choices");
    }
    DWORD corrupt[] = {0, 0xffffffffu, 0x02000000u, 0x01003a3au, 0x01001800u, 0x01008000u};
    for (DWORD value : corrupt) {
        require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\FileSpacer\\Test", L"WindowShortcuts", REG_DWORD,
            &value, sizeof(value)) == ERROR_SUCCESS, "prepare invalid persisted data");
        require(equal(settings::getWindowShortcuts(), DEFAULT_WINDOW_SHORTCUTS), "corrupt/unsupported persisted value falls back safely");
    }
}
void interactionMatrix() {
    setAction(0, DEFAULT_WINDOW_SHORTCUTS.move); setAction(1, DEFAULT_WINDOW_SHORTCUTS.resize);
    require(notify(PSN_APPLY) == PSNRET_NOERROR, "restore starting bindings");
    click(page, IDC_SHORTCUT_RESIZE); openSection = -1;
    const DWORD before = registryValue();
    setCheck(page, IDC_SHORTCUT_ENABLE_MOVE, false);
    require(!IsWindowVisible(editor) && registryValue() == before, "header checkbox neither expands nor saves");
    click(page, IDC_SHORTCUT_REVERT);
    require(IsDlgButtonChecked(page, IDC_SHORTCUT_ENABLE_MOVE) == BST_CHECKED, "Revert works with collapsed sections");
    setAction(1, {false, GestureCtrl, GestureButton::Middle});
    setAction(0, {true, 0, GestureButton::Middle}); click(editor, IDC_SHORTCUT_DEFAULT);
    require(IsDlgButtonChecked(page, IDC_SHORTCUT_ENABLE_RESIZE) == BST_UNCHECKED,
        "per-section default leaves the other action unchanged");
    click(page, IDC_SHORTCUT_REVERT);
    BYTE keys[256] = {}, neutral[256] = {};
    require(GetKeyboardState(keys) != FALSE, "save thread keyboard state");
    for (unsigned input = 0; input < 24; ++input) {
        setAction(1, {false, GestureAlt, GestureButton::Right});
        open(0); click(editor, IDC_SHORTCUT_RECORD);
        BYTE state[256] = {}; const unsigned modifiers = input / 3;
        state[VK_CONTROL] = modifiers & GestureCtrl ? 0x80 : 0;
        state[VK_MENU] = modifiers & GestureAlt ? 0x80 : 0;
        state[VK_SHIFT] = modifiers & GestureShift ? 0x80 : 0;
        require(SetKeyboardState(state) != FALSE, "set only this test thread's modifiers");
        const GestureButton button = static_cast<GestureButton>(input % 3);
        const UINT down[] = {WM_LBUTTONDOWN, WM_MBUTTONDOWN, WM_RBUTTONDOWN};
        HWND capture = GetDlgItem(editor, IDC_SHORTCUT_CAPTURE);
        SendMessageW(capture, down[input % 3], gestureButtonMask(button), 0);
        SendMessageW(capture, gestureButtonUp(button), 0, 0);
        require(SetKeyboardState(neutral) != FALSE, "clear local modifiers after capture");
        require(!IsWindowVisible(capture), "all 24 captures close, valid or invalid");
        require(IsDlgButtonChecked(editor, IDC_SHORTCUT_LEFT + input % 3) == BST_CHECKED, "captured button retained");
        require((IsWindowEnabled(GetDlgItem(sheet, 0x3021)) != FALSE) == allowed(modifiers, input % 3),
            "capture validity controls native Apply");
        HDC context = GetDC(capture); require(context != nullptr, "capture feedback color DC");
        SendMessageW(editor, WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(context),
            reinterpret_cast<LPARAM>(GetDlgItem(editor, IDC_SHORTCUT_FEEDBACK)));
        require(GetTextColor(context) == (allowed(modifiers, input % 3) ? GetSysColor(COLOR_BTNTEXT) : RGB(190, 0, 0)),
            "normal guidance versus red capture error");
        ReleaseDC(capture, context); click(page, IDC_SHORTCUT_REVERT);
    }
    require(SetKeyboardState(keys) != FALSE, "restore thread keyboard state");
    open(0); setAction(0, DEFAULT_WINDOW_SHORTCUTS.resize); click(editor, IDC_SHORTCUT_EXCHANGE);
    require(notify(PSN_APPLY) == PSNRET_NOERROR, "swap can be applied atomically");
    const auto swapped = settings::getWindowShortcuts();
    require(sameShortcut(swapped.move, DEFAULT_WINDOW_SHORTCUTS.resize)
        && sameShortcut(swapped.resize, DEFAULT_WINDOW_SHORTCUTS.move), "both exchanged shortcuts persisted");
    // Recreate the actual page, proving persisted options are independent of its controller lifetime.
    DestroyWindow(sheet); openSection = -1;
    std::printf("PASS: capture, warnings, collapsed activation/Revert, per-action defaults, atomic swap\n");
}
LRESULT CALLBACK testWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (gesture.handleMessage(message, wParam, lParam)) return 0;
    return DefWindowProcW(window, message, wParam, lParam);
}
void engineMatrix() {
    WNDCLASSW definition = {}; definition.lpfnWndProc = testWindow;
    definition.hInstance = GetModuleHandleW(nullptr); definition.lpszClassName = L"FileSpacer shortcut capture test";
    require(RegisterClassW(&definition) != 0, "register native gesture host");
    HWND window = CreateWindowExW(0, definition.lpszClassName, L"gesture test", WS_OVERLAPPEDWINDOW,
        50, 50, 400, 300, nullptr, nullptr, definition.hInstance, nullptr);
    require(window != nullptr, "create actual Win32 gesture host");
    unsigned runs = 0;
    for (unsigned move = 0; move < 2; ++move) for (unsigned input = 0; input < 24; ++input) {
        if (!allowed(input / 3, input % 3)) continue;
        const auto button = static_cast<GestureButton>(input % 3);
        for (unsigned cancel = 0; cancel < 3; ++cancel) {
            SetWindowPos(window, nullptr, 50, 50, 400, 300, SWP_NOZORDER | SWP_NOACTIVATE);
            RECT before = {}; GetWindowRect(window, &before);
            POINT point = {0, 0}; ClientToScreen(window, &point);
            require(gesture.begin(window, window, point, button, move != 0), "start generic capture");
            SendMessageW(window, WM_MOUSEMOVE, gestureButtonMask(button), MAKELPARAM(60, 40));
            if (cancel == 1) SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 0);
            if (cancel == 2) SendMessageW(window, WM_CANCELMODE, 0, 0);
            POINT release = {point.x + 60, point.y + 40}; ScreenToClient(window, &release);
            SendMessageW(window, gestureButtonUp(button), 0, MAKELPARAM(release.x, release.y));
            RECT after = {}; GetWindowRect(window, &after);
            require(cancel ? EqualRect(&before, &after) != FALSE
                : move ? after.left == before.left + 60 && after.top == before.top + 40
                    && after.right == before.right + 60 && after.bottom == before.bottom + 40
                : after.left == before.left && after.top == before.top && after.right == before.right + 60 && after.bottom == before.bottom + 40,
                "generic movement/resizing or cancellation changes actual geometry correctly");
            require(GetCapture() == nullptr && gesture.takeCompleted(), "capture ends and completion is observable");
            ++runs;
        }
    }
    DestroyWindow(window);
    std::printf("PASS: %u native HWND geometry/cancellation scenarios\n", runs);
}
} // namespace

bool logHRESULT(long result, const char *, int, const char *) { return SUCCEEDED(result); }
void logLastError(const char *, int, const char *expression) { require(false, expression); }
namespace filespacer { void notifyWindowSettingsChanged() { ++notifications; } }

int main(int argc, char **argv) {
    require(argc == 2, "language argument");
    SetThreadUILanguage(argv[1][0] == 'f' ? MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH) : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), "COM initialized");
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    require(InitCommonControlsEx(&controls) != FALSE, "native controls initialized"); initDPI();
    settings::testMode = true;
    require(settings::setWindowShortcuts(DEFAULT_WINDOW_SHORTCUTS) == ERROR_SUCCESS, "initialize isolated settings");
    PROPSHEETPAGEW definition = {}; definition.dwSize = sizeof(definition); definition.hInstance = GetModuleHandleW(nullptr);
    definition.pszTemplate = MAKEINTRESOURCEW(IDD_SETTINGS_SHORTCUTS); definition.pfnDlgProc = shortcutsProc;
    PROPSHEETHEADERW header = {}; header.dwSize = sizeof(header); header.dwFlags = PSH_MODELESS | PSH_PROPSHEETPAGE;
    header.hInstance = definition.hInstance; header.pszCaption = L"FileSpacer shortcut matrix"; header.nPages = 1; header.ppsp = &definition;
    sheet = reinterpret_cast<HWND>(PropertySheetW(&header));
    require(sheet != nullptr && sheet != reinterpret_cast<HWND>(static_cast<INT_PTR>(-1)), "create production shortcut property page");
    page = PropSheet_GetCurrentPageHwnd(sheet); pump();
    editor = FindWindowExW(page, nullptr, L"#32770", nullptr);
    require(editor != nullptr && !IsWindowVisible(editor), "one initially collapsed shared editor");
    require(GetDlgItem(sheet, 0x3021) != nullptr, "native property sheet Apply button exists");
    require(GetWindowTextLengthW(GetDlgItem(page, IDC_SHORTCUT_ENABLE_MOVE)) == 0, "no visible checkbox caption");
    for (int index = 0; index < 2; ++index) {
        IAccessible *accessible = nullptr;
        require(SUCCEEDED(AccessibleObjectFromWindow(GetDlgItem(page, IDC_SHORTCUT_ENABLE_MOVE + index),
            static_cast<DWORD>(OBJID_CLIENT), IID_PPV_ARGS(&accessible))), "native checkbox accessibility");
        VARIANT self = {}; self.vt = VT_I4; self.lVal = CHILDID_SELF;
        BSTR name = nullptr;
        const HRESULT named = accessible->get_accName(self, &name);
        const auto expected = formatString(IDS_SHORTCUT_ACCESSIBLE, getString(index == 0 ? IDS_SHORTCUT_MOVE : IDS_SHORTCUT_RESIZE));
        require(SUCCEEDED(named) && name && std::wstring(name) == expected.get(), "blank checkbox retains localized accessible name");
        SysFreeString(name); accessible->Release();
    }
    // Bulk combinations use the real page offscreen; the smaller interaction matrix stays visible.
    ShowWindow(sheet, SW_HIDE); registryAndPageMatrix(); ShowWindow(sheet, SW_SHOWNOACTIVATE);
    pump(); interactionMatrix(); engineMatrix();
    require(notifications > 0, "successful Apply sends normal preference notifications");
    CoUninitialize(); std::printf("PASS: %d checks (%s)\n", checks, argv[1]); return 0;
}
