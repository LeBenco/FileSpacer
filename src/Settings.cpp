#include "Settings.h"
#include <strsafe.h>
#include <atlbase.h>

namespace filespacer {
namespace settings {

bool testMode = false;

const wchar_t KEY_SETTINGS_NORMAL[]     = L"Software\\FileSpacer";
const wchar_t KEY_SETTINGS_TEST[]       = L"Software\\FileSpacer\\Test";
#ifdef FILESPACER_DEBUG
    #define KEY_SETTINGS (testMode ? KEY_SETTINGS_TEST : KEY_SETTINGS_NORMAL)
#else
    #define KEY_SETTINGS KEY_SETTINGS_NORMAL
#endif

const wchar_t KEY_DIRECTORY_VERB[]      = L"Directory\\shell\\filespacer";
const wchar_t KEY_DIRECTORY_BROWSER[]   = L"Software\\Classes\\Directory\\Shell";
const wchar_t KEY_COMPRESSED_BROWSER[]  = L"Software\\Classes\\CompressedFolder\\Shell";
const wchar_t KEY_DRIVE_BROWSER[]       = L"Software\\Classes\\Drive\\Shell";
const wchar_t DATA_BROWSER_SET[]        = L"filespacer";
const wchar_t DATA_BROWSER_CLEAR[]      = L"none";

// type should be a RRF_RT_* constant
// data should already contain default value
LSTATUS getSettingsValue(const wchar_t *name, DWORD type, void *data, DWORD size) {
    return RegGetValue(HKEY_CURRENT_USER, KEY_SETTINGS, name, type, nullptr, data, &size);
}

// type should be a REG_* constant (unlike getSettingsValue!)
LSTATUS setSettingsValue(const wchar_t *name, DWORD type, const void *data, DWORD size) {
    return RegSetKeyValue(HKEY_CURRENT_USER, KEY_SETTINGS, name, type, data, size);
}

wstr_ptr getSettingsString(const wchar_t *name, DWORD type, const wchar_t *defaultValue) {
    DWORD size;
    if (!RegGetValue(HKEY_CURRENT_USER, KEY_SETTINGS, name, type, nullptr, nullptr, &size)) {
        wstr_ptr buffer(new wchar_t[size / sizeof(wchar_t)]);
        RegGetValue(HKEY_CURRENT_USER, KEY_SETTINGS, name, type, nullptr, buffer.get(), &size);
        return buffer;
    } else {
        DWORD count = lstrlen(defaultValue) + 1;
        wstr_ptr buffer(new wchar_t[count]);
        CopyMemory(buffer.get(), defaultValue, count * sizeof(wchar_t));
        return buffer;
    }
}

void setSettingsString(const wchar_t *name, DWORD type, const wchar_t *value) {
    setSettingsValue(name, type, value, (lstrlen(value) + 1) * sizeof(wchar_t));
}

#define SETTINGS_DWORD_VALUE(funcName, type, valueName, defaultValue) \
    type get##funcName() {                                                                         \
        DWORD value = (defaultValue);                                                              \
        getSettingsValue((valueName), RRF_RT_DWORD, &value, sizeof(value));                        \
        return (type)value;                                                                        \
    }                                                                                              \
    void set##funcName(type value) {                                                               \
        DWORD dwValue = (DWORD)value;                                                              \
        setSettingsValue((valueName), REG_DWORD, &dwValue, sizeof(dwValue));                       \
    }

#define SETTINGS_QWORD_VALUE(funcName, type, valueName, defaultValue) \
    type get##funcName() {                                                                         \
        ULONGLONG value = (defaultValue);                                                          \
        getSettingsValue((valueName), RRF_RT_QWORD, &value, sizeof(value));                        \
        return (type)value;                                                                        \
    }                                                                                              \
    void set##funcName(type value) {                                                               \
        ULONGLONG dwValue = (ULONGLONG)value;                                                      \
        setSettingsValue((valueName), REG_QWORD, &dwValue, sizeof(dwValue));                       \
    }

#define SETTINGS_BOOL_VALUE(funcName, valueName, defaultValue) \
    SETTINGS_DWORD_VALUE(funcName, bool, valueName, defaultValue)

#define SETTINGS_SIZE_VALUE(funcName, valueName, defaultValue) \
    SIZE get##funcName() {                                                                         \
        SIZE value = (defaultValue);                                                               \
        getSettingsValue(valueName L"Width", RRF_RT_DWORD, &value.cx, sizeof(value.cx));           \
        getSettingsValue(valueName L"Height", RRF_RT_DWORD, &value.cy, sizeof(value.cy));          \
        return value;                                                                              \
    }                                                                                              \
    void set##funcName(SIZE value) {                                                               \
        setSettingsValue(valueName L"Width", REG_DWORD, &value.cx, sizeof(value.cx));              \
        setSettingsValue(valueName L"Height", REG_DWORD, &value.cy, sizeof(value.cy));             \
    }

#define SETTINGS_STRING_VALUE(funcName, regType, valueName, defaultValue) \
    wstr_ptr get##funcName() {                                                                     \
        return getSettingsString((valueName), RRF_RT_REG_SZ, (defaultValue));                      \
    }                                                                                              \
    void set##funcName(wchar_t *value) {                                                           \
        setSettingsString((valueName), (regType), value);                                          \
    }

SETTINGS_DWORD_VALUE(LastOpenedVersion, DWORD, L"LastOpenedVersion", DEFAULT_LAST_OPENED_VERSION)
#if 0 // Deferred until FileSpacer has its own public services.
SETTINGS_BOOL_VALUE(UpdateCheckEnabled, L"UpdateCheckEnabled", DEFAULT_UPDATE_CHECK_ENABLED);
SETTINGS_QWORD_VALUE(LastUpdateCheck, LONGLONG, L"LastUpdateCheck", DEFAULT_LAST_UPDATE_CHECK)
SETTINGS_QWORD_VALUE(UpdateCheckRate, LONGLONG, L"UpdateCheckRate", DEFAULT_UPDATE_CHECK_RATE)
#endif


SETTINGS_BOOL_VALUE(AdminWarning, L"AdminWarning", DEFAULT_ADMIN_WARNING);

SETTINGS_STRING_VALUE(StartingFolder, REG_EXPAND_SZ, L"StartingFolder", DEFAULT_STARTING_FOLDER)

SETTINGS_SIZE_VALUE(FolderWindowSize, L"FolderWindow2", DEFAULT_FOLDER_WINDOW_SIZE)

SETTINGS_BOOL_VALUE(StatusTextEnabled, L"StatusTextEnabled", DEFAULT_STATUS_TEXT_ENABLED)
SETTINGS_BOOL_VALUE(ToolbarEnabled, L"ToolbarEnabled", DEFAULT_TOOLBAR_ENABLED)
SETTINGS_BOOL_VALUE(QuickAccessEnabled, L"QuickAccessEnabled", DEFAULT_QUICK_ACCESS_ENABLED)
SETTINGS_BOOL_VALUE(PathBarEnabled, L"PathBarEnabled", DEFAULT_PATH_BAR_ENABLED)
SETTINGS_BOOL_VALUE(RefreshButtonEnabled, L"RefreshButtonEnabled", DEFAULT_REFRESH_BUTTON_ENABLED)
SETTINGS_BOOL_VALUE(UpButtonEnabled, L"UpButtonEnabled", DEFAULT_UP_BUTTON_ENABLED)
SETTINGS_BOOL_VALUE(ViewButtonEnabled, L"ViewButtonEnabled", DEFAULT_VIEW_BUTTON_ENABLED)
SETTINGS_BOOL_VALUE(GroupFolderWindows, L"GroupFolderWindows", DEFAULT_GROUP_FOLDER_WINDOWS)
SETTINGS_BOOL_VALUE(KeepSourceWindowOpen, L"KeepSourceWindowOpen", DEFAULT_KEEP_SOURCE_WINDOW_OPEN)
SETTINGS_BOOL_VALUE(KeepSelectionOnActivate, L"KeepSelectionOnActivate", DEFAULT_KEEP_SELECTION_ON_ACTIVATE)
SETTINGS_BOOL_VALUE(LiveNameSearch, L"LiveNameSearch", DEFAULT_LIVE_NAME_SEARCH)
SETTINGS_BOOL_VALUE(FullNamesOnSelection, L"FullNamesOnSelection", DEFAULT_FULL_NAMES_ON_SELECTION)

LSTATUS disableFullNamesOnSelection() {
    const DWORD disabled = 0;
    return setSettingsValue(L"FullNamesOnSelection", REG_DWORD, &disabled, sizeof(disabled));
}

LSTATUS resetGlobalOptions() {
    // Delete current/future preference values, preserving version metadata and subkeys
    // (including the separate Debug /test namespace). Windows associations are elsewhere.
    HKEY raw = nullptr;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, KEY_SETTINGS, 0,
        KEY_QUERY_VALUE | KEY_SET_VALUE, &raw);
    if (status == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (status != ERROR_SUCCESS) return status;
    CRegKey key; key.Attach(raw);
    DWORD index = 0;
    for (;;) {
        wchar_t name[16384]; DWORD length = _countof(name);
        status = RegEnumValueW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) return ERROR_SUCCESS;
        if (status != ERROR_SUCCESS) return status;
        if (lstrcmpiW(name, L"LastOpenedVersion") == 0) { ++index; continue; }
        status = RegDeleteValueW(key, name);
        if (status != ERROR_SUCCESS) return status;
        // The next value occupies this index after deletion.
    }
}

SETTINGS_BOOL_VALUE(DeselectOnOpen, L"DeselectOnOpen", DEFAULT_DESELECT_ON_OPEN)

bool supportsDefaultBrowser() {
    return !RegGetValue(HKEY_CLASSES_ROOT, KEY_DIRECTORY_VERB, L"",
        RRF_RT_ANY, nullptr, nullptr, nullptr);
}

DefaultBrowserResult setDefaultBrowser(bool value) {
    const wchar_t *data = value ? DATA_BROWSER_SET : DATA_BROWSER_CLEAR;
    DWORD size = value ? sizeof(DATA_BROWSER_SET) : sizeof(DATA_BROWSER_CLEAR);
    DefaultBrowserResult result = {};
    // Each association is independent; a failure must not skip the remaining writes.
    result.directory = RegSetKeyValue(HKEY_CURRENT_USER, KEY_DIRECTORY_BROWSER, L"", REG_SZ, data, size);
    result.compressedFolder = RegSetKeyValue(HKEY_CURRENT_USER, KEY_COMPRESSED_BROWSER, L"", REG_SZ, data, size);
    result.drive = RegSetKeyValue(HKEY_CURRENT_USER, KEY_DRIVE_BROWSER, L"", REG_SZ, data, size);
    return result;
}

}} // namespace
