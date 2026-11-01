#include <common.h>
#include "Peek.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace filespacer;

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

// Copies of this executable stand in for the external GUI. This branch never
// invokes the installed PowerToys or changes its preferences.
static int capture(int argc, wchar_t **argv) {
    wchar_t directory[32768] = {};
    if (!GetEnvironmentVariableW(L"FILESPACER_PEEK_CAPTURE", directory, _countof(directory))) return 2;
    const std::wstring target = std::wstring(directory) + L"\\" + std::to_wstring(GetCurrentProcessId()) + L".args";
    const std::wstring temporary = target + L".tmp";
    std::wstring arguments;
    for (int index = 1; index < argc; ++index) arguments += std::wstring(argv[index]) + L'\n';
    {
        std::ofstream output(temporary, std::ios::binary);
        output.write(reinterpret_cast<const char *>(arguments.data()),
            static_cast<std::streamsize>(arguments.size() * sizeof(wchar_t)));
        if (!output) return 3;
    }
    return MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING) ? 0 : 4;
}

struct AssociationSandbox {
    std::wstring key;
    CRegKey root;
    bool overridden = false;
    ~AssociationSandbox() {
        if (overridden) RegOverridePredefKey(HKEY_CLASSES_ROOT, nullptr);
        root.Close();
        if (!key.empty()) RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
    }
};

int wmain(int argc, wchar_t **argv) {
    wchar_t executable[32768] = {};
    if (!GetModuleFileNameW(nullptr, executable, _countof(executable))) return 1;
    if (_wcsicmp(PathFindFileNameW(executable), L"PowerToys.Peek.UI.exe") == 0
            || _wcsicmp(PathFindFileNameW(executable), L"PowerToys.exe") == 0) return capture(argc, argv);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 1;
    int result = 0;
    try {
        GUID id = {};
        require(SUCCEEDED(CoCreateGuid(&id)), "fixture GUID");
        wchar_t guid[40] = {};
        require(StringFromGUID2(id, guid, _countof(guid)) > 0, "fixture name");
        const std::wstring binary = executable;
        const std::wstring fixture = binary.substr(0, binary.find_last_of(L"\\")) + L"\\Peek " + guid;
        require(CreateDirectoryW(fixture.c_str(), nullptr) != FALSE, "fixture directory");
        const std::wstring apps = fixture + L"\\WinUI3Apps";
        require(CreateDirectoryW(apps.c_str(), nullptr) != FALSE, "fixture apps directory");
        const std::wstring runner = fixture + L"\\PowerToys.exe";
        const std::wstring peek = apps + L"\\PowerToys.Peek.UI.exe";
        require(CopyFileW(executable, runner.c_str(), TRUE) != FALSE, "fixture runner");
        require(CopyFileW(executable, peek.c_str(), TRUE) != FALSE, "fixture versioned Peek");

        AssociationSandbox sandbox;
        sandbox.key = L"Software\\FileSpacer\\PeekTests\\" + std::wstring(guid);
        require(sandbox.root.Create(HKEY_CURRENT_USER, sandbox.key.c_str()) == ERROR_SUCCESS, "isolated registry");
        require(RegOverridePredefKey(HKEY_CLASSES_ROOT, sandbox.root) == ERROR_SUCCESS, "process-only HKCR override");
        sandbox.overridden = true;
        CRegKey protocol, command;
        require(protocol.Create(sandbox.root, L"powertoys") == ERROR_SUCCESS, "fixture protocol");
        require(protocol.SetStringValue(L"URL Protocol", L"") == ERROR_SUCCESS, "protocol marker");
        require(command.Create(protocol, L"shell\\open\\command") == ERROR_SUCCESS, "fixture command");
        require(command.SetStringValue(nullptr, (L"\"" + runner + L"\" \"%1\"").c_str()) == ERROR_SUCCESS, "protocol command");
        const auto installation = findPeek();
        require(installation.executable == peek && installation.multipleSelection, "custom installed path and version capability");

        const std::wstring file = fixture + L"\\caf\u00e9 & % ^.txt";
        { std::ofstream output(file); output << "fixture"; }
        CComHeapPtr<ITEMIDLIST> fileId, folderId, virtualId;
        require(SUCCEEDED(SHParseDisplayName(file.c_str(), nullptr, &fileId, 0, nullptr)), "file PIDL");
        require(SUCCEEDED(SHParseDisplayName(fixture.c_str(), nullptr, &folderId, 0, nullptr)), "folder PIDL");
        require(SUCCEEDED(SHGetKnownFolderIDList(FOLDERID_ComputerFolder, 0, nullptr, &virtualId)), "virtual PIDL");
        PCIDLIST_ABSOLUTE ids[] = {fileId, virtualId, folderId};
        CComPtr<IShellItemArray> selection;
        require(SUCCEEDED(SHCreateShellItemArrayFromIDLists(_countof(ids), ids, &selection)), "real Shell selection array");
        const auto paths = peekPaths(selection);
        require(paths == std::vector<std::wstring>({file, fixture}), "filesystem selection and virtual item filtering");
        require(peekPaths(nullptr).empty(), "null selection");

        require(launchPeek({}, paths) == ERROR_FILE_NOT_FOUND, "absent executable");
        require(launchPeek(installation, {}) == ERROR_INVALID_PARAMETER, "empty selection");
        require(launchPeek({peek, false}, paths) == ERROR_NOT_SUPPORTED, "old CLI must not drop selected files");
        require(launchPeek({L"relative.exe", true}, paths) == ERROR_BAD_PATHNAME, "relative executable");
        require(launchPeek(installation, {L"relative.txt"}) == ERROR_BAD_PATHNAME, "relative argument");
        require(launchPeek(installation, {L"C:\\bad\"path"}) == ERROR_BAD_PATHNAME, "quote in path");
        require(launchPeek(installation, {L"C:\\" + std::wstring(32767, L'x')}) == ERROR_FILENAME_EXCED_RANGE, "command line size limit");
        require(launchPeek({apps + L"\\missing\\PowerToys.Peek.UI.exe", true}, paths) != ERROR_SUCCESS, "native launch failure");

        require(SetEnvironmentVariableW(L"FILESPACER_PEEK_CAPTURE", fixture.c_str()) != FALSE, "capture environment");
        const std::vector<std::wstring> transfer = {file, fixture + L"\\", L"C:\\"};
        require(launchPeek(installation, transfer) == ERROR_SUCCESS, "native process launch");
        const ULONGLONG deadline = GetTickCount64() + 10000;
        WIN32_FIND_DATAW found = {};
        HANDLE raw = INVALID_HANDLE_VALUE;
        do {
            raw = FindFirstFileW((fixture + L"\\*.args").c_str(), &found);
            if (raw != INVALID_HANDLE_VALUE) break;
            Sleep(10);
        } while (GetTickCount64() < deadline);
        require(raw != INVALID_HANDLE_VALUE, "external process capture");
        const std::unique_ptr<void, decltype(&FindClose)> search(raw, FindClose);
        std::ifstream input(fixture + L"\\" + found.cFileName, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const std::wstring expected = transfer[0] + L"\n" + transfer[1] + L"\n" + transfer[2] + L"\n";
        require(bytes == std::string(reinterpret_cast<const char *>(expected.data()), expected.size() * sizeof(wchar_t)), "exact Unicode, metacharacters and trailing slash transfer");
        require(SetEnvironmentVariableW(L"FILESPACER_PEEK_CAPTURE", nullptr) != FALSE, "capture cleanup");
        std::cout << "PASS: real Shell items, process-isolated protocol discovery, file version, old CLI rejection, launch errors and exact child argument transfer.\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; result = 1; }
    CoUninitialize();
    return result;
}
