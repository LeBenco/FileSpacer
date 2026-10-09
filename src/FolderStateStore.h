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
    bool maximized = false; // Independent of normal geometry; minimized state is never stored.
    FolderViewSettings view;
    std::vector<uint8_t> icons;
};

// One SQLite connection per process, used only by the application's UI thread.
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
    // Failure never creates a row. expired is true only after a committed age-based deletion.
    bool recordAccess(const std::string &key, bool success, int64_t when, bool *expired = nullptr);
    bool clear(const std::string &key, uint32_t parts);
    bool clearAll();
    bool remove(const std::string &key);
    bool move(const std::string &oldKey, const std::string &newKey);
private:
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
void reportFolderStateError(void *owner);
#endif

} // namespace

