#include <common.h>
#include "Everything.h"
#include "SearchBox.h"
#include "WinUtils.h"
#include <iostream>
#include <stdexcept>

using namespace filespacer;
static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc == 2 && std::wstring(argv[1]) == L"--expect-absent") {
            require(findEverything().empty(), "Everything unexpectedly detected");
            std::cout << "PASS: actual machine has no detected Everything installation.\n";
            return 0;
        }
        for (bool enabled : {false, true}) for (const auto &history : {
                std::vector<std::wstring>{}, std::vector<std::wstring>{L"newest", L"older"},
                std::vector<std::wstring>{L"", L""}}) {
            HMENU menu = createSearchMenu(enabled, history);
            require(menu != nullptr, "create menu");
            const int entries = history.empty() || history[0].empty() ? 0 : 2;
            require(GetMenuItemCount(menu) == (enabled ? 1 : 0) + entries + (enabled && entries ? 1 : 0), "activation combinations / orphan separator");
            for (int index = 0; index < GetMenuItemCount(menu); ++index) {
                MENUITEMINFOW item = {sizeof(item)}; item.fMask = MIIM_FTYPE | MIIM_ID;
                require(GetMenuItemInfoW(menu, index, TRUE, &item) != FALSE, "read menu item");
                if (item.fType & MFT_SEPARATOR)
                    require(enabled && index == 1 && entries == 2, "orphan separator");
                else require(item.wID == (enabled && index == 0 ? 1u : static_cast<UINT>(index + (enabled ? 0 : 2))), "menu command order");
            }
            DestroyMenu(menu);
        }
        require(everythingArguments(L"caf\u00e9 & | % ^ -exit", L"") == L"-search \"caf\u00e9 & | % ^ -exit\"", "global args / shell metacharacters");
        require(everythingArguments(L"\"a b\"", L"") == L"-search \"\"\"\"a b\"\"\"\"", "Everything triple quote escaping");
        require(everythingArguments(L"a|b", L"C:\\Test espace\\\u00e9") == L"-search \"\"\"\"C:\\Test espace\\\u00e9\\\"\"\" <a|b>\"", "folder scope and grouped OR");
        require(everythingArguments(L"", L"C:\\") == L"-search \"\"\"\"C:\\\"\"\"\"", "root trailing slash and empty query");
        require(launchEverything(L"", L"") == ERROR_FILE_NOT_FOUND, "missing installation error");
        require(launchEverything(L"relative.exe", L"") == ERROR_BAD_PATHNAME, "relative executable accepted");
        require(launchEverything(L"C:\\missing\\Everything.exe", L"") != ERROR_SUCCESS, "missing executable reported as success");
        require(launchEverything(L"C:\\bad.cmd", L"anything") == ERROR_BAD_PATHNAME, "command interpreter path accepted");
        require(launchEverything(L"C:\\missing\\Everything.exe", std::wstring(32767, L'x')) == ERROR_FILENAME_EXCED_RANGE, "oversized command");
        std::cout << "PASS: real Win32 menus, independent sections, argument construction and native launch errors (no Everything execution in this test).\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
