// Execute the verbatim production timer block with deterministic API doubles.
// This checks scheduling, not the real Shell or SQLite implementation.
#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace {
using HWND = std::uintptr_t;
constexpr std::uintptr_t TIMER_SAVE_STATE = 1;
struct TimerCalls { unsigned armed = 0, killed = 0, delay = 0; } timers;
int KillTimer(HWND, std::uintptr_t) { ++timers.killed; return 1; }
std::uintptr_t SetTimer(HWND, std::uintptr_t id, unsigned delay, void *) {
    ++timers.armed;
    timers.delay = delay;
    return id;
}

void require(bool passed) { if (!passed) { std::cerr << "FAIL: storage retry scheduling contract.\n"; std::exit(1); } }

struct FolderWindow {
    HWND hwnd = 1;
    bool operational = true, available = false;
    bool provisionalBaseline = true, provisionalReady = true, provisionalRestore = false;
    bool provisionalInputBlocked = false, provisionalCaptureBusy = false;
    unsigned captures = 0, loads = 0, saves = 0, histories = 0;
    bool isWindowOperational() const { return operational; }
    bool loadFolderState() { ++loads; return available; }
    bool captureProvisionalCurrent(bool, bool) { ++captures; return true; }
    void blockProvisionalInput(bool blocked) { provisionalInputBlocked = blocked; }
    void persistViewState() { ++saves; provisionalBaseline = false; }
    void saveSearchHistory() { ++histories; }
    int onTimer(std::uintptr_t wParam = TIMER_SAVE_STATE) {
#include "SaveStateTimer.inc"
        return -1;
    }
};
}

int main() {
    FolderWindow pending;
    require(pending.onTimer() == 0);
    require(timers.killed == 1 && timers.armed == 1);
    require(timers.delay > 0 && timers.delay <= 1000);
    require(pending.loads == 1 && pending.captures == 0 && pending.saves == 0);
    require(pending.onTimer() == 0);
    require(timers.armed == 2 && pending.captures == 0 && pending.loads == 2);
    pending.available = true;
    require(pending.onTimer() == 0);
    require(timers.armed == 2 && pending.saves == 1 && pending.histories == 1);

    timers = {};
    FolderWindow opening;
    opening.provisionalInputBlocked = true;
    require(opening.onTimer() == 0);
    require(!opening.provisionalInputBlocked && opening.captures == 1 && timers.armed == 1);
    require(opening.onTimer() == 0);
    require(opening.captures == 1 && timers.armed == 2);

    timers = {};
    FolderWindow closing;
    closing.operational = false;
    require(closing.onTimer() == 0);
    require(timers.killed == 1 && timers.armed == 0 && closing.loads == 0 && closing.saves == 0);

    timers = {};
    FolderWindow ready;
    ready.provisionalBaseline = false;
    require(ready.onTimer() == 0);
    require(timers.armed == 0 && ready.loads == 0 && ready.saves == 1 && ready.histories == 1);
    std::cout << "PASS: failed initial reads retry without repeated capture, recover without input, and stop on closing.\n";
}
