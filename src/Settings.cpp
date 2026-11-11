#include <common.h>
#include "Settings.h"
#include "FolderStateStore.h"
#include "UIStrings.h"
#include <strsafe.h>
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <knownfolders.h>
#include <windows.data.json.h>
#include <wrl/wrappers/corewrappers.h>
#include <cwchar>

#pragma comment(lib, "RuntimeObject.lib")

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
const wchar_t KEY_SERVER_BROWSER[]      = L"Software\\Classes\\NetServer\\Shell";
const wchar_t KEY_SHARE_BROWSER[]       = L"Software\\Classes\\NetShare\\Shell";
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

std::vector<int> getTitleMenu() {
    DWORD data[65] = {}, bytes = sizeof(data);
    if (RegGetValueW(HKEY_CURRENT_USER, KEY_SETTINGS, L"TitleMenu", RRF_RT_REG_BINARY,
            nullptr, data, &bytes) != ERROR_SUCCESS || bytes < sizeof(DWORD)
            || bytes % sizeof(DWORD) || data[0] != 1)
        return defaultTitleMenu();
    std::vector<int> items;
    for (DWORD i = 1; i < bytes / sizeof(DWORD); ++i) {
        if (data[i] >= TITLE_ENTRY_COUNT) return defaultTitleMenu();
        // Omit retired entries without discarding the rest of a saved composition.
        if (validTitleMenuEntry(static_cast<int>(data[i])))
            items.push_back(static_cast<int>(data[i]));
    }
    return validTitleMenu(items) ? items : defaultTitleMenu();
}

LSTATUS setTitleMenu(const std::vector<int> &items) {
    if (!validTitleMenu(items)) return ERROR_INVALID_DATA;
    std::vector<DWORD> data{1}; // Schema version; a version alone is an empty menu.
    for (int entry : items) data.push_back(static_cast<DWORD>(entry));
    return setSettingsValue(L"TitleMenu", REG_BINARY, data.data(),
        static_cast<DWORD>(data.size() * sizeof(DWORD)));
}

LSTATUS getFolderGroups(std::vector<FolderGroup> &groups) {
    groups.clear();
    const std::wstring path = std::wstring(KEY_SETTINGS) + L"\\FolderGroups";
    CRegKey key;
    LSTATUS status = key.Open(HKEY_CURRENT_USER, path.c_str(), KEY_QUERY_VALUE);
    if (status == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (status != ERROR_SUCCESS) return status;
    for (DWORD index = 0;; ++index) {
        wchar_t name[16384] = {}; DWORD nameLength = _countof(name), bytes = 0, type = 0;
        status = RegEnumValueW(key, index, name, &nameLength, nullptr, &type, nullptr, &bytes);
        if (status == ERROR_NO_MORE_ITEMS) return ERROR_SUCCESS;
        if (status != ERROR_SUCCESS) return status;
        if (type != REG_MULTI_SZ || bytes < 2 * sizeof(wchar_t) || bytes > 1048576
                || bytes % sizeof(wchar_t)) return ERROR_INVALID_DATA;
        std::vector<wchar_t> value(bytes / sizeof(wchar_t), L'\0');
        status = RegGetValueW(key, nullptr, name, RRF_RT_REG_MULTI_SZ, nullptr, value.data(), &bytes);
        if (status != ERROR_SUCCESS) return status;
        const size_t length = bytes / sizeof(wchar_t);
        if (length < 2 || value[length - 1] || value[length - 2]) return ERROR_INVALID_DATA;
        FolderGroup group; group.name.assign(name, nameLength);
        for (size_t at = 0; at < length && value[at];) {
            const auto end = std::find(value.begin() + at, value.begin() + length, L'\0');
            if (end == value.begin() + length) return ERROR_INVALID_DATA;
            group.paths.emplace_back(value.data() + at, static_cast<size_t>(end - value.begin()) - at);
            at = static_cast<size_t>(end - value.begin()) + 1;
        }
        groups.push_back(std::move(group));
    }
}

LSTATUS setFolderGroup(const FolderGroup &group) {
    if (group.name.empty() || group.name.size() > 128 || group.paths.empty()
            || group.name.find(L'\0') != std::wstring::npos) return ERROR_INVALID_PARAMETER;
    std::vector<wchar_t> value;
    for (const auto &path : group.paths) {
        if (path.empty() || path.find(L'\0') != std::wstring::npos) return ERROR_INVALID_PARAMETER;
        value.insert(value.end(), path.begin(), path.end()); value.push_back(L'\0');
        if (value.size() > 524287) return ERROR_INVALID_DATA;
    }
    value.push_back(L'\0');
    const std::wstring path = std::wstring(KEY_SETTINGS) + L"\\FolderGroups";
    return RegSetKeyValueW(HKEY_CURRENT_USER, path.c_str(), group.name.c_str(), REG_MULTI_SZ,
        value.data(), static_cast<DWORD>(value.size() * sizeof(wchar_t)));
}

LSTATUS removeFolderGroup(const std::wstring &name) {
    const std::wstring path = std::wstring(KEY_SETTINGS) + L"\\FolderGroups";
    return RegDeleteKeyValueW(HKEY_CURRENT_USER, path.c_str(), name.c_str());
}

wstr_ptr getSettingsString(const wchar_t *name, DWORD type, const wchar_t *defaultValue) {
    DWORD size;
    if (!RegGetValue(HKEY_CURRENT_USER, KEY_SETTINGS, name, type, nullptr, nullptr, &size)) {
        wstr_ptr buffer(new wchar_t[size / sizeof(wchar_t)]);
        // The value may grow between the two reads (ERROR_MORE_DATA leaves the buffer undefined).
        if (!RegGetValue(HKEY_CURRENT_USER, KEY_SETTINGS, name, type, nullptr, buffer.get(), &size))
            return buffer;
    }
    DWORD count = lstrlen(defaultValue) + 1;
    wstr_ptr buffer(new wchar_t[count]);
    CopyMemory(buffer.get(), defaultValue, count * sizeof(wchar_t));
    return buffer;
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
SETTINGS_BOOL_VALUE(UpdateCheckEnabled, L"UpdateCheckEnabled", DEFAULT_UPDATE_CHECK_ENABLED);
SETTINGS_QWORD_VALUE(LastUpdateCheck, LONGLONG, L"LastUpdateCheck", DEFAULT_LAST_UPDATE_CHECK)
SETTINGS_QWORD_VALUE(UpdateCheckRate, LONGLONG, L"UpdateCheckRate", DEFAULT_UPDATE_CHECK_RATE)

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
SETTINGS_BOOL_VALUE(ViewMenuPreviewEnabled, L"ViewMenuPreviewEnabled", DEFAULT_VIEW_MENU_PREVIEW_ENABLED)
SETTINGS_BOOL_VALUE(DetailsColumnsEnabled, L"DetailsColumnsEnabled", true)
SETTINGS_BOOL_VALUE(DetailsColumnsDeferred, L"DetailsColumnsDeferred", true)
SETTINGS_BOOL_VALUE(GroupFolderWindows, L"GroupFolderWindows", DEFAULT_GROUP_FOLDER_WINDOWS)
SETTINGS_BOOL_VALUE(KeepSourceWindowOpen, L"KeepSourceWindowOpen", DEFAULT_KEEP_SOURCE_WINDOW_OPEN)
SETTINGS_BOOL_VALUE(OpeningAnimationEnabled, L"OpeningAnimationEnabled", DEFAULT_OPENING_ANIMATION_ENABLED)
SETTINGS_BOOL_VALUE(SmartWindowPlacement, L"SmartWindowPlacement", DEFAULT_SMART_WINDOW_PLACEMENT)
SETTINGS_BOOL_VALUE(KeepSelectionOnActivate, L"KeepSelectionOnActivate", DEFAULT_KEEP_SELECTION_ON_ACTIVATE)
SETTINGS_BOOL_VALUE(LiveNameSearch, L"LiveNameSearch", DEFAULT_LIVE_NAME_SEARCH)
SETTINGS_BOOL_VALUE(EverythingEnabled, L"EverythingEnabled", false)
SETTINGS_BOOL_VALUE(PeekEnabled, L"PeekEnabled", false)
SETTINGS_STRING_VALUE(EverythingPath, REG_SZ, L"EverythingPath", L"")

DWORD getOpeningAnimationDuration() {
    DWORD value = DEFAULT_OPENING_ANIMATION_DURATION;
    getSettingsValue(L"OpeningAnimationDuration", RRF_RT_DWORD, &value, sizeof(value));
    return value <= MAX_OPENING_ANIMATION_DURATION ? value : MAX_OPENING_ANIMATION_DURATION;
}

void setOpeningAnimationDuration(DWORD value) {
    if (value <= MAX_OPENING_ANIMATION_DURATION)
        setSettingsValue(L"OpeningAnimationDuration", REG_DWORD, &value, sizeof(value));
}

DWORD getSearchHistoryLimit() {
    DWORD value = DEFAULT_SEARCH_HISTORY_LIMIT;
    getSettingsValue(L"SearchHistoryLimit", RRF_RT_DWORD, &value, sizeof(value));
    return value > MAX_SEARCH_HISTORY_LIMIT ? MAX_SEARCH_HISTORY_LIMIT : value;
}

bool setSearchHistoryLimit(DWORD value) {
    if (value > MAX_SEARCH_HISTORY_LIMIT) return false;
    const DWORD previous = getSearchHistoryLimit();
    if (setSettingsValue(L"SearchHistoryLimit", REG_DWORD, &value, sizeof(value)) != ERROR_SUCCESS)
        return false;
    if (value == previous) return true; // Persist the choice without touching unchanged histories.
    auto store = folderStateStore();
    if (!store || !store->trimSearchHistory(value)) {
        setSettingsValue(L"SearchHistoryLimit", REG_DWORD, &previous, sizeof(previous));
        return false;
    }
    return true;
}

EverythingScope getEverythingScope() {
    DWORD value = 0;
    getSettingsValue(L"EverythingScope", RRF_RT_DWORD, &value, sizeof(value));
    return value == 1 ? EverythingScope::Everywhere : EverythingScope::Folder;
}

void setEverythingScope(EverythingScope value) {
    const DWORD scope = static_cast<DWORD>(value);
    setSettingsValue(L"EverythingScope", REG_DWORD, &scope, sizeof(scope));
}
SETTINGS_BOOL_VALUE(MixedFolderSorting, L"MixedFolderSorting", false)
SETTINGS_BOOL_VALUE(FullNamesOnSelection, L"FullNamesOnSelection", DEFAULT_FULL_NAMES_ON_SELECTION)
SETTINGS_BOOL_VALUE(WindowStateLockBadgeEnabled, L"WindowStateLockBadgeEnabled", true)
SETTINGS_BOOL_VALUE(WindowStateUnlockBadgeEnabled, L"WindowStateUnlockBadgeEnabled", false)
SETTINGS_BOOL_VALUE(WindowStateLockClickEnabled, L"WindowStateLockClickEnabled", false)


bool getWindowStateLockEnabled() {
    DWORD value = 0;
    getSettingsValue(L"WindowStateLockEnabled", RRF_RT_DWORD, &value, sizeof(value));
    return value != 0;
}

namespace {
using JsonObject = ABI::Windows::Data::Json::IJsonObject;
using namespace Microsoft::WRL::Wrappers;

CComPtr<JsonObject> readPowerToysJson(const std::wstring &path) {
    CComPtr<IStream> stream;
    if (FAILED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE,
            0, FALSE, nullptr, &stream))) return {};
    STATSTG info = {};
    if (FAILED(stream->Stat(&info, STATFLAG_NONAME)) || info.cbSize.QuadPart == 0
            || info.cbSize.QuadPart > 1048576) return {};
    std::string bytes(static_cast<size_t>(info.cbSize.QuadPart), '\0');
    ULONG read = 0;
    if (FAILED(stream->Read(&bytes[0], static_cast<ULONG>(bytes.size()), &read))
            || read != bytes.size()) return {};
    stream.Release();
    const int offset = bytes.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    const int size = static_cast<int>(bytes.size()) - offset;
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + offset, size, nullptr, 0);
    if (!length) return {};
    std::wstring text(length, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + offset, size, &text[0], length)) return {};
    CComPtr<ABI::Windows::Data::Json::IJsonObjectStatics> factory;
    CComPtr<JsonObject> root;
    if (FAILED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Data_Json_JsonObject).Get(), IID_PPV_ARGS(&factory)))
            || FAILED(factory->Parse(HStringReference(text.c_str(), static_cast<UINT32>(text.size())).Get(), &root))) return {};
    return root;
}

std::wstring powerToysWindowTitle(HWND window) {
    wchar_t text[255] = {};
    const int length = window ? GetWindowTextW(window, text, _countof(text)) : 0;
    if (length > 0) CharUpperBuffW(text, length);
    return text;
}

struct ProcessHandleDeleter { void operator()(HANDLE handle) const { CloseHandle(handle); } };
std::wstring powerToysProcessPath(DWORD processId) {
    std::unique_ptr<void, ProcessHandleDeleter> owned;
    HANDLE process = GetCurrentProcess();
    if (processId != GetCurrentProcessId()) {
        owned.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
        process = owned.get();
    }
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!process || !QueryFullProcessImageNameW(process, 0, &path[0], &length)) return {};
    path.resize(length); CharUpperBuffW(&path[0], length);
    return path;
}

bool powerToysExcludes(JsonObject *properties, const wchar_t *name,
        const std::wstring &processPath, const std::wstring &title) {
    CComPtr<JsonObject> property;
    HString value;
    if (FAILED(properties->GetNamedObject(HStringReference(name).Get(), &property))
            || FAILED(property->GetNamedString(HStringReference(L"value").Get(), value.GetAddressOf()))) return false;
    UINT32 length = 0;
    const wchar_t *buffer = value.GetRawBuffer(&length);
    if (!length) return false;
    std::wstring rows(buffer, length);
    CharUpperBuffW(&rows[0], length);
    // Same matching rules as PowerToys common/utils/excluded_apps.h and
    // GrabAndMove::ParseExcludedApps: leading whitespace, literal rows, no glob.
    for (size_t start = rows.find_first_not_of(L" \t\r\n"); start != std::wstring::npos;) {
        const size_t end = rows.find_first_of(L"\r\n", start);
        const std::wstring row = rows.substr(start, end == std::wstring::npos ? end : end - start);
        const size_t match = processPath.rfind(row), slash = processPath.rfind(L'\\');
        if ((match != std::wstring::npos && match <= slash + 1 && match + row.size() > slash)
                || title.find(row) != std::wstring::npos) return true;
        start = end == std::wstring::npos ? end : rows.find_first_not_of(L" \t\r\n", end);
    }
    return false;
}
} // namespace

bool powerToysOverridesWindowGestures(HWND window) {
    CComHeapPtr<wchar_t> localAppData;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &localAppData))) return false;
    const std::wstring directory = std::wstring(localAppData) + L"\\Microsoft\\PowerToys\\";
    RoInitializeWrapper runtime(RO_INIT_SINGLETHREADED);
    if (FAILED(runtime)) return false;
    const auto general = readPowerToysJson(directory + L"settings.json");
    CComPtr<JsonObject> enabled;
    boolean active = 0;
    if (!general || FAILED(general->GetNamedObject(HStringReference(L"enabled").Get(), &enabled))
            || FAILED(enabled->GetNamedBoolean(HStringReference(L"GrabAndMove").Get(), &active)) || !active) return false;
    // Saved activation is preventive, even while PowerToys is stopped.
    // Missing/unreadable module settings cannot establish an exclusion.
    const auto module = readPowerToysJson(directory + L"GrabAndMove\\settings.json");
    CComPtr<JsonObject> properties;
    if (!module || FAILED(module->GetNamedObject(HStringReference(L"properties").Get(), &properties))) return true;
    const std::wstring executable = powerToysProcessPath(GetCurrentProcessId());
    if (executable.empty()) return true;
    if (powerToysExcludes(properties, L"excluded_apps", executable, powerToysWindowTitle(window))) return false;
    const HWND foreground = GetForegroundWindow();
    DWORD process = 0;
    GetWindowThreadProcessId(foreground, &process);
    // The modifier list is tested on the foreground window by PowerToys;
    // it is not a global exemption for a background FileSpacer window.
    return !powerToysExcludes(properties, L"excluded_apps_for_modifier",
        process == GetCurrentProcessId() ? executable : powerToysProcessPath(process), powerToysWindowTitle(foreground));
}

bool windowShortcutApplies(const WindowShortcut &binding, GestureButton button, unsigned modifiers, HWND window) {
    // No JSON I/O for mouse events that do not match a configured gesture.
    return matchesShortcut(binding, button, modifiers) && !powerToysOverridesWindowGestures(window);
}

WindowShortcuts getWindowShortcuts() {
    DWORD value = 0;
    if (getSettingsValue(L"WindowShortcuts", RRF_RT_DWORD, &value, sizeof(value)) != ERROR_SUCCESS
            || (value & 0xffffc0c0u) != 0x01000000u)
        return DEFAULT_WINDOW_SHORTCUTS;
    auto decode = [](DWORD bits) -> WindowShortcut {
        return {(bits & 32u) != 0, bits & 7u, static_cast<GestureButton>((bits >> 3) & 3u)};
    };
    const WindowShortcuts bindings = {decode(value), decode(value >> 8)};
    return validShortcuts(bindings) ? bindings : DEFAULT_WINDOW_SHORTCUTS;
}

LSTATUS setWindowShortcuts(const WindowShortcuts &bindings) {
    if (!validShortcuts(bindings)) return ERROR_INVALID_DATA;
    auto encode = [](const WindowShortcut &binding) -> DWORD {
        return (binding.enabled ? 32u : 0u) | binding.modifiers | (static_cast<DWORD>(binding.button) << 3);
    };
    // One registry value commits both actions atomically, including swaps.
    const DWORD value = 0x01000000u | encode(bindings.move) | (encode(bindings.resize) << 8);
    return setSettingsValue(L"WindowShortcuts", REG_DWORD, &value, sizeof(value));
}

bool setWindowStateLockEnabled(bool value) {
    const bool previous = getWindowStateLockEnabled();
    const DWORD enabled = value ? 1 : 0;
    if (setSettingsValue(L"WindowStateLockEnabled", REG_DWORD, &enabled, sizeof(enabled)) != ERROR_SUCCESS)
        return false;
    if (!value) {
        auto store = folderStateStore();
        if (!store || !store->clearLocks()) {
            const DWORD restore = previous ? 1 : 0;
            setSettingsValue(L"WindowStateLockEnabled", REG_DWORD, &restore, sizeof(restore));
            return false;
        }
    }
    return true;
}


LSTATUS resetGlobalOptions() {
    // Delete current/future preference values, preserving version metadata and subkeys
    // (including the separate Debug /test namespace). Windows associations are elsewhere.
    // Folder state locks live in the state database; /reset-options releases them
    // separately so an unavailable database still leaves the Registry reset.
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
    const wchar_t *const verbs[] = {KEY_DIRECTORY_VERB,
        L"NetServer\\shell\\filespacer", L"NetShare\\shell\\filespacer"};
    for (const wchar_t *verb : verbs) {
        if (RegGetValue(HKEY_CLASSES_ROOT, verb, L"",
                RRF_RT_ANY, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            return false;
    }
    return true;
}

namespace {
LSTATUS setMenuString(const std::wstring &key, const wchar_t *name, DWORD type,
        const wchar_t *value, bool &changed) {
    const DWORD size = (lstrlenW(value) + 1) * sizeof(wchar_t);
    std::wstring current(size / sizeof(wchar_t), L'\0');
    DWORD actualSize = size, actualType = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_ANY | RRF_NOEXPAND,
            &actualType, &current[0], &actualSize) == ERROR_SUCCESS
            && actualType == type && actualSize == size
            && memcmp(current.data(), value, size) == 0) return ERROR_SUCCESS;
    const LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, type, value, size);
    if (status == ERROR_SUCCESS) changed = true;
    return status;
}

LSTATUS removeMenuValue(const std::wstring &key, bool &changed) {
    const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, key.c_str(), L"ProgrammaticAccessOnly");
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) return ERROR_SUCCESS;
    if (status == ERROR_SUCCESS) changed = true;
    return status;
}

LSTATUS synchronizeMenu(const std::wstring &shell, bool explorer, bool background, bool &changed) {
    const std::wstring verb = shell + L"\\filespacer";
    const std::wstring alternate = L"Software\\Classes\\" + shell + L"\\filespacer.explorer";
    const std::wstring userVerb = L"Software\\Classes\\" + verb;
    // Only update installed FileSpacer menus; portable and /test launches install nothing.
    const LSTATUS installed = RegGetValueW(HKEY_CLASSES_ROOT, verb.c_str(), nullptr,
        RRF_RT_REG_SZ, nullptr, nullptr, nullptr);
    if (installed != ERROR_SUCCESS && installed != ERROR_FILE_NOT_FOUND
            && installed != ERROR_PATH_NOT_FOUND) return installed;
    if (!explorer || installed != ERROR_SUCCESS) {
        const LSTATUS visible = removeMenuValue(userVerb, changed);
        const LSTATUS removed = RegDeleteTreeW(HKEY_CURRENT_USER, alternate.c_str());
        if (removed == ERROR_SUCCESS) changed = true;
        if (visible != ERROR_SUCCESS) return visible;
        return removed == ERROR_FILE_NOT_FOUND || removed == ERROR_PATH_NOT_FOUND ? ERROR_SUCCESS : removed;
    }
    LSTATUS status = setMenuString(alternate, nullptr, REG_SZ, getString(IDS_OPEN_IN_EXPLORER), changed);
    if (status == ERROR_SUCCESS) status = setMenuString(alternate, L"Icon", REG_EXPAND_SZ,
        L"%SystemRoot%\\explorer.exe,0", changed);
    if (status == ERROR_SUCCESS) status = setMenuString(alternate + L"\\command", nullptr, REG_EXPAND_SZ,
        background ? L"\"%SystemRoot%\\explorer.exe\" \"%V\"" : L"\"%SystemRoot%\\explorer.exe\" \"%1\"", changed);
    // A verb left without "command" would still be listed by the Shell: never keep a partial write.
    if (status != ERROR_SUCCESS) {
        RegDeleteTreeW(HKEY_CURRENT_USER, alternate.c_str());
        return status;
    }
    // Default-only Shell menus omit programmatic-only verbs, even the selected default.
    // Hide only the background entry, which is never used to open an item.
    if (status == ERROR_SUCCESS) status = background
        ? setMenuString(userVerb, L"ProgrammaticAccessOnly", REG_SZ, L"", changed)
        : removeMenuValue(userVerb, changed);
    return status;
}

LSTATUS synchronizeType(const wchar_t *type, bool &changed) {
    const std::wstring shell = std::wstring(type) + L"\\shell";
    DWORD size = 0;
    LSTATUS status = RegGetValueW(HKEY_CLASSES_ROOT, shell.c_str(), nullptr,
        RRF_RT_REG_SZ, nullptr, nullptr, &size);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
        return status;
    std::wstring browser(size / sizeof(wchar_t) + 1, L'\0');
    if (status == ERROR_SUCCESS) {
        status = RegGetValueW(HKEY_CLASSES_ROOT, shell.c_str(), nullptr,
            RRF_RT_REG_SZ, nullptr, &browser[0], &size);
        if (status != ERROR_SUCCESS) return status;
    }
    // Shell's default value may be an ordered list of verbs separated by spaces or commas.
    wchar_t *first = &browser[0] + std::wcsspn(browser.c_str(), L" ,");
    first[std::wcscspn(first, L" ,")] = L'\0';
    const bool explorer = lstrcmpiW(first, DATA_BROWSER_SET) == 0;
    const LSTATUS menu = synchronizeMenu(shell, explorer, false, changed);
    if (lstrcmpW(type, L"Directory") != 0) return menu;
    const LSTATUS background = synchronizeMenu(L"Directory\\Background\\shell", explorer, true, changed);
    return menu != ERROR_SUCCESS ? menu : background;
}
}

DefaultBrowserResult synchronizeContextMenus() {
    bool changed = false;
    DefaultBrowserResult result = {};
    result.directory = synchronizeType(L"Directory", changed);
    result.compressedFolder = synchronizeType(L"CompressedFolder", changed);
    result.drive = synchronizeType(L"Drive", changed);
    result.networkServer = synchronizeType(L"NetServer", changed);
    result.networkShare = synchronizeType(L"NetShare", changed);
    if (changed) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return result;
}

DefaultBrowserResult setDefaultBrowser(bool value) {
    const wchar_t *data = value ? DATA_BROWSER_SET : DATA_BROWSER_CLEAR;
    DWORD size = value ? sizeof(DATA_BROWSER_SET) : sizeof(DATA_BROWSER_CLEAR);
    DefaultBrowserResult result = {};
    // Each association is independent; a failure must not skip the remaining writes.
    result.directory = RegSetKeyValue(HKEY_CURRENT_USER, KEY_DIRECTORY_BROWSER, L"", REG_SZ, data, size);
    result.compressedFolder = RegSetKeyValue(HKEY_CURRENT_USER, KEY_COMPRESSED_BROWSER, L"", REG_SZ, data, size);
    result.drive = RegSetKeyValue(HKEY_CURRENT_USER, KEY_DRIVE_BROWSER, L"", REG_SZ, data, size);
    result.networkServer = RegSetKeyValue(HKEY_CURRENT_USER, KEY_SERVER_BROWSER, L"", REG_SZ, data, size);
    result.networkShare = RegSetKeyValue(HKEY_CURRENT_USER, KEY_SHARE_BROWSER, L"", REG_SZ, data, size);
    const DefaultBrowserResult menus = synchronizeContextMenus();
    if (result.directory == ERROR_SUCCESS) result.directory = menus.directory;
    if (result.compressedFolder == ERROR_SUCCESS) result.compressedFolder = menus.compressedFolder;
    if (result.drive == ERROR_SUCCESS) result.drive = menus.drive;
    if (result.networkServer == ERROR_SUCCESS) result.networkServer = menus.networkServer;
    if (result.networkShare == ERROR_SUCCESS) result.networkShare = menus.networkShare;
    // Associations can change even when the menus were already synchronized.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return result;
}

}} // namespace
