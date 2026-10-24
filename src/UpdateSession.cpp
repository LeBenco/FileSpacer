#include "UpdateSession.h"
#include "UpdateSessionStore.h"
#include "ItemWindow.h"
#include "CreateItemWindow.h"
#include "ClosedWindows.h"
#include "UIStrings.h"
#include "WinUtils.h"
#include <vector>
#include <shlobj.h>
#include <sqlite3.h>

namespace filespacer {
namespace {

struct Capture {
    std::vector<std::string> paths;
    bool failed = false;
};

BOOL CALLBACK captureWindow(HWND window, LPARAM parameter) {
    auto &capture = *reinterpret_cast<Capture *>(parameter);
    wchar_t name[64] = {};
    if (!GetClassNameW(window, name, ARRAYSIZE(name))
            || lstrcmpW(name, L"FileSpacer Item Window") != 0) return TRUE;
    // EnumThreadWindows restricts this pointer lookup to our own main STA.
    auto impl = reinterpret_cast<WindowImpl *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!impl) return TRUE;
    CComPtr<ItemWindow> itemWindow = static_cast<ItemWindow *>(impl);
    CComPtr<IShellItem> item = itemWindow->item;
    try {
        SFGAOF attributes = 0;
        ClosedWindow info;
        if (!item || FAILED(item->GetAttributes(SFGAO_FOLDER, &attributes))) capture.failed = true;
        else if (attributes & SFGAO_FOLDER) {
            if (!getClosedWindowInfo(item, nullptr, &info)) capture.failed = true;
            else if (IsWindow(window)
                    && GetWindowLongPtrW(window, GWLP_USERDATA) == reinterpret_cast<LONG_PTR>(impl))
                capture.paths.push_back(std::move(info.parsingName));
        }
    } catch (...) { capture.failed = true; }
    return capture.failed ? FALSE : TRUE;
}
} // namespace

bool isUpdateSessionPath(const wchar_t *filename) {
    if (!filename) return false;
    std::vector<wchar_t> temp(32768);
    const DWORD length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    if (!length || length >= temp.size()) return false;
    const std::wstring prefix = std::wstring(temp.data()) + L"FileSpacerUpdate-";
    const std::wstring path(filename);
    const std::wstring suffix = L"\\windows.sqlite3";
    if (path.size() != prefix.size() + 38 + suffix.size()
            || CompareStringOrdinal(path.c_str(), static_cast<int>(prefix.size()),
                prefix.c_str(), static_cast<int>(prefix.size()), TRUE) != CSTR_EQUAL
            || path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    const std::wstring guid = path.substr(prefix.size(), 38);
    GUID parsed;
    if (guid.front() != L'{' || guid.back() != L'}'
            || FAILED(CLSIDFromString(guid.c_str(), &parsed))) return false;
    const std::wstring directory = path.substr(0, path.size() - suffix.size());
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    const DWORD fileAttributes = GetFileAttributesW(filename);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)
        && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        && (fileAttributes == INVALID_FILE_ATTRIBUTES
            || !(fileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)));
}

bool createUpdateWindowSession(const wchar_t *filename) {
    if (!isUpdateSessionPath(filename)) return false;
    const std::string path = toUtf8(filename, WC_ERR_INVALID_CHARS);
    return !path.empty() && createUpdateSession(path.c_str()) == SQLITE_OK;
}

DWORD inspectUpdateWindowSession(const wchar_t *filename, bool *hasWindows) {
    if (!isUpdateSessionPath(filename)) return ERROR_INVALID_PARAMETER;
    const std::string path = toUtf8(filename, WC_ERR_INVALID_CHARS);
    std::vector<std::string> paths;
    if (path.empty() || readUpdateSession(path.c_str(), &paths) != SQLITE_OK) return ERROR_READ_FAULT;
    *hasWindows = !paths.empty();
    return ERROR_SUCCESS;
}

bool saveUpdateWindows(const wchar_t *filename) {
    if (!isUpdateSessionPath(filename)) return false;
    const std::string path = toUtf8(filename, WC_ERR_INVALID_CHARS);
    if (path.empty()) return false;
    Capture capture;
    EnumThreadWindows(GetCurrentThreadId(), captureWindow, reinterpret_cast<LPARAM>(&capture));
    return !capture.failed
        && saveUpdateSession(path.c_str(), GetCurrentProcessId(), capture.paths) == SQLITE_OK;
}

FolderOpenResult restoreUpdateWindows(const wchar_t *filename) {
    if (!isUpdateSessionPath(filename)) return FolderOpenResult::Failed;
    // Rename atomically before reading: concurrent/repeated launches cannot replay it.
    const std::wstring consumed = std::wstring(filename) + L".restoring";
    if (!MoveFileW(filename, consumed.c_str())) return FolderOpenResult::Failed;
    std::vector<std::string> paths;
    const std::string database = toUtf8(consumed.c_str(), WC_ERR_INVALID_CHARS);
    if (database.empty() || readUpdateSession(database.c_str(), &paths) != SQLITE_OK) {
        MoveFileW(consumed.c_str(), filename); // Preserve the session for a later retry.
        return FolderOpenResult::Failed;
    }
    FolderOpenResult result = FolderOpenResult::Failed;
    unsigned failed = 0;
    for (const auto &path : paths) {
        ClosedWindow info;
        info.parsingName = path;
        const std::wstring name = closedWindowPath(info);
        CComPtr<IShellItem> item;
        SFGAOF attributes = 0;
        if (name.empty() || FAILED(SHCreateItemFromParsingName(name.c_str(), nullptr, IID_PPV_ARGS(&item)))
                || FAILED(item->GetAttributes(SFGAO_FOLDER, &attributes)) || !(attributes & SFGAO_FOLDER)) {
            ++failed; continue;
        }
        const FolderOpenResult opened = openFolderWindow(item, nullptr, SW_SHOWNORMAL);
        if (opened == FolderOpenResult::Created) result = opened;
        else if (opened == FolderOpenResult::Failed) ++failed;
        else if (result != FolderOpenResult::Created) result = FolderOpenResult::Activated;
    }
    DeleteFileW(consumed.c_str());
    if (failed) MessageBoxW(nullptr, formatString(IDS_UPDATE_RESTORE_FAILED, failed).get(),
        getString(IDS_APP_NAME), MB_OK | MB_ICONWARNING);
    return result;
}

DWORD launchUpdateSession(const std::wstring &executable, const std::wstring &filename) {
    std::wstring command = quoteCommandArgument(executable.c_str()) + L" /restore-update "
        + quoteCommandArgument(filename.c_str());
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
            0, nullptr, nullptr, &startup, &process)) return GetLastError();
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return ERROR_SUCCESS;
}

} // namespace filespacer
