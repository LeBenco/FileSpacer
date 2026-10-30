#include <common.h>
#include "ClosedWindows.h"
#include "UIStrings.h"
#include "main.h"
#include <shlobj.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib, "Advapi32.lib")

namespace filespacer {
namespace {
std::string database;
std::wstring registry;
CComPtr<IShellItem> pinnedFolder;
uint64_t builtFingerprint = 0;
bool built = false, purged = false, failLink = true, failCommit = false;
unsigned linkCalls = 0, commitCalls = 0;
size_t destinationCount = 0;
std::vector<std::wstring> removed;
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
std::wstring wide(const std::string &value) { return std::wstring(value.begin(), value.end()); }
const wchar_t *jumpListRegistryKey() { return registry.c_str(); }
bool openJumpListKey(const wchar_t *appID, CRegKey *key) {
    return key->Create(HKEY_CURRENT_USER, (registry + L"\\" + appID).c_str()) == ERROR_SUCCESS;
}
bool jumpListKeyStale(FolderStateStore &, const wchar_t *) { return false; }
bool canReopenClosedWindow(const ClosedWindow &) { return false; }
std::wstring folderArguments(const wchar_t *folder) { return std::wstring(L"/test \"") + folder + L"\""; }
std::wstring jumpListFolderIcon(IShellItem *) { return L"fixture icon"; }
bool removedDestinationNames(const wchar_t *, std::vector<std::wstring> *names) { *names = removed; return true; }
bool makeJumpListLink(const wchar_t *, const wchar_t *, const wchar_t *,
        const std::wstring &, IShellLinkW **out) {
    ++linkCalls;
    *out = nullptr;
    if (failLink) { failLink = false; return false; }
    return SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(out)));
}
bool buildJumpList(const wchar_t *, const wchar_t *, const wchar_t *, const std::wstring &,
        const std::vector<CComPtr<IShellLinkW>> &destinations) {
    ++commitCalls; destinationCount = destinations.size();
    if (failCommit) { failCommit = false; return false; }
    return true;
}
}
std::string folderStateDatabasePath() { return database; }
std::wstring closedWindowName(const ClosedWindow &) { return L"fixture"; }
std::wstring closedWindowPath(const ClosedWindow &) { return L""; }
const wchar_t *getString(UINT) { return L"fixture"; }
local_wstr_ptr formatString(UINT, ...) {
    auto text = local_wstr_ptr(static_cast<wchar_t *>(LocalAlloc(LMEM_FIXED, 2 * sizeof(wchar_t))));
    if (text) { text[0] = L'x'; text[1] = L'\0'; }
    return text;
}
HRESULT enumerateQuickAccessFolders(std::vector<CComPtr<IShellItem>> *folders, UINT *stage, bool) {
    *stage = 0; folders->push_back(pinnedFolder); return S_OK;
}
#include "JumpListProduction.inc"

void run(const wchar_t *path, const wchar_t *folder) {
    const int count = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    require(count > 0, "UTF-8 path conversion failed");
    database.resize(static_cast<size_t>(count));
    require(WideCharToMultiByte(CP_UTF8, 0, path, -1, &database[0], count, nullptr, nullptr) != 0, "UTF-8 conversion failed");
    database.pop_back();
    GUID id = {};
    require(SUCCEEDED(CoCreateGuid(&id)), "Fixture GUID failed");
    wchar_t name[40] = {};
    require(StringFromGUID2(id, name, ARRAYSIZE(name)) != 0, "Fixture GUID formatting failed");
    registry = std::wstring(L"Software\\FileSpacer.JumpListRetryTest.") + name;
    require(SUCCEEDED(SHCreateItemFromParsingName(folder, nullptr, IID_PPV_ARGS(&pinnedFolder))), "Native pinned folder unavailable");
    try {
        updateReopenJumpLists();
        require(linkCalls == 1 && commitCalls == 1 && destinationCount == 0, "Injected link failure was not reached");
        updateReopenJumpLists();
        require(linkCalls == 2 && commitCalls == 2 && destinationCount == 1, "Unchanged input did not retry a partial update");
        updateReopenJumpLists();
        require(linkCalls == 2 && commitCalls == 2, "A complete unchanged update was not cached");
        removed.push_back(L"temporary model"); failLink = true;
        updateReopenJumpLists();
        require(commitCalls == 3 && destinationCount == 0, "Partial replacement was not reached");
        removed.clear();
        updateReopenJumpLists();
        require(commitCalls == 4 && destinationCount == 1, "Returning to a former model reused a cache invalidated by a partial write");
        removed.push_back(L"changed removed destination"); failCommit = true;
        updateReopenJumpLists();
        require(commitCalls == 5, "Changed input did not reach the failing commit");
        updateReopenJumpLists();
        require(commitCalls == 6 && destinationCount == 1, "An unsuccessful commit was cached");
        updateReopenJumpLists();
        require(commitCalls == 6, "Successful retry did not restore caching");
    } catch (...) {
        pinnedFolder.Release();
        const LSTATUS cleanup = RegDeleteTreeW(HKEY_CURRENT_USER, registry.c_str());
        if (cleanup != ERROR_SUCCESS) std::fprintf(stderr, "Fixture registry cleanup failed: %ld\n", cleanup);
        throw;
    }
    pinnedFolder.Release();
    require(RegDeleteTreeW(HKEY_CURRENT_USER, registry.c_str()) == ERROR_SUCCESS, "Fixture registry cleanup failed");
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 3 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    int result = 0;
    try {
        filespacer::run(argv[1], argv[2]);
        std::puts("PASS: partial links and failed commits retry unchanged inputs; complete results are cached (production method, injected Shell-boundary failures).");
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    CoUninitialize();
    return result;
}
