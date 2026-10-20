#include "FolderIdentity.h"
#include <sqlite3.h>
#include <shlobj.h>
#include <cstdio>
#include <string>
static std::string utf8(const wchar_t *text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (size) WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], size, nullptr, nullptr);
    if (!result.empty()) result.pop_back();
    return result;
}
int wmain(int argc, wchar_t **argv) {
    if (argc == 3 && wcscmp(argv[1], L"--key") == 0) {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
        IShellItem *item = nullptr;
        if (FAILED(SHCreateItemFromParsingName(argv[2], nullptr, IID_PPV_ARGS(&item)))) return 1;
        auto key = filespacer::folderWindowProperty(filespacer::getFolderIdentity(item));
        item->Release(); CoUninitialize();
        if (key.empty()) return 1;
        printf("%s\n", utf8(key.c_str()).c_str());
        return 0;
    }
    if (argc != 4 || wcscmp(argv[1], L"--exists") != 0) return 2;
    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(utf8(argv[2]).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 1;
    }
    sqlite3_busy_timeout(db, 3000);
    sqlite3_stmt *statement = nullptr;
    bool ok = sqlite3_prepare_v2(db, "SELECT 1 FROM folder_state WHERE key=?1", -1, &statement, nullptr) == SQLITE_OK;
    std::string key = utf8(argv[3]);
    if (ok) ok = sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_STATIC) == SQLITE_OK;
    int result = ok ? sqlite3_step(statement) : SQLITE_ERROR;
    if (result == SQLITE_ROW || result == SQLITE_DONE) printf("%d\n", result == SQLITE_ROW ? 1 : 0);
    sqlite3_finalize(statement); sqlite3_close(db);
    return result == SQLITE_ROW || result == SQLITE_DONE ? 0 : 1;
}
