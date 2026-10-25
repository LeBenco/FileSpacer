#pragma once
#include <array>
#include <cstdint>
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
};

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
class FolderStateStore {
public:
    explicit FolderStateStore(const char *utf8Path);
    ~FolderStateStore();
    FolderStateStore(const FolderStateStore &) = delete;
    FolderStateStore &operator=(const FolderStateStore &) = delete;
    bool ready() const { return initialized; }
    const std::string &error() const { return lastError; }
    bool read(const std::string &key, FolderState *state);
    bool save(const std::string &key, const FolderState &state, uint32_t parts);
    // Capture and change the lock in one transaction. Ordinary saves consult the stored lock.
    bool setLocked(const std::string &key, const FolderState &state, uint32_t parts, bool locked);
    bool clearLocks();
    // Failure never creates a row. expired is true only after a committed age-based deletion.
    bool recordAccess(const std::string &key, bool success, int64_t when, bool *expired = nullptr);
    bool clear(const std::string &key, uint32_t parts);
    bool clearAll();
    bool remove(const std::string &key);
    bool move(const std::string &oldKey, const std::string &newKey);
    bool readSearchHistory(const std::string &key, unsigned limit, std::vector<std::string> *queries);
    bool saveSearchHistory(const std::string &key, const std::vector<std::string> &queries, unsigned limit);
    bool trimSearchHistory(unsigned limit);
    bool lastClosedWindow(ClosedWindow *window);
    bool takeClosedWindow(ClosedWindow *window);
    bool rememberClosedWindow(const ClosedWindow &window);
    bool returnClosedWindow(const ClosedWindow &window);
private:
    bool writeState(const std::string &key, const FolderState &state, uint32_t parts, int lockChange);
    bool readClosedWindow(ClosedWindow *window);
    bool writeClosedWindow(const ClosedWindow &window, bool returning);
    bool execute(const char *sql);
    bool result(int code);
    bool enforceQuota();
    bool finishWrite(bool success);
    sqlite3 *db = nullptr;
    bool initialized = false;
    std::string lastError;
};

#ifdef _WIN32
// Lazy initialization keeps the existing /test mode in a separate database.
FolderStateStore *folderStateStore();
// A background reader opens its own connection; it must not use the UI connection.
std::string folderStateDatabasePath();
void reportFolderStateError(void *owner);
#endif

} // namespace

