#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <chrono>

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
            changed.icons.clear();
            require(store.save("empty",changed,FolderState::Icons),"empty blob");
            require(store.read("empty",&s) && s.present==FolderState::Icons && s.icons.empty(),"empty blob is not NULL");
            const std::string escaped="folder'; DELETE FROM folder_state; --";
            require(store.save(escaped,original,FolderState::All),"bound key");
            require(store.read("folder",&s) && s.present==FolderState::All,"SQL injection");
            require(store.move("folder","renamed"),"move");
            require(store.read("folder",&s) && !s.present,"old key retained");
            require(store.read("renamed",&s) && s.present==FolderState::All && s.x==444,"new key lost state");
            require(store.move("renamed","renamed"),"same key");
            require(store.move("missing","empty"),"missing source");
            require(store.read("empty",&s) && s.present==FolderState::Icons,"missing source erased existing target");
            rawSQL(path,"CREATE TRIGGER fail_lock BEFORE INSERT ON folder_state WHEN new.key='locked' BEGIN SELECT RAISE(ABORT,'forced lock failure'); END");
            require(!store.setLocked("locked",original,FolderState::All,true),"failed lock was accepted");
            require(store.read("locked",&s) && !s.locked && !s.present,"failed lock leaked a snapshot");
            rawSQL(path,"DROP TRIGGER fail_lock");
            require(store.setLocked("locked", original, FolderState::All, true),"lock snapshot");
            FolderState temporary = original; temporary.x=123; temporary.width=345;
            temporary.maximized=false; temporary.view.mode=1; temporary.icons={1,2,3};
            require(store.save("locked",temporary,FolderState::All),"locked save");
            require(store.read("locked",&s) && s.locked && s.x==original.x
                && s.width==original.width && s.maximized && s.view.mode==original.view.mode
                && s.icons==temporary.icons,"lock must protect geometry/view but not icons");
            require(store.move("locked","locked-renamed"),"locked rekey");
            require(store.read("locked-renamed",&s) && s.locked,"rekey lost lock");
            require(store.setLocked("locked-renamed",temporary,FolderState::All,false),"unlock snapshot");
            require(store.read("locked-renamed",&s) && !s.locked && s.x==123
                && s.width==345 && !s.maximized && s.view.mode==1,"unlock must capture visible state");
            require(store.setLocked("locked-renamed",original,FolderState::All,true),"relock");
            rawSQL(path,"CREATE TRIGGER fail_unlock BEFORE UPDATE ON folder_state WHEN old.locked=1 BEGIN SELECT RAISE(ABORT,'forced unlock failure'); END");
            require(!store.setLocked("locked-renamed",temporary,FolderState::All,false),"failed unlock accepted");
            require(!store.clearLocks(),"failed global clearing accepted");
            require(store.read("locked-renamed",&s) && s.locked && s.x==original.x,"failure lost the reference or lock");
            rawSQL(path,"DROP TRIGGER fail_unlock");
            require(store.clearLocks(),"global unlock");
            temporary.locked = true; // An old in-memory lock must never be written back.
            require(store.save("locked-renamed",temporary,FolderState::All),"stale snapshot save");
            require(store.read("locked-renamed",&s) && !s.locked && s.x==123,"stale save revived lock");
            require(store.clearAll(),"reset all");
            require(store.read("renamed",&s) && !s.present,"reset left data");
            require(store.save("folder",original,FolderState::All),"after reset");
            std::cout << "PASS: full/partial saves, signed coordinates, reset, rekey, bound keys, empty blobs.\n";
        }
        {
            FolderStateStore store(path.c_str()); require(store.ready(), "column option store");
            auto enabled = fixture();
            enabled.view.present |= FolderViewSettings::ColumnFill;
            enabled.view.fillColumns = true;
            require(store.save("column-fill-on",enabled,FolderState::View),"save enabled column option");
            auto disabled = enabled; disabled.view.fillColumns = false;
            require(store.save("column-fill-off",disabled,FolderState::View),"save independent disabled column option");
            require(store.setLocked("column-fill-locked",enabled,FolderState::View,true),"lock column option");
            require(store.save("column-fill-locked",disabled,FolderState::View),"locked option save");
            FolderState geometry; geometry.x=44; geometry.y=55;
            require(store.save("column-fill-on",geometry,FolderState::Position),"geometry with stored column option");
            FolderViewSettings baseline, current;
            const auto unknown = enabled.view.mergeChanges(baseline,current);
            require(unknown.fillColumns && (unknown.present & FolderViewSettings::ColumnFill),"unobserved option erased saved choice");
            baseline = enabled.view; current = disabled.view;
            require(!enabled.view.mergeChanges(baseline,current).fillColumns,"explicit column option change was not merged");
        }
        {
            FolderStateStore store(path.c_str()); FolderState s;
            require(store.ready() && store.read("folder",&s) && s.present==FolderState::All && s.maximized,"reopen lost data");
            require(!(s.view.present & FolderViewSettings::ColumnFill) && !s.view.fillColumns,"legacy view encoding gained a column option");
            require(store.read("column-fill-on",&s) && (s.view.present & FolderViewSettings::ColumnFill)
                && s.view.fillColumns && s.x==44,"enabled option lost after geometry update or reopen");
            require(store.read("column-fill-off",&s) && (s.view.present & FolderViewSettings::ColumnFill)
                && !s.view.fillColumns,"disabled option lost after reopen");
            require(store.read("column-fill-locked",&s) && s.locked && s.view.fillColumns,"locked saved option changed");
            rawSQL(path,"UPDATE folder_state SET view=CAST(substr(view,1,length(view)-4)||x'02000000' AS BLOB) WHERE key='column-fill-off'");
            require(!store.read("column-fill-off",&s),"invalid column-option boolean accepted");
            require(store.remove("column-fill-on") && store.remove("column-fill-off") && store.remove("column-fill-locked"),"column option cleanup");
            std::cout << "PASS: column option versioning, per-folder choices, reopen, partial writes, lock and provisional merge.\n";
            std::cout << "PASS: data survives closing and reopening the database.\n";
            std::vector<uint8_t> scroll;
            std::vector<uint8_t> expected(48,0);expected.front()=1;expected.back()=6;
            require(store.readViewScroll("folder",&scroll) && scroll.empty(),"new viewport must be absent");
            require(store.saveViewScroll("missing",expected) && store.read("missing",&s) && !s.present,"viewport resurrected a missing folder");
            auto began=std::chrono::steady_clock::now();
            require(store.saveViewScroll("folder",expected),"viewport save failed");
            const double writeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            began=std::chrono::steady_clock::now();
            require(store.readViewScroll("folder",&scroll) && scroll==expected,"viewport read differs");
            const double readMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            std::cout << "VIEWPORT_STORAGE bytes=48 commit_ms=" << writeMs << " read_ms=" << readMs << "\n";
            require(store.save("folder",fixture(),FolderState::All) && store.readViewScroll("folder",&scroll)
                && scroll==expected,"ordinary save overwrote viewport");
            require(store.setLocked("folder",fixture(),FolderState::All,true),"viewport lock fixture");
            auto changed=expected;changed.back()=9;
            require(store.saveViewScroll("folder",changed) && store.readViewScroll("folder",&scroll)
                && scroll==changed,"layout lock froze navigation viewport");
            rawSQL(path,"CREATE TRIGGER fail_scroll BEFORE UPDATE OF scroll ON folder_state BEGIN SELECT RAISE(ABORT,'forced viewport failure'); END");
            require(store.saveViewScroll("folder",changed),"unchanged viewport caused an update");
            require(!store.saveViewScroll("folder",expected) && store.readViewScroll("folder",&scroll)
                && scroll==changed,"failed viewport write changed stored bytes");
            rawSQL(path,"DROP TRIGGER fail_scroll");
            require(store.move("folder","viewport-moved") && store.readViewScroll("viewport-moved",&scroll)
                && scroll==changed,"rekey lost viewport");
            require(store.clearAll() && store.readViewScroll("viewport-moved",&scroll)
                && scroll.empty(),"global reset retained viewport");
            require(store.save("folder",fixture(),FolderState::All),"restore malformed-blob fixture");
            std::cout << "PASS: independent viewport persistence, locking, rekey, no resurrection, rollback and reset.\n";
        }
        rawSQL(path,"UPDATE folder_state SET view=x'00000000' WHERE key='folder'");
        {
            FolderStateStore store(path.c_str());FolderState s;
            require(!store.read("folder",&s) && !store.error().empty(),"malformed blob silently accepted");
        }
        rawSQL(path,"PRAGMA user_version=99");
        { FolderStateStore store(path.c_str()); require(!store.ready(),"future schema accepted"); }
        erase(path);
        rawSQL(path,"CREATE TABLE folder_state(key TEXT PRIMARY KEY NOT NULL,x INTEGER,y INTEGER,width INTEGER,height INTEGER,"
            "view BLOB,icons BLOB,last_success_at INTEGER NOT NULL DEFAULT 0,first_failed_at INTEGER,maximized INTEGER,locked INTEGER NOT NULL DEFAULT 0) WITHOUT ROWID;"
            "INSERT INTO folder_state(key,x,y,width,height,maximized,locked) VALUES('legacy-six',12,34,600,400,1,1);PRAGMA user_version=6;");
        {
            FolderStateStore store(path.c_str());FolderState s;std::vector<uint8_t> scroll;
            require(store.ready() && store.read("legacy-six",&s) && s.x==12 && s.y==34 && s.width==600
                && s.height==400 && s.maximized && s.locked,"schema-six migration lost its layout");
            require(store.readViewScroll("legacy-six",&scroll) && scroll.empty(),"legacy viewport was invented");
            const std::vector<uint8_t> expected={9,8,7};
            require(store.saveViewScroll("legacy-six",expected) && store.readViewScroll("legacy-six",&scroll)
                && scroll==expected,"migrated viewport cannot be stored");
            std::cout << "PASS: schema-six migration preserves existing state and enables viewport persistence.\n";
        }
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
