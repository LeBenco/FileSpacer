// Execute the verbatim production timer block and storage retry scheduler with
// deterministic API doubles. This checks scheduling, not the real Shell,
// SQLite or the waiting thread itself.
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace {
using HWND = std::uintptr_t;
using HANDLE = void *;
constexpr std::uintptr_t TIMER_SAVE_STATE = 1;
constexpr unsigned MSG_STORAGE_AVAILABLE = 0x8000 + 52;
constexpr unsigned DUPLICATE_SAME_ACCESS = 2;
constexpr int FALSE = 0;
struct TimerCalls { unsigned armed = 0, killed = 0, delay = 0; } timers;
int KillTimer(HWND, std::uintptr_t) { ++timers.killed; return 1; }
std::uintptr_t SetTimer(HWND, std::uintptr_t id, unsigned delay, void *) {
    ++timers.armed;
    timers.delay = delay;
    return id;
}
struct Storage {
    void *blockingMutex = nullptr;
    bool busy = false, available = false, duplicateFails = false;
    unsigned duplicates = 0, posted = 0;
    std::uintptr_t postedSerial = 0;
} storage;
void *folderStateBlockingMutex() { return storage.blockingMutex; }
bool folderStateBusy() { return storage.busy; }
void *folderStateStore(bool = false) { return storage.available ? &storage : nullptr; }
HANDLE GetCurrentProcess() { return &storage; }
int DuplicateHandle(HANDLE, HANDLE source, HANDLE, HANDLE *target, unsigned, int, unsigned) {
    ++storage.duplicates;
    if (storage.duplicateFails) return 0;
    *target = static_cast<char *>(source) + 1; // A distinct handle value for the same object.
    return 1;
}
int PostMessage(HWND, unsigned message, std::uintptr_t wParam, std::uintptr_t) {
    if (message == MSG_STORAGE_AVAILABLE) { ++storage.posted; storage.postedSerial = wParam; }
    return 1;
}
template <class T> T checkLE(T value) { return value; }

void require(bool passed, const char *what) {
    if (!passed) { std::cerr << "FAIL: storage retry scheduling contract: " << what << "\n"; std::exit(1); }
}

struct StorageWaitThread {
    HWND owner; HANDLE mutex; std::uintptr_t serial; bool started = false;
    StorageWaitThread(HWND ownerWindow, HANDLE mutexHandle, std::uintptr_t waitSerial)
        : owner(ownerWindow), mutex(mutexHandle), serial(waitSerial) {}
    void start() { started = true; }
};
struct WaitPointer {
    StorageWaitThread *p = nullptr;
    ~WaitPointer() { delete p; }
    void Attach(StorageWaitThread *thread) { delete p; p = thread; }
    void Release() { delete p; p = nullptr; }
    StorageWaitThread *operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

struct FolderWindow {
    HWND hwnd = 1;
    bool operational = true, available = false;
    bool provisionalBaseline = true, provisionalReady = true, provisionalRestore = false;
    bool provisionalInputBlocked = false, provisionalCaptureBusy = false;
    unsigned captures = 0, loads = 0, saves = 0, histories = 0, retries = 0;
    WaitPointer storageWait;
    std::uintptr_t storageWaitSerial = 0;
    unsigned storageRetryDelay = 0;
    bool countRetries = false; // Count scheduleStorageRetry() instead of running it.
    bool isWindowOperational() const { return operational; }
    bool loadFolderState() { ++loads; return available; }
    bool captureProvisionalCurrent(bool, bool) { ++captures; return true; }
    void blockProvisionalInput(bool blocked) { provisionalInputBlocked = blocked; }
    void persistViewState() { ++saves; provisionalBaseline = false; }
    void saveSearchHistory() { ++histories; }
    void scheduleStorageRetry();
    void scheduleStorageRetryProduction();
    int onTimer(std::uintptr_t wParam = TIMER_SAVE_STATE) {
#include "SaveStateTimer.inc"
        return -1;
    }
};
void FolderWindow::scheduleStorageRetry() {
    if (countRetries) { ++retries; return; }
    scheduleStorageRetryProduction();
}
}

// The production scheduler is compiled under its own name; the timer block
// calls the counting wrapper above.
#define scheduleStorageRetry scheduleStorageRetryProduction
#include "StorageRetry.inc"
#undef scheduleStorageRetry

int main() {
    // Timer block: a failed load schedules the retry once, re-arms no timer
    // itself and captures nothing; a successful load saves and resets the delay.
    FolderWindow pending;
    pending.countRetries = true;
    pending.storageRetryDelay = 4000;
    require(pending.onTimer() == 0, "timer handled");
    require(timers.killed == 1 && timers.armed == 0, "failed load arms no timer from the block");
    require(pending.retries == 1 && pending.loads == 1 && pending.captures == 0 && pending.saves == 0,
        "failed load schedules one retry");
    require(pending.storageRetryDelay == 4000, "failed load keeps the backoff state");
    pending.available = true;
    require(pending.onTimer() == 0, "timer handled after recovery");
    require(timers.armed == 0 && pending.saves == 1 && pending.histories == 1 && pending.storageRetryDelay == 0,
        "recovered load saves, keeps no timer and resets the backoff");

    timers = {};
    FolderWindow opening;
    opening.countRetries = true;
    opening.provisionalInputBlocked = true;
    require(opening.onTimer() == 0, "blocked timer handled");
    require(!opening.provisionalInputBlocked && opening.captures == 1 && opening.retries == 1 && timers.armed == 0,
        "blocked input is captured once, then the retry is scheduled");
    require(opening.onTimer() == 0, "blocked timer handled twice");
    require(opening.captures == 1 && opening.retries == 2, "no repeated capture");

    timers = {};
    FolderWindow closing;
    closing.countRetries = true;
    closing.operational = false;
    require(closing.onTimer() == 0, "closing timer handled");
    require(timers.killed == 1 && timers.armed == 0 && closing.loads == 0 && closing.retries == 0, "closing stops");

    timers = {};
    FolderWindow ready;
    ready.provisionalBaseline = false;
    require(ready.onTimer() == 0, "ready timer handled");
    require(timers.armed == 0 && ready.loads == 0 && ready.saves == 1 && ready.histories == 1, "ready saves");

    // Scheduler: a held mutex is waited on by a thread with a duplicated
    // handle, without any timer; a pending wait is not doubled.
    timers = {}; storage = {};
    int object = 0;
    storage.blockingMutex = &object;
    FolderWindow held;
    held.storageRetryDelay = 2000;
    held.scheduleStorageRetry();
    require(static_cast<bool>(held.storageWait) && held.storageWait->started, "mutex wait thread started");
    require(held.storageWait->mutex == static_cast<char *>(storage.blockingMutex) + 1
        && held.storageWait->owner == held.hwnd && held.storageWait->serial == 1 && held.storageWaitSerial == 1,
        "wait thread owns the duplicated handle for this window");
    require(storage.duplicates == 1 && timers.armed == 0 && held.storageRetryDelay == 0, "no timer while waiting");
    held.scheduleStorageRetry();
    require(storage.duplicates == 1 && held.storageWaitSerial == 1 && timers.armed == 0, "pending wait not doubled");

    // SQLITE_BUSY alone: growing delay, bounded, never a thread.
    timers = {}; storage = {};
    storage.busy = true;
    FolderWindow busy;
    unsigned previous = 0;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        busy.scheduleStorageRetry();
        require(timers.armed == attempt + 1 && !busy.storageWait && storage.duplicates == 0, "busy arms a timer only");
        require(timers.delay == busy.storageRetryDelay && timers.delay >= 1000 && timers.delay <= 16000,
            "busy delay stays within bounds");
        require(timers.delay > previous || timers.delay == 16000, "busy delay grows until its bound");
        previous = timers.delay;
    }
    require(previous == 16000, "busy delay reaches its bound");

    // Neither: no automatic retry; a store reopened meanwhile continues from the queue.
    timers = {}; storage = {};
    FolderWindow other;
    other.storageRetryDelay = 8000;
    other.scheduleStorageRetry();
    require(timers.armed == 0 && !other.storageWait && other.storageRetryDelay == 0 && storage.posted == 0,
        "other failures schedule nothing");
    storage.available = true;
    other.scheduleStorageRetry();
    require(timers.armed == 0 && !other.storageWait && storage.posted == 1 && storage.postedSerial == other.storageWaitSerial
        && other.storageWaitSerial == 1, "a reopened store is continued by the private message");

    // A failed handle duplication falls back to the bounded delay.
    timers = {}; storage = {};
    storage.blockingMutex = &object;
    storage.duplicateFails = true;
    FolderWindow fallback;
    fallback.scheduleStorageRetry();
    require(storage.duplicates == 1 && !fallback.storageWait && timers.armed == 1 && timers.delay == 1000,
        "failed duplication backs off");
    std::cout << "PASS: failed loads wait for the held mutex without a timer, back off only on SQLITE_BUSY within bounds, and stop on closing.\n";
}
