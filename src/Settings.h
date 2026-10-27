#pragma once
#include <common.h>

#include <WinUtils.h>
#include "WindowShortcuts.h"

namespace filespacer {

namespace settings {

#ifdef FILESPACER_DEBUG
extern bool testMode;
#endif

// http://smallvoid.com/article/winnt-shell-keyword.html
const DWORD     DEFAULT_LAST_OPENED_VERSION = 0;
const bool      DEFAULT_UPDATE_CHECK_ENABLED= false;
const LONGLONG  DEFAULT_LAST_UPDATE_CHECK   = 0;
const LONGLONG  DEFAULT_UPDATE_CHECK_RATE   = 10000000LL * 60 * 60 * 24 * 7; // 1 week
const bool      DEFAULT_ADMIN_WARNING       = true;
const wchar_t   DEFAULT_STARTING_FOLDER[]   = L"shell:Desktop";
const SIZE      DEFAULT_FOLDER_WINDOW_SIZE  = {580, 435};
const bool      DEFAULT_STATUS_TEXT_ENABLED = true;
const bool      DEFAULT_TOOLBAR_ENABLED     = true;
const bool      DEFAULT_QUICK_ACCESS_ENABLED = true;
const bool      DEFAULT_PATH_BAR_ENABLED     = true;
const bool      DEFAULT_REFRESH_BUTTON_ENABLED = true;
const bool      DEFAULT_UP_BUTTON_ENABLED    = true;
const bool      DEFAULT_VIEW_BUTTON_ENABLED  = true;
const bool      DEFAULT_VIEW_MENU_PREVIEW_ENABLED = true;
const bool      DEFAULT_GROUP_FOLDER_WINDOWS = false;
const bool      DEFAULT_DESELECT_ON_OPEN    = true;
const bool      DEFAULT_KEEP_SOURCE_WINDOW_OPEN = false;
const bool      DEFAULT_SMART_WINDOW_PLACEMENT = true;
const bool      DEFAULT_KEEP_SELECTION_ON_ACTIVATE = true;
const bool      DEFAULT_LIVE_NAME_SEARCH     = false;
const DWORD     DEFAULT_SEARCH_HISTORY_LIMIT = 5;
const DWORD     MAX_SEARCH_HISTORY_LIMIT = 20;
enum class EverythingScope : DWORD { Folder, Everywhere };
const bool      DEFAULT_FULL_NAMES_ON_SELECTION = true;

DWORD getLastOpenedVersion();
void setLastOpenedVersion(DWORD value);
bool getUpdateCheckEnabled();
void setUpdateCheckEnabled(bool value);
LONGLONG getLastUpdateCheck();
void setLastUpdateCheck(LONGLONG value);
LONGLONG getUpdateCheckRate();
void setUpdateCheckRate(LONGLONG value); // TODO: add to Settings

bool getAdminWarning();
void setAdminWarning(bool value);

wstr_ptr getStartingFolder();
void setStartingFolder(wchar_t *value);

SIZE getFolderWindowSize(); // assuming 96 dpi
void setFolderWindowSize(SIZE value);

bool getStatusTextEnabled();
void setStatusTextEnabled(bool value);
bool getToolbarEnabled();
void setToolbarEnabled(bool value);
bool getQuickAccessEnabled();
void setQuickAccessEnabled(bool value);
bool getPathBarEnabled();
void setPathBarEnabled(bool value);
bool getRefreshButtonEnabled();
void setRefreshButtonEnabled(bool value);
bool getUpButtonEnabled();
void setUpButtonEnabled(bool value);
bool getViewButtonEnabled();
void setViewButtonEnabled(bool value);
bool getViewMenuPreviewEnabled();
void setViewMenuPreviewEnabled(bool value);
bool getGroupFolderWindows();
void setGroupFolderWindows(bool value);
bool getKeepSourceWindowOpen();
void setKeepSourceWindowOpen(bool value);
bool getSmartWindowPlacement();
void setSmartWindowPlacement(bool value);
bool getKeepSelectionOnActivate();
void setKeepSelectionOnActivate(bool value);
bool getLiveNameSearch();
void setLiveNameSearch(bool value);
DWORD getSearchHistoryLimit();
bool setSearchHistoryLimit(DWORD value);
bool getEverythingEnabled();
void setEverythingEnabled(bool value);
wstr_ptr getEverythingPath(); // Empty selects automatic detection.
void setEverythingPath(wchar_t *value);
EverythingScope getEverythingScope();
void setEverythingScope(EverythingScope value);
bool getWindowStateLockEnabled();
bool setWindowStateLockEnabled(bool value);
bool getWindowStateLockBadgeEnabled();
void setWindowStateLockBadgeEnabled(bool value);
bool getFullNamesOnSelection();
void setFullNamesOnSelection(bool value);
LSTATUS resetGlobalOptions();
WindowShortcuts getWindowShortcuts();
LSTATUS setWindowShortcuts(const WindowShortcuts &value);

bool getDeselectOnOpen();
void setDeselectOnOpen(bool value); // TODO: add to Settings

bool supportsDefaultBrowser();
struct DefaultBrowserResult {
    LSTATUS directory;
    LSTATUS compressedFolder;
    LSTATUS drive;
};
DefaultBrowserResult setDefaultBrowser(bool value);

}} // namespace
