#include "SettingsDialog.h"
#include "Settings.h"
#include "FolderStateStore.h"
#include "CreateItemWindow.h"
#include "Update.h"
#include "main.h"
#include "WinUtils.h"
#include "UIStrings.h"
#include <atlbase.h>
#include <prsht.h>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <string>
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

static void resetAllFolderState(HWND owner) {
    TASKDIALOGCONFIG config = {sizeof(config)};
    config.hwndParent = owner;
    config.hInstance = GetModuleHandle(nullptr);
    config.pszWindowTitle = MAKEINTRESOURCE(IDS_CONFIRM_CAPTION);
    config.pszMainInstruction = MAKEINTRESOURCE(IDS_RESET_FOLDER_STATE_CONFIRM);
    config.pszMainIcon = TD_WARNING_ICON;
    config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    config.nDefaultButton = IDNO;
    int button = IDNO;
    if (!checkHR(TaskDialogIndirect(&config, &button, nullptr, nullptr)) || button != IDYES)
        return;

    // Keep the settings dialog open: openSettingsDialog holds a process lock.
    // Disable it only during this command, including any normal save prompts.
    EnableWindow(owner, FALSE);
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
        return store->clearAll() ? IDS_RESET_FOLDER_STATE_SUCCESS
            : IDS_RESET_FOLDER_STATE_DELETE_ERROR;
    }();
    EnableWindow(owner, TRUE);
    bool success = result == IDS_RESET_FOLDER_STATE_SUCCESS;
    TaskDialog(owner, GetModuleHandle(nullptr),
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
        auto notify = [](HWND window, LPARAM) -> BOOL {
            wchar_t className[64];
            if (GetClassName(window, className, _countof(className))
                    && lstrcmp(className, L"FileSpacer Item Window") == 0)
                checkLE(SendNotifyMessage(window, ItemWindow::settingsChangedMessage, 0, 0));
            return TRUE;
        };
        checkLE(EnumWindows(notify, 0));
    }
}

INT_PTR CALLBACK generalProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
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
            CheckDlgButton(hwnd, IDC_KEEP_SELECTION_ON_ACTIVATE,
                settings::getKeepSelectionOnActivate() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_LIVE_NAME_SEARCH,
                settings::getLiveNameSearch() ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;
        }
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (notif->code == PSN_KILLACTIVE) {
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
                int pathLength = GetWindowTextLength(GetDlgItem(hwnd, IDC_START_FOLDER_PATH));
                std::vector<wchar_t> startingFolder(static_cast<size_t>(pathLength) + 1);
                if (GetDlgItemText(hwnd, IDC_START_FOLDER_PATH,
                        startingFolder.data(), pathLength + 1))
                    settings::setStartingFolder(startingFolder.data());
                bool groupFolderWindows = !!IsDlgButtonChecked(hwnd, IDC_GROUP_FOLDER_WINDOWS);
                bool liveNameSearch = !!IsDlgButtonChecked(hwnd, IDC_LIVE_NAME_SEARCH);
                bool windowSettingsChanged = liveNameSearch != settings::getLiveNameSearch();
                settings::setGroupFolderWindows(groupFolderWindows);
                settings::setKeepSourceWindowOpen(!!IsDlgButtonChecked(hwnd, IDC_KEEP_SOURCE_WINDOW_OPEN));
                settings::setKeepSelectionOnActivate(
                    !!IsDlgButtonChecked(hwnd, IDC_KEEP_SELECTION_ON_ACTIVATE));
                settings::setLiveNameSearch(liveNameSearch);
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
            if (LOWORD(wParam) == IDC_RESET_FOLDER_STATE && HIWORD(wParam) == BN_CLICKED) {
                resetAllFolderState(GetParent(hwnd));
                return TRUE;
            } else if (LOWORD(wParam) == IDC_START_FOLDER_BROWSE && HIWORD(wParam) == BN_CLICKED) {
                CComHeapPtr<wchar_t> selected;
                if (chooseFolder(GetParent(hwnd), selected)) {
                    setCBPath(GetDlgItem(hwnd, IDC_START_FOLDER_PATH), selected);
                    PropSheet_Changed(GetParent(hwnd), hwnd);
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_EXPLORER_SETTINGS && HIWORD(wParam) == BN_CLICKED) {
                // https://docs.microsoft.com/en-us/windows/win32/shell/executing-control-panel-items#folder-options
                ShellExecute(nullptr, L"open",
                    L"rundll32.exe", L"shell32.dll,Options_RunDLL 7", nullptr, SW_SHOWNORMAL);
                return TRUE;
            } else if (HIWORD(wParam) == EN_CHANGE
                    || LOWORD(wParam) == IDC_START_FOLDER_PATH && pathCBChanged(wParam, lParam)
                    || LOWORD(wParam) == IDC_GROUP_FOLDER_WINDOWS && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_KEEP_SOURCE_WINDOW_OPEN && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_LIVE_NAME_SEARCH && HIWORD(wParam) == BN_CLICKED
                    || LOWORD(wParam) == IDC_KEEP_SELECTION_ON_ACTIVATE && HIWORD(wParam) == BN_CLICKED) {
                PropSheet_Changed(GetParent(hwnd), hwnd);
                return TRUE;
            }
            return FALSE;
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
                bool windowSettingsChanged = statusTextEnabled != settings::getStatusTextEnabled()
                    || toolbarEnabled != settings::getToolbarEnabled()
                    || quickAccessEnabled != settings::getQuickAccessEnabled()
                    || pathBarEnabled != settings::getPathBarEnabled()
                    || refreshButtonEnabled != settings::getRefreshButtonEnabled()
                    || upButtonEnabled != settings::getUpButtonEnabled()
                    || viewButtonEnabled != settings::getViewButtonEnabled();
                settings::setStatusTextEnabled(statusTextEnabled);
                settings::setToolbarEnabled(toolbarEnabled);
                settings::setQuickAccessEnabled(quickAccessEnabled);
                settings::setPathBarEnabled(pathBarEnabled);
                settings::setRefreshButtonEnabled(refreshButtonEnabled);
                settings::setUpButtonEnabled(upButtonEnabled);
                settings::setViewButtonEnabled(viewButtonEnabled);
                // The view implementation is chosen when each folder window is created.
                settings::setFullNamesOnSelection(!!IsDlgButtonChecked(hwnd, IDC_FULL_NAMES_ON_SELECTION));
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

INT_PTR CALLBACK browserProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (notif->code == PSN_KILLACTIVE) {
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
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
#if 0 // Deferred until FileSpacer has its own public services.
            CheckDlgButton(hwnd, IDC_AUTO_UPDATE,
                settings::getUpdateCheckEnabled() ? BST_CHECKED : BST_UNCHECKED);
#endif

            SetDlgItemText(hwnd, IDC_VERSION, _T(FILESPACER_VERSION_STRING));
            SetDlgItemText(hwnd, IDC_LEGAL_INFO, getString(IDS_LEGAL_INFO));
            return TRUE;
        }
        case WM_NOTIFY: {
            NMHDR *notif = (NMHDR *)lParam;
            if (notif->code == PSN_KILLACTIVE) {
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
                return TRUE;
            } else if (notif->code == PSN_APPLY) {
#if 0 // Deferred until FileSpacer has its own public services.
                bool updatesEnabled = !!IsDlgButtonChecked(hwnd, IDC_AUTO_UPDATE);
                bool enableNow = updatesEnabled && !settings::getUpdateCheckEnabled();
                settings::setUpdateCheckEnabled(updatesEnabled);
                if (enableNow)
                    autoUpdateCheck();
#endif

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
#if 0 // Deferred until FileSpacer has its own public services.
            if (LOWORD(wParam) == IDC_UPDATES_LINK && HIWORD(wParam) == BN_CLICKED) {
                HINSTANCE instance = GetModuleHandle(nullptr);
                UpdateInfo info;
                if (DWORD error = checkUpdate(&info)) {
                    TaskDialog(GetParent(hwnd), instance, MAKEINTRESOURCE(IDS_UPDATE_ERROR),
                        nullptr, getErrorMessage(error).get(), 0, nullptr, nullptr);
                    return TRUE;
                }
                if (info.isNewer) {
                    openUpdate(info);
                } else {
                    TaskDialog(GetParent(hwnd), instance, MAKEINTRESOURCE(IDS_NO_UPDATE_CAPTION),
                        nullptr, MAKEINTRESOURCE(IDS_NO_UPDATE), 0, nullptr, nullptr);
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_HELP_LINK && HIWORD(wParam) == BN_CLICKED) {
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
            } else if (LOWORD(wParam) == IDC_AUTO_UPDATE && HIWORD(wParam) == BN_CLICKED) {
                PropSheet_Changed(GetParent(hwnd), hwnd);
                return TRUE;
            } else
#endif
            if (HIWORD(wParam) == CBN_SELCHANGE) {

                SendMessage(GetParent(hwnd), LOWORD(wParam), lParam, (LPARAM)tempPtr(0xDD8AD83Ell)); 
                return TRUE;
            }
            return FALSE;
    }
    return FALSE;
}

void openSettingsDialog(HWND owner, SettingsPage page) {
    if (settingsDialog) {
        SetActiveWindow(settingsDialog);
        PropSheet_SetCurSel(settingsDialog, nullptr, page);
        return;
    }

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

    // An invisible owner keeps the native dialog out of the taskbar and alive
    // if Maintenance closes all folder windows, including its original caller.
    RECT ownerRect = {};
    checkLE(GetWindowRect(owner, &ownerRect));
    HWND modalOwner = checkLE(CreateWindowEx(0, L"Static", L"", WS_POPUP,
        ownerRect.left, ownerRect.top, ownerRect.right - ownerRect.left,
        ownerRect.bottom - ownerRect.top, nullptr, nullptr, hInstance, nullptr));
    if (!modalOwner)
        return;
    sheet.hwndParent = modalOwner;

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
    for (HWND window : disabledWindows) {
        if (IsWindow(window)) // Maintenance may have closed it.
            EnableWindow(window, TRUE);
    }
    if (IsWindow(owner))
        SetActiveWindow(owner);
    checkLE(DestroyWindow(modalOwner));
    unlockProcess();
}

} // namespace
