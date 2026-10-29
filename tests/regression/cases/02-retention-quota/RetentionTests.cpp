#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "User32.lib")
#endif
using namespace filespacer;
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
struct Database {
    sqlite3 *db = nullptr;
    explicit Database(const char *path) { require(sqlite3_open(path, &db) == SQLITE_OK, "open observer"); }
    ~Database() { sqlite3_close(db); }
    void sql(const char *sql) { require(sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK, sqlite3_errmsg(db)); }
    int64_t number(const char *sql) {
        sqlite3_stmt *s = nullptr;
        require(sqlite3_prepare_v2(db, sql, -1, &s, nullptr) == SQLITE_OK, "prepare observer");
        const int code = sqlite3_step(s);
        const int64_t result = code == SQLITE_ROW ? sqlite3_column_int64(s, 0) : -1;
        sqlite3_finalize(s); require(code == SQLITE_ROW, "step observer"); return result;
    }
};
static int64_t used(Database &db) {
    return db.number("SELECT (page_count-freelist_count)*page_size FROM pragma_page_count(),pragma_freelist_count(),pragma_page_size()");
}
static FolderState read(FolderStateStore &store, const char *key) {
    FolderState s; require(store.read(key, &s), store.error().c_str()); return s;
}
static void successful(FolderStateStore &store, const char *key, int64_t when) {
    require(store.recordAccess(key, true, when), store.error().c_str());
}
static bool failed(FolderStateStore &store, const char *key, int64_t when) {
    bool expired = true; require(store.recordAccess(key, false, when, &expired), store.error().c_str()); return expired;
}
static void state(FolderStateStore &store, const char *key, size_t size, int64_t when) {
    successful(store, key, when);
    FolderState s; s.lastSuccessAt = when; s.x = 123; s.y = -456; s.width = 640; s.height = 480;
    s.icons.resize(size, 42);
    require(store.save(key, s, FolderState::Position | FolderState::Size | FolderState::Icons), store.error().c_str());
}
int main(int argc, char **argv) {
    const std::string path = argc > 1 ? argv[1] : "retention-test.sqlite3";
    try {
        if (FILE *f = std::fopen(path.c_str(), "rb")) { std::fclose(f); throw std::runtime_error("Choose a nonexistent test database path"); }
        {
            FolderStateStore store(path.c_str()); require(store.ready(), store.error().c_str());
            Database db(path.c_str());
            require(db.number("PRAGMA user_version") == 6, "schema version");
            require(db.number("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='search_history'") == 1,
                "separate history table missing");
            require(db.number("PRAGMA synchronous") == 2, "FULL must remain enabled");
            // Connection-specific WAL settings are inspected on a second production store
            // through observable recycling; this observer's own pragma values are defaults.
            state(store, "folder", 32768, 1600000000);
            require(store.setLocked("folder", read(store, "folder"), FolderState::All, true), "locked retention fixture");
            require(!failed(store, "folder", 1709208000), "first failure deleted row"); // 2024-02-29 12:00 UTC
            FolderState s = read(store, "folder");
            require(s.firstFailedAt == 1709208000 && s.lastSuccessAt == 1600000000 && s.icons.size() == 32768, "first failure metadata/state");
            require(!failed(store, "folder", 1740743999), "expired before calendar anniversary"); // 2025-02-28 11:59:59
            require(read(store, "folder").firstFailedAt == 1709208000, "failure timestamp moved");
            successful(store, "other", 1900000000);
            require(db.number("SELECT count(*) FROM folder_state WHERE key='folder'") == 1, "inactivity purged folder");
            require(!failed(store, "folder", 1740830399), "expired a second too early");
            require(failed(store, "folder", 1740830400), "leap-year anniversary did not delete"); // March 1 12:00
            require(db.number("SELECT count(*) FROM folder_state WHERE key='folder'") == 0, "failure must DELETE row");
            require(!failed(store, "absent", 1800000000) && db.number("SELECT count(*) FROM folder_state WHERE key='absent'") == 0, "failure created error-only row");
            successful(store, "folder", 1800000000);
            require(!failed(store, "folder", 1800001000), "new first failure");
            successful(store, "folder", 1800002000);
            require(!read(store, "folder").firstFailedAt, "success did not clear failure period");
            require(!failed(store, "folder", 1900000000), "new first failure inherited old period");
            // A success is refreshed at most once a day; ending a failure period is always written.
            successful(store, "daily", 1850000000);
            successful(store, "daily", 1850086399);
            require(read(store, "daily").lastSuccessAt == 1850000000, "success rewritten within a day");
            successful(store, "daily", 1850086400);
            require(read(store, "daily").lastSuccessAt == 1850086400, "success not refreshed after a day");
            require(!failed(store, "daily", 1850090000), "daily first failure");
            successful(store, "daily", 1850090001);
            s = read(store, "daily");
            require(s.lastSuccessAt == 1850090001 && !s.firstFailedAt, "success ending a failure period not recorded at once");
            FolderState old; old.x = 10; old.y = 20; old.lastSuccessAt = 1; old.firstFailedAt = 1;
            require(store.save("folder", old, FolderState::Position), "save old snapshot");
            s = read(store, "folder");
            require(s.lastSuccessAt == 1800002000 && s.firstFailedAt == 1900000000, "geometry save overwrote metadata");
            require(store.move("folder", "moved"), "rekey metadata");
            s = read(store, "moved"); require(s.lastSuccessAt == 1800002000 && s.firstFailedAt == 1900000000, "rekey lost metadata");
            db.sql("CREATE TRIGGER fail_access BEFORE UPDATE ON folder_state WHEN new.last_success_at=2000000000 BEGIN SELECT RAISE(ABORT,'forced failure'); END");
            require(!store.recordAccess("moved", true, 2000000000), "SQLite failure ignored");
            s = read(store, "moved"); require(s.lastSuccessAt == 1800002000 && s.firstFailedAt == 1900000000, "failed transaction changed metadata");
            db.sql("DROP TRIGGER fail_access");
            db.sql("CREATE TRIGGER fail_delete BEFORE DELETE ON folder_state WHEN old.key='moved' BEGIN SELECT RAISE(ABORT,'forced deletion failure'); END");
            bool expired = true;
            require(!store.recordAccess("moved", false, 2000000000, &expired) && !expired, "failed deletion reported as committed");
            require(db.number("SELECT count(*) FROM folder_state WHERE key='moved'") == 1, "failed deletion lost row");
            db.sql("DROP TRIGGER fail_delete");
            require(store.clearAll(), "reset before quota");
            state(store, "old", 40000000, 100);
            state(store, "middle", 40000000, 200);
            state(store, "new", 40000000, 300);
            require(used(db) <= 100000000, "used-page quota not enforced");
            require(db.number("SELECT count(*) FROM folder_state WHERE key='old'") == 0
                && db.number("SELECT count(*) FROM folder_state WHERE key='middle'") == 1
                && db.number("SELECT count(*) FROM folder_state WHERE key='new'") == 1, "quota did not evict oldest row");
            require(db.number("PRAGMA freelist_count") > 0, "deletion did not free reusable pages");
            const int64_t pages = db.number("PRAGMA page_count");
            require(pages * db.number("PRAGMA page_size") > 100000000, "fixture does not prove physical/used distinction");
            successful(store, "small", 400);
            require(db.number("SELECT count(*) FROM folder_state WHERE key='middle'") == 1, "physical size caused false eviction");
            state(store, "replacement", 15000000, 500);
            require(db.number("PRAGMA page_count") <= pages + 4, "free pages were not reused");
            // Snapshot recreation after a quota deletion retains the real last access time.
            FolderState snapshot = read(store, "middle");
            db.sql("DELETE FROM folder_state WHERE key='middle'");
            require(store.save("middle", snapshot, FolderState::Position), "recreate purged open snapshot");
            require(read(store, "middle").lastSuccessAt == 200, "save fabricated a new successful access");
            state(store, "oversized", 105000000, 600);
            require(used(db) <= 100000000 && db.number("SELECT count(*) FROM folder_state WHERE key='oversized'") == 0, "oversized row or reinsertion loop");
            // A reader pins WAL history; used-page quota must not include repeated journal frames.
            successful(store, "wal", 700);
            Database reader(path.c_str()); reader.sql("BEGIN; SELECT * FROM folder_state");
            FolderState walState; walState.icons.resize(65536); walState.lastSuccessAt = 700;
            for (int i = 0; i < 200; ++i) {
                walState.icons[0] = static_cast<uint8_t>(i);
                require(store.save("wal", walState, FolderState::Icons), "WAL write");
            }
            require(used(db) < 1000000 && db.number("SELECT count(*) FROM folder_state WHERE key='wal'") == 1, "WAL history triggered quota");
            reader.sql("COMMIT");
            require(store.save("wal", walState, FolderState::Icons), "checkpoint after reader");
            require(store.save("wal", walState, FolderState::Icons), "recycle after checkpoint");
            FILE *journal = std::fopen((path+"-wal").c_str(), "rb");
            require(journal != nullptr, "open WAL observer");
            std::fseek(journal, 0, SEEK_END);
            long retained = std::ftell(journal); std::fclose(journal);
            require(retained >= 0 && retained <= 8000000, "recycled WAL retained more than configured limit");
            std::cout << "PASS: failure anniversary, success recovery, daily success refresh, unknown keys, stale saves, transactional errors, rekey metadata, 100MB quota, free-page reuse, oversized row, WAL history.\n";
#ifdef _WIN32
            require(store.clearAll(), "reset Windows quota");
            HWND window = CreateWindowExW(0, L"STATIC", L"quota test", WS_OVERLAPPED, 0, 0, 10, 10, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            require(window != nullptr && SetPropW(window, L"open", reinterpret_cast<HANDLE>(1)), "native quota window");
            state(store, "open", 40000000, 100);
            state(store, "closed", 40000000, 200);
            require(store.saveSearchHistory("closed", {"closed query"}, 5), "closed quota history");
            require(store.setLocked("closed", read(store, "closed"), FolderState::Position, true), "locked quota fixture");
            state(store, "recent", 40000000, 300);
            require(db.number("SELECT count(*) FROM folder_state WHERE key='open'") == 1
                && db.number("SELECT count(*) FROM folder_state WHERE key='closed'") == 0, "closed rows must be preferred");
            require(db.number("SELECT count(*) FROM search_history WHERE key='closed'") == 0,
                "quota left orphan search history");
            state(store, "open", 105000000, 400);
            require(db.number("SELECT count(*) FROM folder_state WHERE key='open'") == 0 && IsWindow(window), "quota priority over open state");
            RemovePropW(window, L"open"); DestroyWindow(window);
            std::cout << "PASS: native window detection, closed-folder preference, eviction without closing an open window.\n";
#endif
            FolderStateStore second(path.c_str()); require(second.ready(), "second connection");
            FolderState stale = read(second, "wal");
            successful(store, "wal", 999);
            require(second.save("wal", stale, FolderState::Icons), "second connection stale save");
            require(read(store, "wal").lastSuccessAt == 999, "second connection overwrote newer success");
            std::cout << "PASS: stale metadata from another connection is preserved.\n";
        }
        std::remove(path.c_str());
        std::remove((path+"-wal").c_str()); std::remove((path+"-shm").c_str());
        { Database old(path.c_str()); old.sql("PRAGMA user_version=1"); }
        { FolderStateStore store(path.c_str()); require(!store.ready(), "old schema migrated/accepted unexpectedly"); }
        std::remove(path.c_str());
        std::cout << "PASS: old schema rejected; no automatic migration.\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
