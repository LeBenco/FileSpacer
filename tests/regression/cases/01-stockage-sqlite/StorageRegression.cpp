#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <chrono>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif

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
static int rawInteger(const std::string &path, const char *sql) {
    sqlite3 *db = nullptr; sqlite3_stmt *row = nullptr;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "scalar open");
    const bool ready = sqlite3_prepare_v2(db,sql,-1,&row,nullptr) == SQLITE_OK
        && sqlite3_step(row) == SQLITE_ROW;
    const int value = ready ? sqlite3_column_int(row,0) : 0;
    sqlite3_finalize(row); sqlite3_close(db);
    require(ready,"scalar query"); return value;
}
static std::string fileBytes(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "read fixture bytes");
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
// A small history spans one interior page and several table leaves. Damage a
// middle leaf of that tree, so recovery must use its descending scan too.
static int damageHistoryPage(const std::string &path, const char *table) {
    sqlite3 *db = nullptr;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "open history tree");
    sqlite3_stmt *statement = nullptr;
    require(sqlite3_prepare_v2(db, "SELECT rootpage FROM sqlite_schema WHERE name=?1", -1, &statement, nullptr) == SQLITE_OK
        && sqlite3_bind_text(statement,1,table,-1,SQLITE_STATIC) == SQLITE_OK
        && sqlite3_step(statement) == SQLITE_ROW, "locate history root");
    const int root = sqlite3_column_int(statement,0);
    sqlite3_finalize(statement);
    sqlite3_close(db);
    const std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path.c_str(),"r+b"), std::fclose);
    if (!file) throw std::runtime_error("open history page");
    unsigned char page[4096];
    require(std::fseek(file.get(),static_cast<long>(root-1)*4096,SEEK_SET) == 0
        && std::fread(page,1,sizeof(page),file.get()) == sizeof(page) && page[0] == 5,
        "history tree must have an interior root");
    const unsigned cells = (static_cast<unsigned>(page[3])<<8u)|page[4];
    require(cells >= 2 && cells < 100, "history needs middle and terminal leaves");
    const unsigned slot = 12 + 2*(cells/2);
    const unsigned cell = (static_cast<unsigned>(page[slot])<<8u)|page[slot+1];
    require(cell <= sizeof(page)-4, "history cell outside root");
    const unsigned child = (static_cast<unsigned>(page[cell])<<24u)|(static_cast<unsigned>(page[cell+1])<<16u)
        |(static_cast<unsigned>(page[cell+2])<<8u)|page[cell+3];
    require(child > 0 && std::fseek(file.get(),static_cast<long>(child-1)*4096,SEEK_SET) == 0
        && std::fread(page,1,sizeof(page),file.get()) == sizeof(page) && page[0] == 13,
        "selected history child must be a table leaf");
    const int lost = static_cast<int>((static_cast<unsigned>(page[3])<<8u)|page[4]);
    require(lost > 0, "history leaf must contain rows");
    std::fill_n(page,sizeof(page),static_cast<unsigned char>(0));
    require(std::fseek(file.get(),static_cast<long>(child-1)*4096,SEEK_SET) == 0
        && std::fwrite(page,1,sizeof(page),file.get()) == sizeof(page), "damage history leaf");
    return lost;
}
int main(int argc, char **argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--crash-wal") {
            FolderStateStore store(argv[2]); require(store.ready(), store.error().c_str());
            require(store.save("crash-committed",fixture(),FolderState::All),"commit before abrupt exit");
            std::_Exit(0); // OS closes handles; SQLite gets no destructor/checkpoint.
        }
        if (argc == 3 && std::string(argv[1]) == "--verify-wal") {
            FolderStateStore store(argv[2]); FolderState state;
            require(store.ready() && store.read("crash-committed",&state) && state.x==fixture().x
                && state.icons==fixture().icons,"native WAL recovery after abrupt exit lost committed data");
            std::cout << "PASS: native WAL recovery after abrupt exit, without resetting or deleting journals.\n";
            return 0;
        }
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
            require(store.errorCode()==SQLITE_CONSTRAINT_TRIGGER && store.error()=="forced write failure",
                "rollback erased the original SQLite error or its extended code");
            require(!store.canResetDamagedDatabase(),"ordinary write error offered database reset");
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
            require(store.saveSearchHistory("renamed",{"history"},20),"reset history fixture");
            ClosedWindow closed;closed.key="renamed";closed.parsingName="renamed";closed.displayName="renamed";
            require(store.rememberClosedWindow(closed),"reset closed-window fixture");
            require(store.clearAll(),"reset all");
            require(store.read("renamed",&s) && !s.present,"reset left data");
            std::vector<std::string> history;
            require(store.readSearchHistory("renamed",20,&history) && history.empty()
                && store.lastClosedWindow(&closed) && !closed.sequence,"reset retained history");
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
            auto weighted = enabled;
            weighted.view.present |= FolderViewSettings::ColumnWeights;
            weighted.view.columnWeights = weighted.view.columns; weighted.view.columnWeights[0].width = 77;
            require(store.save("column-weights",weighted,FolderState::View),"save column weights");
            auto resized = weighted.view; resized.columnWeights[0].width = 78;
            const auto kept = weighted.view.mergeChanges(baseline,current);
            require((kept.present & FolderViewSettings::ColumnWeights) && kept.columnWeights.size()==1
                && kept.columnWeights[0].width==77,"unobserved column weights were erased");
            require(weighted.view.mergeChanges(baseline,resized).columnWeights[0].width==78,"supplied column weights were not merged");
        }
        {
            FolderStateStore store(path.c_str()); FolderState s;
            require(store.ready() && store.read("folder",&s) && s.present==FolderState::All && s.maximized,"reopen lost data");
            require(!(s.view.present & FolderViewSettings::ColumnFill) && !s.view.fillColumns,"absent column option became present");
            require(store.read("column-fill-on",&s) && (s.view.present & FolderViewSettings::ColumnFill)
                && s.view.fillColumns && s.x==44,"enabled option lost after geometry update or reopen");
            require(store.read("column-fill-off",&s) && (s.view.present & FolderViewSettings::ColumnFill)
                && !s.view.fillColumns,"disabled option lost after reopen");
            require(store.read("column-fill-locked",&s) && s.locked && s.view.fillColumns,"locked saved option changed");
            require(!(s.view.present & FolderViewSettings::ColumnWeights) && s.view.columnWeights.empty(),"absent column weights became present");
            require(store.read("column-weights",&s) && (s.view.present & FolderViewSettings::ColumnWeights) && s.view.fillColumns
                && s.view.columnWeights.size()==1 && s.view.columnWeights[0].key==s.view.columns[0].key
                && s.view.columnWeights[0].width==77 && s.view.columns[0].width==220,"column weights lost after reopen");
            rawSQL(path,"UPDATE folder_state SET view=CAST(substr(view,1,length(view)-8)||x'02000000'||substr(view,length(view)-3) AS BLOB) WHERE key='column-fill-off'");
            require(!store.read("column-fill-off",&s),"invalid column-option boolean accepted");
            require(store.remove("column-fill-on") && store.remove("column-fill-off") && store.remove("column-fill-locked")
                && store.remove("column-weights"),"column option cleanup");
            std::cout << "PASS: column option and weight encoding, per-folder choices, reopen, partial writes, lock and provisional merge.\n";
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
        rawSQL(path,"INSERT INTO search_history(key,query) VALUES('folder','reset-history');"
            "INSERT INTO closed_windows(key,parsing_name,display_name) VALUES('folder','folder','folder');PRAGMA user_version=99");
        {
            FolderStateStore store(path.c_str()); require(!store.ready(),"future schema accepted");
            require(store.error().find("99")!=std::string::npos && store.error().find(std::to_string(FOLDER_STATE_FORMAT_VERSION))!=std::string::npos
                && store.errorCode()==0,"newer schema diagnosis was lost");
            // A database of a later version is intact: it is never offered for a reset.
            require(store.createdByNewerVersion() && !store.canResetDamagedDatabase(),"newer schema offered for reset");
#ifdef _WIN32
            require(!store.resetDamagedDatabase(path.c_str(),L"unused"),"newer schema was reset");
#endif
        }
        rawSQL(path,"PRAGMA user_version=1");
        {
            FolderStateStore store(path.c_str()); require(!store.ready(),"unknown old schema accepted");
            require(!store.createdByNewerVersion() && store.canResetDamagedDatabase(),"unsupported schema diagnosis was lost");
#ifdef _WIN32
            const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);
            require(count>0,"reset path length");
            std::wstring wide(static_cast<size_t>(count),L'\0');
            require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,&wide[0],count)!=0,"reset path");
            wide.pop_back();
            const std::wstring backup=wide+L".schema-backup";
            require(store.resetDamagedDatabase(path.c_str(),backup.c_str()),store.error().c_str());
            FolderStateStore fresh(path.c_str());FolderState state;ClosedWindow closed;std::vector<std::string> history;
            require(fresh.ready() && fresh.read("folder",&state) && !state.present
                && fresh.readSearchHistory("folder",20,&history) && history.empty()
                && fresh.lastClosedWindow(&closed) && !closed.sequence,
                "deletion/recreation differs from clearAll for saved layouts and histories");
            std::cout << "PASS: deletion/recreation and clearAll both remove saved layouts, search and closed-window histories.\n";
            const size_t slash=path.find_last_of("\\/");
            const std::string saved=path+".schema-backup\\"+path.substr(slash==std::string::npos?0:slash+1);
            const auto backupBytes=fileBytes(saved);
            require(fresh.recoverFrom(saved.c_str())==FolderStateStore::Recovery::None
                && fresh.read("folder",&state) && !state.present
                && fresh.readSearchHistory("folder",20,&history) && history.empty()
                && fresh.lastClosedWindow(&closed) && !closed.sequence
                && fileBytes(saved)==backupBytes,"old-format backup was imported or modified");
            std::cout << "PASS: confirmed reset keeps the old-format backup without importing its rows.\n";
#endif
        }
        erase(path);
        {
            FolderStateStore store(path.c_str());
            require(store.ready() && store.save("older",fixture(),FolderState::All),"old-format fixture");
        }
        rawSQL(path,"PRAGMA journal_mode=DELETE;PRAGMA user_version=6");
        {
            const auto before=fileBytes(path);
            FolderStateStore store(path.c_str());
            require(!store.ready() && store.canResetDamagedDatabase() && !store.createdByNewerVersion(),
                "previous format accepted or not diagnosed");
            require(fileBytes(path)==before,"opening an older format changed its bytes");
        }
        rawSQL(path,"PRAGMA user_version=0");
        {
            const auto before=fileBytes(path);
            FolderStateStore store(path.c_str());
            require(!store.ready() && store.canResetDamagedDatabase(),"unversioned non-empty database accepted");
            require(fileBytes(path)==before,"opening an unversioned database changed its bytes");
        }
        erase(path);
        {
            const std::string damaged=path+".current-recovery";erase(damaged);
            {
                FolderStateStore source(damaged.c_str());
                ClosedWindow closed;closed.key="folder";closed.parsingName="folder";closed.displayName="folder";
                require(source.ready() && source.save("folder",fixture(),FolderState::All)
                    && source.saveSearchHistory("folder",{"reset-history"},20)
                    && source.rememberClosedWindow(closed),"current-format recovery fixture");
            }
            rawSQL(damaged,"UPDATE folder_state SET view=x'00000000' WHERE key='folder'");
            FolderStateStore target(path.c_str());FolderState state;ClosedWindow closed;std::vector<std::string> history;
            require(target.ready() && target.recoverFrom(damaged.c_str())==FolderStateStore::Recovery::Partial,
                "recovery verdict with one undecodable current-format view");
            const FolderState expected=fixture();
            require(target.read("folder",&state) && (state.present&FolderState::Position) && state.x==expected.x
                && state.y==expected.y && (state.present&FolderState::Icons) && state.icons==expected.icons
                && !(state.present&FolderState::View),"recovered layout differs, or undecodable settings were kept");
            require(target.readSearchHistory("folder",20,&history) && history.size()==1 && history[0]=="reset-history"
                && target.lastClosedWindow(&closed) && closed.key=="folder","recovery lost search or closed-window history");
            erase(damaged);
            std::cout << "PASS: current-format recovery keeps readable data and leaves out undecodable settings.\n";
        }
        erase(path);
        // All published format-7 encodings migrate together; unrelated state is opaque.
        {
            FolderState expected=fixture(); expected.view.iconSize=72;
            ClosedWindow closed; closed.key="v3";closed.parsingName="folder";closed.displayName="saved";
            closed.viewState={1,2,3,8,9};
            {
                FolderStateStore source(path.c_str());
                require(source.ready() && source.save("a-v1",expected,FolderState::All),"v1 migration fixture");
                expected.view.present|=FolderViewSettings::ColumnFill;expected.view.fillColumns=true;
                require(source.save("v2",expected,FolderState::All),"v2 migration fixture");
                expected.view.present|=FolderViewSettings::ColumnWeights;
                expected.view.columnWeights=expected.view.columns;
                require(source.setLocked("v3",expected,FolderState::All,true)
                    && source.save("v8",expected,FolderState::All)
                    && source.save("null",expected,FolderState::Position)
                    && source.saveSearchHistory("v3",{"keep-history"},20)
                    && source.saveViewScroll("v3",{3,7,8})
                    && source.rememberClosedWindow(closed),"mixed migration fixture");
            }
            rawSQL(path,"PRAGMA journal_mode=DELETE;"
                "UPDATE folder_state SET view=CAST(x'01000000'||substr(view,5,length(view)-12) AS BLOB) WHERE key='a-v1';"
                "UPDATE folder_state SET view=CAST(x'02000000'||substr(view,5,length(view)-8) AS BLOB) WHERE key='v2';"
                "UPDATE folder_state SET view=CAST(x'03000000'||substr(view,5) AS BLOB) WHERE key='v3';"
                "PRAGMA user_version=7");
            {
                FolderStateStore store(path.c_str());FolderState state;
                require(store.ready(),store.error().c_str());
                for(const char *key:{"a-v1","v2","v3","v8"}) {
                    require(store.read(key,&state) && state.x==expected.x && state.y==expected.y
                        && state.width==expected.width && state.height==expected.height && state.maximized
                        && state.icons==expected.icons && state.view.iconSize==72 && state.view.columns.size()==1 && state.view.columns[0].key==expected.view.columns[0].key
                        && state.view.columns[0].width==expected.view.columns[0].width,
                        "migration changed geometry, icons, raw icon size or columns");
                    require(state.view.fillColumns==(std::string(key)!="a-v1")
                        && state.view.columnWeights.size()==(std::string(key)=="v3" || std::string(key)=="v8" ? 1u : 0u),
                        "legacy fill or weights decoded incorrectly");
                }
                std::vector<std::string> history;std::vector<uint8_t> scroll;ClosedWindow restored;
                require(store.read("v3",&state) && state.locked
                    && store.readSearchHistory("v3",20,&history) && history==std::vector<std::string>{"keep-history"}
                    && store.readViewScroll("v3",&scroll) && scroll==std::vector<uint8_t>({3,7,8})
                    && store.lastClosedWindow(&restored) && restored.key==closed.key && restored.viewState==closed.viewState
                    && store.read("null",&state) && !(state.present&FolderState::View),
                    "migration lost lock, histories, viewport, opaque closed-window state or NULL view");
            }
            require(rawInteger(path,"PRAGMA user_version")==8
                && rawInteger(path,"SELECT count(*) FROM folder_state WHERE hex(substr(view,1,4))='08000000'")==4,
                "migration did not write the independent view encoding and schema");
            rawSQL(path,"PRAGMA journal_mode=DELETE");
            { FolderStateStore again(path.c_str());require(again.ready(),"reopen migrated database"); }
            // WAL activation may change the header; compare the stable view bytes instead.
            require(rawInteger(path,"SELECT count(*) FROM folder_state WHERE hex(substr(view,1,4))='08000000'")==4,
                "migration was not idempotent");
            erase(path);
            {
                FolderStateStore source(path.c_str());
                require(source.save("a-valid",fixture(),FolderState::All)
                    && source.save("z-bad",fixture(),FolderState::All)
                    && source.saveSearchHistory("z-bad",{"keep-history"},20),"failed migration fixture");
            }
            rawSQL(path,"PRAGMA journal_mode=DELETE;"
                "UPDATE folder_state SET view=CAST(x'01000000'||substr(view,5,length(view)-12) AS BLOB) WHERE key='a-valid';"
                "UPDATE folder_state SET view=x'01000000' WHERE key='z-bad';PRAGMA user_version=7");
            const auto before=fileBytes(path);
            {
                FolderStateStore rejected(path.c_str());
                require(!rejected.ready() && rejected.canResetDamagedDatabase()
                    && rejected.error().find("format 7")!=std::string::npos,"invalid legacy view accepted");
            }
            require(fileBytes(path)==before && rawInteger(path,"PRAGMA user_version")==7
                && rawInteger(path,"SELECT count(*) FROM folder_state WHERE hex(substr(view,1,4))='01000000'")==2,
                "failed migration partially changed the database");
            const std::string targetPath=path+".legacy-recovery";erase(targetPath);
            {
                FolderStateStore target(targetPath.c_str());FolderState state;std::vector<std::string> history;
                require(target.ready() && target.recoverFrom(path.c_str())==FolderStateStore::Recovery::Partial
                    && target.read("a-valid",&state) && (state.present&FolderState::View)
                    && target.read("z-bad",&state) && !(state.present&FolderState::View) && state.icons==fixture().icons
                    && target.readSearchHistory("z-bad",20,&history) && history==std::vector<std::string>{"keep-history"}
                    && fileBytes(path)==before,"legacy partial recovery lost data or modified its source");
            }
            erase(targetPath);erase(path);
            std::cout << "PASS: format-7 migration preserves state, is atomic and supports read-only partial recovery.\n";
        }
        // A damaged page stops a scan: the rows on both sides of it are kept.
        {
            const std::string damaged=path+".damaged";erase(damaged);
            const int rows=24;
            FolderState filler=fixture();filler.icons.assign(600,0x5a);
            {
                FolderStateStore source(damaged.c_str());require(source.ready(),source.error().c_str());
                for(int index=0;index<rows;++index) {
                    char key[32];std::snprintf(key,sizeof(key),"row-%04d",index);
                    require(source.save(key,filler,FolderState::All),"create recovery fixture");
                }
            }
            rawSQL(damaged,"PRAGMA wal_checkpoint(TRUNCATE)");
            // The damage is aimed at a populated leaf of the folder_state tree, the
            // only populated tree here: SQLite reads a zeroed overflow page back
            // without any error, so damage placed blindly may stop no scan at all.
            FILE *file=std::fopen(damaged.c_str(),"r+b");require(file!=nullptr,"open recovery fixture");
            std::vector<long> leaves;std::vector<int> cells;unsigned char header[8];
            for(long page=0;std::fseek(file,page*4096,SEEK_SET)==0 && std::fread(header,1,sizeof(header),file)==sizeof(header);++page) {
                const int count=header[3]<<8|header[4];
                if(header[0]==0x0a && count>0) { leaves.push_back(page);cells.push_back(count); }
            }
            require(leaves.size()>=3,"recovery fixture has too few leaf pages to damage one between two others");
            const size_t middle=leaves.size()/2;
            const std::vector<char> zeros(4096,0);
            require(std::fseek(file,leaves[middle]*4096,SEEK_SET)==0
                && std::fwrite(zeros.data(),1,zeros.size(),file)==zeros.size(),"damage recovery fixture");
            std::fclose(file);
            FolderStateStore rebuilt(path.c_str());require(rebuilt.ready(),rebuilt.error().c_str());
            require(rebuilt.recoverFrom(damaged.c_str())==FolderStateStore::Recovery::Partial,"recovery verdict on a damaged page");
            int recovered=0;FolderState s;
            for(int index=0;index<rows;++index) {
                char key[32];std::snprintf(key,sizeof(key),"row-%04d",index);
                require(rebuilt.read(key,&s),"rebuilt database cannot be read");
                if(s.present) { ++recovered; require(s.icons==filler.icons,"recovered row was altered"); }
            }
            require(recovered==rows-cells[middle],"a damaged page lost other rows than its own");
            require(rebuilt.save("after",fixture(),FolderState::All),"rebuilt database does not store new states");
            std::cout << "PASS: recovery around a damaged page keeps the " << recovered << " rows of " << rows
                << " stored elsewhere, unaltered, in a usable database.\n";
            erase(damaged);
        }
        erase(path);
        {
            const std::string sourcePath = path+".recovery-source";
            erase(sourcePath);
            {
                FolderStateStore source(sourcePath.c_str());
                require(source.ready() && source.save("recover",fixture(),FolderState::All), "prepare recovery source");
            }
            FolderStateStore rebuilt(path.c_str()); FolderState state;
            require(rebuilt.ready(),rebuilt.error().c_str());
            require(rebuilt.recoverFrom((sourcePath+".missing").c_str()) == FolderStateStore::Recovery::Failed,
                "source opening failure reported as an empty recovery");
            rawSQL(path,"CREATE TRIGGER fail_recovery BEFORE INSERT ON folder_state "
                "BEGIN SELECT RAISE(ABORT,'controlled recovery write failure'); END;");
            require(rebuilt.recoverFrom(sourcePath.c_str()) == FolderStateStore::Recovery::Failed
                && (static_cast<unsigned>(rebuilt.errorCode())&255u) == SQLITE_CONSTRAINT, "destination error reported as a successful recovery");
            require(rebuilt.read("recover",&state) && !state.present, "failed recovery committed a partial row");
            rawSQL(path,"DROP TRIGGER fail_recovery;");
            require(rebuilt.recoverFrom(sourcePath.c_str()) == FolderStateStore::Recovery::Total
                && rebuilt.read("recover",&state) && state.icons == fixture().icons && state.x == fixture().x,
                "recovery after the write error did not restore the exact state");
            erase(sourcePath);
            std::cout << "PASS: recovery distinguishes technical failures, rolls back, and can recover totally afterwards.\n";
        }
        erase(path);
        for (const char *table : {"closed_windows", "search_history"}) {
            const std::string damaged = path+".history"; erase(damaged);
            {
                FolderStateStore source(damaged.c_str());
                require(source.ready() && source.save("folder",fixture(),FolderState::Position), "prepare history owner");
            }
            const int rows = 18;
            std::vector<std::string> expected;
            for (int index=0; index<rows; ++index) {
                const std::string name = "row-"+std::to_string(index+100);
                expected.push_back(name+std::string(850,'x'));
                const bool closed = std::string(table) == "closed_windows";
                std::string insert = "INSERT INTO ";
                insert += table;
                insert += closed ? "(sequence,key,parsing_name,display_name) VALUES(" : "(sequence,key,query) VALUES(";
                insert += std::to_string(1000+index);
                insert += ",'";
                insert += closed ? name : "folder";
                insert += "','";
                insert += expected.back();
                if (closed) { insert += "','"; insert += name; }
                insert += "')";
                rawSQL(damaged,insert.c_str());
            }
            const int lost = damageHistoryPage(damaged,table);
            {
                FolderStateStore rebuilt(path.c_str());
                require(rebuilt.ready() && rebuilt.recoverFrom(damaged.c_str()) == FolderStateStore::Recovery::Partial,
                    "history recovery must scan from both ends");
                std::vector<std::string> recovered;
                if (std::string(table) == "search_history") {
                    require(rebuilt.readSearchHistory("folder",20,&recovered), "read recovered search history");
                } else {
                    ClosedWindow closed;
                    for (;;) {
                        require(rebuilt.takeClosedWindow(&closed), "read recovered closed-window history");
                        if (!closed.sequence) break;
                        require(closed.sequence >= 1000 && closed.sequence < 1000+rows
                            && closed.parsingName == expected[static_cast<size_t>(closed.sequence-1000)],
                            "closed-window recovery changed a sequence or identity");
                        recovered.push_back(closed.parsingName);
                    }
                }
                require(recovered.size() == static_cast<size_t>(rows-lost) && recovered.front() == expected.back()
                    && recovered.back() == expected.front(), "history recovery lost rows outside the damaged leaf");
                for (size_t index=0; index<recovered.size(); ++index) {
                    require(std::find(expected.begin(),expected.end(),recovered[index]) != expected.end(), "history identity changed");
                    if (index) require(recovered[index-1] > recovered[index], "recovered history recency is reversed");
                }
            }
            erase(damaged); erase(path);
        }
        std::cout << "PASS: both histories keep their exact identities and recency across a damaged middle page.\n";
        {
            FILE *file=std::fopen(path.c_str(),"wb"); require(file!=nullptr,"create damaged fixture");
            std::fputs("not a database",file);std::fclose(file);
            FolderStateStore store(path.c_str());require(!store.ready(),"corrupt database accepted");
            require(store.errorCode()==SQLITE_NOTADB && store.canResetDamagedDatabase(),"corrupt database diagnosis missing");
#ifdef _WIN32
            const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);
            require(count>0,"fixture path length");
            std::wstring wide(static_cast<size_t>(count),L'\0');
            require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,&wide[0],count)!=0,"fixture path");
            wide.pop_back();
            const std::wstring backup=wide+L".backup";
            const std::wstring link=wide+L".link";
            if(CreateSymbolicLinkW(link.c_str(),wide.c_str(),SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
                FolderStateStore linkedStore((path+".link").c_str());
                require(linkedStore.canResetDamagedDatabase(),"linked corrupt fixture did not fail opening");
                require(!linkedStore.resetDamagedDatabase((path+".link").c_str(),backup.c_str())
                    && GetFileAttributesW(wide.c_str())!=INVALID_FILE_ATTRIBUTES,
                    "reset followed a symbolic link and removed its target");
                require(DeleteFileW(link.c_str())!=0,"remove fixture link");
                std::cout << "PASS: symbolic database links are refused without deleting their target.\n";
            } else {
                std::cout << "SKIP: symbolic-link fixture unavailable (Windows error " << GetLastError() << ").\n";
            }
            const HANDLE busy=CreateFileW(wide.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            require(busy!=INVALID_HANDLE_VALUE,"open controlled busy database");
            const bool refused=!store.resetDamagedDatabase(path.c_str(),backup.c_str());
            CloseHandle(busy);
            require(refused && GetFileAttributesW(backup.c_str())==INVALID_FILE_ATTRIBUTES,
                "reset bypassed a live user or created a backup while files were in use");
            require(CreateDirectoryW(backup.c_str(),nullptr)!=0,"reserve backup failure fixture");
            {
                FolderStateStore noBackup(path.c_str());
                require(!noBackup.resetDamagedDatabase(path.c_str(),backup.c_str())
                    && GetFileAttributesW(wide.c_str())!=INVALID_FILE_ATTRIBUTES,
                    "failed backup removed the original database");
            }
            require(RemoveDirectoryW(backup.c_str())!=0,"remove empty backup failure fixture");
            // Retry from a fresh diagnosis, as the real dialog does after confirmation.
            FolderStateStore retry(path.c_str());
            const wchar_t *suffixes[]={L"-wal",L"-shm",L"-journal"};
            for(const auto suffix:suffixes) {
                FILE *sidecar=nullptr;
                require(_wfopen_s(&sidecar,(wide+suffix).c_str(),L"wb")==0 && sidecar,"sidecar fixture");
                std::fputs("sidecar backup fixture",sidecar);std::fclose(sidecar);
            }
            require(retry.resetDamagedDatabase(path.c_str(),backup.c_str()),retry.error().c_str());
            require(GetFileAttributesW(wide.c_str())==INVALID_FILE_ATTRIBUTES,"reset left the original database");
            const size_t slash=wide.find_last_of(L"\\/");
            const std::wstring backupFile=backup+L"\\"+wide.substr(slash==std::wstring::npos?0:slash+1);
            FILE *saved=nullptr;
            require(_wfopen_s(&saved,backupFile.c_str(),L"rb")==0 && saved,"backup not present");
            char contents[32]={}; const size_t bytes=std::fread(contents,1,sizeof(contents),saved); std::fclose(saved);
            require(std::string(contents,bytes)=="not a database","backup changed the damaged original");
            for(const auto suffix:suffixes) {
                require(GetFileAttributesW((wide+suffix).c_str())==INVALID_FILE_ATTRIBUTES,"reset left a sidecar");
                require(_wfopen_s(&saved,(backupFile+suffix).c_str(),L"rb")==0 && saved,"sidecar backup missing");
                const size_t size=std::fread(contents,1,sizeof(contents),saved);std::fclose(saved);
                require(std::string(contents,size)=="sidecar backup fixture","sidecar backup changed");
            }
            FolderStateStore recreated(path.c_str());FolderState state;
            require(recreated.ready() && recreated.read("folder",&state) && !state.present,
                "a fresh database was not recreated after reset");
            require(!recreated.canResetDamagedDatabase(),"healthy database offered destructive recovery");
            std::cout << "PASS: reset refuses files in use, preserves original bytes in backup, and recreates an empty database.\n";
#endif
        }
        erase(path);
        std::cout << "PASS: damaged database, malformed view data, future schema are rejected.\n";
        return 0;
    } catch(const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
