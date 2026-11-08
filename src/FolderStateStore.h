#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct sqlite3;

namespace filespacer {

struct FolderViewSettings {
    enum Part : uint32_t { Mode = 1, Flags = 2, Columns = 4, Sort = 8, Group = 16 };
    // PROPERTYKEY encoded as the 16 GUID bytes followed by a 32-bit property ID.
    using Key = std::array<uint8_t, 20>;
    struct Column { Key key = {}; uint32_t width = 0; };
    struct SortColumn { Key key = {}; int32_t direction = 0; };
    uint32_t present = 0;
    int32_t mode = 0, iconSize = 0;
    uint32_t flags = 0;
    std::vector<Column> columns;
    std::vector<SortColumn> sort;
    Key groupBy = {};
    bool groupAscending = false;
    // Apply observed provisional changes to this known reference, preserving
    // fields that were unchanged or unavailable in either observation.
    FolderViewSettings mergeChanges(const FolderViewSettings &baseline,
        const FolderViewSettings &current, const std::vector<Column> &initialWidths = {}) const;
};

// A successful access refreshes last_success_at at most once per
// SUCCESS_REFRESH_SECONDS (a sliding window since the stored value); a
// failure, or the success ending a failure period, is always recorded.
constexpr int64_t SUCCESS_REFRESH_SECONDS = 24 * 60 * 60;
// Search queries kept per folder; the preference limit never exceeds it.
constexpr unsigned MAX_STORED_SEARCH_HISTORY = 20;

struct FolderState {
    enum Part : uint32_t { Position = 1, Size = 2, View = 4, Icons = 8, Maximized = 16, All = 31 };
    int64_t lastSuccessAt = 0, firstFailedAt = 0; // UTC seconds; zero means no known failure.
    uint32_t present = 0;
    int32_t x = 0, y = 0, width = 0, height = 0; // 96-DPI units, signed 32-bit coordinates.
    bool locked = false; // Read-only snapshot; ordinary saves never write the lock.
    bool maximized = false; // Independent of normal geometry; minimized state is never stored.
    FolderViewSettings view;
    std::vector<uint8_t> icons;
};

struct ClosedWindow {
    int64_t sequence = 0;
    std::string key, parsingName, displayName; // UTF-8; key is the existing folder identity.
    std::vector<uint8_t> viewState; // One-shot Shell selection and scroll snapshot.
};

// The main SQLite connection is UI-only; background readers open their own connection.
// Short write transactions update only requested parts and enforce the used-page quota.
// Writes, opening/migration and closing are serialized across processes by a
// named mutex; reads rely on WAL alone (readers and writers never block each other).
class FolderStateStore {
public:
    // Retried UI initialization may skip lock waits. Ready connections always
    // retain the ordinary operation deadline (one bound per operation).
    explicit FolderStateStore(const char *utf8Path, bool waitForInitialization = true);
    ~FolderStateStore();
    FolderStateStore(const FolderStateStore &) = delete;
    FolderStateStore &operator=(const FolderStateStore &) = delete;
    bool ready() const { return initialized; }
    const std::string &error() const { return lastError; }
    // Diagnosis of the last wait: the named mutex another process held (owned
    // by this object; duplicate it to wait on it elsewhere), or null when it
    // was acquired. busy() reports SQLite's own lock contention instead.
    void *blockingMutex() const { return mutexBlocked ? accessMutex.get() : nullptr; }
    bool busy() const { return sqliteBusy; }
    bool read(const std::string &key, FolderState *state);
    bool readLocked(const std::string &key, bool *locked); // An absent row is not locked.
    bool contains(const std::string &key, bool *found);
    bool save(const std::string &key, const FolderState &state, uint32_t parts);
    // Capture and change the lock in one transaction. Ordinary saves consult the stored lock.
    bool setLocked(const std::string &key, const FolderState &state, uint32_t parts, bool locked);
    bool clearLocks();
    // Failure never creates a row. expired is true only after a committed age-based deletion.
    // A success within SUCCESS_REFRESH_SECONDS of the stored one, with no failure to clear, changes nothing.
    bool recordAccess(const std::string &key, bool success, int64_t when, bool *expired = nullptr);
    bool clearAll();
    bool remove(const std::string &key);
    bool move(const std::string &oldKey, const std::string &newKey);
    bool readSearchHistory(const std::string &key, unsigned limit, std::vector<std::string> *queries);
    bool readViewScroll(const std::string &key, std::vector<uint8_t> *bytes);
    // Updates only an existing folder, independently of the locked layout.
    bool saveViewScroll(const std::string &key, const std::vector<uint8_t> &bytes);
    // stateMissing reports a committed no-op: no folder_state row exists yet for this key.
    bool saveSearchHistory(const std::string &key, const std::vector<std::string> &queries, unsigned limit,
        bool mergeRecent = false, bool *stateMissing = nullptr);
    bool trimSearchHistory(unsigned limit);
    bool lastClosedWindow(ClosedWindow *window);
    bool takeClosedWindow(ClosedWindow *window);
    bool rememberClosedWindow(const ClosedWindow &window);
    bool returnClosedWindow(const ClosedWindow &window);
private:
    class Lock;
    struct CloseHandle { void operator()(void *handle) const; };
    bool createMutex(const char *utf8Path);
    bool writeState(const std::string &key, const FolderState &state, uint32_t parts, int lockChange);
    bool readClosedWindow(ClosedWindow *window);
    bool writeClosedWindow(const ClosedWindow &window, bool returning);
    bool execute(const char *sql);
    bool result(int code);
    bool beginRead(); // Reads take no mutex; they only get the operation deadline.
    bool enforceQuota();
    bool finishWrite(bool success);
    sqlite3 *db = nullptr;
    bool initialized = false;
    bool mutexBlocked = false, sqliteBusy = false;
    std::string lastError;
    std::unique_ptr<void, CloseHandle> accessMutex;
};

#ifdef _WIN32
// Lazy initialization keeps the existing /test mode in a separate database.
// Ordinary retries stay non-blocking; closing can request the usual lock deadline.
FolderStateStore *folderStateStore(bool waitForInitialization = false);
// After a failed folderStateStore(): the named mutex another process held,
// kept until the next attempt (duplicate it to wait on it), or null; and
// whether SQLite itself reported a busy database (no Windows event exists).
void *folderStateBlockingMutex();
bool folderStateBusy();
// A background reader opens its own connection; it must not use the UI connection.
std::string folderStateDatabasePath();
void reportFolderStateError(void *owner);
#endif

} // namespace

