#include <common.h>
#include "Settings.h"
#include "UIStrings.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlguid.h>
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
    require(exists(HKEY_CLASSES_ROOT, shell + L"\\filespacer", L"ProgrammaticAccessOnly") == false,
        "explicit commands use dynamic state instead of a global visibility mask");
    require(exists(HKEY_CLASSES_ROOT, shell + L"\\filespacer.explorer") == true,
        "Explorer command exists independently of the effective default");
    require(read(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command") == L"original command",
        "original FileSpacer command preserved");
    require(read(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command", L"DelegateExecute") == L"original delegate",
        "original COM delegate preserved");
    require(read(HKEY_CLASSES_ROOT, shell + L"\\filespacer", L"CommandStateHandler")
        == L"{B6AE98A3-F59D-4F07-B5E8-CAD54DC5516C}", "FileSpacer command has host-dependent visibility");
    require(read(HKEY_CLASSES_ROOT, shell + L"\\filespacer.explorer", L"CommandStateHandler")
        == read(HKEY_CLASSES_ROOT, shell + L"\\filespacer", L"CommandStateHandler"), "both commands use the same host filter");
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
    if (!background && explorer) {
        const std::wstring competing = shell + L"\\aaa-other-manager";
        put(HKEY_CLASSES_ROOT, competing, nullptr, L"Other file manager");
        put(HKEY_CLASSES_ROOT, competing + L"\\command", nullptr, L"other manager command");
        HMENU defaultPopup = CreatePopupMenu();
        require(defaultPopup != nullptr, "create default-action menu");
        const HRESULT defaultResult = menu->QueryContextMenu(defaultPopup, 0, 1, 1000, CMF_DEFAULTONLY);
        const UINT defaultId = GetMenuDefaultItem(defaultPopup, FALSE, 0);
        wchar_t defaultVerb[256] = {};
        const HRESULT defaultName = defaultId == static_cast<UINT>(-1) ? E_FAIL
            : menu->GetCommandString(defaultId - 1, GCS_VERBW, nullptr,
                reinterpret_cast<char *>(defaultVerb), _countof(defaultVerb));
        DestroyMenu(defaultPopup);
        require(RegDeleteTreeW(HKEY_CLASSES_ROOT, competing.c_str()) == ERROR_SUCCESS, "remove competing manager fixture");
        require(SUCCEEDED(defaultResult) && SUCCEEDED(defaultName)
            && lstrcmpiW(defaultVerb, L"open") == 0,
            "native default-only menu keeps the unconditional open action");
    }
    menu.Release();
    require(SUCCEEDED(SHCreateDefaultContextMenu(&context, IID_PPV_ARGS(&menu))), "recreate normal Shell menu");
    HMENU popup = CreatePopupMenu();
    require(popup != nullptr, "create undisplayed menu");
    HRESULT queried = menu->QueryContextMenu(popup, 0, 1, 1000, CMF_NORMAL);
    const int count = GetMenuItemCount(popup);
    wchar_t verb[256] = {};
    // This executable is neither Explorer nor FileSpacer: both explicit verbs are hidden.
    const UINT id = background ? 0 : GetMenuItemID(popup, 0);
    const HRESULT named = background ? S_OK : menu->GetCommandString(id - 1, GCS_VERBW,
        nullptr, reinterpret_cast<char *>(verb), _countof(verb));
    DestroyMenu(popup);
    require(SUCCEEDED(queried) && count == (background ? 0 : 1) && SUCCEEDED(named),
        "foreign host retains only the base open command");
    if (!background) require(lstrcmpW(verb, L"open") == 0, "base open keeps its canonical verb");
}
void fixtures() {
    put(HKEY_CLASSES_ROOT, L"Folder\\shell", nullptr, L"open");
    put(HKEY_CLASSES_ROOT, L"Folder\\shell\\open\\command", nullptr, L"native Explorer command");
    const wchar_t *types[] = {L"Directory", L"Directory\\Background", L"CompressedFolder", L"Drive", L"NetServer", L"NetShare"};
    for (const wchar_t *type : types) {
        const std::wstring shell = std::wstring(type) + L"\\shell";
        put(HKEY_CLASSES_ROOT, shell, nullptr, L"none");
        if (lstrcmpW(type, L"Directory\\Background") != 0) {
            put(HKEY_CLASSES_ROOT, shell + L"\\open\\command", nullptr, L"native Explorer command");
        }
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer", nullptr, L"Open in FileSpacer");
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer", L"ProgrammaticAccessOnly", L"");
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command", nullptr, L"original command");
        put(HKEY_CLASSES_ROOT, shell + L"\\filespacer\\command", L"DelegateExecute", L"original delegate");
    }
}

void checkServerDefault(IShellItem *server, bool fileSpacerDefault) {
    CComPtr<IQueryAssociations> associations;
    require(SUCCEEDED(server->BindToHandler(nullptr, BHID_AssociationArray, IID_PPV_ARGS(&associations))),
        "query actual server target associations");
    wchar_t command[2048] = {}; DWORD size = _countof(command);
    require(SUCCEEDED(associations->GetString(0, ASSOCSTR_COMMAND, nullptr, command, &size)),
        "read actual server default command");
    require(lstrcmpW(command, fileSpacerDefault ? L"original command" : L"native Explorer command") == 0,
        "server shortcut target follows the selected file manager");
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
    put(HKEY_CLASSES_ROOT, shell + L"open\\command", nullptr,
        L"\"" + std::wstring(image) + L"\" /record \"" + marker + L"\"");
    require(RegDeleteKeyValueW(HKEY_CLASSES_ROOT, (shell + L"open\\command").c_str(), L"DelegateExecute") == ERROR_SUCCESS,
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
        require(!!ShellExecuteExW(&info), "ShellExecute resolves the selected default command");
        Handle process(info.hProcess, CloseHandle);
        require(process && WaitForSingleObject(process.get(), 10000) == WAIT_OBJECT_0, "default invocation completed");
        DWORD code = 1;
        require(GetExitCodeProcess(process.get(), &code) && code == 0
            && GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES, "actual subprocess was FileSpacer's command, not the alternative");
        require(!!DeleteFileW(marker.c_str()), "remove launch marker");
    }
    DEFCONTEXTMENU context = {}; context.cKeys = 1; context.aKeys = &key.m_hKey;
    CComPtr<IContextMenu> menu;
    require(SUCCEEDED(SHCreateDefaultContextMenu(&context, IID_PPV_ARGS(&menu))), "create default invocation menu");
    HMENU popup = CreatePopupMenu();
    require(popup != nullptr, "create default invocation popup");
    const HRESULT queried = menu->QueryContextMenu(popup, 0, 1, 1000, CMF_DEFAULTONLY);
    const UINT defaultId = GetMenuDefaultItem(popup, FALSE, 0);
    DestroyMenu(popup);
    require(SUCCEEDED(queried) && defaultId != static_cast<UINT>(-1), "native opening action exists");
    CMINVOKECOMMANDINFO invocation = {sizeof(invocation)};
    invocation.fMask = CMIC_MASK_NOASYNC;
    invocation.lpVerb = MAKEINTRESOURCEA(defaultId - 1);
    invocation.nShow = SW_HIDE;
    require(SUCCEEDED(menu->InvokeCommand(&invocation)), "invoke the native default action");
    for (int attempt = 0; attempt < 200
            && GetFileAttributesW(marker.c_str()) == INVALID_FILE_ATTRIBUTES; ++attempt) Sleep(50);
    require(GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES,
        "native default action actually ran FileSpacer's command");
    require(!!DeleteFileW(marker.c_str()), "remove native default invocation marker");
    put(HKEY_CLASSES_ROOT, shell + L"open\\command", nullptr, L"original command");
    put(HKEY_CLASSES_ROOT, shell + L"open\\command", L"DelegateExecute", L"original delegate");
}
}

bool logHRESULT(long result, const char *, int, const char *) { return SUCCEEDED(result); }
void logLastError(const char *, int, const char *expression) { throw std::runtime_error(expression); }

int wmain(int argc, wchar_t **argv) {
    if (argc == 3) {
        if (lstrcmpW(argv[1], L"/record") != 0) return 2;
        std::unique_ptr<void, decltype(&CloseHandle)> marker(CreateFileW(argv[2], GENERIC_WRITE,
            FILE_SHARE_DELETE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);
        return marker.get() == INVALID_HANDLE_VALUE ? 3 : 0;
    }
    SetThreadUILanguage(argc == 2 && lstrcmpW(argv[1], L"fr") == 0 ? 0x040c : 0x0409);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) { std::fprintf(stderr, "FAIL: COM initialization %08lx\n", initialized); return 1; }
    const std::wstring path = L"Software\\FileSpacer.ContextMenuTest-" + std::to_wstring(GetCurrentProcessId());
    HKEY user = nullptr, classes = nullptr;
    int exitCode = 0;
    wchar_t image[MAX_PATH] = {};
    std::wstring shortcutPath;
    try {
        const DWORD imageLength = GetModuleFileNameW(nullptr, image, _countof(image));
        require(imageLength && imageLength < _countof(image), "shortcut fixture executable path");
        shortcutPath = std::wstring(image) + L".lnk";
        CComPtr<IShellItem> server, shortcut, shortcutTarget;
        require(SUCCEEDED(SHCreateItemFromParsingName(L"\\\\localhost", nullptr, IID_PPV_ARGS(&server))),
            "parse a real server root without enumerating shares");
        CComHeapPtr<ITEMIDLIST> serverID;
        require(SUCCEEDED(SHGetIDListFromObject(server, &serverID)), "get native server identity");
        CComPtr<IShellLinkW> link;
        require(SUCCEEDED(link.CoCreateInstance(CLSID_ShellLink)), "create native server shortcut");
        require(SUCCEEDED(link->SetIDList(serverID)), "set server shortcut target");
        CComQIPtr<IPersistFile> persisted(link);
        require(persisted && SUCCEEDED(persisted->Save(shortcutPath.c_str(), FALSE)), "save isolated shortcut");
        require(SUCCEEDED(SHCreateItemFromParsingName(shortcutPath.c_str(), nullptr, IID_PPV_ARGS(&shortcut))),
            "parse the saved shortcut");
        require(SUCCEEDED(shortcut->BindToHandler(nullptr, BHID_LinkTargetItem, IID_PPV_ARGS(&shortcutTarget))),
            "resolve the real shortcut target through the Shell");
        require(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0,
            KEY_ALL_ACCESS, nullptr, &user, nullptr) == ERROR_SUCCESS, "create isolated registry");
        require(RegCreateKeyExW(user, L"Software\\Classes", 0, nullptr, 0,
            KEY_ALL_ACCESS, nullptr, &classes, nullptr) == ERROR_SUCCESS, "create isolated classes");
        require(RegOverridePredefKey(HKEY_CURRENT_USER, user) == ERROR_SUCCESS, "isolate user writes");
        require(RegOverridePredefKey(HKEY_CLASSES_ROOT, classes) == ERROR_SUCCESS, "isolate merged class reads");
        require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "portable launch leaves missing registration alone");
        require(!exists(HKEY_CLASSES_ROOT, L"Directory\\shell\\filespacer.explorer"), "portable launch installs nothing");
        put(HKEY_CLASSES_ROOT, L"Directory\\shell\\filespacer", nullptr, L"Open in FileSpacer");
        require(!settings::supportsDefaultBrowser(), "old installation needs the network verbs before claiming the default");
        fixtures();
        require(settings::supportsDefaultBrowser(), "complete installation supports default file manager selection");
        for (int pass = 0; pass < 4; ++pass) {
            const bool explorer = (pass % 2) == 0;
            require(settings::setDefaultBrowser(explorer).status() == ERROR_SUCCESS, "toggle all associations and menus");
            checkMenu(L"Directory", explorer); checkMenu(L"Directory", explorer, true);
            checkMenu(L"CompressedFolder", explorer); checkMenu(L"Drive", explorer);
            checkMenu(L"NetServer", explorer); checkMenu(L"NetShare", explorer);
            checkServerDefault(shortcutTarget, explorer);
            require(read(HKEY_CLASSES_ROOT, L"Directory\\shell\\open\\command")
                == (explorer ? L"original command" : L"native Explorer command"),
                "switching the default changes and restores the base open command");
            require(exists(HKEY_CLASSES_ROOT, L"Directory\\shell\\open\\command", L"DelegateExecute") == explorer,
                "restoration preserves the original absence of a COM delegate");
            require(read(HKEY_CLASSES_ROOT, L"Directory\\shell") == (explorer ? L"open" : L"none"), "default association preserved");
            require(settings::synchronizeContextMenus().status() == ERROR_SUCCESS, "repeat synchronization succeeds");
            if (pass == 0) checkDefaultOpening();
        }
        require(settings::setDefaultBrowser(true).status() == ERROR_SUCCESS, "activate before an external command change");
        put(HKEY_CLASSES_ROOT, L"Directory\\shell\\open\\command", nullptr, L"external command");
        require(settings::setDefaultBrowser(false).status() == ERROR_SUCCESS, "deactivate after an external command change");
        require(read(HKEY_CLASSES_ROOT, L"Directory\\shell\\open\\command") == L"external command",
            "restoration preserves an external replacement");
        put(HKEY_CLASSES_ROOT, L"Directory\\shell\\open\\command", nullptr, L"native Explorer command");
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
        put(HKEY_CLASSES_ROOT, L"Directory\\shell", nullptr, L"none");
        require(RegSetKeyValueW(HKEY_CLASSES_ROOT, L"NetServer\\shell", nullptr, REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS,
            "inject malformed network server association");
        put(HKEY_CLASSES_ROOT, L"NetShare\\shell", nullptr, L"filespacer");
        result = settings::synchronizeContextMenus();
        require(result.directory == ERROR_SUCCESS && result.networkServer != ERROR_SUCCESS
            && result.networkShare == ERROR_SUCCESS && result.status() != ERROR_SUCCESS,
            "server failure is reported without skipping the share menu");
        checkMenu(L"NetShare", true);
        put(HKEY_CLASSES_ROOT, L"NetServer\\shell", nullptr, L"filespacer");
        require(RegSetKeyValueW(HKEY_CLASSES_ROOT, L"NetShare\\shell", nullptr, REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS,
            "inject malformed network share association");
        result = settings::synchronizeContextMenus();
        require(result.directory == ERROR_SUCCESS && result.networkServer == ERROR_SUCCESS
            && result.networkShare != ERROR_SUCCESS && result.status() != ERROR_SUCCESS,
            "share failure is reported without skipping the server menu");
        checkMenu(L"NetServer", true);
    } catch (const std::exception &error) { std::fprintf(stderr, "FAIL: %s (%d)\n", error.what(), checks); exitCode = 1; }
    if (RegOverridePredefKey(HKEY_CLASSES_ROOT, nullptr) != ERROR_SUCCESS) exitCode = 1;
    if (RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS) exitCode = 1;
    if (classes) RegCloseKey(classes);
    if (user) RegCloseKey(user);
    if (RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()) != ERROR_SUCCESS) exitCode = 1;
    if (!shortcutPath.empty() && GetFileAttributesW(shortcutPath.c_str()) != INVALID_FILE_ATTRIBUTES
            && !DeleteFileW(shortcutPath.c_str())) exitCode = 1;
    CoUninitialize();
    if (!exitCode) std::printf("PASS: %d checks; isolated registrations, real undisplayed Shell menus, both toggle directions and external changes.\n", checks);
    return exitCode;
}
