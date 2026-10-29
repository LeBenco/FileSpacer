#include "ClosedWindows.h"
#include "CreateItemWindow.h"
#include "main.h"
#include "Settings.h"
#include "UIStrings.h"
#include "ShellUtils.h"
#include "WinUtils.h"
#include <vector>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <propkey.h>
#include <propvarutil.h>

namespace filespacer {
namespace {
std::wstring wide(const std::string &text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, nullptr, 0);
    if (!size) return {};
    std::wstring result(static_cast<size_t>(size), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, &result[0], size)) return {};
    result.pop_back();
    return result;
}
} // namespace

bool getClosedWindowInfo(IShellItem *item, const wchar_t *title, ClosedWindow *window) {
    const std::wstring display = title ? title : L"";
    CComHeapPtr<wchar_t> name;
    if (FAILED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &name))) return false;
    window->parsingName = toUtf8(name);
    window->displayName = toUtf8(display.c_str());
    return !window->parsingName.empty();
}

std::wstring closedWindowName(const ClosedWindow &window) {
    return wide(window.displayName.empty() ? window.parsingName : window.displayName);
}

std::wstring closedWindowPath(const ClosedWindow &window) {
    return wide(window.parsingName);
}

FolderOpenResult reopenLastWindow(HWND owner) {
    auto store = folderStateStore();
    ClosedWindow window;
    if (!store || !store->lastClosedWindow(&window)) {
        reportFolderStateError(owner);
        return FolderOpenResult::Failed;
    }
    if (!window.sequence) {
        MessageBoxW(owner, getString(IDS_REOPEN_EMPTY), getString(IDS_APP_NAME), MB_OK | MB_ICONINFORMATION);
        return FolderOpenResult::Failed;
    }
    if (!canReopenClosedWindow(window)) return FolderOpenResult::Failed;
    if (!store->takeClosedWindow(&window)) {
        reportFolderStateError(owner);
        return FolderOpenResult::Failed;
    }
    // Another process may have changed the latest entry or opened its folder
    // between the peek and the atomic take. Never skip it to an older closure.
    if (!canReopenClosedWindow(window)) {
        if (window.sequence && !store->returnClosedWindow(window)) reportFolderStateError(owner);
        refreshReopenJumpLists();
        return FolderOpenResult::Failed;
    }
    refreshReopenJumpLists();
    const std::wstring path = closedWindowPath(window);
    CComPtr<IShellItem> item;
    HRESULT result = path.empty() ? E_INVALIDARG
        : SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item));
    SFGAOF attributes = 0;
    if (SUCCEEDED(result)) result = item->GetAttributes(SFGAO_FOLDER, &attributes);
    FolderOpenResult opened = FolderOpenResult::Failed;
    if (SUCCEEDED(result) && (attributes & SFGAO_FOLDER))
        opened = openFolderWindow(item, owner ? MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST)
            : nullptr, SW_SHOWNORMAL, &window);
    if (opened == FolderOpenResult::Failed || opened == FolderOpenResult::Pending) {
        if (!store->returnClosedWindow(window)) reportFolderStateError(owner);
        refreshReopenJumpLists();
        if (opened == FolderOpenResult::Failed)
            MessageBoxW(owner, formatString(IDS_REOPEN_FAILED, path.c_str()).get(),
                getString(IDS_APP_NAME), MB_OK | MB_ICONERROR);
    }
    return opened;
}

namespace {
const wchar_t *jumpListRegistryKey() {
#ifdef FILESPACER_DEBUG
    if (settings::testMode) return L"Software\\FileSpacer\\Test\\JumpLists";
#endif
    return L"Software\\FileSpacer\\JumpLists";
}

bool openJumpListKey(const wchar_t *appID, CRegKey *key) {
    const std::wstring path = std::wstring(jumpListRegistryKey()) + L"\\" + appID;
    return checkHR(HRESULT_FROM_WIN32(key->Create(HKEY_CURRENT_USER, path.c_str(),
        nullptr, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE)));
}

std::wstring folderArguments(const wchar_t *path) {
    std::wstring arguments = quoteCommandArgument(path);
#ifdef FILESPACER_DEBUG
    if (settings::testMode) arguments += L" /test";
#endif
    return arguments;
}

} // namespace

HRESULT enumerateQuickAccessFolders(std::vector<CComPtr<IShellItem>> *folders, UINT *stage,
        bool strict) {
    // Only these two names depend on Explorer implementation details; they are
    // resolved at runtime and the failing step is reported through *stage.
    CComPtr<IShellItem> quickAccess;
    *stage = IDS_QUICK_ACCESS_OPEN;
    HRESULT result = SHCreateItemFromParsingName(L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}",
        nullptr, IID_PPV_ARGS(&quickAccess));
    if (FAILED(result)) return result;
    PROPERTYKEY pinnedKey = {};
    *stage = IDS_QUICK_ACCESS_KEY;
    result = PSGetPropertyKeyFromName(L"System.Home.IsPinned", &pinnedKey);
    if (FAILED(result)) return result;
    CComPtr<IEnumShellItems> items;
    *stage = IDS_QUICK_ACCESS_ENUM;
    result = quickAccess->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items));
    if (FAILED(result)) return result;
    for (;;) {
        CComPtr<IShellItem> item;
        *stage = IDS_QUICK_ACCESS_ENUM;
        result = items->Next(1, &item, nullptr);
        if (result == S_FALSE) return S_OK;
        if (FAILED(result)) return result;
        *stage = IDS_QUICK_ACCESS_READ;
        SFGAOF attributes = 0;
        BOOL pinned = FALSE;
        result = item->GetAttributes(SFGAO_FOLDER, &attributes);
        if (SUCCEEDED(result) && (attributes & SFGAO_FOLDER)) {
            CComQIPtr<IShellItem2> properties(item);
            result = properties ? properties->GetBool(pinnedKey, &pinned) : E_NOINTERFACE;
        }
        if (FAILED(result)) {
            if (strict) return result;
            checkHR(result); // The Jump List skips an unreadable entry.
            continue;
        }
        if (pinned) folders->push_back(item);
    }
}

namespace {

std::wstring jumpListFolderIcon(IShellItem *item) {
    CComHeapPtr<ITEMIDLIST> pidl;
    if (item && SUCCEEDED(SHGetIDListFromObject(item, &pidl))) {
        SHFILEINFOW image = {};
        SHGetFileInfoW(reinterpret_cast<const wchar_t *>(pidl.m_pData), 0, &image,
            sizeof(image), SHGFI_PIDL | SHGFI_ICON | SHGFI_SMALLICON);
        const std::wstring resource = folderRelaunchIcon(pidl, image.hIcon);
        if (image.hIcon) DestroyIcon(image.hIcon);
        if (!resource.empty()) return resource;
    }
    SHSTOCKICONINFO stock = {sizeof(stock)};
    if (!checkHR(SHGetStockIconInfo(SIID_FOLDER, SHGSI_ICONLOCATION, &stock))) return {};
    return std::wstring(stock.szPath) + L"," + std::to_wstring(stock.iIcon);
}

bool makeJumpListLink(const wchar_t *executable, const wchar_t *arguments,
        const wchar_t *name, const std::wstring &icon, IShellLinkW **result) {
    CComPtr<IShellLinkW> link;
    if (icon.empty() || !checkHR(link.CoCreateInstance(CLSID_ShellLink))) return false;
    std::wstring iconPath = icon;
    const int iconIndex = PathParseIconLocationW(&iconPath[0]);
    if (!checkHR(link->SetPath(executable)) || !checkHR(link->SetArguments(arguments))
            || !checkHR(link->SetIconLocation(iconPath.c_str(), iconIndex))) return false;
    CComQIPtr<IPropertyStore> properties(link);
    PROPVARIANT title = {};
    if (!properties || !checkHR(InitPropVariantFromString(name, &title))) return false;
    const bool named = checkHR(properties->SetValue(PKEY_Title, title));
    PropVariantClear(&title);
    return named && checkHR(properties->Commit()) && checkHR(link.CopyTo(result));
}

std::wstring removedDestinationKey(const wchar_t *arguments) {
    const std::wstring id = folderTaskbarID(arguments);
    return id.empty() ? std::wstring{} : L"Removed." + id;
}

bool rememberRemovedDestinations(IObjectArray *removed, CRegKey &key) {
    UINT count = 0;
    if (!checkHR(removed->GetCount(&count))) return false;
    for (UINT index = 0; index < count; ++index) {
        CComPtr<IUnknown> object;
        if (!checkHR(removed->GetAt(index, IID_PPV_ARGS(&object)))) return false;
        std::wstring arguments;
        CComQIPtr<IShellLinkW> link(object);
        if (link) {
            std::vector<wchar_t> buffer(32768);
            if (!checkHR(link->GetArguments(buffer.data(), static_cast<int>(buffer.size())))) return false;
            arguments = buffer.data();
        } else {
            CComQIPtr<IShellItem> item(object);
            CComHeapPtr<wchar_t> path;
            if (!item || !checkHR(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &path)))
                return false;
            arguments = folderArguments(path);
        }
        if (!arguments.empty()) {
            const std::wstring name = removedDestinationKey(arguments.c_str());
            if (name.empty()) return false;
            if (!checkHR(HRESULT_FROM_WIN32(key.SetDWORDValue(name.c_str(), 1)))) return false;
        }
    }
    return true;
}

bool buildJumpList(const wchar_t *appID, const wchar_t *executable, const wchar_t *reopenName,
        const std::wstring &reopenIcon,
        const std::vector<CComPtr<IShellLinkW>> &destinations) {
    CRegKey key;
    CComPtr<ICustomDestinationList> list;
    if (!openJumpListKey(appID, &key) || !checkHR(list.CoCreateInstance(CLSID_DestinationList))
            || !checkHR(list->SetAppID(appID))) return false;
    UINT slots = 0;
    CComPtr<IObjectArray> removed;
    if (!checkHR(list->BeginList(&slots, IID_PPV_ARGS(&removed)))) return false;
    const bool complete = [&]() {
        if (!rememberRemovedDestinations(removed, key)) return false;
        CComPtr<IObjectCollection> quickAccess;
        if (!checkHR(quickAccess.CoCreateInstance(CLSID_EnumerableObjectCollection))) return false;
        UINT added = 0;
        for (const auto &link : destinations) {
            std::vector<wchar_t> arguments(32768);
            if (!checkHR(link->GetArguments(arguments.data(), static_cast<int>(arguments.size())))) return false;
            DWORD excluded = 0;
            const std::wstring name = removedDestinationKey(arguments.data());
            if (name.empty()) return false;
            const LSTATUS status = key.QueryDWORDValue(name.c_str(), excluded);
            if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
                checkHR(HRESULT_FROM_WIN32(status));
                return false;
            }
            if (excluded) continue;
            if (!checkHR(quickAccess->AddObject(link))) return false;
            ++added;
        }
        if (added) {
            CComQIPtr<IObjectArray> array(quickAccess);
            const HRESULT category = list->AppendCategory(getString(IDS_QUICK_ACCESS), array);
            // Windows privacy settings can disallow destinations while permitting tasks.
            if (category != E_ACCESSDENIED && !checkHR(category)) return false;
        }
        if (!reopenName) return checkHR(list->CommitList());
        std::wstring arguments = L"/reopen-last";
#ifdef FILESPACER_DEBUG
        if (settings::testMode) arguments += L" /test";
#endif
        CComPtr<IShellLinkW> task;
        CComPtr<IObjectCollection> tasks;
        if (!makeJumpListLink(executable, arguments.c_str(), reopenName,
                reopenIcon, &task)
                || !checkHR(tasks.CoCreateInstance(CLSID_EnumerableObjectCollection))
                || !checkHR(tasks->AddObject(task))) return false;
        CComQIPtr<IObjectArray> array(tasks);
        return checkHR(list->AddUserTasks(array)) && checkHR(list->CommitList());
    }();
    if (!complete) checkHR(list->AbortList());
    return complete;
}

// Value names of the destinations the user removed from one Jump List.
bool removedDestinationNames(const wchar_t *appID, std::vector<std::wstring> *names) {
    CRegKey key;
    const std::wstring path = std::wstring(jumpListRegistryKey()) + L"\\" + appID;
    if (!checkHR(HRESULT_FROM_WIN32(key.Open(HKEY_CURRENT_USER, path.c_str(), KEY_READ)))) return false;
    for (DWORD index = 0;; ++index) {
        wchar_t name[16384] = {};
        DWORD size = ARRAYSIZE(name);
        const LSTATUS status = RegEnumValueW(key, index, name, &size, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) return true;
        if (!checkHR(HRESULT_FROM_WIN32(status))) return false;
        names->push_back(name);
    }
}

bool folderWindowOpen(const std::wstring &property) {
    struct Search { const std::wstring &property; bool found; } search{property, false};
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto &search = *reinterpret_cast<Search *>(data);
        if (GetPropW(window, search.property.c_str())) { search.found = true; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

// A key tagged by a window (StateKey) whose folder has no stored state and no
// open window in any process is stale. Untagged keys from earlier versions are
// kept: deleting one would forget the destinations the user removed from it.
bool jumpListKeyStale(FolderStateStore &store, const wchar_t *appID) {
    CRegKey key;
    const std::wstring path = std::wstring(jumpListRegistryKey()) + L"\\" + appID;
    if (key.Open(HKEY_CURRENT_USER, path.c_str(), KEY_READ) != ERROR_SUCCESS) return false;
    wchar_t tag[1024] = {};
    ULONG length = ARRAYSIZE(tag);
    if (key.QueryStringValue(L"StateKey", tag, &length) != ERROR_SUCCESS || !tag[0]) return false;
    std::string stateKey;
    for (const wchar_t *character = tag; *character; ++character) stateKey.push_back(static_cast<char>(*character));
    bool found = true;
    if (!store.contains(stateKey, &found) || found) return false;
    return !folderWindowOpen(tag); // The window property is the state key itself.
}

SRWLOCK refreshLock = SRWLOCK_INIT;
bool refreshRunning = false, refreshPending = false;
// Read and written only while the cross-process mutex is held. Memory only:
// every start rebuilds once, then skips rebuilds whose inputs are unchanged.
uint64_t builtFingerprint = 0;
bool built = false, purged = false;
} // namespace

void updateReopenJumpLists() {
    // Serialize writers across processes. Each reads the latest committed history
    // after acquiring the mutex, so an older refresh cannot overwrite a newer one.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\FileSpacer.JumpListUpdate");
    if (!mutex) return;
    const DWORD waited = WaitForSingleObject(mutex, INFINITE);
    if (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED) {
        [&]() {
            std::vector<wchar_t> executable(32768);
            const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
            if (!length || length >= executable.size()) return;
            const std::string path = folderStateDatabasePath();
            if (path.empty()) return;
            FolderStateStore store(path.c_str()); // This worker owns its SQLite connection.
            ClosedWindow last;
            if (!store.lastClosedWindow(&last)) return;
            std::wstring reopenName = getString(IDS_REOPEN_LAST);
            if (last.sequence)
                reopenName = formatString(IDS_REOPEN_NAMED, closedWindowName(last).c_str()).get();
            bool canReopen = canReopenClosedWindow(last);
            std::wstring reopenIcon;
            if (canReopen) {
                CComPtr<IShellItem> folder;
                SFGAOF attributes = 0;
                canReopen = SUCCEEDED(SHCreateItemFromParsingName(closedWindowPath(last).c_str(),
                        nullptr, IID_PPV_ARGS(&folder)))
                    && SUCCEEDED(folder->GetAttributes(SFGAO_FOLDER | SFGAO_VALIDATE, &attributes))
                    && (attributes & SFGAO_FOLDER);
                if (canReopen) reopenIcon = jumpListFolderIcon(folder);
            }

            struct Destination { std::wstring parsingName, name, icon; };
            std::vector<CComPtr<IShellItem>> folders;
            std::vector<Destination> pinned;
            UINT stage = 0;
            if (checkHR(enumerateQuickAccessFolders(&folders, &stage, false))) {
                for (const auto &folder : folders) {
                    CComHeapPtr<wchar_t> name, parsingName;
                    if (SUCCEEDED(folder->GetDisplayName(SIGDN_NORMALDISPLAY, &name))
                            && SUCCEEDED(folder->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsingName)))
                        pinned.push_back({parsingName.m_pData, name.m_pData, jumpListFolderIcon(folder)});
                }
            }
            CRegKey application;
            if (!openJumpListKey(APP_ID, &application)) return;
            application.Close();
            CRegKey known;
            if (!checkHR(HRESULT_FROM_WIN32(known.Open(HKEY_CURRENT_USER,
                    jumpListRegistryKey(), KEY_READ | KEY_WRITE)))) return;
            std::vector<std::wstring> appIDs;
            for (DWORD index = 0;; ++index) {
                wchar_t appID[129] = {};
                DWORD size = ARRAYSIZE(appID);
                const LSTATUS status = RegEnumKeyExW(known, index, appID, &size,
                    nullptr, nullptr, nullptr, nullptr);
                if (status == ERROR_NO_MORE_ITEMS) break;
                if (!checkHR(HRESULT_FROM_WIN32(status))) break;
                appIDs.push_back(appID);
            }
            if (!purged) {
                purged = true; // Once per process: the first update is the one at startup.
                for (auto it = appIDs.begin(); it != appIDs.end();) {
                    if (jumpListKeyStale(store, it->c_str())
                            && checkHR(HRESULT_FROM_WIN32(RegDeleteTreeW(known, it->c_str()))))
                        it = appIDs.erase(it);
                    else ++it;
                }
            }
            // Skip the rebuild when nothing it is built from has changed.
            JumpListModel model;
            model.add(executable.data());
            model.add(getString(IDS_QUICK_ACCESS));
            model.add(canReopen ? reopenName.c_str() : L"");
            model.add(reopenIcon);
            model.add(wide(last.key));
            for (const auto &entry : pinned) { model.add(entry.parsingName); model.add(entry.name); model.add(entry.icon); }
            for (const auto &appID : appIDs) {
                model.add(appID);
                std::vector<std::wstring> removedNames;
                if (!removedDestinationNames(appID.c_str(), &removedNames)) return;
                for (const auto &name : removedNames) model.add(name);
            }
            const uint64_t fingerprint = model.fingerprint();
            if (built && fingerprint == builtFingerprint) return;
            std::vector<CComPtr<IShellLinkW>> destinations;
            for (const auto &entry : pinned) {
                CComPtr<IShellLinkW> link;
                if (makeJumpListLink(executable.data(), folderArguments(entry.parsingName.c_str()).c_str(),
                        entry.name.c_str(), entry.icon, &link))
                    destinations.push_back(link);
            }
            bool complete = true;
            for (const auto &appID : appIDs)
                complete = buildJumpList(appID.c_str(), executable.data(), canReopen ? reopenName.c_str() : nullptr,
                    reopenIcon, destinations) && complete;
            if (complete) { built = true; builtFingerprint = fingerprint; }
        }();
        ReleaseMutex(mutex);
    }
    CloseHandle(mutex);
}

void refreshReopenJumpLists() {
    AcquireSRWLockExclusive(&refreshLock);
    refreshPending = true;
    if (refreshRunning) {
        ReleaseSRWLockExclusive(&refreshLock);
        return;
    }
    refreshRunning = true;
    lockProcess();
    ReleaseSRWLockExclusive(&refreshLock);
    if (!SHCreateThread([](void *) -> DWORD {
            for (;;) {
                AcquireSRWLockExclusive(&refreshLock);
                if (!refreshPending) {
                    refreshRunning = false;
                    ReleaseSRWLockExclusive(&refreshLock);
                    break;
                }
                refreshPending = false;
                ReleaseSRWLockExclusive(&refreshLock);
                updateReopenJumpLists();
            }
            unlockProcess();
            return 0;
        }, nullptr, CTF_COINIT_STA, nullptr)) {
        AcquireSRWLockExclusive(&refreshLock);
        refreshRunning = false;
        ReleaseSRWLockExclusive(&refreshLock);
        unlockProcess();
    }
}

void scheduleReopenJumpList(const std::wstring &appID, const std::string &stateKey) {
    // Keep identities after their windows close: a folder may still be pinned.
    CRegKey key;
    if (!openJumpListKey(appID.c_str(), &key)) return;
    if (!stateKey.empty()) checkHR(HRESULT_FROM_WIN32(key.SetStringValue(L"StateKey", wide(stateKey).c_str())));
    refreshReopenJumpLists();
}

} // namespace
