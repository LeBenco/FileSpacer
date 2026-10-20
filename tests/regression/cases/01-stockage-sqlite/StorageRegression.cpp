#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace filespacer;
static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
static void erase(const std::string &path) {
    std::remove(path.c_str()); std::remove((path+"-wal").c_str()); std::remove((path+"-shm").c_str());
}
static FolderState fixture() {
    FolderState s;
    s.maximized = true;
    s.x = -72000; s.y = 68000; s.width = 902; s.height = 650;
    s.view.present = 31; s.view.mode = 4; s.view.iconSize = 32; s.view.flags = 5;
    FolderViewSettings::Column column;
    column.key[0] = 123; column.key[19] = 4; column.width = 220;
    s.view.columns.push_back(column);
    FolderViewSettings::SortColumn sort;
    sort.key = column.key; sort.direction = -1; s.view.sort.push_back(sort);
    s.view.groupBy[5] = 99; s.view.groupAscending = true;
    s.icons.resize(32000);
    for (size_t i=0; i<s.icons.size(); ++i) s.icons[i] = static_cast<uint8_t>(i);
    return s;
}
static void rawSQL(const std::string &path, const char *sql) {
    sqlite3 *db = nullptr;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "raw open failed");
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    require(result == SQLITE_OK, "raw SQL failed");
}
int main(int argc, char **argv) {
    try {
        if (argc >= 4 && std::string(argv[1]) == "--writer") {
            FolderStateStore store(argv[2]); require(store.ready(), store.error().c_str());
            FolderState state = fixture();
            for (int n=0; n<100; ++n) {
                state.x=n;
                require(store.save(argv[3], state, FolderState::All), store.error().c_str());
            }
            return 0;
        }
        if (argc >= 4 && std::string(argv[1]) == "--verify-writers") {
            FolderStateStore store(argv[2]); require(store.ready(), store.error().c_str());
            const int count = std::atoi(argv[3]);
            for (int n=0;n<count;++n) {
                FolderState state;
                require(store.read("writer-"+std::to_string(n), &state), store.error().c_str());
                require(state.present==FolderState::All && state.x==99 && state.icons==fixture().icons,"concurrent writer lost data");
            }
            std::cout << "PASS: " << count << " concurrent processes, committed data complete.\n";
            return 0;
        }
        const std::string path = argc > 1 ? argv[1] : "storage-test.sqlite3";
        if (FILE *existing=std::fopen(path.c_str(),"rb")) {
            std::fclose(existing);
            throw std::runtime_error("Test database already exists; choose a new temporary path");
        }
        {
            FolderStateStore store(path.c_str()); require(store.ready(), store.error().c_str());
            FolderState s;
            require(store.read("missing", &s) && !s.present, "missing row must be empty");
            const auto original = fixture();
            require(store.save("folder", original, FolderState::All), "save full");
            require(store.read("folder", &s) && s.present==FolderState::All && s.x==-72000 && s.y==68000
                && s.width==902 && s.maximized && s.view.mode==4 && s.view.columns[0].width==220
                && s.view.sort[0].direction==-1 && s.view.groupBy[5]==99
                && s.view.groupAscending && s.icons==original.icons, "full round trip");
            FolderState changed; changed.x=444; changed.y=-555;
            require(store.save("folder",changed,FolderState::Position),"partial position");
            require(store.read("folder",&s) && s.x==444 && s.y==-555 && s.width==902
                && s.maximized && s.icons==original.icons && s.view.columns[0].key==original.view.columns[0].key,"partial write erased another part");
            rawSQL(path,"CREATE TRIGGER fail_save BEFORE UPDATE ON folder_state WHEN new.x=999 BEGIN SELECT RAISE(ABORT,'forced write failure'); END");
            FolderState rejected = original; rejected.x=999; rejected.width=123;
            require(!store.save("folder",rejected,FolderState::All),"forced save failure was ignored");
            require(store.read("folder",&s) && s.x==444 && s.width==902 && s.icons==original.icons,"failed save was partially committed");
            rawSQL(path,"DROP TRIGGER fail_save");
            require(store.clear("folder",FolderState::Size),"partial clear");
            require(store.read("folder",&s) && s.present==(FolderState::All & ~FolderState::Size) && s.icons==original.icons,"partial clear erased another part");
            changed.icons.clear();
            require(store.save("empty",changed,FolderState::Icons),"empty blob");
            require(store.read("empty",&s) && s.present==FolderState::Icons && s.icons.empty(),"empty blob is not NULL");
            const std::string escaped="folder'; DELETE FROM folder_state; --";
            require(store.save(escaped,original,FolderState::All),"bound key");
            require(store.read("folder",&s) && s.present==(FolderState::All & ~FolderState::Size),"SQL injection");
            require(store.move("folder","renamed"),"move");
            require(store.read("folder",&s) && !s.present,"old key retained");
            require(store.read("renamed",&s) && s.present==(FolderState::All & ~FolderState::Size) && s.x==444,"new key lost state");
            require(store.move("renamed","renamed"),"same key");
            require(store.move("missing","empty"),"missing source");
            require(store.read("empty",&s) && s.present==FolderState::Icons,"missing source erased existing target");
            require(store.clearAll(),"reset all");
            require(store.read("renamed",&s) && !s.present,"reset left data");
            require(store.save("folder",original,FolderState::All),"after reset");
            std::cout << "PASS: full/partial saves, signed coordinates, reset, rekey, bound keys, empty blobs.\n";
        }
        {
            FolderStateStore store(path.c_str()); FolderState s;
            require(store.ready() && store.read("folder",&s) && s.present==FolderState::All && s.maximized,"reopen lost data");
            std::cout << "PASS: data survives closing and reopening the database.\n";

        }
        rawSQL(path,"UPDATE folder_state SET view=x'00000000' WHERE key='folder'");
        {
            FolderStateStore store(path.c_str());FolderState s;
            require(!store.read("folder",&s) && !store.error().empty(),"malformed blob silently accepted");
        }
        rawSQL(path,"PRAGMA user_version=99");
        { FolderStateStore store(path.c_str()); require(!store.ready(),"future schema accepted"); }
        erase(path);
        {
            FILE *file=std::fopen(path.c_str(),"wb"); require(file!=nullptr,"create damaged fixture");
            std::fputs("not a database",file);std::fclose(file);
            FolderStateStore store(path.c_str());require(!store.ready(),"corrupt database accepted");
        }
        erase(path);
        std::cout << "PASS: damaged database, malformed view data, future schema are rejected.\n";
        return 0;
    } catch(const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
