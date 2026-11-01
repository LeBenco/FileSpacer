#include <common.h>
#include "Peek.h"
#include "WinUtils.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.data.json.h>
#include <wrl/wrappers/corewrappers.h>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "RuntimeObject.lib")

namespace filespacer {
namespace {
std::wstring powerToysRunner() {
    wchar_t runner[32768] = {};
    DWORD length = _countof(runner);
    if (SUCCEEDED(AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_EXECUTABLE,
            L"powertoys", L"open", runner, &length)) && !PathIsRelativeW(runner)
            && _wcsicmp(PathFindFileNameW(runner), L"PowerToys.exe") == 0)
        return runner;
    for (const KNOWNFOLDERID *id : {&FOLDERID_LocalAppData, &FOLDERID_ProgramFiles}) {
        CComHeapPtr<wchar_t> directory;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DONT_VERIFY, nullptr, &directory))) {
            const std::wstring candidate = std::wstring(directory) + L"\\PowerToys\\PowerToys.exe";
            const DWORD attributes = GetFileAttributesW(candidate.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                return candidate;
        }
    }
    return {};
}

DWORD startPowerToys(const std::wstring &executable, const std::wstring &arguments) {
    if (executable.empty()) return ERROR_FILE_NOT_FOUND;
    std::wstring command = quoteCommandArgument(executable.c_str()) + L" " + arguments;
    if (command.size() >= 32767) return ERROR_FILENAME_EXCED_RANGE;
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        return GetLastError();
    const std::unique_ptr<void, decltype(&CloseHandle)> thread(process.hThread, CloseHandle);
    const std::unique_ptr<void, decltype(&CloseHandle)> child(process.hProcess, CloseHandle);
    return ERROR_SUCCESS;
}

PeekInstallation peekAt(const std::wstring &directory) {
    const std::wstring executable = directory + L"\\WinUI3Apps\\PowerToys.Peek.UI.exe";
    DWORD ignored = 0;
    const DWORD bytes = GetFileVersionInfoSizeW(executable.c_str(), &ignored);
    if (!bytes) return {};
    std::vector<BYTE> data(bytes);
    if (!GetFileVersionInfoW(executable.c_str(), 0, bytes, data.data())) return {};
    VS_FIXEDFILEINFO *version = nullptr;
    UINT size = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&version), &size)
            || size < sizeof(*version) || version->dwSignature != VS_FFI_SIGNATURE)
        return {};
    const WORD major = HIWORD(version->dwFileVersionMS);
    const WORD minor = LOWORD(version->dwFileVersionMS);
    // The original 0.96 CLI reads only its last argument. Multiple paths are
    // supported by the documented 0.102 CLI; do not silently discard a selection.
    if (!major && minor < 96) return {};
    return {executable, major > 0 || minor >= 102};
}
}

PeekInstallation findPeek() {
    // The public protocol association also locates custom installation folders.
    // Only resolve it: opening the protocol would not invoke Peek's file CLI.
    const std::wstring path = powerToysRunner();
    if (!path.empty()) {
        auto installation = peekAt(path.substr(0, path.find_last_of(L"\\/")));
        if (!installation.executable.empty()) return installation;
    }
    // Default locations documented by Microsoft Learn (per user, then machine).
    for (const KNOWNFOLDERID *id : {&FOLDERID_LocalAppData, &FOLDERID_ProgramFiles}) {
        CComHeapPtr<wchar_t> directory;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DONT_VERIFY, nullptr, &directory))) {
            auto installation = peekAt(std::wstring(directory) + L"\\PowerToys");
            if (!installation.executable.empty()) return installation;
        }
    }
    return {};
}

bool powerToysPeekEnabled() {
    CComHeapPtr<wchar_t> directory;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &directory))) return false;
    const std::wstring file = std::wstring(directory) + L"\\Microsoft\\PowerToys\\settings.json";
    const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const std::unique_ptr<void, decltype(&CloseHandle)> input(handle, CloseHandle);
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(input.get(), &size) || size.QuadPart < 1 || size.QuadPart > 1048576) return false;
    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!ReadFile(input.get(), &bytes[0], static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size()) return false;
    const int offset = bytes.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    const int count = static_cast<int>(bytes.size()) - offset;
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + offset, count, nullptr, 0);
    if (!length) return false;
    std::wstring text(length, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + offset, count, &text[0], length)) return false;
    using namespace Microsoft::WRL::Wrappers;
    RoInitializeWrapper runtime(RO_INIT_SINGLETHREADED);
    if (FAILED(runtime)) return false;
    CComPtr<ABI::Windows::Data::Json::IJsonObjectStatics> factory;
    CComPtr<ABI::Windows::Data::Json::IJsonObject> root, enabled;
    boolean active = false;
    return SUCCEEDED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Data_Json_JsonObject).Get(), IID_PPV_ARGS(&factory)))
        && SUCCEEDED(factory->Parse(HStringReference(text.c_str(), static_cast<UINT32>(text.size())).Get(), &root))
        && SUCCEEDED(root->GetNamedObject(HStringReference(L"enabled").Get(), &enabled))
        && SUCCEEDED(enabled->GetNamedBoolean(HStringReference(L"Peek").Get(), &active)) && active;
}

DWORD openPeekSettings() {
    return startPowerToys(powerToysRunner(), L"--open-settings=Peek");
}

std::vector<std::wstring> peekPaths(IShellItemArray *selection) {
    DWORD count = 0;
    if (!selection || FAILED(selection->GetCount(&count))) return {};
    std::vector<std::wstring> paths;
    for (DWORD index = 0; index < count; ++index) {
        CComPtr<IShellItem> item;
        if (FAILED(selection->GetItemAt(index, &item))) return {};
        CComHeapPtr<wchar_t> path;
        // Virtual Shell items have no filesystem path and cannot use this CLI.
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path && path[0])
            paths.emplace_back(path);
    }
    return paths;
}

DWORD launchPeek(const PeekInstallation &installation, const std::vector<std::wstring> &paths) {
    if (installation.executable.empty()) return ERROR_FILE_NOT_FOUND;
    if (paths.empty()) return ERROR_INVALID_PARAMETER;
    if (!installation.multipleSelection && paths.size() > 1) return ERROR_NOT_SUPPORTED;
    if (PathIsRelativeW(installation.executable.c_str())
            || installation.executable.find(L'"') != std::wstring::npos
            || _wcsicmp(PathFindFileNameW(installation.executable.c_str()), L"PowerToys.Peek.UI.exe") != 0)
        return ERROR_BAD_PATHNAME;
    std::wstring arguments;
    for (const auto &path : paths) {
        if (path.empty() || PathIsRelativeW(path.c_str()) || path.find(L'"') != std::wstring::npos)
            return ERROR_BAD_PATHNAME;
        if (!arguments.empty()) arguments += L' ';
        arguments += quoteCommandArgument(path.c_str());
        if (arguments.size() >= 32767) return ERROR_FILENAME_EXCED_RANGE;
    }
    return startPowerToys(installation.executable, arguments);
}

} // namespace filespacer
