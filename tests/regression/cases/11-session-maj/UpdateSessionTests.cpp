#include "UpdateSessionStore.h"
#include <sqlite3.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace filespacer;

void sql(const char *file, const char *query) {
    sqlite3 *db = nullptr;
    assert(sqlite3_open(file, &db) == SQLITE_OK);
    assert(sqlite3_exec(db, query, nullptr, nullptr, nullptr) == SQLITE_OK);
    assert(sqlite3_close(db) == SQLITE_OK);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Pass a fresh temporary database path.\n"); return 2;
    }
    const char *file = argv[1];
    assert(createUpdateSession(file) == SQLITE_OK);
    std::vector<std::string> read;
    assert(readUpdateSession(file, &read) == SQLITE_OK && read.empty());
    // More than the closed-window history's 20 entries, multiple processes,
    // long/Unicode paths and a virtual folder's Shell parsing name.
    std::vector<std::string> first;
    for (unsigned i = 0; i < 31; ++i) first.push_back("C:\\Projects\\Folder " + std::to_string(i));
    first.push_back("C:\\Donn\xc3\xa9" "es\\\xe6\x96\x87\xe4\xbb\xb6");
    first.push_back("C:\\" + std::string(500, 'x'));
    const std::vector<std::string> second = {"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", "\\\\server\\share"};
    assert(saveUpdateSession(file, 123, first) == SQLITE_OK);
    assert(saveUpdateSession(file, 456, second) == SQLITE_OK);
    std::vector<std::string> expected = first;
    expected.insert(expected.end(), second.begin(), second.end());
    assert(readUpdateSession(file, &read) == SQLITE_OK && read == expected);
    // A failed replacement must retain its previous snapshot and the other process.
    sql(file, "CREATE TRIGGER fail_insert BEFORE INSERT ON windows WHEN NEW.path='FAIL' BEGIN SELECT RAISE(ABORT,'test'); END");
    assert(saveUpdateSession(file, 123, {"replacement", "FAIL"}) != SQLITE_OK);
    assert(readUpdateSession(file, &read) == SQLITE_OK && read == expected);
    sql(file, "DROP TRIGGER fail_insert");
    assert(saveUpdateSession(file, 123, {"C:\\Replacement"}) == SQLITE_OK);
    expected = second;
    expected.push_back("C:\\Replacement");
    assert(readUpdateSession(file, &read) == SQLITE_OK && read == expected);
    // Repeated captures replace rather than duplicate; a process with no windows clears only itself.
    assert(saveUpdateSession(file, 123, {"C:\\Replacement"}) == SQLITE_OK);
    assert(saveUpdateSession(file, 456, {}) == SQLITE_OK);
    assert(readUpdateSession(file, &read) == SQLITE_OK && read == std::vector<std::string>{"C:\\Replacement"});
    assert(saveUpdateSession(file, 123, {""}) == SQLITE_MISMATCH);
    assert(saveUpdateSession(file, 123, {std::string("A\0B", 3)}) == SQLITE_MISMATCH);
    assert(saveUpdateSession(file, 123, {std::string(131073, 'x')}) == SQLITE_MISMATCH);
    assert(saveUpdateSession(file, 123, std::vector<std::string>(4097, "C:\\A")) == SQLITE_TOOBIG);
    assert(saveUpdateSession(file, 123, std::vector<std::string>(4096, "C:\\A")) == SQLITE_OK);
    assert(saveUpdateSession(file, 456, {"C:\\B"}) == SQLITE_TOOBIG); // Limit applies across processes.
    assert(saveUpdateSession(file, 123, std::vector<std::string>(129, std::string(131072, 'x'))) == SQLITE_TOOBIG);
    assert(readUpdateSession(file, &read) == SQLITE_OK && read.size() == 4096);
    // Existing/bad databases and malformed rows are rejected without changing caller data.
    assert(createUpdateSession(file) != SQLITE_OK);
    sql(file, "UPDATE windows SET path=CAST(x'410042' AS TEXT) WHERE position=0");
    const auto before = read;
    assert(readUpdateSession(file, &read) == SQLITE_MISMATCH && read == before);
    sql(file, "DROP TABLE windows");
    assert(readUpdateSession(file, &read) != SQLITE_OK && read == before);
    assert(saveUpdateSession(file, 123, first) != SQLITE_OK);
    assert(std::remove(file) == 0);
    std::puts("Update session tests passed.");
}
