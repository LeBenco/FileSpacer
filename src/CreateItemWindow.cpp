#include "CreateItemWindow.h"
#include "FolderWindow.h"
#include "FolderIdentity.h"
#include <ctime>
#include <algorithm>
#include <vector>
#include "UIStrings.h"
#include <Shlguid.h>
#include <shlobj.h>
#include <shellapi.h>
#include <sherrors.h>
#include <strsafe.h>
#include <propkey.h>

namespace filespacer {

// Serialize lookup + publication, not the lifetime or loading of the folder view.
static thread_local std::vector<std::wstring> pendingOpens;

class FolderOpenLock {
public:
    explicit FolderOpenLock(const std::wstring &property) {
        std::wstring name = L"Local\\" + property + L".Open";
        mutex = CreateMutexW(nullptr, FALSE, name.c_str());
        if (!mutex)
            return;
        pendingOpens.push_back(property);
        registered = true;
        DWORD wait = WaitForSingleObject(mutex, 0);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
            acquired = true;
        } else if (wait == WAIT_TIMEOUT) {
            // Keep STA COM/window messages running while another process creates the window.
            DWORD index = MAXDWORD;
            SetLastError(ERROR_SUCCESS);
            HRESULT hr = CoWaitForMultipleHandles(
                COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES,
                10000, 1, &mutex, &index);
            acquired = SUCCEEDED(hr) && (index == 0 || index == WAIT_ABANDONED_0);
        }
    }
    ~FolderOpenLock() {
        if (acquired)
            ReleaseMutex(mutex);
        if (mutex)
            CloseHandle(mutex);
        if (registered)
            pendingOpens.pop_back();
    }
    explicit operator bool() const { return acquired; }
    FolderOpenLock(const FolderOpenLock &) = delete;
    FolderOpenLock &operator=(const FolderOpenLock &) = delete;
private:
    HANDLE mutex = nullptr;
    bool acquired = false;
    bool registered = false;
};

struct FolderWindowSearch {
    const std::wstring &property;
    HWND window = nullptr;
};

static BOOL CALLBACK findFolderWindow(HWND window, LPARAM data) {
    auto &search = *reinterpret_cast<FolderWindowSearch *>(data);
    wchar_t className[64] = {};
    if (GetClassNameW(window, className, ARRAYSIZE(className))
            && lstrcmpW(className, L"FileSpacer Item Window") == 0
            && GetPropW(window, search.property.c_str())) {
        search.window = window;
        SetLastError(ERROR_SUCCESS);
        return FALSE;
    }
    return TRUE;
}

bool isFolderAccessFailure(HRESULT result) {
    return FAILED(result) && result != E_ABORT && result != E_OUTOFMEMORY
        && result != E_INVALIDARG && result != E_NOINTERFACE && result != E_NOTIMPL
        && result != HRESULT_FROM_WIN32(ERROR_CANCELLED)
        && result != HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED)
        && result != COPYENGINE_E_USER_CANCELLED;
}

void recordFolderPathFailure(const wchar_t *path, HRESULT result) {
    if (!path || !*path || !isFolderAccessFailure(result)) return;
    const std::wstring property = folderWindowProperty(getPathFolderIdentity(path));
    if (property.empty()) return; // No heuristic lookup of an unavailable local identity.
    std::string key; // ASCII window-property name.
    for (wchar_t character : property) key.push_back(static_cast<char>(character));
    auto store = folderStateStore();
    bool expired = false;
    if (!store || !store->recordAccess(key, false, static_cast<int64_t>(std::time(nullptr)), &expired)) {
        reportFolderStateError(nullptr);
    } else if (expired) {
        // A different process can still have this folder open. Its delayed close must not
        // recreate an expired row; a subsequent successful access can create a new one.
        FolderWindowSearch search{property};
        EnumWindows(findFolderWindow, reinterpret_cast<LPARAM>(&search));
        if (search.window) ItemWindow::expireFolderState(search.window);
    }
}

void recordFolderItemFailure(IShellItem *item, HRESULT result) {
    if (!item || !isFolderAccessFailure(result)) return;
    CComHeapPtr<wchar_t> path;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        recordFolderPathFailure(path, result);
}

FolderOpenResult openFolderWindow(IShellItem *item, HMONITOR monitor, int showCmd) {
    std::wstring identity = getFolderIdentity(item);
    std::wstring property = folderWindowProperty(identity);
    if (property.empty())
        return FolderOpenResult::Failed;
    // A mutex is recursive on its owning thread. Reject a reentrant creation too.
    bool pending = std::find(pendingOpens.begin(), pendingOpens.end(), property) != pendingOpens.end();
    if (pending)
        return FolderOpenResult::Pending;
    FolderOpenLock lock(property);
    if (!lock)
        return FolderOpenResult::Failed;
    FolderWindowSearch search{property};
    BOOL enumerated = EnumWindows(findFolderWindow, reinterpret_cast<LPARAM>(&search));
    if (search.window) {
        ShowWindowAsync(search.window, IsIconic(search.window) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(search.window);
        ItemWindow::flashWindow(search.window);
        return FolderOpenResult::Activated;
    }
    if (!enumerated)
        return FolderOpenResult::Failed;
    CComPtr<ItemWindow> window;
    window.Attach(new FolderWindow(item, identity));
    if (!window->create(window->requestedRect(monitor), showCmd))
        return FolderOpenResult::Failed;
    // Preserve foreground activation when a 32-bit Shell client launches this x64 server.
    if (showCmd == SW_SHOWNORMAL)
        window->setForeground();
    return FolderOpenResult::Created;
}

CComPtr<IShellItem> resolveLink(IShellItem *const linkItem) {
    // https://stackoverflow.com/a/46064112
    CComPtr<IShellLink> link;
    if (SUCCEEDED(linkItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&link)))) {
        if (checkHR(link->Resolve(nullptr, SLR_NO_UI | SLR_UPDATE))) {
            CComHeapPtr<ITEMIDLIST> targetPIDL;
            if (checkHR(link->GetIDList(&targetPIDL))) {
                CComPtr<IShellItem> targetItem;
                if (checkHR(SHCreateItemFromIDList(targetPIDL, IID_PPV_ARGS(&targetItem)))) {
                    SFGAOF attr;
                    if (FAILED(targetItem->GetAttributes(SFGAO_VALIDATE, &attr))) // doesn't exist
                        return linkItem;
                    // don't need to recurse, shortcuts to shortcuts are not allowed
                    return targetItem;
                }
            }
        }
    }
    return linkItem;
}

CComPtr<IShellItem> itemFromPath(wchar_t *path) {
    while (1) {
        CComPtr<IShellItem> item;
        // parse name vs display name https://stackoverflow.com/q/42966489
        HRESULT hr = SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&item));
        if (checkHR(hr))
            return item;
        recordFolderPathFailure(path, hr);
        int result = MessageBox(nullptr, formatString(IDS_CANT_FIND_ITEM, path).get(),
            getString(IDS_ERROR_CAPTION), MB_CANCELTRYCONTINUE | MB_ICONERROR);
        if (result == IDCANCEL) {
            return nullptr;
        } else if (result == IDCONTINUE) {
            if (checkHR(SHGetKnownFolderItem(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr,
                    IID_PPV_ARGS(&item))))
                return item;
        } // else retry
    }
}


void debugDisplayNames(HWND hwnd, IShellItem *const item) {
    static SIGDN nameTypes[] = {
        SIGDN_NORMALDISPLAY, SIGDN_PARENTRELATIVE,
        SIGDN_PARENTRELATIVEEDITING, SIGDN_PARENTRELATIVEFORUI,
        SIGDN_PARENTRELATIVEFORADDRESSBAR, SIGDN_FILESYSPATH,
        SIGDN_DESKTOPABSOLUTEEDITING, SIGDN_DESKTOPABSOLUTEPARSING,
        SIGDN_PARENTRELATIVEPARSING, SIGDN_URL
    };
    CComHeapPtr<wchar_t> names[_countof(nameTypes)];
    for (int i = 0; i < _countof(names); i++)
        checkHR(item->GetDisplayName(nameTypes[i], &names[i]));
    static const PROPERTYKEY *pkeys[] = {
        // https://learn.microsoft.com/en-us/windows/win32/properties/core-bumper
        &PKEY_ItemName, &PKEY_ItemNameDisplay,
        &PKEY_ItemNameDisplayWithoutExtension,
        &PKEY_ItemPathDisplay, &PKEY_ItemType,
        &PKEY_FileName, &PKEY_FileExtension,
        // https://learn.microsoft.com/en-us/windows/win32/properties/shell-bumper
        &PKEY_NamespaceCLSID,
    };
    CComHeapPtr<wchar_t> props[_countof(pkeys)];
    CComQIPtr<IShellItem2> item2(item);
    if (item2) {
        for (int i = 0; i < _countof(pkeys); i++)
            checkHR(item2->GetString(*pkeys[i], &props[i]));
    }
    showDebugMessage(hwnd, L"Item Display Names", L""
        "Normal Display:\t\t%1\r\n"             "Parent Relative:\t\t%2\r\n"
        "Parent Relative Editing:\t%3\r\n"      "Parent Relative UI (Win8):\t%4\r\n"
        "Parent Relative Address Bar:\t%5\r\n"  "File System Path:\t\t%6\r\n"
        "Desktop Absolute Editing:\t%7\r\n"     "Desktop Absolute Parsing:\t%8\r\n"
        "Parent Relative Parsing:\t%9\r\n"      "URL:\t\t\t%10\r\n"
        "System.ItemName:\t\t%11\r\n"           "System.ItemNameDisplay:\t%12\r\n"
        "System.ItemNameDisplayWithoutExtension: %13\r\n"
        "System.ItemPathDisplay:\t%14\r\n"      "System.ItemType:\t\t%15\r\n"
        "System.FileName:\t\t%16\r\n"           "System.FileExtension:\t%17\r\n"
        "System.NamespaceCLSID:\t%18",
        &*names[0], &*names[1], &*names[2], &*names[3], &*names[4], &*names[5],
        &*names[6], &*names[7], &*names[8], &*names[9],
        &*props[0], &*props[1], &*props[2], &*props[3], &*props[4], &*props[5],
        &*props[6], &*props[7]);
}

} // namespace
