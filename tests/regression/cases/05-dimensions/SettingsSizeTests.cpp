#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>
#include <limits>

#include "Settings.h"
#include "SettingsDialog.h"
#include "UIStrings.h"
#include <commctrl.h>
#include <prsht.h>

namespace {
SIZE stored = {400, 500};
bool options[8] = {true, true, true, true, true, true, true, true};
int writes = 0, notifications = 0, testErrorCount = 0, cases = 0;
std::wstring lastError;
void require(bool condition, const char *description) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); std::abort(); }
}
}
namespace filespacer {
namespace settings {
SIZE getFolderWindowSize() { return stored; }
void setFolderWindowSize(SIZE value) { stored = value; ++writes; }
#define TEST_OPTION(name, index) \
    bool get##name() { return options[index]; } \
    void set##name(bool value) { options[index] = value; ++writes; }
TEST_OPTION(StatusTextEnabled, 0)
TEST_OPTION(ToolbarEnabled, 1)
TEST_OPTION(QuickAccessEnabled, 2)
TEST_OPTION(PathBarEnabled, 3)
TEST_OPTION(RefreshButtonEnabled, 4)
TEST_OPTION(UpButtonEnabled, 5)
TEST_OPTION(ViewButtonEnabled, 6)
TEST_OPTION(FullNamesOnSelection, 7)
#undef TEST_OPTION
}
void notifyWindowSettingsChanged() { ++notifications; }
}

HRESULT CALLBACK dismissError(HWND dialog, UINT message, WPARAM, LPARAM, LONG_PTR) {
    if (message == TDN_CREATED) PostMessageW(dialog, TDM_CLICK_BUTTON, IDOK, 0);
    return S_OK;
}
HRESULT capturedTaskDialog(HWND owner, HINSTANCE instance, LPCWSTR title, LPCWSTR instruction,
        LPCWSTR content, UINT buttons, LPCWSTR icon, int *button) {
    ++testErrorCount;
    lastError = content;
    TASKDIALOGCONFIG config = {sizeof(config)};
    config.hwndParent = owner; config.hInstance = instance; config.pszWindowTitle = title;
    config.pszMainInstruction = instruction; config.pszContent = content;
    config.dwCommonButtons = static_cast<TASKDIALOG_COMMON_BUTTON_FLAGS>(buttons);
    config.pszMainIcon = icon; config.pfCallback = dismissError;
    return TaskDialogIndirect(&config, button, nullptr, nullptr);
}
#define TaskDialog capturedTaskDialog
namespace filespacer {
#include "SettingsDisplayProduction.inc"
}
#undef TaskDialog
using namespace filespacer;

HWND sheet = nullptr;
HWND displayPage = nullptr;
void pump() {
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (!IsDialogMessageW(sheet, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
}
void setText(int id, const wchar_t *text) { require(SetDlgItemText(displayPage, id, text) != FALSE, "set edit text"); }
std::wstring getText(int id) {
    HWND edit = GetDlgItem(displayPage, id);
    std::wstring text(static_cast<size_t>(GetWindowTextLength(edit)) + 1, L'\0');
    text.resize(GetWindowText(edit, &text[0], static_cast<int>(text.size()))); return text;
}
LONG_PTR notify(UINT code) {
    PSHNOTIFY notification = {};
    notification.hdr.code = code;
    notification.hdr.hwndFrom = sheet;
    require(displayProc(displayPage, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notification)) == TRUE,
        "notification handled");
    const LONG_PTR result = GetWindowLongPtrW(displayPage, DWLP_MSGRESULT);
    // Capture the notification result before dispatching unrelated dialog messages.
    pump();
    return result;
}
void reset() {
    stored = {400, 500};
    for (auto &option : options) option = true;
    writes = notifications = testErrorCount = 0;
    lastError.clear();
    displayProc(displayPage, WM_INITDIALOG, 0, 0);
    pump();
}
void invalid(int control, const wchar_t *text, UINT event = PSN_APPLY) {
    reset();
    CheckDlgButton(displayPage, IDC_STATUS_TEXT_ENABLED, BST_UNCHECKED);
    setText(control, text);
    require(notify(event) == (event == PSN_APPLY ? PSNRET_INVALID : TRUE), "invalid submission refused");
    require(testErrorCount == 1 && !lastError.empty(), "one explanatory error");
    require(writes == 0 && notifications == 0, "invalid submission writes no options");
    require(getText(control) == (control == IDC_FOLDER_WINDOW_WIDTH ? L"400" : L"500"),
        "restore previous stored dimension");
    HWND edit = GetDlgItem(displayPage, control);
    require(GetFocus() == edit, "focus returns to offending field");
    DWORD start = 0, end = 0;
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    require(start == 0 && end == getText(control).size(), "entire restored text selected");
    ++cases;
}
int main() {
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    require(InitCommonControlsEx(&controls) != FALSE, "initialize native controls");
    PROPSHEETPAGEW pageDefinition = {};
    pageDefinition.dwSize = sizeof(pageDefinition);
    pageDefinition.hInstance = GetModuleHandleW(nullptr);
    pageDefinition.pszTemplate = MAKEINTRESOURCEW(IDD_SETTINGS_DISPLAY);
    pageDefinition.pfnDlgProc = displayProc;
    PROPSHEETHEADERW header = {};
    header.dwSize = sizeof(header); header.dwFlags = PSH_MODELESS | PSH_PROPSHEETPAGE;
    header.hInstance = GetModuleHandleW(nullptr); header.pszCaption = L"FileSpacer settings size tests";
    header.nPages = 1; header.ppsp = &pageDefinition;
    sheet = reinterpret_cast<HWND>(PropertySheetW(&header));
    require(sheet != nullptr && sheet != reinterpret_cast<HWND>(static_cast<intptr_t>(-1)), "create native property sheet");
    displayPage = PropSheet_GetCurrentPageHwnd(sheet);
    require(displayPage != nullptr, "obtain native display page");
    pump();
    const SIZE minimum = minimumFolderWindowSize();
    require(minimum.cx > 0 && minimum.cy > 0, "native minima are positive");
    reset();
    require(getText(IDC_FOLDER_WINDOW_WIDTH_LABEL).find(std::to_wstring(minimum.cx)) != std::wstring::npos,
        "width label includes actual minimum");
    require(getText(IDC_FOLDER_WINDOW_HEIGHT_LABEL).find(std::to_wstring(minimum.cy)) != std::wstring::npos,
        "height label includes actual minimum");
    ++cases;
    for (int id : {IDC_FOLDER_WINDOW_WIDTH_LABEL, IDC_FOLDER_WINDOW_HEIGHT_LABEL}) {
        HWND label = GetDlgItem(displayPage, id);
        std::wstring text = getText(id);
        const size_t mnemonic = text.find(L'&');
        if (mnemonic != std::wstring::npos) text.erase(mnemonic, 1);
        HDC dc = GetDC(label);
        require(dc != nullptr, "obtain label DC");
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(label, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE extent = {};
        require(GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &extent) != FALSE,
            "measure label in its actual dialog font");
        RECT bounds = {};
        GetClientRect(label, &bounds);
        require(extent.cx <= bounds.right - bounds.left, "minimum label fits without clipping");
        SelectObject(dc, oldFont); ReleaseDC(label, dc);
    }
    const wchar_t *bad[] = {L"", L"0", L"-1", L"abc", L"1234px", L"2147483648", L"99999999999999999999999", L"12.5", L" 400"};
    for (const wchar_t *text : bad) { invalid(IDC_FOLDER_WINDOW_WIDTH, text); invalid(IDC_FOLDER_WINDOW_HEIGHT, text); }
    std::wstring belowWidth = std::to_wstring(minimum.cx - 1);
    std::wstring belowHeight = std::to_wstring(minimum.cy - 1);
    invalid(IDC_FOLDER_WINDOW_WIDTH, belowWidth.c_str());
    invalid(IDC_FOLDER_WINDOW_HEIGHT, belowHeight.c_str());
    invalid(IDC_FOLDER_WINDOW_WIDTH, L"0", PSN_KILLACTIVE);
    invalid(IDC_FOLDER_WINDOW_HEIGHT, L"0", PSN_KILLACTIVE);
    reset();
    setText(IDC_FOLDER_WINDOW_WIDTH, std::to_wstring(minimum.cx).c_str());
    setText(IDC_FOLDER_WINDOW_HEIGHT, std::to_wstring(minimum.cy).c_str());
    require(notify(PSN_KILLACTIVE) == FALSE && writes == 0, "valid leaving does not save yet");
    require(notify(PSN_APPLY) == PSNRET_NOERROR && testErrorCount == 0, "exact minima accepted");
    require(stored.cx == minimum.cx && stored.cy == minimum.cy && writes == 9, "valid dimensions and options saved");
    ++cases;
    reset();
    CheckDlgButton(displayPage, IDC_FULL_NAMES_ON_SELECTION, BST_UNCHECKED);
    require(notify(PSN_APPLY) == PSNRET_NOERROR && !options[7] && notifications == 1,
        "changing only selected-name rendering notifies existing windows");
    require(notify(PSN_APPLY) == PSNRET_NOERROR && notifications == 1,
        "unchanged selected-name rendering does not notify twice");
    ++cases;
    reset();
    setText(IDC_FOLDER_WINDOW_WIDTH, L"640"); setText(IDC_FOLDER_WINDOW_HEIGHT, L"480");
    CheckDlgButton(displayPage, IDC_STATUS_TEXT_ENABLED, BST_UNCHECKED);
    require(notify(PSN_APPLY) == PSNRET_NOERROR && notifications == 1, "valid options update existing windows");
    require(stored.cx == 640 && stored.cy == 480 && !options[0], "ordinary positive values accepted");
    setText(IDC_FOLDER_WINDOW_WIDTH, L"0");
    require(notify(PSN_APPLY) == PSNRET_INVALID && getText(IDC_FOLDER_WINDOW_WIDTH) == L"640",
        "restore latest applied value, not opening value");
    ++cases;
    reset();
    setText(IDC_FOLDER_WINDOW_WIDTH, L"0640"); setText(IDC_FOLDER_WINDOW_HEIGHT, L"0480");
    require(notify(PSN_APPLY) == PSNRET_NOERROR && stored.cx == 640 && stored.cy == 480,
        "decimal leading zeros accepted");
    ++cases;
    reset();
    setText(IDC_FOLDER_WINDOW_WIDTH, L"0");
    require(SendMessageW(sheet, PSM_APPLY, 0, 0) == FALSE, "real property sheet refuses Apply");
    pump();
    require(writes == 0 && testErrorCount == 1 && getText(IDC_FOLDER_WINDOW_WIDTH) == L"400",
        "real Apply restores invalid width without saving");
    require(GetFocus() == GetDlgItem(displayPage, IDC_FOLDER_WINDOW_WIDTH),
        "property sheet does not steal restored field focus");
    ++cases;
    reset();
    setText(IDC_FOLDER_WINDOW_WIDTH, L"0"); setText(IDC_FOLDER_WINDOW_HEIGHT, L"0");
    require(notify(PSN_APPLY) == PSNRET_INVALID && testErrorCount == 1 && getText(IDC_FOLDER_WINDOW_HEIGHT) == L"0",
        "two invalid fields: restore first only");
    require(notify(PSN_APPLY) == PSNRET_INVALID && testErrorCount == 2 && getText(IDC_FOLDER_WINDOW_HEIGHT) == L"500",
        "next attempt restores second field without saving");
    require(writes == 0, "no partial dimensions saved");
    ++cases;
    DestroyWindow(sheet);
    std::printf("PASS: %d settings dimension cases.\n", cases);
}
