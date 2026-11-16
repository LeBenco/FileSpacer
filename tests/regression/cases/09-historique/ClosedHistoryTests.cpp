#include "FolderStateStore.h"
#include "ClosedWindows.h"
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
    {
        FolderStateStore store(path.c_str()); assert(store.ready());
        FolderState state;state.x=12;state.y=34;state.width=600;state.height=400;state.maximized=true;
        if (!store.save("existing",state,FolderState::Position|FolderState::Size|FolderState::Maximized)) return 1;
        assert(store.read("existing",&state));
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
    {
        // Jump List fingerprint: equal models match; any field change, reordering or
        // boundary shift between fields changes the fingerprint.
        JumpListModel a, b, c, d, e;
        for (JumpListModel *m : {&a, &b}) { m->add(L"C:\\App\\FileSpacer.exe"); m->add(L"Quick access"); m->add(L""); m->add(L"folder"); }
        c.add(L"C:\\App\\FileSpacer.exe"); c.add(L"Quick access"); c.add(L"folder"); c.add(L"");
        d.add(L"C:\\App\\FileSpacer.exe"); d.add(L"Quick acc"); d.add(L"ess"); d.add(L"folder");
        e.add(L"C:\\App\\FileSpacer.exe"); e.add(L"Quick access"); e.add(L""); e.add(L"folder"); e.add(L"");
        assert(a.fingerprint() == b.fingerprint());
        assert(a.fingerprint() != c.fingerprint() && a.fingerprint() != d.fingerprint() && a.fingerprint() != e.fingerprint());
        std::cout<<"PASS: empreinte des Jump Lists stable et sensible aux champs, à leur ordre et à leurs limites.\n";
    }
    std::cout<<"PASS: state preservation, UTF-8/binary history, LIFO order, return after failure,"
        " uniqueness, limit of 20, persistence, reset and concurrent consumption by two connections.\n";
}
