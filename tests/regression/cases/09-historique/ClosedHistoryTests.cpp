#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace filespacer;

ClosedWindow record(const std::string &key) {
    ClosedWindow value;
    value.key = key;
    value.parsingName = "C:\\Dossiers\\" + key;
    value.displayName = "Documents été " + key;
    value.viewState = {0,1,2,0,255};
    return value;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string path = argv[1];
    std::remove(path.c_str());
    sqlite3 *db = nullptr;
    assert(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    assert(sqlite3_exec(db,
        "CREATE TABLE folder_state(key TEXT PRIMARY KEY NOT NULL,x INTEGER,y INTEGER,width INTEGER,height INTEGER,"
        "view BLOB,icons BLOB,last_success_at INTEGER NOT NULL DEFAULT 0,first_failed_at INTEGER,maximized INTEGER) WITHOUT ROWID;"
        "INSERT INTO folder_state(key,x,y,width,height,maximized) VALUES('existing',12,34,600,400,1);"
        "PRAGMA user_version=3;",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_close(db);
    {
        FolderStateStore store(path.c_str()); assert(store.ready());
        FolderState state; assert(store.read("existing",&state));
        assert(state.x==12 && state.y==34 && state.width==600 && state.maximized);
        ClosedWindow value;
        assert(store.lastClosedWindow(&value) && value.sequence==0);
        assert(store.takeClosedWindow(&value) && value.sequence==0);
        assert(store.rememberClosedWindow(record("A")));
        assert(store.rememberClosedWindow(record("B")));
        assert(store.rememberClosedWindow(record("C")));
        assert(store.lastClosedWindow(&value) && value.key=="C");
        assert(value.displayName==record("C").displayName && value.viewState==record("C").viewState);
        assert(store.takeClosedWindow(&value) && value.key=="C");
        const ClosedWindow retry = value;
        assert(store.rememberClosedWindow(record("D")));
        assert(store.returnClosedWindow(retry));
        assert(store.takeClosedWindow(&value) && value.key=="D");
        assert(store.takeClosedWindow(&value) && value.key=="C");
        assert(value.sequence==retry.sequence);
        assert(store.rememberClosedWindow(record("B")));
        assert(store.takeClosedWindow(&value) && value.key=="B");
        const ClosedWindow older = value;
        auto newer = record("B"); newer.displayName="newest";
        assert(store.rememberClosedWindow(newer));
        assert(store.returnClosedWindow(older));
        assert(store.takeClosedWindow(&value) && value.key=="B" && value.displayName=="newest");
        assert(store.takeClosedWindow(&value) && value.key=="A");
        assert(store.takeClosedWindow(&value) && value.sequence==0);
        for(int i=0;i<30;++i) assert(store.rememberClosedWindow(record(std::to_string(i))));
        for(int i=29;i>=10;--i) assert(store.takeClosedWindow(&value) && value.key==std::to_string(i));
        assert(store.takeClosedWindow(&value) && value.sequence==0);
        assert(!store.rememberClosedWindow({}));
        assert(store.rememberClosedWindow(record("persistent")));
    }
    {
        FolderStateStore store(path.c_str()); ClosedWindow value;
        assert(store.lastClosedWindow(&value) && value.key=="persistent");
        assert(store.clearAll()); FolderState state;
        assert(store.read("existing",&state) && state.present==0);
        assert(store.takeClosedWindow(&value) && value.sequence==0);
        for(int i=0;i<20;++i) assert(store.rememberClosedWindow(record(std::to_string(i))));
    }
    std::atomic<int> ready{0}; std::atomic<bool> start{false};
    std::vector<std::string> left,right;
    auto take = [&](std::vector<std::string> &out) {
        FolderStateStore store(path.c_str()); assert(store.ready());
        ++ready; while(!start.load()) std::this_thread::yield();
        for(;;) { ClosedWindow value; assert(store.takeClosedWindow(&value));
            if(!value.sequence) break; out.push_back(value.key); }
    };
    std::thread a(take,std::ref(left)),b(take,std::ref(right));
    while(ready.load()!=2) std::this_thread::yield();
    start=true; a.join(); b.join();
    left.insert(left.end(),right.begin(),right.end());
    std::sort(left.begin(),left.end());
    assert(left.size()==20 && std::adjacent_find(left.begin(),left.end())==left.end());
    std::cout<<"PASS: migration préservant les états, historique UTF-8/binaire, ordre LIFO, retour après échec,"
        " unicité, limite de 20, persistance, reset et consommation simultanée par deux connexions.\n";
}
