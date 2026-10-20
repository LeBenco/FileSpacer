#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cassert>
#include <cstdio>
#include <string>
using namespace filespacer;
int main(int argc, char **argv) {
    assert(argc == 2);
    FolderStateStore store(argv[1]);
    assert(store.ready());
    FolderState state; state.x = 31; state.y = 41; state.lastSuccessAt = 100;
    const std::string removed = "deleted'folder";
    assert(store.save(removed, state, FolderState::Position));
    state.x = 99;
    assert(store.save("other-folder", state, FolderState::Position));
    assert(store.remove(removed));
    FolderState read;
    assert(store.read(removed, &read) && read.present == 0);
    assert(store.read("other-folder", &read) && read.x == 99);
    assert(store.remove(removed)); // Repeated Shell removal notifications are harmless.
    sqlite3 *db = nullptr;
    assert(sqlite3_open(argv[1], &db) == SQLITE_OK);
    assert(sqlite3_exec(db, "CREATE TRIGGER reject_removal BEFORE DELETE ON folder_state "
        "BEGIN SELECT RAISE(ABORT,'simulated write failure'); END", nullptr, nullptr, nullptr) == SQLITE_OK);
    assert(!store.remove("other-folder"));
    assert(store.read("other-folder", &read) && read.x == 99);
    sqlite3_close(db);
    puts("PASS: exact row removal, repeated notification, unrelated state retained, failed deletion reported.");
}
