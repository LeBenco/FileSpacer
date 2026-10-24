#include "ClosedWindows.h"
#include "CreateItemWindow.h"
#include "main.h"
#include "Settings.h"
#include "UIStrings.h"
#include "ShellUtils.h"
#include "WinUtils.h"
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

bool getQuickAccessFolders(std::vector<CComPtr<IShellItem>> *folders) {
    // Use the same Explorer namespace/property as the existing toolbar menu.
    // Their names are Explorer implementation details, resolved at runtime.
    CComPtr<IShellItem> quickAccess;
    CComPtr<IEnumShellItems> items;
    PROPERTYKEY pinnedKey = {};
    if (!checkHR(SHCreateItemFromParsingName(L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}",
            nullptr, IID_PPV_ARGS(&quickAccess)))
            || !checkHR(PSGetPropertyKeyFromName(L"System.Home.IsPinned", &pinnedKey))
            || !checkHR(quickAccess->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items))))
        return false;
    for (;;) {
        CComPtr<IShellItem> item;
        const HRESULT next = items->Next(1, &item, nullptr);
        if (next == S_FALSE) return true;
        if (!checkHR(next)) return false;
        SFGAOF attributes = 0;
        if (!checkHR(item->GetAttributes(SFGAO_FOLDER, &attributes))) continue;
        if (!(attributes & SFGAO_FOLDER)) continue;
        CComQIPtr<IShellItem2> properties(item);
        BOOL pinned = FALSE;
        if (!properties || !checkHR(properties->GetBool(pinnedKey, &pinned))) continue;
        if (pinned) folders->push_back(item);
    }
}

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
            wchar_t buffer[32768] = {};
            if (!checkHR(link->GetArguments(buffer, ARRAYSIZE(buffer)))) return false;
            arguments = buffer;
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

void buildJumpList(const wchar_t *appID, const wchar_t *executable, const wchar_t *reopenName,
        const std::wstring &reopenIcon,
        const std::vector<CComPtr<IShellLinkW>> &destinations) {
    CRegKey key;
    CComPtr<ICustomDestinationList> list;
    if (!openJumpListKey(appID, &key) || !checkHR(list.CoCreateInstance(CLSID_DestinationList))
            || !checkHR(list->SetAppID(appID))) return;
    UINT slots = 0;
    CComPtr<IObjectArray> removed;
    if (!checkHR(list->BeginList(&slots, IID_PPV_ARGS(&removed)))) return;
    const bool complete = [&]() {
        if (!rememberRemovedDestinations(removed, key)) return false;
        CComPtr<IObjectCollection> quickAccess;
        if (!checkHR(quickAccess.CoCreateInstance(CLSID_EnumerableObjectCollection))) return false;
        UINT added = 0;
        for (const auto &link : destinations) {
            wchar_t arguments[32768] = {};
            if (!checkHR(link->GetArguments(arguments, ARRAYSIZE(arguments)))) return false;
            DWORD excluded = 0;
            const std::wstring name = removedDestinationKey(arguments);
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
}

SRWLOCK refreshLock = SRWLOCK_INIT;
bool refreshRunning = false, refreshPending = false;
} // namespace

void updateReopenJumpLists() {
    // Serialize writers across processes. Each reads the latest committed history
    // after acquiring the mutex, so an older refresh cannot overwrite a newer one.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\FileSpacer.JumpListUpdate");
    if (!mutex) return;
    const DWORD waited = WaitForSingleObject(mutex, INFINITE);
    if (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED) {
        [&]() {
            wchar_t executable[32768] = {};
            const DWORD length = GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
            if (!length || length >= ARRAYSIZE(executable)) return;
            const std::string path = folderStateDatabasePath();
            if (path.empty()) return;
            FolderStateStore store(path.c_str()); // This worker owns its SQLite connection.
            ClosedWindow last;
            if (!store.lastClosedWindow(&last)) return;
            std::wstring reopenName = getString(IDS_REOPEN_LAST);
            if (last.sequence)
                reopenName = formatString(IDS_REOPEN_NAMED, closedWindowName(last).c_str()).get();
            const bool canReopen = canReopenClosedWindow(last);
            std::wstring reopenIcon;
            if (canReopen) {
                CComPtr<IShellItem> folder;
                if (checkHR(SHCreateItemFromParsingName(closedWindowPath(last).c_str(),
                        nullptr, IID_PPV_ARGS(&folder))))
                    reopenIcon = jumpListFolderIcon(folder);
                else
                    reopenIcon = jumpListFolderIcon(nullptr); // Unavailable folder: native stock icon.
            }

            std::vector<CComPtr<IShellItem>> folders;
            std::vector<CComPtr<IShellLinkW>> destinations;
            if (getQuickAccessFolders(&folders)) {
                for (const auto &folder : folders) {
                    CComHeapPtr<wchar_t> name, parsingName;
                    CComPtr<IShellLinkW> link;
                    if (SUCCEEDED(folder->GetDisplayName(SIGDN_NORMALDISPLAY, &name))
                            && SUCCEEDED(folder->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsingName))
                            && makeJumpListLink(executable, folderArguments(parsingName).c_str(), name,
                                jumpListFolderIcon(folder), &link))
                        destinations.push_back(link);
                }
            }
            CRegKey application;
            if (!openJumpListKey(APP_ID, &application)) return;
            application.Close();
            CRegKey known;
            if (!checkHR(HRESULT_FROM_WIN32(known.Open(HKEY_CURRENT_USER,
                    jumpListRegistryKey(), KEY_READ)))) return;
            for (DWORD index = 0;; ++index) {
                wchar_t appID[129] = {};
                DWORD size = ARRAYSIZE(appID);
                const LSTATUS status = RegEnumKeyExW(known, index, appID, &size,
                    nullptr, nullptr, nullptr, nullptr);
                if (status == ERROR_NO_MORE_ITEMS) break;
                if (!checkHR(HRESULT_FROM_WIN32(status))) break;
                buildJumpList(appID, executable, canReopen ? reopenName.c_str() : nullptr,
                    reopenIcon, destinations);
            }
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

void scheduleReopenJumpList(const std::wstring &appID) {
    // Keep identities after their windows close: a folder may still be pinned.
    CRegKey key;
    if (openJumpListKey(appID.c_str(), &key)) refreshReopenJumpLists();
}

} // namespace
