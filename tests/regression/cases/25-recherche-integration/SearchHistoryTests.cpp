#include <common.h>
#include "FolderStateStore.h"
#include "Settings.h"
#include <sqlite3.h>
#include <iostream>
#include <stdexcept>
#include <fstream>

using namespace filespacer;
static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
static std::vector<std::string> history(FolderStateStore &store, const char *key, unsigned limit = 20) {
    std::vector<std::string> result;
    require(store.readSearchHistory(key, limit, &result), store.error().c_str());
    return result;
}
static void sql(const char *path, const char *command) {
    sqlite3 *db = nullptr;
    require(sqlite3_open(path, &db) == SQLITE_OK, "open fixture");
    const int code = sqlite3_exec(db, command, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    require(code == SQLITE_OK, "fixture SQL");
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "isolated database path required");
        const char *path = argv[1];
        require(!std::ifstream(path).good(), "fixture already exists");
        require(settings::DEFAULT_SEARCH_HISTORY_LIMIT == 5, "default history limit");
        const std::vector<std::string> recent = {"newest", "middle", "oldest"};
        {
            FolderStateStore store(path);
            require(store.ready(), store.error().c_str());
            FolderState state;
            state.x = 123; state.y = -456; state.view.present = FolderViewSettings::Mode;
            state.view.mode = 4; state.icons = {1, 2, 3};
            require(store.save("A", state, FolderState::Position | FolderState::View | FolderState::Icons), "save state A");
            require(store.save("B", state, FolderState::Position), "save state B");
            require(store.saveSearchHistory("A", recent, 5), "save history A");
            require(store.saveSearchHistory("B", {"other folder"}, 5), "save history B");
            require(history(store, "A") == recent && history(store, "B") == std::vector<std::string>{"other folder"}, "folder isolation/order");
            sql(path, "CREATE TRIGGER fail_history BEFORE INSERT ON search_history WHEN new.query='blocked' "
                "BEGIN SELECT RAISE(ABORT,'forced history failure'); END;");
            require(!store.saveSearchHistory("A", {"blocked"}, 5) && history(store, "A") == recent,
                "failed history write lost committed queries");
            sql(path, "DROP TRIGGER fail_history;");
            FolderState after;
            require(store.read("A", &after) && after.x == 123 && after.y == -456
                && after.view.mode == 4 && after.icons == state.icons, "history changed existing state");
        }
        {
            FolderStateStore store(path);
            require(history(store, "A") == recent, "close/reopen persistence");
            std::vector<std::string> many;
            for (int index = 21; index >= 0; --index) many.push_back("query-" + std::to_string(index));
            require(store.saveSearchHistory("A", many, 20), "maximum 20");
            many.resize(20);
            require(history(store, "A") == many, "20 entries and chronology");
            require(store.trimSearchHistory(2), "reduce all histories");
            many.resize(2);
            require(history(store, "A") == many && history(store, "B").size() == 1, "closed folder reduction");
            require(store.trimSearchHistory(20) && history(store, "A") == many, "increase resurrected trimmed queries");
            require(store.trimSearchHistory(0) && history(store, "A").empty() && history(store, "B").empty(), "zero clears all folders");
            require(store.saveSearchHistory("A", recent, 0) && history(store, "A").empty(), "disabled history saved queries");
            require(store.saveSearchHistory("A", {"accent-\xc3\xa9", "quote-\" & | %", ""}, 20), "special text");
            require(history(store, "A") == std::vector<std::string>({"accent-\xc3\xa9", "quote-\" & | %"}), "special text round trip/empty entry");
            require(store.move("A", "renamed") && history(store, "A").empty()
                && history(store, "renamed").size() == 2, "rename must preserve history");
            const int64_t first = 1704067200; // 2024-01-01 UTC
            bool expired = false;
            require(store.recordAccess("renamed", false, first, &expired) && !expired
                && history(store, "renamed").size() == 2, "temporary failure removed history");
            require(store.recordAccess("renamed", true, first + 100) && history(store, "renamed").size() == 2, "recovery removed history");
            require(store.recordAccess("renamed", false, first + 200, &expired) && !expired, "first failure after recovery");
            require(store.recordAccess("renamed", false, first + 366LL * 86400 + 200, &expired)
                && expired && history(store, "renamed").empty(), "qualified expiration did not clean history");
            require(store.saveSearchHistory("renamed", recent, 5) && history(store, "renamed").empty(), "late history write resurrected expired folder");
            require(store.saveSearchHistory("B", recent, 5) && store.remove("B") && history(store, "B").empty(), "deletion cleanup");
        }
        // A version-5 database and unrelated search data survive migration verbatim.
        sql(path, "DROP TRIGGER delete_search_history; DROP TABLE search_history; PRAGMA user_version=5;"
            "CREATE TABLE legacy_search(query TEXT, filter BLOB); INSERT INTO legacy_search VALUES('saved query',x'010203');");
        {
            FolderStateStore store(path); require(store.ready(), "version 5 migration failed");
            require(history(store, "B").empty(), "migration created history entries");
            require(store.clearAll(), "clear all states");
        }
        sql(path, "CREATE TRIGGER verify_legacy BEFORE DELETE ON legacy_search BEGIN SELECT RAISE(ABORT,'legacy touched'); END;"
            "CREATE TABLE assertion(value INTEGER CHECK(value=1));"
            "INSERT INTO assertion SELECT query='saved query' AND filter=x'010203' FROM legacy_search;");
        {
            FolderStateStore store(path);
            FolderState original; original.x = 789; original.y = -123; original.icons = {4, 5};
            require(store.save("version4", original, FolderState::Position | FolderState::Icons), "prepare version 4 state");
        }
        sql(path, "DROP TRIGGER delete_search_history; DROP TABLE search_history;"
            "ALTER TABLE folder_state DROP COLUMN locked; PRAGMA user_version=4;");
        {
            FolderStateStore store(path); require(store.ready(), "version 4 migration failed");
            FolderState migrated;
            require(store.read("version4", &migrated) && migrated.x == 789 && migrated.y == -123
                && migrated.icons == std::vector<uint8_t>({4, 5}) && !migrated.locked,
                "version 4 migration changed existing state");
            require(history(store, "version4").empty(), "version 4 migration invented history");
        }
        std::cout << "PASS: history isolation, ordering, persistence, 0/5/20, limits, state preservation, migration, rename and conservative cleanup.\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
