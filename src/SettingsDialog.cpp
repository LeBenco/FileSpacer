#include "SettingsDialog.h"
#include "Settings.h"
#include "Everything.h"
#include "FolderStateStore.h"
#include "CreateItemWindow.h"
#include "ClosedWindows.h"
#include "Update.h"
#include "UpdateInstaller.h"
#include "main.h"
#include "WinUtils.h"
#include "UIStrings.h"
#include "DPI.h"
#include <atlbase.h>
#include <prsht.h>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <uxtheme.h>
#include <vsstyle.h>
#pragma warning(push)
#pragma warning(disable: 4458) // Shadowed members in Windows SDK GDI+ headers.
#include <gdiplus.h>
#pragma warning(pop)
#include <string>
#include <type_traits>
#include <vector>

namespace filespacer {

const wchar_t *SPECIAL_PATHS[] = {
    L"shell:Desktop",
    L"shell:MyComputerFolder",
    L"shell:Links",
    L"shell:Recent",
    L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}" // Quick access
};

static HWND settingsDialog = nullptr;
static bool resetFolderStateRequested = false;

static bool confirmResetAllFolderState(HWND owner) {
    TASKDIALOGCONFIG config = {sizeof(config)};
    config.hwndParent = owner;
    config.hInstance = GetModuleHandle(nullptr);
    config.pszWindowTitle = MAKEINTRESOURCE(IDS_CONFIRM_CAPTION);
    config.pszMainInstruction = MAKEINTRESOURCE(IDS_RESET_FOLDER_STATE_CONFIRM);
    config.pszMainIcon = TD_WARNING_ICON;
    config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    config.nDefaultButton = IDNO;
    int button = IDNO;
    return checkHR(TaskDialogIndirect(&config, &button, nullptr, nullptr)) && button == IDYES;
}

static void resetAllFolderState() {
    int result = []() -> int {
        std::vector<HWND> windows;
        auto collect = [](HWND window, LPARAM data) -> BOOL {
            wchar_t className[64];
            if (GetClassName(window, className, _countof(className))
                    && lstrcmp(className, L"FileSpacer Item Window") == 0)
                reinterpret_cast<std::vector<HWND> *>(data)->push_back(window);
            return TRUE;
        };
        if (!EnumWindows(collect, reinterpret_cast<LPARAM>(&windows)))
            return IDS_RESET_FOLDER_STATE_CLOSE_ERROR;

        // Close folder windows, leaving their auxiliary taskbar owners to normal destruction.
        // No folder identities or pointers from another process are needed.
        for (HWND window : windows) {
            wchar_t className[64];
            // Closing other windows can destroy an HWND in this snapshot.
            if (!GetClassName(window, className, _countof(className))
                    || lstrcmp(className, L"FileSpacer Item Window") != 0)
                continue;
            DWORD_PTR messageResult = 0;
            SendMessageTimeout(window, WM_CLOSE, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 5000, &messageResult);
            if (IsWindow(window)) // cancelled, inaccessible or unresponsive
                return IDS_RESET_FOLDER_STATE_CLOSE_ERROR;
        }
        windows.clear();
        if (!EnumWindows(collect, reinterpret_cast<LPARAM>(&windows))
                || !windows.empty())
            return IDS_RESET_FOLDER_STATE_CLOSE_ERROR;

        // All folder windows have finished their normal saves. Delete rows atomically,
        // keeping global application preferences and the old Shell bags untouched.
        auto store = folderStateStore();
        if (!store) return IDS_RESET_FOLDER_STATE_STORAGE_ERROR;
        if (!store->clearAll()) return IDS_RESET_FOLDER_STATE_DELETE_ERROR;
        refreshReopenJumpLists();
        return IDS_RESET_FOLDER_STATE_SUCCESS;
    }();
    bool success = result == IDS_RESET_FOLDER_STATE_SUCCESS;
    TaskDialog(nullptr, GetModuleHandle(nullptr),
        MAKEINTRESOURCE(success ? IDS_SETTINGS_CAPTION : IDS_ERROR_CAPTION),
        MAKEINTRESOURCE(result), nullptr, TDCBF_OK_BUTTON,
        success ? TD_INFORMATION_ICON : TD_ERROR_ICON, nullptr);
}

bool chooseFolder(HWND owner, CComHeapPtr<wchar_t> &pathOut) {
    CComPtr<IFileOpenDialog> openDialog;
    if (!checkHR(openDialog.CoCreateInstance(__uuidof(FileOpenDialog))))
        return false;
    FILEOPENDIALOGOPTIONS opts;
    if (!checkHR(openDialog->GetOptions(&opts)))
        return false;
    if (!checkHR(openDialog->SetOptions(opts | FOS_PICKFOLDERS | FOS_ALLNONSTORAGEITEMS)))
        return false;
    if (!checkHR(openDialog->Show(GetParent(owner))))
        return false;
    CComPtr<IShellItem> item;
    if (!checkHR(openDialog->GetResult(&item)))
        return false;
    return checkHR(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pathOut));
}

void setCBPath(HWND hwnd, const wchar_t *path) {
    COMBOBOXEXITEM item = {CBEIF_TEXT, -1, (wchar_t *)path};
    SendMessage(hwnd, CBEM_SETITEM, 0, (LPARAM)&item);
}

LRESULT CALLBACK pathCBProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR, DWORD_PTR) {
    switch (message) {
        case WM_DROPFILES: {
            UINT pathLength = DragQueryFile((HDROP)wParam, 0, nullptr, 0);
            std::vector<wchar_t> path(static_cast<size_t>(pathLength) + 1);
            if (DragQueryFile((HDROP)wParam, 0, path.data(), pathLength + 1)) {
                setCBPath(hwnd, path.data());
                SendMessage(GetParent(hwnd), WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(hwnd), CBN_SELCHANGE), (LPARAM)hwnd);
            }
            DragFinish((HDROP)wParam);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void setupPathCB(HWND hwnd, const wchar_t *path) {
    checkHR(SHAutoComplete((HWND)SendMessage(hwnd, CBEM_GETEDITCONTROL, 0, 0), SHACF_FILESYS_DIRS));
    SetWindowSubclass(hwnd, pathCBProc, 0, 0);
    DragAcceptFiles(hwnd, TRUE); // TODO: this doesn't work between processes!
    setCBPath(hwnd, path);
}

bool pathCBChanged(WPARAM wParam, LPARAM lParam) {
    return HIWORD(wParam) == CBN_EDITCHANGE && SendMessage((HWND)lParam, CBEM_HASEDITCHANGED, 0, 0)
        || HIWORD(wParam) == CBN_SELCHANGE;
}

void notifyWindowSettingsChanged() {
    if (ItemWindow::settingsChangedMessage) {
        auto notify = [](HWND window, LPARAM historyLimit) -> BOOL {
            LPARAM testNamespace = 0;
#ifdef FILESPACER_DEBUG
            testNamespace = settings::testMode ? 1 : 0;
#endif
            wchar_t className[64];
            if (GetClassName(window, className, _countof(className))
                    && lstrcmp(className, L"FileSpacer Item Window") == 0)
                checkLE(SendNotifyMessage(window, ItemWindow::settingsChangedMessage,
                    static_cast<WPARAM>(historyLimit), testNamespace));
            return TRUE;
        };
        // Include the applied limit: another process can receive this after a
        // later preference change, and must still discard a cleared/trimmed cache.
        checkLE(EnumWindows(notify, settings::getSearchHistoryLimit() + 1));
    }
}

static bool searchHistoryLimit(HWND hwnd, DWORD *value);

struct GeneralHelp {
    struct WindowDeleter { void operator()(HWND window) const { DestroyWindow(window); } };
    std::unique_ptr<std::remove_pointer<HWND>::type, WindowDeleter> tooltip;
    TOOLINFOW tool = {sizeof(TOOLINFOW)};
    int activeControl = 0;
    Gdiplus::GraphicsPath infoGlyph;
    Gdiplus::RectF infoBounds;

    explicit GeneralHelp(HWND page) {
        layout(page);
        for (int id : {IDC_SMART_WINDOW_PLACEMENT_HELP, IDC_GROUP_FOLDER_WINDOWS_HELP})
            checkLE(SetWindowSubclass(GetDlgItem(page, id), buttonProc, 0, 0));
        tooltip.reset(checkLE(CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_NOPREFIX | TTS_BALLOON | TTS_CLOSE,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            GetParent(page), nullptr, GetModuleHandleW(nullptr), nullptr)));
        if (!tooltip) return;
        if (!checkLE(SetWindowSubclass(tooltip.get(), tooltipProc, 0, reinterpret_cast<DWORD_PTR>(this)))) {
            tooltip.reset();
            return;
        }
        tool.uFlags = TTF_TRACK;
        tool.hwnd = page;
        tool.uId = 1;
        tool.lpszText = const_cast<wchar_t *>(getString(IDS_SMART_WINDOW_PLACEMENT_HELP));
        checkLE(SendMessageW(tooltip.get(), TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool)));
        SendMessageW(tooltip.get(), TTM_SETMAXTIPWIDTH, 0, scaleDPI(360));
    }
    ~GeneralHelp() {
        if (tooltip) RemoveWindowSubclass(tooltip.get(), tooltipProc, 0);
    }
    static LRESULT CALLBACK tooltipProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
            UINT_PTR id, DWORD_PTR data) {
        if (message == WM_NCDESTROY) {
            // Windows can destroy the owned popup before its property page.
            auto help = reinterpret_cast<GeneralHelp *>(data);
            help->tooltip.release();
            RemoveWindowSubclass(window, tooltipProc, id);
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }
    static LRESULT CALLBACK buttonProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
            UINT_PTR id, DWORD_PTR) {
        if (message == WM_KILLFOCUS) {
            auto help = reinterpret_cast<GeneralHelp *>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
            if (help) help->hide();
        }
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, buttonProc, id);
        return DefSubclassProc(window, message, wParam, lParam);
    }
    void layout(HWND page) {
        auto releaseDC = [page](HDC context) { ReleaseDC(page, context); };
        std::unique_ptr<std::remove_pointer<HDC>::type, decltype(releaseDC)> context(GetDC(page), releaseDC);
        if (!context) return;
        const HFONT font = reinterpret_cast<HFONT>(SendMessageW(page, WM_GETFONT, 0, 0));
        const HGDIOBJ previous = SelectObject(context.get(), font);
        TEXTMETRICW metrics = {};
        checkLE(GetTextMetricsW(context.get(), &metrics));
        // Use Microsoft's Info glyph at the dialog's text size, with the Windows 10 font as fallback.
        Gdiplus::FontFamily fluent(L"Segoe Fluent Icons"), mdl2(L"Segoe MDL2 Assets");
        const Gdiplus::FontFamily *family = fluent.GetLastStatus() == Gdiplus::Ok ? &fluent : &mdl2;
        infoGlyph.Reset();
        infoBounds = Gdiplus::RectF();
        if (infoGlyph.AddString(L"\uE946", 1, family, Gdiplus::FontStyleRegular,
                static_cast<Gdiplus::REAL>(metrics.tmHeight), Gdiplus::PointF(0, 0), nullptr) == Gdiplus::Ok)
            infoGlyph.GetBounds(&infoBounds);
        const int labels[] = {IDC_SMART_WINDOW_PLACEMENT, IDC_GROUP_FOLDER_WINDOWS};
        const int buttons[] = {IDC_SMART_WINDOW_PLACEMENT_HELP, IDC_GROUP_FOLDER_WINDOWS_HELP};
        for (int index = 0; index < _countof(labels); ++index) {
            const HWND label = GetDlgItem(page, labels[index]);
            const int length = GetWindowTextLengthW(label);
            std::wstring text(static_cast<size_t>(length) + 1, L'\0');
            GetWindowTextW(label, &text[0], length + 1);
            RECT extent = {};
            DrawTextW(context.get(), text.c_str(), length, &extent, DT_CALCRECT | DT_SINGLELINE);
            SIZE check = {GetSystemMetrics(SM_CXMENUCHECK), GetSystemMetrics(SM_CYMENUCHECK)};
            std::unique_ptr<std::remove_pointer<HTHEME>::type, decltype(&CloseThemeData)> theme(
                OpenThemeData(label, L"Button"), CloseThemeData);
            if (theme) {
                SIZE themed = {};
                if (SUCCEEDED(GetThemePartSize(theme.get(), context.get(), BP_CHECKBOX,
                        CBS_UNCHECKEDNORMAL, nullptr, TS_TRUE, &themed))) check = themed;
            }
            RECT bounds = windowRect(label);
            MapWindowPoints(nullptr, page, reinterpret_cast<POINT *>(&bounds), 2);
            const int labelWidth = check.cx + scaleDPI(3) + extent.right;
            const int size = scaleDPI(20);
            const int padding = static_cast<int>((size - infoBounds.Width) / 2 + 0.5f);
            checkLE(SetWindowPos(label, nullptr, 0, 0, labelWidth, bounds.bottom - bounds.top,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
            checkLE(SetWindowPos(GetDlgItem(page, buttons[index]), HWND_TOP,
                bounds.left + labelWidth + scaleDPI(4) - padding,
                bounds.top + (metrics.tmHeight - size) / 2, size, size, SWP_NOACTIVATE));
        }
        SelectObject(context.get(), previous);
    }
    bool draw(const DRAWITEMSTRUCT &item) const {
        if (item.CtlType != ODT_BUTTON || (item.CtlID != IDC_SMART_WINDOW_PLACEMENT_HELP
                && item.CtlID != IDC_GROUP_FOLDER_WINDOWS_HELP)) return false;
        const int saved = SaveDC(item.hDC);
        if (!saved) return false;
        if (FAILED(DrawThemeParentBackground(item.hwndItem, item.hDC, &item.rcItem)))
            FillRect(item.hDC, &item.rcItem, GetSysColorBrush(COLOR_BTNFACE));
        {
            const COLORREF ink = GetSysColor(item.itemState & ODS_DISABLED ? COLOR_GRAYTEXT
                : item.itemState & ODS_SELECTED ? COLOR_HOTLIGHT : COLOR_BTNTEXT);
            Gdiplus::SolidBrush fill(Gdiplus::Color(255, GetRValue(ink), GetGValue(ink), GetBValue(ink)));
            const Gdiplus::REAL cx = static_cast<Gdiplus::REAL>(item.rcItem.left + item.rcItem.right) / 2;
            const Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(item.rcItem.top + item.rcItem.bottom) / 2;
            Gdiplus::Graphics graphics(item.hDC);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            // Center the original glyph's visible outline without changing its proportions.
            graphics.TranslateTransform(cx - infoBounds.X - infoBounds.Width / 2,
                cy - infoBounds.Y - infoBounds.Height / 2);
            graphics.FillPath(&fill, &infoGlyph);
        }
        if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT))
            DrawFocusRect(item.hDC, &item.rcItem);
        RestoreDC(item.hDC, saved);
        return true;
    }
    void hide() {
        if (tooltip) SendMessageW(tooltip.get(), TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&tool));
        activeControl = 0;
    }
    void toggle(int control) {
        if (!tooltip) return;
        const bool wasVisible = activeControl == control && IsWindowVisible(tooltip.get());
        hide();
        if (wasVisible) return;
        const bool placement = control == IDC_SMART_WINDOW_PLACEMENT_HELP;
        tool.lpszText = const_cast<wchar_t *>(getString(placement
            ? IDS_SMART_WINDOW_PLACEMENT_HELP : IDS_GROUP_FOLDER_WINDOWS_HELP));
        SendMessageW(tooltip.get(), TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tool));
        SendMessageW(tooltip.get(), TTM_SETTITLEW, TTI_NONE, reinterpret_cast<LPARAM>(getString(placement
            ? IDS_SMART_WINDOW_PLACEMENT_HELP_TITLE : IDS_GROUP_FOLDER_WINDOWS_HELP_TITLE)));
        const RECT anchor = windowRect(GetDlgItem(tool.hwnd, control));
        SendMessageW(tooltip.get(), TTM_TRACKPOSITION, 0,
            MAKELPARAM((anchor.left + anchor.right) / 2, anchor.bottom));
        activeControl = control;
        SendMessageW(tooltip.get(), TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tool));
    }
};

INT_PTR CALLBACK generalProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto help = reinterpret_cast<GeneralHelp *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_INITDIALOG: {
            auto state = std::make_unique<GeneralHelp>(hwnd);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state.release()));
            for (int i = 0; i < _countof(SPECIAL_PATHS); i++) {
                COMBOBOXEXITEM item = {CBEIF_TEXT, -1, (wchar_t *)SPECIAL_PATHS[i]};
                SendDlgItemMessage(hwnd, IDC_START_FOLDER_PATH, CBEM_INSERTITEM, 0, (LPARAM)&item);
            }
            setupPathCB(GetDlgItem(hwnd, IDC_START_FOLDER_PATH),
                settings::getStartingFolder().get());
            CheckDlgButton(hwnd, IDC_GROUP_FOLDER_WINDOWS,
                settings::getGroupFolderWindows() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_KEEP_SOURCE_WINDOW_OPEN,
                settings::getKeepSourceWindowOpen() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_SMART_WINDOW_PLACEMENT,
                settings::getSmartWindowPlacement() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_KEEP_SELECTION_ON_ACTIVATE,
                settings::getKeepSelectionOnActivate() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_WINDOW_STATE_LOCK_ENABLED,
                settings::getWindowStateLockEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_WINDOW_STATE_LOCK_BADGE_ENABLED,
                settings::getWindowStateLockBadgeEnabled() ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(hwnd, IDC_WINDOW_STATE_LOCK_BADGE_ENABLED),
                !!IsDlgButtonChecked(hwnd, IDC_WINDOW_STATE_LOCK_ENABLED));
            CheckDlgButton(hwnd, IDC_LIVE_NAME_SEARCH,
                settings::getLiveNameSearch() ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemInt(hwnd, IDC_SEARCH_HISTORY_LIMIT, settings::getSearchHistoryLimit(), FALSE);
            return TRUE;
        }
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (help && notif->hwndFrom == help->tooltip.get() && notif->code == TTN_POP) {
                help->activeControl = 0;
                return TRUE;
            }
            if (notif->code == PSN_KILLACTIVE) {
                if (help) help->hide();
                DWORD limit = 0;
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, !searchHistoryLimit(hwnd, &limit));
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
                if (help) help->hide();
                DWORD limit = 0;
                if (!searchHistoryLimit(hwnd, &limit)) {
                    SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                    return TRUE;
                }
                const bool lockEnabled = !!IsDlgButtonChecked(hwnd, IDC_WINDOW_STATE_LOCK_ENABLED);
                if (lockEnabled != settings::getWindowStateLockEnabled()) {
                    if (!lockEnabled) {
                        TASKDIALOGCONFIG config = {sizeof(config)};
                        config.hwndParent = GetParent(hwnd);
                        config.hInstance = GetModuleHandle(nullptr);
                        config.pszWindowTitle = MAKEINTRESOURCE(IDS_CONFIRM_CAPTION);
                        config.pszMainInstruction = MAKEINTRESOURCE(IDS_DISABLE_STATE_LOCK_CONFIRM);
                        config.pszMainIcon = TD_WARNING_ICON;
                        config.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
                        config.nDefaultButton = IDCANCEL;
                        int button = IDCANCEL;
                        if (!checkHR(TaskDialogIndirect(&config, &button, nullptr, nullptr)) || button != IDOK) {
                            SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                            return TRUE;
                        }
                    }
                    if (!settings::setWindowStateLockEnabled(lockEnabled)) {
                        reportFolderStateError(GetParent(hwnd));
                        SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                        return TRUE;
                    }
                    notifyWindowSettingsChanged();
                }
                const bool historyChanged = limit != settings::getSearchHistoryLimit();
                if (!settings::setSearchHistoryLimit(limit)) {
                    reportFolderStateError(GetParent(hwnd));
                    SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                    return TRUE;
                }
                int pathLength = GetWindowTextLength(GetDlgItem(hwnd, IDC_START_FOLDER_PATH));
                std::vector<wchar_t> startingFolder(static_cast<size_t>(pathLength) + 1);
                if (GetDlgItemText(hwnd, IDC_START_FOLDER_PATH,
                        startingFolder.data(), pathLength + 1))
                    settings::setStartingFolder(startingFolder.data());
                bool groupFolderWindows = !!IsDlgButtonChecked(hwnd, IDC_GROUP_FOLDER_WINDOWS);
                bool liveNameSearch = !!IsDlgButtonChecked(hwnd, IDC_LIVE_NAME_SEARCH);
                const bool smartPlacement = !!IsDlgButtonChecked(hwnd, IDC_SMART_WINDOW_PLACEMENT);
                const bool lockBadgeEnabled = !!IsDlgButtonChecked(hwnd, IDC_WINDOW_STATE_LOCK_BADGE_ENABLED);
                bool windowSettingsChanged = historyChanged
                    || liveNameSearch != settings::getLiveNameSearch()
                    || smartPlacement != settings::getSmartWindowPlacement()
                    || lockBadgeEnabled != settings::getWindowStateLockBadgeEnabled();
                settings::setGroupFolderWindows(groupFolderWindows);
                settings::setKeepSourceWindowOpen(!!IsDlgButtonChecked(hwnd, IDC_KEEP_SOURCE_WINDOW_OPEN));
                settings::setSmartWindowPlacement(smartPlacement);
                settings::setKeepSelectionOnActivate(
                    !!IsDlgButtonChecked(hwnd, IDC_KEEP_SELECTION_ON_ACTIVATE));
                settings::setLiveNameSearch(liveNameSearch);
                settings::setWindowStateLockBadgeEnabled(lockBadgeEnabled);
                if (windowSettingsChanged)
                    notifyWindowSettingsChanged();
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            } else if (notif->code == PSN_HELP) {
#if 0 // Deferred public help.
                ShellExecute(nullptr, L"open",
                    L"https://github.com/vanjac/chromafiler/wiki/Settings#general",
                    nullptr, nullptr, SW_SHOWNORMAL);
#endif
                return TRUE;
            }
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SMART_WINDOW_PLACEMENT_HELP
                    || LOWORD(wParam) == IDC_GROUP_FOLDER_WINDOWS_HELP) {
                if (help && HIWORD(wParam) == BN_CLICKED) help->toggle(LOWORD(wParam));
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_START_FOLDER_BROWSE && HIWORD(wParam) == BN_CLICKED) {
                CComHeapPtr<wchar_t> selected;
                if (chooseFolder(GetParent(hwnd), selected)) {
                    setCBPath(GetDlgItem(hwnd, IDC_START_FOLDER_PATH), selected);
                    PropSheet_Changed(GetParent(hwnd), hwnd);
                }
                return TRUE;
            } else if (HIWORD(wParam) == EN_CHANGE
                    || LOWORD(wParam) == IDC_START_FOLDER_PATH && pathCBChanged(wParam, lParam)
                    || LOWORD(wParam) == IDC_GROUP_FOLDER_WINDOWS && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_KEEP_SOURCE_WINDOW_OPEN && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_SMART_WINDOW_PLACEMENT && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_WINDOW_STATE_LOCK_ENABLED && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_WINDOW_STATE_LOCK_BADGE_ENABLED && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_LIVE_NAME_SEARCH && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_KEEP_SELECTION_ON_ACTIVATE && HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == IDC_WINDOW_STATE_LOCK_ENABLED && HIWORD(wParam) == BN_CLICKED)
                    EnableWindow(GetDlgItem(hwnd, IDC_WINDOW_STATE_LOCK_BADGE_ENABLED),
                        !!IsDlgButtonChecked(hwnd, IDC_WINDOW_STATE_LOCK_ENABLED));
                PropSheet_Changed(GetParent(hwnd), hwnd);
                return TRUE;
            }
            return FALSE;
        case WM_DRAWITEM:
            return help && help->draw(*reinterpret_cast<DRAWITEMSTRUCT *>(lParam));
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
            if (help) { help->hide(); help->layout(hwnd); }
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return FALSE;
        case WM_DESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete help;
            return TRUE;
        default:
            return FALSE;
    }
}

void enableToolbarOptions(HWND hwnd) {
    const bool enabled = !!IsDlgButtonChecked(hwnd, IDC_TOOLBAR_ENABLED);
    const int controls[] = {IDC_QUICK_ACCESS_ENABLED, IDC_PATH_BAR_ENABLED,
        IDC_REFRESH_BUTTON_ENABLED, IDC_UP_BUTTON_ENABLED, IDC_VIEW_BUTTON_ENABLED};
    for (int control : controls)
        EnableWindow(GetDlgItem(hwnd, control), enabled);
}

static SIZE minimumFolderWindowSize() {
    // Preferences store the complete window size in pixels at 96 DPI.
    return {GetSystemMetricsForDpi(SM_CXMINTRACK, USER_DEFAULT_SCREEN_DPI),
        GetSystemMetricsForDpi(SM_CYMINTRACK, USER_DEFAULT_SCREEN_DPI)};
}

static bool readFolderWindowSize(HWND hwnd, SIZE &size) {
    const SIZE minimum = minimumFolderWindowSize();
    const int controls[] = {IDC_FOLDER_WINDOW_WIDTH, IDC_FOLDER_WINDOW_HEIGHT};
    const LONG minima[] = {minimum.cx, minimum.cy};
    LONG *values[] = {&size.cx, &size.cy};
    const UINT errors[] = {IDS_FOLDER_WIDTH_INVALID, IDS_FOLDER_HEIGHT_INVALID};
    for (int index = 0; index < 2; ++index) {
        HWND edit = GetDlgItem(hwnd, controls[index]);
        const int length = GetWindowTextLength(edit);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        text.resize(GetWindowText(edit, &text[0], length + 1));
        BOOL translated = FALSE;
        const LONG value = static_cast<int>(GetDlgItemInt(hwnd, controls[index], &translated, TRUE));
        // ES_NUMBER allows non-digit paste; GetDlgItemInt accepts numeric prefixes.
        if (translated && !text.empty()
                && text.find_first_not_of(L"0123456789") == std::wstring::npos
                && value > 0 && value >= minima[index]) {
            *values[index] = value;
            continue;
        }
        auto error = formatString(errors[index], minima[index]);
        TaskDialog(GetParent(hwnd), GetModuleHandle(nullptr), MAKEINTRESOURCE(IDS_ERROR_CAPTION),
            nullptr, error.get(), TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
        const SIZE previous = settings::getFolderWindowSize();
        SetDlgItemInt(hwnd, controls[index], index == 0 ? previous.cx : previous.cy, TRUE);
        // Let the property sheet finish changing focus before selecting this edit.
        // WM_NEXTDLGCTL also selects the entire text of an edit control.
        PostMessage(hwnd, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(edit), TRUE);
        return false;
    }
    return true;
}

INT_PTR CALLBACK displayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            const SIZE minimum = minimumFolderWindowSize();
            auto widthLabel = formatString(IDS_FOLDER_WIDTH_LABEL, minimum.cx);
            auto heightLabel = formatString(IDS_FOLDER_HEIGHT_LABEL, minimum.cy);
            SetDlgItemText(hwnd, IDC_FOLDER_WINDOW_WIDTH_LABEL, widthLabel.get());
            SetDlgItemText(hwnd, IDC_FOLDER_WINDOW_HEIGHT_LABEL, heightLabel.get());
            SIZE size = settings::getFolderWindowSize();
            SetDlgItemInt(hwnd, IDC_FOLDER_WINDOW_WIDTH, size.cx, TRUE);
            SetDlgItemInt(hwnd, IDC_FOLDER_WINDOW_HEIGHT, size.cy, TRUE);
            CheckDlgButton(hwnd, IDC_STATUS_TEXT_ENABLED,
                settings::getStatusTextEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_TOOLBAR_ENABLED,
                settings::getToolbarEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_QUICK_ACCESS_ENABLED,
                settings::getQuickAccessEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_PATH_BAR_ENABLED,
                settings::getPathBarEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_REFRESH_BUTTON_ENABLED,
                settings::getRefreshButtonEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_UP_BUTTON_ENABLED,
                settings::getUpButtonEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_VIEW_BUTTON_ENABLED,
                settings::getViewButtonEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_FULL_NAMES_ON_SELECTION,
                settings::getFullNamesOnSelection() ? BST_CHECKED : BST_UNCHECKED);
            enableToolbarOptions(hwnd);
            return TRUE;
        }
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (notif->code == PSN_KILLACTIVE) {
                SIZE size = {};
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, !readFolderWindowSize(hwnd, size));
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
                SIZE size = {};
                if (!readFolderWindowSize(hwnd, size)) {
                    SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }
                settings::setFolderWindowSize(size);
                bool statusTextEnabled = !!IsDlgButtonChecked(hwnd, IDC_STATUS_TEXT_ENABLED);
                bool toolbarEnabled = !!IsDlgButtonChecked(hwnd, IDC_TOOLBAR_ENABLED);
                bool quickAccessEnabled = !!IsDlgButtonChecked(hwnd, IDC_QUICK_ACCESS_ENABLED);
                bool pathBarEnabled = !!IsDlgButtonChecked(hwnd, IDC_PATH_BAR_ENABLED);
                bool refreshButtonEnabled = !!IsDlgButtonChecked(hwnd, IDC_REFRESH_BUTTON_ENABLED);
                bool upButtonEnabled = !!IsDlgButtonChecked(hwnd, IDC_UP_BUTTON_ENABLED);
                bool viewButtonEnabled = !!IsDlgButtonChecked(hwnd, IDC_VIEW_BUTTON_ENABLED);
                bool fullNamesOnSelection = !!IsDlgButtonChecked(hwnd, IDC_FULL_NAMES_ON_SELECTION);
                bool windowSettingsChanged = statusTextEnabled != settings::getStatusTextEnabled()
                    || toolbarEnabled != settings::getToolbarEnabled()
                    || quickAccessEnabled != settings::getQuickAccessEnabled()
                    || pathBarEnabled != settings::getPathBarEnabled()
                    || refreshButtonEnabled != settings::getRefreshButtonEnabled()
                    || upButtonEnabled != settings::getUpButtonEnabled()
                    || viewButtonEnabled != settings::getViewButtonEnabled()
                    || fullNamesOnSelection != settings::getFullNamesOnSelection();
                settings::setStatusTextEnabled(statusTextEnabled);
                settings::setToolbarEnabled(toolbarEnabled);
                settings::setQuickAccessEnabled(quickAccessEnabled);
                settings::setPathBarEnabled(pathBarEnabled);
                settings::setRefreshButtonEnabled(refreshButtonEnabled);
                settings::setUpButtonEnabled(upButtonEnabled);
                settings::setViewButtonEnabled(viewButtonEnabled);
                settings::setFullNamesOnSelection(fullNamesOnSelection);
                if (windowSettingsChanged)
                    notifyWindowSettingsChanged();
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_TOOLBAR_ENABLED && HIWORD(wParam) == BN_CLICKED)
                enableToolbarOptions(hwnd);
            if (((LOWORD(wParam) == IDC_FOLDER_WINDOW_WIDTH
                        || LOWORD(wParam) == IDC_FOLDER_WINDOW_HEIGHT) && HIWORD(wParam) == EN_CHANGE)
                    || (HIWORD(wParam) == BN_CLICKED && (
                        LOWORD(wParam) == IDC_STATUS_TEXT_ENABLED
                        || LOWORD(wParam) == IDC_TOOLBAR_ENABLED
                        || LOWORD(wParam) == IDC_QUICK_ACCESS_ENABLED
                        || LOWORD(wParam) == IDC_PATH_BAR_ENABLED
                        || LOWORD(wParam) == IDC_REFRESH_BUTTON_ENABLED
                        || LOWORD(wParam) == IDC_UP_BUTTON_ENABLED
                        || LOWORD(wParam) == IDC_VIEW_BUTTON_ENABLED
                        || LOWORD(wParam) == IDC_FULL_NAMES_ON_SELECTION))) {
                PropSheet_Changed(GetParent(hwnd), hwnd);
                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
    }
}

void changeDefaultBrowser(HWND owner, bool value) {
    HINSTANCE instance = GetModuleHandle(nullptr);
    if (!settings::supportsDefaultBrowser()) {
        TaskDialog(owner, instance, MAKEINTRESOURCE(IDS_BROWSER_SET_FAILED),
            nullptr, MAKEINTRESOURCE(IDS_REQUIRE_CONTEXT), 0, TD_ERROR_ICON, nullptr);
        return;
    }
    const settings::DefaultBrowserResult result = settings::setDefaultBrowser(value);
    const LSTATUS statuses[] = {result.directory, result.compressedFolder, result.drive};
    const UINT labels[] = {IDS_BROWSER_DIRECTORY, IDS_BROWSER_COMPRESSED_FOLDER, IDS_BROWSER_DRIVE};
    std::wstring failures;
    for (int index = 0; index < 3; ++index) {
        if (statuses[index] == ERROR_SUCCESS)
            continue;
        auto error = getErrorMessage(static_cast<DWORD>(statuses[index]));
        auto line = formatString(IDS_BROWSER_ASSOCIATION_ERROR, getString(labels[index]),
            static_cast<DWORD>(statuses[index]), error.get());
        if (!failures.empty())
            failures += L"\n\n";
        failures += line.get();
    }
    if (failures.empty()) {
        TaskDialog(owner, instance, MAKEINTRESOURCE(IDS_SUCCESS_CAPTION), nullptr,
            MAKEINTRESOURCE(value ? IDS_BROWSER_SET : IDS_BROWSER_RESET), 0, nullptr, nullptr);
    } else {
        auto message = formatString(IDS_BROWSER_ASSOCIATIONS_FAILED, failures.c_str());
        TaskDialog(owner, instance, MAKEINTRESOURCE(IDS_BROWSER_SET_FAILED),
            nullptr, message.get(), TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
    }
}

static bool searchHistoryLimit(HWND hwnd, DWORD *value) {
    wchar_t text[16] = {};
    const int length = GetWindowTextLengthW(GetDlgItem(hwnd, IDC_SEARCH_HISTORY_LIMIT));
    GetDlgItemTextW(hwnd, IDC_SEARCH_HISTORY_LIMIT, text, _countof(text));
    DWORD number = 0;
    bool valid = length > 0 && length < 3;
    for (int index = 0; valid && index < length; ++index) {
        valid = text[index] >= L'0' && text[index] <= L'9';
        if (valid) number = number * 10 + text[index] - L'0';
    }
    if (valid && number <= settings::MAX_SEARCH_HISTORY_LIMIT) {
        *value = number;
        return true;
    }
    MessageBoxW(GetParent(hwnd), getString(IDS_SEARCH_HISTORY_INVALID),
        getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
    SetDlgItemInt(hwnd, IDC_SEARCH_HISTORY_LIMIT, settings::getSearchHistoryLimit(), FALSE);
    SetFocus(GetDlgItem(hwnd, IDC_SEARCH_HISTORY_LIMIT));
    SendDlgItemMessageW(hwnd, IDC_SEARCH_HISTORY_LIMIT, EM_SETSEL, 0, -1);
    return false;
}

INT_PTR CALLBACK browserProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            CheckDlgButton(hwnd, IDC_EVERYTHING_ENABLED,
                settings::getEverythingEnabled() ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemTextW(hwnd, IDC_EVERYTHING_PATH, settings::getEverythingPath().get());
            SendDlgItemMessageW(hwnd, IDC_EVERYTHING_PATH, EM_SETCUEBANNER, TRUE,
                reinterpret_cast<LPARAM>(getString(IDS_EVERYTHING_AUTO)));
            SendDlgItemMessageW(hwnd, IDC_EVERYTHING_SCOPE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(getString(IDS_EVERYTHING_FOLDER)));
            SendDlgItemMessageW(hwnd, IDC_EVERYTHING_SCOPE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(getString(IDS_EVERYTHING_EVERYWHERE)));
            SendDlgItemMessageW(hwnd, IDC_EVERYTHING_SCOPE, CB_SETCURSEL,
                static_cast<WPARAM>(settings::getEverythingScope()), 0);
            return TRUE;
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (notif->code == PSN_KILLACTIVE) {
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
                const int length = GetWindowTextLengthW(GetDlgItem(hwnd, IDC_EVERYTHING_PATH));
                std::vector<wchar_t> path(static_cast<size_t>(length) + 1);
                GetDlgItemTextW(hwnd, IDC_EVERYTHING_PATH, path.data(), length + 1);
                settings::setEverythingPath(path.data());
                settings::setEverythingEnabled(!!IsDlgButtonChecked(hwnd, IDC_EVERYTHING_ENABLED));
                const LRESULT scope = SendDlgItemMessageW(hwnd, IDC_EVERYTHING_SCOPE, CB_GETCURSEL, 0, 0);
                settings::setEverythingScope(scope == 1 ? settings::EverythingScope::Everywhere
                    : settings::EverythingScope::Folder);
                notifyWindowSettingsChanged();
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            } else if (notif->code == PSN_HELP) {
#if 0 // Deferred public help.
                ShellExecute(nullptr, L"open",
                    L"https://github.com/vanjac/chromafiler/wiki/Settings#default-browser",
                    nullptr, nullptr, SW_SHOWNORMAL);
#endif
                return TRUE;
            }
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_EXPLORER_SETTINGS && HIWORD(wParam) == BN_CLICKED) {
                // https://docs.microsoft.com/en-us/windows/win32/shell/executing-control-panel-items#folder-options
                ShellExecute(nullptr, L"open",
                    L"rundll32.exe", L"shell32.dll,Options_RunDLL 7", nullptr, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_EVERYTHING_DETECT && HIWORD(wParam) == BN_CLICKED) {
                const auto path = findEverything();
                if (path.empty()) MessageBoxW(GetParent(hwnd), getString(IDS_EVERYTHING_NOT_FOUND),
                    getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
                else SetDlgItemTextW(hwnd, IDC_EVERYTHING_PATH, path.c_str());
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_EVERYTHING_BROWSE && HIWORD(wParam) == BN_CLICKED) {
                CComPtr<IFileOpenDialog> dialog;
                if (SUCCEEDED(dialog.CoCreateInstance(__uuidof(FileOpenDialog)))) {
                    const COMDLG_FILTERSPEC filter = {getString(IDS_EVERYTHING_EXECUTABLE), L"*.exe"};
                    dialog->SetFileTypes(1, &filter);
                    if (SUCCEEDED(dialog->Show(GetParent(hwnd)))) {
                        CComPtr<IShellItem> item;
                        CComHeapPtr<wchar_t> path;
                        if (SUCCEEDED(dialog->GetResult(&item))
                                && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                            SetDlgItemTextW(hwnd, IDC_EVERYTHING_PATH, path);
                    }
                }
                return TRUE;
            }
            if ((LOWORD(wParam) == IDC_EVERYTHING_ENABLED && HIWORD(wParam) == BN_CLICKED)
                    || (LOWORD(wParam) == IDC_EVERYTHING_SCOPE && HIWORD(wParam) == CBN_SELCHANGE)
                    || (LOWORD(wParam) == IDC_EVERYTHING_PATH && HIWORD(wParam) == EN_CHANGE))
                PropSheet_Changed(GetParent(hwnd), hwnd);
            if (LOWORD(wParam) == IDC_SET_DEFAULT_BROWSER && HIWORD(wParam) == BN_CLICKED) {
                changeDefaultBrowser(GetParent(hwnd), true);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_RESET_DEFAULT_BROWSER
                    && HIWORD(wParam) == BN_CLICKED) {
                changeDefaultBrowser(GetParent(hwnd), false);
                return TRUE;
            }
            return FALSE;
    }
    return FALSE;
}

INT_PTR CALLBACK aboutProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            SendDlgItemMessage(hwnd, IDC_LEGAL_INFO, EM_SETTABSTOPS, 1, (LPARAM)tempPtr(16u));
            CheckDlgButton(hwnd, IDC_AUTO_UPDATE,
                settings::getUpdateCheckEnabled() ? BST_CHECKED : BST_UNCHECKED);

            SetDlgItemText(hwnd, IDC_VERSION, _T(FILESPACER_VERSION_STRING));
            SetDlgItemText(hwnd, IDC_LEGAL_INFO, getString(IDS_LEGAL_INFO));
            initUpdateControls(hwnd);
            return TRUE;
        }
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (notif->code == PSN_KILLACTIVE) {
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
                bool updatesEnabled = !!IsDlgButtonChecked(hwnd, IDC_AUTO_UPDATE);
                bool enableNow = updatesEnabled && !settings::getUpdateCheckEnabled();
                settings::setUpdateCheckEnabled(updatesEnabled);
                if (enableNow)
                    autoUpdateCheck();

                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            } else if (notif->code == PSN_HELP) {
#if 0 // Deferred public help.
                ShellExecute(nullptr, L"open",
                    L"https://github.com/vanjac/chromafiler/wiki/Settings#updateabout",
                    nullptr, nullptr, SW_SHOWNORMAL);
#endif
                return TRUE;
            }
            return FALSE;
        }
        case WM_COMMAND:
#if 0 // Deferred public help, website and donation links.
            if (LOWORD(wParam) == IDC_HELP_LINK && HIWORD(wParam) == BN_CLICKED) {
                ShellExecute(nullptr, L"open", L"https://github.com/vanjac/chromafiler/wiki",
                    nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_WEBSITE_LINK && HIWORD(wParam) == BN_CLICKED) {
                ShellExecute(nullptr, L"open", L"https://chroma.zone/chromafiler/",
                    nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_DONATE_LINK && HIWORD(wParam) == BN_CLICKED) {
                ShellExecute(nullptr, L"open", L"https://chroma.zone/donate",
                    nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            }
#endif
            if (LOWORD(wParam) == IDC_RESET_FOLDER_STATE && HIWORD(wParam) == BN_CLICKED) {
                const HWND dialog = GetParent(hwnd);
                if (confirmResetAllFolderState(dialog) && settingsDialog == dialog && IsWindow(dialog))
                    resetFolderStateRequested = !!checkLE(PostMessageW(dialog, PSM_PRESSBUTTON, PSBTN_CANCEL, 0));
                return TRUE;
            } else if (LOWORD(wParam) == IDC_AUTO_UPDATE && HIWORD(wParam) == BN_CLICKED) {
                PropSheet_Changed(GetParent(hwnd), hwnd);
                return TRUE;
            }
            return FALSE;
    }
    return FALSE;
}

void requestSettingsDialogClose() {
    if (settingsDialog)
        PostMessageW(settingsDialog, PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
}

void openSettingsDialog(HWND owner, SettingsPage page) {
    if (settingsDialog) {
        SetActiveWindow(settingsDialog);
        PropSheet_SetCurSel(settingsDialog, nullptr, page);
        return;
    }
    resetFolderStateRequested = false;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    PROPSHEETPAGE pages[NUM_SETTINGS_PAGES];

    pages[SETTINGS_GENERAL] = {sizeof(PROPSHEETPAGE)};
    // pages[SETTINGS_GENERAL].dwFlags = PSP_HASHELP; // Deferred public help.
    pages[SETTINGS_GENERAL].hInstance = hInstance;
    pages[SETTINGS_GENERAL].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_GENERAL);
    pages[SETTINGS_GENERAL].pfnDlgProc = generalProc;

    pages[SETTINGS_DISPLAY] = {sizeof(PROPSHEETPAGE)};
    pages[SETTINGS_DISPLAY].hInstance = hInstance;
    pages[SETTINGS_DISPLAY].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_DISPLAY);
    pages[SETTINGS_DISPLAY].pfnDlgProc = displayProc;

    pages[SETTINGS_SHORTCUTS] = {sizeof(PROPSHEETPAGE)};
    pages[SETTINGS_SHORTCUTS].hInstance = hInstance;
    pages[SETTINGS_SHORTCUTS].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_SHORTCUTS);
    pages[SETTINGS_SHORTCUTS].pfnDlgProc = shortcutsProc;

    pages[SETTINGS_BROWSER] = {sizeof(PROPSHEETPAGE)};
    // pages[SETTINGS_BROWSER].dwFlags = PSP_HASHELP; // Deferred public help.
    pages[SETTINGS_BROWSER].hInstance = hInstance;
    pages[SETTINGS_BROWSER].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_BROWSER);
    pages[SETTINGS_BROWSER].pfnDlgProc = browserProc;

    pages[SETTINGS_ABOUT] = {sizeof(PROPSHEETPAGE)};
    // pages[SETTINGS_ABOUT].dwFlags = PSP_HASHELP; // Deferred public help.
    pages[SETTINGS_ABOUT].hInstance = hInstance;
    pages[SETTINGS_ABOUT].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_ABOUT);
    pages[SETTINGS_ABOUT].pfnDlgProc = aboutProc;

    PROPSHEETHEADER sheet = {sizeof(sheet)};
    sheet.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_NOCONTEXTHELP | PSH_USECALLBACK;
    sheet.hInstance = hInstance;
    sheet.hwndParent = owner;
    sheet.pszIcon = MAKEINTRESOURCE(IDR_APP_ICON);
    sheet.pszCaption = MAKEINTRESOURCE(IDS_SETTINGS_CAPTION);
    sheet.nPages = _countof(pages);
    sheet.nStartPage = page;
    sheet.ppsp = pages;
    sheet.pfnCallback = [](HWND window, UINT message, LPARAM) -> int {
        if (message == PSCB_INITIALIZED)
            settingsDialog = window;
        return 0;
    };

    lockProcess();
    std::vector<HWND> disabledWindows;
    auto disable = [](HWND window, LPARAM data) -> BOOL {
        if (IsWindowVisible(window) && IsWindowEnabled(window)) {
            reinterpret_cast<std::vector<HWND> *>(data)->push_back(window);
            EnableWindow(window, FALSE);
        }
        return TRUE;
    };
    checkLE(EnumThreadWindows(GetCurrentThreadId(), disable,
        reinterpret_cast<LPARAM>(&disabledWindows)));
    // PropertySheet runs its native modal message loop and destroys the dialog.
    checkLE(PropertySheet(&sheet) != -1);
    settingsDialog = nullptr;
    const bool resetRequested = resetFolderStateRequested;
    resetFolderStateRequested = false;
    // Maintenance closes folder windows only after the settings pages are gone.
    if (resetRequested) resetAllFolderState();
    for (HWND window : disabledWindows) {
        if (IsWindow(window)) // Maintenance may have closed it.
            EnableWindow(window, TRUE);
    }
    if (IsWindow(owner))
        SetActiveWindow(owner);
    unlockProcess();
}

} // namespace
