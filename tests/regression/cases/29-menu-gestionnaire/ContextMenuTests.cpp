#include <common.h>
#include "Settings.h"
#include "UIStrings.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <atlbase.h>
#include <cstdio>
#include <cwchar>
#include <stdexcept>

using namespace filespacer;

namespace {
int checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
void put(HKEY root, const std::wstring &key, const wchar_t *name, const std::wstring &value) {
    require(RegSetKeyValueW(root, key.c_str(), name, REG_SZ, value.c_str(),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS, "write isolated fixture");
}
std::wstring read(HKEY root, const std::wstring &key, const wchar_t *name = nullptr) {
    wchar_t value[2048] = {}; DWORD size = sizeof(value);
    require(RegGetValueW(root, key.c_str(), name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
        nullptr, value, &size) == ERROR_SUCCESS, "read actual menu registration");
    return value;
}
bool exists(HKEY root, const std::wstring &key, const wchar_t *name = nullptr) {
    return RegGetValueW(root, key.c_str(), name, RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}
void checkMenu(const wchar_t *type, bool explorer, bool background = false) {
    const std::wstring shell = std::wstring(type) + (background ? L"\\Background\\shell" : L"\\shell");
    require(exists(HKEY_CLASSES_ROOT, shell + L"\\filespacer", L"ProgrammaticAccessOnly") == explorer,
        "FileSpacer visibility follows the effective default");
    require(exists(HKEY_CLASSES_ROOT, shell + L"\\filespacer.explorer") == explorer,
        "Explorer alternative follows the effective default");
    require(read(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command") == L"original command",
        "original FileSpacer command preserved");
    require(read(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command", L"DelegateExecute") == L"original delegate",
        "original COM delegate preserved");
    if (!explorer) return;
    const std::wstring alternate = shell + L"\\filespacer.explorer";
    require(read(HKEY_CLASSES_ROOT, alternate) == getString(IDS_OPEN_IN_EXPLORER), "localized Explorer caption");
    require(read(HKEY_CLASSES_ROOT, alternate + L"\\command") == (background
        ? L"\"%SystemRoot%\\explorer.exe\" \"%V\"" : L"\"%SystemRoot%\\explorer.exe\" \"%1\""), "correct item/background argument");
    HKEY raw = nullptr;
    require(RegOpenKeyExW(HKEY_CLASSES_ROOT, shell.c_str(), 0, KEY_READ, &raw) == ERROR_SUCCESS, "open registered shell");
    CRegKey shellKey; shellKey.Attach(raw);
    // The real Shell builds this menu from the isolated class, without displaying UI.
    const std::wstring classPath = std::wstring(type) + (background ? L"\\Background" : L"");
    require(RegOpenKeyExW(HKEY_CLASSES_ROOT, classPath.c_str(), 0, KEY_READ, &raw) == ERROR_SUCCESS, "open registered class");
    CRegKey classKey; classKey.Attach(raw);
    DEFCONTEXTMENU context = {}; context.cKeys = 1; context.aKeys = &classKey.m_hKey;
    CComPtr<IContextMenu> menu;
    require(SUCCEEDED(SHCreateDefaultContextMenu(&context, IID_PPV_ARGS(&menu))), "create real Shell menu");
    HMENU popup = CreatePopupMenu();
    require(popup != nullptr, "create undisplayed menu");
    HRESULT queried = menu->QueryContextMenu(popup, 0, 1, 1000, CMF_NORMAL);
    const int count = GetMenuItemCount(popup);
    wchar_t caption[256] = {}, verb[256] = {};
    const UINT id = GetMenuItemID(popup, 0);
    GetMenuStringW(popup, 0, caption, _countof(caption), MF_BYPOSITION);
    HRESULT named = menu->GetCommandString(id - 1, GCS_VERBW, nullptr, reinterpret_cast<char *>(verb), _countof(verb));
    DestroyMenu(popup);
    require(SUCCEEDED(queried) && count == 1 && SUCCEEDED(named), "exactly one FileSpacer-provided visible command");
    require(lstrcmpW(verb, L"filespacer.explorer") == 0 && lstrcmpW(caption, getString(IDS_OPEN_IN_EXPLORER)) == 0,
        "real Shell displays the Explorer command");
}
void fixtures() {
    const wchar_t *types[] = {L"Directory", L"Directory\\Background", L"CompressedFolder", L"Drive"};
    for (const wchar_t *type : types) {
        const std::wstring shell = std::wstring(type) + L"\\shell";
        put(HKEY_CLASSES_ROOT, shell, nullptr, L"none");
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer", nullptr, L"Open in FileSpacer");
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command", nullptr, L"original command");
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command", L"DelegateExecute", L"original delegate");
    }
}

void checkDefaultOpening() {
    wchar_t image[MAX_PATH] = {};
    require(GetModuleFileNameW(nullptr, image, _countof(image)) < _countof(image), "test executable path");
    const std::wstring fixture = std::wstring(image) + L".fixture", marker = std::wstring(image) + L".opened";
    struct Cleanup {
        const std::wstring &fixture, &marker;
        ~Cleanup() { DeleteFileW(fixture.c_str()); DeleteFileW(marker.c_str()); }
    } cleanup{fixture, marker};
    using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;
    Handle file(CreateFileW(fixture.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);
    require(file.get() != INVALID_HANDLE_VALUE, "create unique launch fixture");
    file.reset();
    const std::wstring shell = L"Directory\\shell\\";
    put(HKEY_CLASSES_ROOT, shell + L"filespacer\\command", nullptr,
        L"\"" + std::wstring(image) + L"\" /record \"" + marker + L"\"");
    require(RegDeleteKeyValueW(HKEY_CLASSES_ROOT, (shell + L"filespacer\\command").c_str(), L"DelegateExecute") == ERROR_SUCCESS,
        "use test subprocess instead of the production COM server");
    put(HKEY_CLASSES_ROOT, shell + L"filespacer.explorer\\command", nullptr,
        L"\"" + std::wstring(image) + L"\" /wrong \"" + marker + L"\"");
    HKEY raw = nullptr;
    require(RegOpenKeyExW(HKEY_CLASSES_ROOT, L"Directory", 0, KEY_READ, &raw) == ERROR_SUCCESS, "open launch class");
    CRegKey key; key.Attach(raw);
    const ULONG masks[] = {SEE_MASK_CLASSKEY, SEE_MASK_CLASSKEY | SEE_MASK_INVOKEIDLIST};
    for (ULONG mask : masks) {
        SHELLEXECUTEINFOW info = {sizeof(info)};
        info.fMask = mask | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
        info.hkeyClass = key; info.lpFile = fixture.c_str(); info.nShow = SW_HIDE;
        require(!!ShellExecuteExW(&info), "ShellExecute resolves the hidden default command");
        Handle process(info.hProcess, CloseHandle);
        require(process && WaitForSingleObject(process.get(), 10000) == WAIT_OBJECT_0, "default invocation completed");
        DWORD code = 1;
        require(GetExitCodeProcess(process.get(), &code) && code == 0
            && GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES, "actual subprocess was FileSpacer's command, not the alternative");
        require(!!DeleteFileW(marker.c_str()), "remove launch marker");
    }
    put(HKEY_CLASSES_ROOT, shell + L"filespacer\\command", nullptr, L"original command");
    put(HKEY_CLASSES_ROOT, shell + L"filespacer\\command", L"DelegateExecute", L"original delegate");
}
}

bool logHRESULT(long result, const char *, int, const char *) { return SUCCEEDED(result); }
void logLastError(const char *, int, const char *expression) { throw std::runtime_error(expression); }

int wmain(int argc, wchar_t **argv) {
    if (argc == 3) {
        if (lstrcmpW(argv[1], L"/record") != 0) return 2;
        std::unique_ptr<void, decltype(&CloseHandle)> marker(CreateFileW(argv[2], GENERIC_WRITE,
            0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);
        return marker.get() == INVALID_HANDLE_VALUE ? 3 : 0;
    }
    SetThreadUILanguage(argc == 2 && lstrcmpW(argv[1], L"fr") == 0 ? 0x040c : 0x0409);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) { std::fprintf(stderr, "FAIL: COM initialization %08lx\n", initialized); return 1; }
    const std::wstring path = L"Software\\FileSpacer.ContextMenuTest-" + std::to_wstring(GetCurrentProcessId());
    HKEY user = nullptr, classes = nullptr;
    int exitCode = 0;
    try {
        require(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0,
            KEY_ALL_ACCESS, nullptr, &user, nullptr) == ERROR_SUCCESS, "create isolated registry");
        require(RegCreateKeyExW(user, L"Software\\Classes", 0, nullptr, 0,
            KEY_ALL_ACCESS, nullptr, &classes, nullptr) == ERROR_SUCCESS, "create isolated classes");
        require(RegOverridePredefKey(HKEY_CURRENT_USER, user) == ERROR_SUCCESS, "isolate user writes");
        require(RegOverridePredefKey(HKEY_CLASSES_ROOT, classes) == ERROR_SUCCESS, "isolate merged class reads");
        require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "portable launch leaves missing registration alone");
        require(!exists(HKEY_CLASSES_ROOT, L"Directory\\shell\\filespacer.explorer"), "portable launch installs nothing");
        fixtures();
        for (int pass = 0; pass < 4; ++pass) {
            const bool explorer = (pass % 2) == 0;
            require(settings::setDefaultBrowser(explorer).status() == ERROR_SUCCESS, "toggle all associations and menus");
            checkMenu(L"Directory", explorer); checkMenu(L"Directory", explorer, true);
            checkMenu(L"CompressedFolder", explorer); checkMenu(L"Drive", explorer);
            require(read(HKEY_CLASSES_ROOT, L"Directory\\shell") == (explorer ? L"filespacer" : L"none"), "default association preserved");
            require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "repeat synchronization succeeds");
            if (pass == 0) checkDefaultOpening();
        }
        // External changes can differ between classes; startup sync must not overwrite them.
        put(HKEY_CLASSES_ROOT, L"Directory\\shell", nullptr, L"FILESPACER");
        put(HKEY_CLASSES_ROOT, L"CompressedFolder\\shell", nullptr, L"other-manager");
        require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "resynchronize external changes");
        checkMenu(L"Directory", true); checkMenu(L"Directory", true, true);
        checkMenu(L"CompressedFolder", false); checkMenu(L"Drive", false);
        require(read(HKEY_CLASSES_ROOT, L"CompressedFolder\\shell") == L"other-manager", "external default not overwritten");
        put(HKEY_CLASSES_ROOT, L"Directory\\shell", nullptr, L"filespacer,open");
        require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "read native ordered verb list");
        checkMenu(L"Directory", true);
        put(HKEY_CLASSES_ROOT, L"Directory\\shell", nullptr, L"other-manager filespacer");
        require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "read external manager first in verb list");
        checkMenu(L"Directory", false);
        DWORD invalid = 1;
        require(RegSetKeyValueW(HKEY_CLASSES_ROOT, L"Directory\\shell", nullptr, REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS,
            "inject malformed association");
        put(HKEY_CLASSES_ROOT, L"Drive\\shell", nullptr, L"filespacer");
        auto result = settings::synchronizeContextMenus();
        require(result.directory != ERROR_SUCCESS && result.drive == ERROR_SUCCESS, "failure reported without skipping independent menus");
        checkMenu(L"Drive", true);
    } catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s (%d)\n", error.what(), checks); exitCode = 1; }
    if (RegOverridePredefKey(HKEY_CLASSES_ROOT, nullptr) != ERROR_SUCCESS) exitCode = 1;
    if (RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS) exitCode = 1;
    if (classes) RegCloseKey(classes);
    if (user) RegCloseKey(user);
    if (RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()) != ERROR_SUCCESS) exitCode = 1;
    CoUninitialize();
    if (!exitCode) std::printf("PASS: %d checks; isolated registrations, real undisplayed Shell menus, both toggle directions and external changes.\n", checks);
    return exitCode;
}
