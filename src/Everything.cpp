#include <common.h>
#include "Everything.h"
#include "WinUtils.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <atlbase.h>
#include <commctrl.h>
#include <type_traits>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comctl32.lib")

namespace filespacer {
namespace {
bool executableExists(const std::wstring &path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

struct IconLoad {
    int pixels;
    HICON icon = nullptr;
};

BOOL CALLBACK loadFirstIcon(HMODULE module, const wchar_t *, wchar_t *name, LONG_PTR parameter) {
    auto load = reinterpret_cast<IconLoad *>(parameter);
    if (FAILED(LoadIconWithScaleDown(module, name, load->pixels, load->pixels, &load->icon))) {
        if (load->icon) DestroyIcon(load->icon);
        load->icon = nullptr;
    }
    return FALSE; // The first group is the executable's primary icon; no hardcoded resource ID.
}
}

HICON extractEverythingIcon(const std::wstring &executable, int pixels) {
    if (executable.empty() || PathIsRelativeW(executable.c_str()) || pixels <= 0) return nullptr;
    // Map resources without executing the binary or loading its imports. Release
    // the mapping immediately so no persistent handle interferes with updates.
    const std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&FreeLibrary)> module(
        LoadLibraryExW(executable.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE), FreeLibrary);
    if (!module) return nullptr;
    IconLoad load = {pixels};
    EnumResourceNamesW(module.get(), RT_GROUP_ICON, loadFirstIcon, reinterpret_cast<LONG_PTR>(&load));
    return load.icon;
}

std::wstring findEverything() {
    for (HKEY hive : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        for (DWORD view : {RRF_SUBKEY_WOW6464KEY, RRF_SUBKEY_WOW6432KEY}) {
            wchar_t path[32768] = {};
            DWORD bytes = sizeof(path);
            if (RegGetValueW(hive, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Everything.exe",
                    nullptr, RRF_RT_REG_SZ | view, nullptr, path, &bytes) == ERROR_SUCCESS
                    && executableExists(path))
                return path;
            bytes = sizeof(path);
            if (RegGetValueW(hive, L"Software\\voidtools\\Everything", L"InstallLocation",
                    RRF_RT_REG_SZ | view, nullptr, path, &bytes) == ERROR_SUCCESS) {
                std::wstring candidate = std::wstring(path) + L"\\Everything.exe";
                if (executableExists(candidate)) return candidate;
            }
        }
    }
    for (const KNOWNFOLDERID *id : {&FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86,
            &FOLDERID_LocalAppData}) {
        CComHeapPtr<wchar_t> directory;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, 0, nullptr, &directory))) {
            std::wstring candidate = std::wstring(directory) +
                (id == &FOLDERID_LocalAppData ? L"\\Programs\\Everything\\Everything.exe"
                                            : L"\\Everything\\Everything.exe");
            if (executableExists(candidate)) return candidate;
        }
    }
    return {};
}

std::wstring everythingArguments(const std::wstring &query, const std::wstring &folder) {
    std::wstring search = query;
    if (!folder.empty()) {
        std::wstring path = folder;
        if (path.back() != L'\\') path += L'\\';
        search = L"\"" + path + L"\"" + (query.empty() ? L"" : L" <" + query + L">");
    }
    // Everything has its own parser: a literal quote is three quotes, not \".
    // No command interpreter is involved, so &, |, %, ^ and backslashes stay literal.
    std::wstring arguments = L"-search \"";
    for (wchar_t character : search) {
        if (character == L'"') arguments += L"\"\"\"";
        else arguments += character;
    }
    return arguments + L'"';
}

DWORD launchEverything(const std::wstring &executable, const std::wstring &arguments) {
    if (executable.empty()) return ERROR_FILE_NOT_FOUND;
    if (PathIsRelativeW(executable.c_str()) || executable.find(L'"') != std::wstring::npos
            || _wcsicmp(PathFindExtensionW(executable.c_str()), L".exe") != 0)
        return ERROR_BAD_PATHNAME;
    std::wstring command = quoteCommandArgument(executable.c_str()) + L" " + arguments;
    if (command.size() >= 32767) return ERROR_FILENAME_EXCED_RANGE;
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
            0, nullptr, nullptr, &startup, &process))
        return GetLastError();
    const std::unique_ptr<void, decltype(&CloseHandle)> thread(process.hThread, CloseHandle);
    const std::unique_ptr<void, decltype(&CloseHandle)> child(process.hProcess, CloseHandle);
    return ERROR_SUCCESS;
}

} // namespace
