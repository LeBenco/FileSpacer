#include "FolderStateStore.h"
#include <sqlite3.h>
#include <cstring>
#include <memory>
#include <utility>
#include <algorithm>
#if defined(_WIN32) && defined(FILESPACER_STORAGE_TEST)
#include <windows.h>
#endif

#if defined(_WIN32) && !defined(FILESPACER_STORAGE_TEST)
#include "common.h"
#include "Settings.h"
#include "UIStrings.h"
#include "WinUtils.h"
#include <shlobj.h>
#include <atlbase.h>
#include <commctrl.h>
#include <shellapi.h>
#include <type_traits>
#include <RestartManager.h>
#pragma comment(lib, "Rstrtmgr.lib")
#endif

namespace filespacer {
namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
// Single wait bound of one operation, shared by the mutex wait and SQLite's busy handler.
constexpr unsigned OPERATION_WAIT_MS = 3000;

void appendInt(std::vector<uint8_t> &bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
std::vector<uint8_t> encodeView(const FolderViewSettings &view) {
    std::vector<uint8_t> bytes;
    const bool columnFill = (view.present & FolderViewSettings::ColumnFill) != 0;
    appendInt(bytes, columnFill ? 2 : 1); // Keep the original encoding when no new choice is present.
    appendInt(bytes, view.present);
    appendInt(bytes, static_cast<uint32_t>(view.mode));
    appendInt(bytes, static_cast<uint32_t>(view.iconSize));
    appendInt(bytes, view.flags);
    appendInt(bytes, static_cast<uint32_t>(view.columns.size()));
    for (const auto &column : view.columns) {
        bytes.insert(bytes.end(), column.key.begin(), column.key.end());
        appendInt(bytes, column.width);
    }
    appendInt(bytes, static_cast<uint32_t>(view.sort.size()));
    for (const auto &column : view.sort) {
        bytes.insert(bytes.end(), column.key.begin(), column.key.end());
        appendInt(bytes, static_cast<uint32_t>(column.direction));
    }
    bytes.insert(bytes.end(), view.groupBy.begin(), view.groupBy.end());
    appendInt(bytes, view.groupAscending ? 1 : 0);
    if (columnFill) appendInt(bytes, view.fillColumns ? 1 : 0);
    return bytes;
}

bool decodeView(const uint8_t *bytes, size_t size, FolderViewSettings *out) {
    size_t offset = 0;
    auto integer = [&](uint32_t *value) {
        if (size - offset < 4) return false;
        *value = 0;
        for (unsigned i = 0; i < 4; ++i)
            *value |= static_cast<uint32_t>(bytes[offset++]) << (8 * i);
        return true;
    };
    auto key = [&](FolderViewSettings::Key *value) {
        if (size - offset < value->size()) return false;
        std::memcpy(value->data(), bytes + offset, value->size());
        offset += value->size();
        return true;
    };
    FolderViewSettings view;
    uint32_t version, mode, iconSize, count;
    if (!integer(&version) || (version != 1 && version != 2) || !integer(&view.present)
            || (view.present & ~(version == 1 ? 31u : 63u)) || !integer(&mode) || !integer(&iconSize)
            || !integer(&view.flags) || !integer(&count) || count > (size-offset)/24)
        return false;
    view.mode = static_cast<int32_t>(mode);
    view.iconSize = static_cast<int32_t>(iconSize);
    view.columns.resize(count);
    for (auto &column : view.columns)
        if (!key(&column.key) || !integer(&column.width)) return false;
    if (!integer(&count) || count > (size-offset)/24) return false;
    view.sort.resize(count);
    for (auto &column : view.sort) {
        uint32_t direction;
        if (!key(&column.key) || !integer(&direction)) return false;
        column.direction = static_cast<int32_t>(direction);
    }
    uint32_t ascending;
    if (!key(&view.groupBy) || !integer(&ascending) || ascending > 1) return false;
    view.groupAscending = ascending != 0;
    if (version == 2) {
        uint32_t fill;
        if (!integer(&fill) || fill > 1) return false;
        view.fillColumns = fill != 0;
    }
    if (offset != size) return false;
    *out = std::move(view);
    return true;
}
}

FolderViewSettings FolderViewSettings::mergeChanges(const FolderViewSettings &baseline,
        const FolderViewSettings &current, const std::vector<Column> &initialWidths) const {
    FolderViewSettings merged = *this;
    auto observed = [&](uint32_t part) { return (baseline.present & current.present & part) != 0; };
    if (current.present & ColumnFill) {
        if (!(present & ColumnFill) || (observed(ColumnFill) && current.fillColumns != baseline.fillColumns))
            merged.fillColumns = current.fillColumns;
        merged.present |= ColumnFill;
    }
    if (current.present & Mode) {
        if (!(present & Mode) || (observed(Mode) && current.mode != baseline.mode)) merged.mode = current.mode;
        if (!(present & Mode) || (observed(Mode) && current.iconSize != baseline.iconSize)) merged.iconSize = current.iconSize;
        merged.present |= Mode;
    }
    if (current.present & Flags) {
        if (!(present & Flags)) merged.flags = current.flags;
        else if (observed(Flags)) {
            const uint32_t changed = baseline.flags ^ current.flags;
            merged.flags = (flags & ~changed) | (current.flags & changed);
        }
        merged.present |= Flags;
    }
    if (current.present & Columns) {
        if (!(present & Columns)) merged.columns = current.columns;
        else if (observed(Columns)) {
            auto find = [](const std::vector<Column> &list, const Key &key) {
                return std::find_if(list.begin(), list.end(), [&](const Column &column) { return column.key == key; });
            };
            bool changedOrder = baseline.columns.size() != current.columns.size();
            for (size_t i = 0; !changedOrder && i < baseline.columns.size(); ++i)
                changedOrder = baseline.columns[i].key != current.columns[i].key;
            if (changedOrder) {
                merged.columns = current.columns;
                // Columns unknown to the provisional view remain present, in
                // their saved relative order, after the explicitly chosen list.
                for (const auto &column : columns)
                    if (find(baseline.columns, column.key) == baseline.columns.end()
                            && find(current.columns, column.key) == current.columns.end())
                        merged.columns.push_back(column);
            }
            for (const auto &column : current.columns) {
                const auto before = find(baseline.columns, column.key), saved = find(columns, column.key);
                const auto initiallyHidden = find(initialWidths, column.key);
                const bool changedWidth = column.width <= INT32_MAX && (before != baseline.columns.end()
                    && before->width <= INT32_MAX ? before->width != column.width
                    : initiallyHidden != initialWidths.end() && initiallyHidden->width <= INT32_MAX
                        && initiallyHidden->width != column.width);
                auto target = std::find_if(merged.columns.begin(), merged.columns.end(),
                    [&](const Column &value) { return value.key == column.key; });
                if (target != merged.columns.end()) {
                    if (changedWidth) target->width = column.width;
                    else if (saved != columns.end()) target->width = saved->width;
                } else if (changedWidth) {
                    merged.columns.push_back(column);
                }
            }
        }
        merged.present |= Columns;
    }
    if (current.present & Sort) {
        bool changed = baseline.sort.size() != current.sort.size();
        for (size_t i = 0; !changed && i < baseline.sort.size(); ++i)
            changed = baseline.sort[i].key != current.sort[i].key || baseline.sort[i].direction != current.sort[i].direction;
        // The ordered sort chain is one choice; merging individual directions
        // into another saved chain would change the meaning of that choice.
        if (!(present & Sort) || (observed(Sort) && changed)) merged.sort = current.sort;
        merged.present |= Sort;
    }
    if (current.present & Group) {
        if (!(present & Group) || (observed(Group) && (baseline.groupBy != current.groupBy
                || baseline.groupAscending != current.groupAscending))) {
            merged.groupBy = current.groupBy;
            merged.groupAscending = current.groupAscending;
        }
        merged.present |= Group;
    }
    return merged;
}

bool FolderStateStore::result(int code) {
    sqliteBusy = code == SQLITE_BUSY; // Extended result codes are not enabled on this connection.
    if (code == SQLITE_OK || code == SQLITE_DONE || code == SQLITE_ROW) {
        lastError.clear();
        lastErrorCode = 0;
        return true;
    }
    lastErrorCode = db ? sqlite3_extended_errcode(db) : code;
    if ((lastErrorCode & 255) != code) lastErrorCode = code;
    lastError = db ? sqlite3_errmsg(db) : sqlite3_errstr(code);
    return false;
}
bool FolderStateStore::beginRead() {
    if (!initialized) return false;
    return result(sqlite3_busy_timeout(db, static_cast<int>(OPERATION_WAIT_MS)));
}
bool FolderStateStore::execute(const char *sql) {
    return result(sqlite3_exec(db, sql, nullptr, nullptr, nullptr));
}

void FolderStateStore::CloseHandle::operator()(void *handle) const {
#ifdef _WIN32
    ::CloseHandle(handle);
#else
    (void)handle;
#endif
}

bool FolderStateStore::createMutex(const char *utf8Path) {
#ifdef _WIN32
    auto failed = [&]() {
        const DWORD code = GetLastError();
        lastError = "Cannot coordinate folder-state database (Windows error " + std::to_string(code) + ")";
        return false;
    };
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path, -1, nullptr, 0);
    if (!length) return failed();
    std::vector<wchar_t> path(static_cast<size_t>(length));
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path, -1, path.data(), length)) return failed();
    // Identify the actual file, including aliases, before SQLite can acquire locks.
    const HANDLE raw = CreateFileW(path.data(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return failed();
    const std::unique_ptr<void, CloseHandle> file(raw);
    FILE_ID_INFO identity = {};
    if (!GetFileInformationByHandleEx(file.get(), FileIdInfo, &identity, sizeof(identity))) return failed();
    std::wstring name = L"Global\\FileSpacer.FolderState." + std::to_wstring(identity.VolumeSerialNumber) + L".";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (const auto byte : identity.FileId.Identifier) {
        name.push_back(hex[byte >> 4]);
        name.push_back(hex[byte & 15]);
    }
    accessMutex.reset(CreateMutexExW(nullptr, name.c_str(), 0, SYNCHRONIZE | MUTEX_MODIFY_STATE));
    if (!accessMutex) return failed();
#else
    (void)utf8Path;
#endif
    return true;
}

// Write-side serialization. One operation waits at most `timeout` in total:
// the mutex wait consumes part of it and SQLite's busy handler gets the rest
// (the two bounds are never added). Reads do not take the mutex (WAL).
class FolderStateStore::Lock {
public:
    explicit Lock(FolderStateStore &store, unsigned timeout = OPERATION_WAIT_MS)
        : mutex(store.accessMutex.get()), left(timeout) {
#ifdef _WIN32
        DWORD waited = WAIT_FAILED, code = ERROR_INVALID_HANDLE; // No mutex: nothing was called.
        if (mutex) {
            const ULONGLONG started = GetTickCount64();
            waited = WaitForSingleObject(mutex, timeout);
            const ULONGLONG elapsed = GetTickCount64() - started;
            left = elapsed < timeout ? timeout - static_cast<unsigned>(elapsed) : 0;
            if (waited == WAIT_FAILED) code = GetLastError();
            else if (waited == WAIT_TIMEOUT) code = ERROR_TIMEOUT;
        }
        acquired = waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED;
        store.mutexBlocked = waited == WAIT_TIMEOUT;
        if (!acquired) {
            store.lastError = "Folder-state database mutex wait failed (Windows error " + std::to_string(code) + ")";
            store.lastErrorCode = 0;
            store.sqliteBusy = false;
        } else if (store.db) sqlite3_busy_timeout(store.db, static_cast<int>(left));
#else
        acquired = true;
        if (store.db) sqlite3_busy_timeout(store.db, static_cast<int>(left));
#endif
    }
    unsigned remaining() const { return left; } // Budget left for SQLite after the mutex wait.
    ~Lock() {
#ifdef _WIN32
        if (acquired) ReleaseMutex(mutex);
#endif
    }
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;
    explicit operator bool() const { return acquired; }
private:
    void *mutex;
    unsigned left;
    bool acquired = false;
};

FolderStateStore::FolderStateStore(const char *utf8Path, bool waitForInitialization) {
    if (!createMutex(utf8Path)) return;
    Lock lock(*this, waitForInitialization ? OPERATION_WAIT_MS : 0);
    if (!lock) return;
    const int opened = sqlite3_open_v2(utf8Path, &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    auto closeOpening = [this](sqlite3 *connection) { sqlite3_close(connection); db = nullptr; };
    // Failed initialization closes before the constructor's lock is released.
    // Its destructor must not acquire that lock again after a non-waiting retry.
    std::unique_ptr<sqlite3, decltype(closeOpening)> opening(db, closeOpening);
    if (!result(opened)) return;
    sqlite3_busy_timeout(db, static_cast<int>(lock.remaining())); // The lock could not set it before the open.
    if (!execute("PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;"
            "PRAGMA wal_autocheckpoint=1000; PRAGMA journal_size_limit=8000000;"
            "BEGIN IMMEDIATE;")) return;
    // Read the version after acquiring the write lock: another process may migrate first.
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &raw, nullptr))) {
        finishWrite(false);
        return;
    }
    Statement version(raw, sqlite3_finalize);
    if (!result(sqlite3_step(version.get()))) {
        version.reset();
        finishWrite(false);
        return;
    }
    int schema = sqlite3_column_int(version.get(), 0);
    version.reset();
    if (schema != 0 && schema != 2 && schema != 3 && schema != 4 && schema != 5 && schema != 6 && schema != 7) {
        unsupportedSchema = true;
        lastError = "Unsupported FileSpacer database version " + std::to_string(schema)
            + " (supported: 2 through 7)";
        finishWrite(false);
        return;
    }
    // An up-to-date schema needs no DDL and no user_version write.
    if (schema != 7 && ((schema == 2 && !execute("ALTER TABLE folder_state ADD COLUMN maximized INTEGER;"))
            || !execute("CREATE TABLE IF NOT EXISTS folder_state("
            "key TEXT PRIMARY KEY NOT NULL, x INTEGER, y INTEGER, width INTEGER, height INTEGER,"
            "view BLOB, icons BLOB, last_success_at INTEGER NOT NULL DEFAULT 0,"
            "first_failed_at INTEGER, maximized INTEGER, locked INTEGER NOT NULL DEFAULT 0, scroll BLOB) WITHOUT ROWID;"
            "CREATE INDEX IF NOT EXISTS folder_state_age ON folder_state(last_success_at);"
            "CREATE TABLE IF NOT EXISTS closed_windows("
            "sequence INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT UNIQUE NOT NULL,"
            "parsing_name TEXT NOT NULL, display_name TEXT NOT NULL, view_state BLOB);"
            "CREATE TABLE IF NOT EXISTS search_history("
            "sequence INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT NOT NULL, query TEXT NOT NULL,"
            "UNIQUE(key,query));"
            "CREATE TRIGGER IF NOT EXISTS delete_search_history AFTER DELETE ON folder_state "
            "BEGIN DELETE FROM search_history WHERE key=old.key; END;"
            "PRAGMA user_version=7;"))) {
        finishWrite(false);
        return;
    }
    if (schema >= 2 && schema <= 4 && !execute(
            "ALTER TABLE folder_state ADD COLUMN locked INTEGER NOT NULL DEFAULT 0;")) {
        finishWrite(false);
        return;
    }
    if (schema >= 2 && schema <= 6 && !execute("ALTER TABLE folder_state ADD COLUMN scroll BLOB;")) {
        finishWrite(false);
        return;
    }
    if (finishWrite(true)) {
        initialized = true; // Each operation sets its own busy timeout from its remaining budget.
        opening.release();
    }
}
FolderStateStore::~FolderStateStore() {
    if (!db) return; // A constructor that never opened SQLite has nothing to serialize.
    // Closing the last connection can checkpoint/unlink WAL. Serialize that too,
    // but only for the ordinary bound: the checkpoint is itself protected by
    // SQLite's file locks, so a holder that never releases cannot block exit.
    // Destruction must release SQLite handles even after an operation timed out.
#ifdef _WIN32
    // Do not allocate an error string from a destructor if the wait itself fails.
    const DWORD waited = accessMutex ? WaitForSingleObject(accessMutex.get(), OPERATION_WAIT_MS) : WAIT_FAILED;
#endif
    if (db) sqlite3_close(db); // Last connection checkpoints WAL automatically.
#ifdef _WIN32
    if (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED) ReleaseMutex(accessMutex.get());
#endif
}

bool FolderStateStore::canResetDamagedDatabase() const {
    const int primary = lastErrorCode & 255;
    return !initialized && (unsupportedSchema || primary == SQLITE_CORRUPT || primary == SQLITE_NOTADB
        || primary == SQLITE_ERROR);
}
#ifdef _WIN32
bool FolderStateStore::resetDamagedDatabase(const char *utf8Path, const wchar_t *backupDirectory) {
    if (!canResetDamagedDatabase() || db) return false;
    Lock lock(*this);
    if (!lock) return false;
    auto reset = [&]() -> bool {
        auto failed = [&]() {
            const DWORD code = GetLastError();
            lastErrorCode = 0;
            lastError = "Cannot back up folder-state database (Windows error " + std::to_string(code) + ")";
            return false;
        };
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path, -1, nullptr, 0);
        if (!count) return failed();
        std::wstring path(static_cast<size_t>(count), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path, -1, &path[0], count)) return failed();
        path.pop_back();
        const wchar_t *suffixes[] = {L"", L"-wal", L"-shm", L"-journal"};
        std::unique_ptr<void, CloseHandle> files[4];
        for (size_t index = 0; index < ARRAYSIZE(suffixes); ++index) {
            const HANDLE file = CreateFileW((path + suffixes[index]).c_str(), GENERIC_READ | DELETE,
                0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (file == INVALID_HANDLE_VALUE) {
                const DWORD error = GetLastError();
                if (index && error == ERROR_FILE_NOT_FOUND) continue;
                SetLastError(error);
                return failed();
            }
            files[index].reset(file);
            BY_HANDLE_FILE_INFORMATION information = {};
            if (!GetFileInformationByHandle(file, &information)) return failed();
            if (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                SetLastError(ERROR_CANT_ACCESS_FILE);
                return failed(); // Never follow a symbolic link while deleting database files.
            }
        }
        // Keep every source exclusively open until backup and deletion finish.
        // Existing users prevent this operation; no foreign handle is closed.
        if (!CreateDirectoryW(backupDirectory, nullptr)) return failed();
        const size_t slash = path.find_last_of(L"\\/");
        const std::wstring name = path.substr(slash == std::wstring::npos ? 0 : slash + 1);
        std::vector<uint8_t> buffer(64 * 1024);
        for (size_t index = 0; index < ARRAYSIZE(suffixes); ++index) {
            if (!files[index]) continue;
            const std::wstring destination = std::wstring(backupDirectory) + L"\\" + name + suffixes[index];
            const HANDLE raw = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (raw == INVALID_HANDLE_VALUE) return failed();
            const std::unique_ptr<void, CloseHandle> copy(raw);
            for (;;) {
                DWORD read = 0, written = 0;
                if (!ReadFile(files[index].get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
                    return failed();
                if (!read) break;
                if (!WriteFile(copy.get(), buffer.data(), read, &written, nullptr)) return failed();
                if (written != read) {
                    SetLastError(ERROR_WRITE_FAULT);
                    return failed();
                }
            }
            if (!FlushFileBuffers(copy.get())) return failed();
        }
        FILE_DISPOSITION_INFO disposition = {TRUE};
        // Mark the main database last; keep all handles open until every mark
        // succeeds. On failure cancel pending deletions before releasing them.
        const size_t order[] = {1, 2, 3, 0};
        for (const size_t index : order) {
            if (!files[index]) continue;
            if (!SetFileInformationByHandle(files[index].get(), FileDispositionInfo,
                    &disposition, sizeof(disposition))) {
                const DWORD error = GetLastError();
                FILE_DISPOSITION_INFO cancel = {FALSE};
                bool restored = true;
                for (auto &file : files)
                    if (file && !SetFileInformationByHandle(file.get(), FileDispositionInfo, &cancel, sizeof(cancel)))
                        restored = false;
                lastErrorCode = 0;
                lastError = "Cannot reset folder-state database (Windows error " + std::to_string(error) + ")"
                    + (restored ? "" : "; use the complete backup to restore the files");
                return false;
            }
        }
        // Sidecars disappear before the main file can be recreated by another process.
        for (size_t index = 1; index < ARRAYSIZE(suffixes); ++index) files[index].reset();
        files[0].reset();
        return true;
    };
    return reset();
}
#endif

bool FolderStateStore::read(const std::string &key, FolderState *state) {
    if (!beginRead()) return false;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db,
            "SELECT x,y,width,height,view,icons,last_success_at,first_failed_at,maximized,locked "
            "FROM folder_state WHERE key=?1", -1, &raw, nullptr)))
        return false;
    Statement statement(raw, sqlite3_finalize);
    if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))) return false;
    int code = sqlite3_step(raw);
    if (!result(code)) return false;
    FolderState value;
    if (code == SQLITE_ROW) {
        value.locked = sqlite3_column_int(raw, 9) != 0;
        value.lastSuccessAt = sqlite3_column_int64(raw, 6);
        value.firstFailedAt = sqlite3_column_int64(raw, 7);
        if (sqlite3_column_type(raw, 8) != SQLITE_NULL) {
            value.present |= FolderState::Maximized;
            value.maximized = sqlite3_column_int(raw, 8) != 0;
        }
        if (sqlite3_column_type(raw, 0) != SQLITE_NULL && sqlite3_column_type(raw, 1) != SQLITE_NULL) {
            value.present |= FolderState::Position;
            value.x = sqlite3_column_int(raw, 0); value.y = sqlite3_column_int(raw, 1);
        }
        if (sqlite3_column_type(raw, 2) != SQLITE_NULL && sqlite3_column_type(raw, 3) != SQLITE_NULL) {
            value.width = sqlite3_column_int(raw, 2); value.height = sqlite3_column_int(raw, 3);
            if (value.width > 0 && value.height > 0) value.present |= FolderState::Size;
        }
        if (sqlite3_column_type(raw, 4) != SQLITE_NULL) {
            if (!decodeView(static_cast<const uint8_t *>(sqlite3_column_blob(raw, 4)),
                    static_cast<size_t>(sqlite3_column_bytes(raw, 4)), &value.view)) {
                lastError = "Invalid FileSpacer view data";
                return false;
            }
            value.present |= FolderState::View;
        }
        if (sqlite3_column_type(raw, 5) != SQLITE_NULL) {
            const auto bytes = static_cast<const uint8_t *>(sqlite3_column_blob(raw, 5));
            const int size = sqlite3_column_bytes(raw, 5);
            if (size) value.icons.assign(bytes, bytes + size);
            value.present |= FolderState::Icons;
        }
    }
    *state = std::move(value);
    return true;
}

bool FolderStateStore::contains(const std::string &key, bool *found) {
    *found = false;
    if (!beginRead()) return false;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "SELECT 1 FROM folder_state WHERE key=?1", -1, &raw, nullptr))) return false;
    Statement statement(raw, sqlite3_finalize);
    if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))) return false;
    const int code = sqlite3_step(raw);
    if (!result(code)) return false;
    *found = code == SQLITE_ROW;
    return true;
}

bool FolderStateStore::readLocked(const std::string &key, bool *locked) {
    *locked = false;
    if (!beginRead()) return false;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "SELECT locked FROM folder_state WHERE key=?1", -1, &raw, nullptr)))
        return false;
    Statement statement(raw, sqlite3_finalize);
    if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))) return false;
    const int code = sqlite3_step(raw);
    if (!result(code)) return false;
    *locked = code == SQLITE_ROW && sqlite3_column_int(raw, 0) != 0;
    return true;
}

bool FolderStateStore::save(const std::string &key, const FolderState &state, uint32_t parts) {
    return writeState(key, state, parts, -1);
}

bool FolderStateStore::setLocked(const std::string &key, const FolderState &state,
        uint32_t parts, bool locked) {
    if (key.empty()) return false;
    return writeState(key, state, parts, locked ? 1 : 0);
}

bool FolderStateStore::clearLocks() {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock) return false;
    return execute("UPDATE folder_state SET locked=0 WHERE locked!=0");
}

bool FolderStateStore::writeState(const std::string &key, const FolderState &state,
        uint32_t parts, int lockChange) {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock) return false;
    parts &= FolderState::All;
    if (!parts) return true;
    if (key.empty()) return true;
    if (!execute("BEGIN IMMEDIATE")) return false;
    const bool success = [&]() {
#if defined(_WIN32) && !defined(FILESPACER_STORAGE_TEST)
        // Read after taking the write lock: global disabling clears locks after
        // disabling the preference, and waits for any earlier writer to finish.
        if (lockChange >= 0 && !settings::getWindowStateLockEnabled()) return false;
#endif
        sqlite3_stmt *raw = nullptr;
        if (!result(sqlite3_prepare_v2(db,
            "INSERT INTO folder_state(key,x,y,width,height,view,icons,last_success_at,first_failed_at,maximized,locked) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?9,?10,?11,max(?12,0)) "
            "ON CONFLICT(key) DO UPDATE SET "
            "x=CASE WHEN (?8&1)!=0 AND (locked=0 OR ?12>=0) THEN excluded.x ELSE x END,"
            "y=CASE WHEN (?8&1)!=0 AND (locked=0 OR ?12>=0) THEN excluded.y ELSE y END,"
            "width=CASE WHEN (?8&2)!=0 AND (locked=0 OR ?12>=0) THEN excluded.width ELSE width END,"
            "height=CASE WHEN (?8&2)!=0 AND (locked=0 OR ?12>=0) THEN excluded.height ELSE height END,"
            "view=CASE WHEN (?8&4)!=0 AND (locked=0 OR ?12>=0) THEN excluded.view ELSE view END,"
            "icons=CASE WHEN (?8&8)!=0 THEN excluded.icons ELSE icons END,"
            "maximized=CASE WHEN (?8&16)!=0 AND (locked=0 OR ?12>=0) THEN excluded.maximized ELSE maximized END,"
            "locked=CASE WHEN ?12>=0 THEN ?12 ELSE locked END", -1, &raw, nullptr)))
            return false;
        Statement statement(raw, sqlite3_finalize);
        if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))) return false;
        if (parts & FolderState::Position) {
            if (!result(sqlite3_bind_int(raw, 2, state.x)) || !result(sqlite3_bind_int(raw, 3, state.y))) return false;
        }
        if (parts & FolderState::Size) {
            if (!result(sqlite3_bind_int(raw, 4, state.width)) || !result(sqlite3_bind_int(raw, 5, state.height))) return false;
        }
        std::vector<uint8_t> view;
        if (parts & FolderState::View) {
            view = encodeView(state.view);
            if (!result(sqlite3_bind_blob64(raw, 6, view.data(), view.size(), SQLITE_TRANSIENT))) return false;
        }
        if (parts & FolderState::Icons) {
            // A zero-length BLOB represents a successfully enumerated empty folder, not NULL.
            if (!result(sqlite3_bind_blob64(raw, 7, state.icons.empty() ? "" :
                    static_cast<const void *>(state.icons.data()), state.icons.size(), SQLITE_TRANSIENT))) return false;
        }
        if (!result(sqlite3_bind_int(raw, 8, static_cast<int>(parts)))) return false;
        if (!result(sqlite3_bind_int64(raw, 9, state.lastSuccessAt))) return false;
        if (state.firstFailedAt && !result(sqlite3_bind_int64(raw, 10, state.firstFailedAt))) return false;
        if ((parts & FolderState::Maximized)
                && !result(sqlite3_bind_int(raw, 11, state.maximized ? 1 : 0))) return false;
        if (!result(sqlite3_bind_int(raw, 12, lockChange))) return false;
        // An existing row keeps its current access metadata, even if this snapshot is older.
        return result(sqlite3_step(raw));
    }();
    return finishWrite(success);
}


bool FolderStateStore::readViewScroll(const std::string &key, std::vector<uint8_t> *bytes) {
    if (!bytes) return false;
    bytes->clear();
    if (!beginRead()) return false;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "SELECT scroll FROM folder_state WHERE key=?1", -1, &raw, nullptr))) return false;
    Statement statement(raw, sqlite3_finalize);
    if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))) return false;
    const int step = sqlite3_step(raw);
    if (!result(step)) return false;
    if (step == SQLITE_ROW) {
        const int size = sqlite3_column_bytes(raw, 0);
        const auto data = static_cast<const uint8_t *>(sqlite3_column_blob(raw, 0));
        if (size && !data) { lastError = "Cannot read folder viewport"; return false; }
        if (size > 0) bytes->assign(data, data + size);
    }
    return true;
}

bool FolderStateStore::saveViewScroll(const std::string &key, const std::vector<uint8_t> &bytes) {
    if (!initialized) return false;
    if (key.empty() || bytes.empty()) return true;
    Lock lock(*this);
    if (!lock || !execute("BEGIN IMMEDIATE")) return false;
    const bool success = [&]() {
        sqlite3_stmt *raw = nullptr;
        if (!result(sqlite3_prepare_v2(db,
                "UPDATE folder_state SET scroll=?2 WHERE key=?1 AND scroll IS NOT ?2", -1, &raw, nullptr))) return false;
        Statement statement(raw, sqlite3_finalize);
        return result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_bind_blob64(raw, 2, bytes.data(), bytes.size(), SQLITE_TRANSIENT))
            && result(sqlite3_step(raw));
    }();
    return finishWrite(success);
}

bool FolderStateStore::recordAccess(const std::string &key, bool success, int64_t when, bool *expired) {
    if (expired) *expired = false;
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock) return false;
    if (key.empty()) return true;
    if (!execute("BEGIN IMMEDIATE")) return false;
    bool removed = false;
    bool written = [&]() {
        sqlite3_stmt *raw = nullptr;
        if (!success) {
            // Calendar anniversary in UTC. SQLite's ceiling maps February 29 to March 1.
            if (!result(sqlite3_prepare_v2(db,
                    "DELETE FROM folder_state WHERE key=?1 AND first_failed_at IS NOT NULL "
                    "AND ?2>=unixepoch(first_failed_at,'unixepoch','+1 year','ceiling')",
                    -1, &raw, nullptr))) return false;
            Statement statement(raw, sqlite3_finalize);
            if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))
                    || !result(sqlite3_bind_int64(raw, 2, when))
                    || !result(sqlite3_step(raw))) return false;
            removed = sqlite3_changes(db) != 0;
        }
        raw = nullptr;
        const char *sql = success
            ? "INSERT INTO folder_state(key,last_success_at) VALUES(?1,?2) "
              "ON CONFLICT(key) DO UPDATE SET last_success_at=?2,first_failed_at=NULL "
              "WHERE first_failed_at IS NOT NULL OR ?2>=last_success_at+?3"
            : "UPDATE folder_state SET first_failed_at=coalesce(first_failed_at,?2) WHERE key=?1";
        if (!result(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr))) return false;
        Statement statement(raw, sqlite3_finalize);
        return result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_bind_int64(raw, 2, when))
            && (!success || result(sqlite3_bind_int64(raw, 3, SUCCESS_REFRESH_SECONDS)))
            && result(sqlite3_step(raw));
    }();
    written = finishWrite(written);
    if (written && expired) *expired = removed;
    return written;
}

bool FolderStateStore::finishWrite(bool success) {
    if (success) success = enforceQuota();
    if (success) success = execute("COMMIT");
    if (!success) {
        const std::string error = lastError;
        const int code = lastErrorCode;
        const bool busy = sqliteBusy;
        execute("ROLLBACK");
        lastError = error;
        lastErrorCode = code;
        sqliteBusy = busy;
    }
    return success;
}

bool FolderStateStore::enforceQuota() {
    // Free whole pages are reusable; partly filled pages still count in full.
    constexpr int64_t limit = 100000000;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db,
            "SELECT (page_count-freelist_count)*page_size FROM "
            "pragma_page_count(),pragma_freelist_count(),pragma_page_size()",
            -1, &raw, nullptr))) return false;
    Statement usage(raw, sqlite3_finalize);
    auto withinLimit = [&]() {
        sqlite3_reset(usage.get());
        const int code = sqlite3_step(usage.get());
        if (!result(code) || code != SQLITE_ROW) return -1;
        const bool fits = sqlite3_column_int64(usage.get(), 0) <= limit;
        sqlite3_reset(usage.get()); // Do not hold a read cursor while deleting rows.
        return fits ? 1 : 0;
    };
    int fits = withinLimit();
    if (fits != 0) return fits == 1;

    // Discard old undo entries before evicting the normal spatial folder settings.
    while (fits == 0) {
        if (!execute("DELETE FROM closed_windows WHERE sequence="
                "(SELECT min(sequence) FROM closed_windows)")) return false;
        if (!sqlite3_changes(db)) break;
        fits = withinLimit();
    }
    if (fits != 0) return fits == 1;

    raw = nullptr;
    if (!result(sqlite3_prepare_v2(db,
            "SELECT key FROM folder_state ORDER BY last_success_at,key", -1, &raw, nullptr)))
        return false;
    std::vector<std::string> candidates;
    {
        Statement statement(raw, sqlite3_finalize);
        int code;
        while ((code = sqlite3_step(raw)) == SQLITE_ROW)
            candidates.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(raw, 0)));
        if (!result(code)) return false;
    }
#if defined(_WIN32)
    // Only Win32 window metadata is queried here: no COM, filesystem or network access.
    std::vector<HWND> windows;
    if (!EnumWindows([](HWND window, LPARAM data) -> BOOL {
            reinterpret_cast<std::vector<HWND> *>(data)->push_back(window);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&windows))) {
        lastError = "Could not enumerate windows for the folder-state quota";
        return false;
    }
#endif
    std::vector<std::string> open;
    raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "DELETE FROM folder_state WHERE key=?1", -1, &raw, nullptr)))
        return false;
    Statement deletion(raw, sqlite3_finalize);
    auto remove = [&](const std::string &key) {
        sqlite3_reset(deletion.get());
        return result(sqlite3_bind_text(deletion.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_step(deletion.get()));
    };
    for (const auto &key : candidates) {
        bool isOpen = false;
#if defined(_WIN32)
        const std::wstring property(key.begin(), key.end()); // Keys are ASCII digests.
        for (HWND window : windows) {
            if (GetPropW(window, property.c_str())) { isOpen = true; break; }
        }
#endif
        if (isOpen) { open.push_back(key); continue; }
        if (!remove(key)) return false;
        fits = withinLimit();
        if (fits != 0) return fits == 1;
    }
    // The quota takes priority over keeping open folders' rows in the database.
    for (const auto &key : open) {
        if (!remove(key)) return false;
        fits = withinLimit();
        if (fits != 0) return fits == 1;
    }
    lastError = "Folder-state schema exceeds the used-page quota";
    return false;
}

bool FolderStateStore::remove(const std::string &key) {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock) return false;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "DELETE FROM folder_state WHERE key=?1",
            -1, &raw, nullptr))) return false;
    Statement statement(raw, sqlite3_finalize);
    if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_STATIC))) return false;
    return result(sqlite3_step(raw));
}

bool FolderStateStore::clearAll() {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock || !execute("BEGIN IMMEDIATE")) return false;
    return finishWrite(execute("DELETE FROM folder_state; DELETE FROM closed_windows;"));
}

bool FolderStateStore::readClosedWindow(ClosedWindow *window) {
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db,
            "SELECT sequence,key,parsing_name,display_name,view_state FROM closed_windows "
            "ORDER BY sequence DESC LIMIT 1", -1, &raw, nullptr))) return false;
    Statement statement(raw, sqlite3_finalize);
    const int code = sqlite3_step(raw);
    if (!result(code)) return false;
    ClosedWindow value;
    if (code == SQLITE_ROW) {
        value.sequence = sqlite3_column_int64(raw, 0);
        value.key = reinterpret_cast<const char *>(sqlite3_column_text(raw, 1));
        value.parsingName = reinterpret_cast<const char *>(sqlite3_column_text(raw, 2));
        value.displayName = reinterpret_cast<const char *>(sqlite3_column_text(raw, 3));
        const int size = sqlite3_column_bytes(raw, 4);
        if (size > 0) {
            const auto *bytes = static_cast<const uint8_t *>(sqlite3_column_blob(raw, 4));
            value.viewState.assign(bytes, bytes + size);
        }
    }
    *window = std::move(value);
    return true;
}

bool FolderStateStore::lastClosedWindow(ClosedWindow *window) {
    if (!beginRead()) return false;
    return readClosedWindow(window);
}

bool FolderStateStore::takeClosedWindow(ClosedWindow *window) {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock || !execute("BEGIN IMMEDIATE")) return false;
    ClosedWindow value;
    bool success = readClosedWindow(&value);
    if (success && value.sequence) {
        sqlite3_stmt *raw = nullptr;
        success = result(sqlite3_prepare_v2(db,
            "DELETE FROM closed_windows WHERE sequence=?1", -1, &raw, nullptr));
        Statement statement(raw, sqlite3_finalize);
        if (success) success = result(sqlite3_bind_int64(raw, 1, value.sequence))
            && result(sqlite3_step(raw));
    }
    if (!finishWrite(success)) return false;
    *window = std::move(value);
    return true;
}

bool FolderStateStore::writeClosedWindow(const ClosedWindow &window, bool returning) {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock) return false;
    if (window.key.empty() || window.parsingName.empty()
            || window.viewState.size() > static_cast<size_t>(INT32_MAX)
            || !execute("BEGIN IMMEDIATE")) return false;
    bool success = [&]() {
        sqlite3_stmt *raw = nullptr;
        const char *sql = returning
            ? "INSERT OR IGNORE INTO closed_windows(sequence,key,parsing_name,display_name,view_state) "
              "VALUES(?5,?1,?2,?3,?4)"
            : "INSERT OR REPLACE INTO closed_windows(key,parsing_name,display_name,view_state) "
              "VALUES(?1,?2,?3,?4)";
        if (!result(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr))) return false;
        Statement statement(raw, sqlite3_finalize);
        if (!result(sqlite3_bind_text(raw, 1, window.key.c_str(), -1, SQLITE_TRANSIENT))
                || !result(sqlite3_bind_text(raw, 2, window.parsingName.c_str(), -1, SQLITE_TRANSIENT))
                || !result(sqlite3_bind_text(raw, 3, window.displayName.c_str(), -1, SQLITE_TRANSIENT)))
            return false;
        if (!window.viewState.empty() && !result(sqlite3_bind_blob(raw, 4,
                window.viewState.data(), static_cast<int>(window.viewState.size()), SQLITE_TRANSIENT)))
            return false;
        if (returning && !result(sqlite3_bind_int64(raw, 5, window.sequence))) return false;
        return result(sqlite3_step(raw)) && execute("DELETE FROM closed_windows WHERE sequence NOT IN "
            "(SELECT sequence FROM closed_windows ORDER BY sequence DESC LIMIT 20)");
    }();
    return finishWrite(success);
}

bool FolderStateStore::rememberClosedWindow(const ClosedWindow &window) {
    return writeClosedWindow(window, false);
}

bool FolderStateStore::returnClosedWindow(const ClosedWindow &window) {
    return window.sequence > 0 && writeClosedWindow(window, true);
}

bool FolderStateStore::move(const std::string &oldKey, const std::string &newKey) {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock) return false;
    if (oldKey == newKey) return true;
    if (!execute("BEGIN IMMEDIATE")) return false;
    sqlite3_stmt *raw = nullptr;
    bool success = result(sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO folder_state "
        "SELECT ?2,x,y,width,height,view,icons,last_success_at,first_failed_at,maximized,locked,scroll "
        "FROM folder_state WHERE key=?1", -1, &raw, nullptr));
    {
        Statement statement(raw, sqlite3_finalize);
        if (success) success = result(sqlite3_bind_text(raw, 1, oldKey.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_bind_text(raw, 2, newKey.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_step(raw));
    }
    // Move history before deleting the old state (whose trigger removes its history).
    for (const char *sql : {"DELETE FROM search_history WHERE key=?2",
            "UPDATE search_history SET key=?2 WHERE key=?1"}) {
        raw = nullptr;
        if (success) success = result(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr));
        Statement statement(raw, sqlite3_finalize);
        if (success && sqlite3_bind_parameter_index(raw, "?1"))
            success = result(sqlite3_bind_text(raw, 1, oldKey.c_str(), -1, SQLITE_TRANSIENT));
        if (success) success = result(sqlite3_bind_text(raw, 2, newKey.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_step(raw));
    }
    raw = nullptr;
    if (success) success = result(sqlite3_prepare_v2(db,
        "DELETE FROM folder_state WHERE key=?1", -1, &raw, nullptr));
    {
        Statement statement(raw, sqlite3_finalize);
        if (success) success = result(sqlite3_bind_text(raw, 1, oldKey.c_str(), -1, SQLITE_TRANSIENT))
            && result(sqlite3_step(raw));
    }
    return finishWrite(success);
}

bool FolderStateStore::readSearchHistory(const std::string &key, unsigned limit,
        std::vector<std::string> *queries) {
    if (!beginRead()) return false;
    sqlite3_stmt *raw = nullptr;
    if (!result(sqlite3_prepare_v2(db, "SELECT query FROM search_history WHERE key=?1 "
            "ORDER BY sequence DESC LIMIT ?2", -1, &raw, nullptr))) return false;
    Statement statement(raw, sqlite3_finalize);
    if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))
            || !result(sqlite3_bind_int(raw, 2, static_cast<int>((std::min)(limit, MAX_STORED_SEARCH_HISTORY)))))
        return false;
    std::vector<std::string> values;
    int code;
    while ((code = sqlite3_step(raw)) == SQLITE_ROW)
        values.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(raw, 0)));
    if (!result(code)) return false;
    *queries = std::move(values);
    return true;
}

bool FolderStateStore::saveSearchHistory(const std::string &key,
        const std::vector<std::string> &queries, unsigned limit, bool mergeRecent, bool *stateMissing) {
    if (stateMissing) *stateMissing = false;
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock || !execute("BEGIN IMMEDIATE")) return false;
    const bool success = [&]() {
        {
            // Never recreate a folder state removed by maintenance or a confirmed
            // deletion; a state never written yet keeps its queries pending.
            sqlite3_stmt *probe = nullptr;
            if (!result(sqlite3_prepare_v2(db, "SELECT 1 FROM folder_state WHERE key=?1", -1, &probe, nullptr)))
                return false;
            Statement statement(probe, sqlite3_finalize);
            if (!result(sqlite3_bind_text(probe, 1, key.c_str(), -1, SQLITE_TRANSIENT))) return false;
            const int code = sqlite3_step(probe);
            if (!result(code)) return false;
            if (code != SQLITE_ROW) {
                if (stateMissing) *stateMissing = true;
                return true;
            }
        }
        std::vector<std::string> merged;
        if (mergeRecent) {
            // Only explicit pending queries may precede the latest committed history.
            // Read under this write transaction so another client cannot be lost
            // between the read and replacement below.
            const unsigned maximum = (std::min)(limit, MAX_STORED_SEARCH_HISTORY);
            auto append = [&](const std::string &query) {
                if (!query.empty() && merged.size() < maximum
                        && std::find(merged.begin(), merged.end(), query) == merged.end())
                    merged.push_back(query);
            };
            for (const auto &query : queries) append(query);
            if (maximum) {
                std::vector<std::string> existing;
                if (!readSearchHistory(key, maximum, &existing)) return false;
                for (const auto &query : existing) append(query);
            }
        }
        const auto &values = mergeRecent ? merged : queries;
        sqlite3_stmt *raw = nullptr;
        if (!result(sqlite3_prepare_v2(db, "DELETE FROM search_history WHERE key=?1",
                -1, &raw, nullptr))) return false;
        {
            Statement statement(raw, sqlite3_finalize);
            if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))
                    || !result(sqlite3_step(raw))) return false;
        }
        raw = nullptr;
        if (!result(sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO search_history(key,query) "
                "SELECT ?1,?2 WHERE EXISTS(SELECT 1 FROM folder_state WHERE key=?1)",
                -1, &raw, nullptr))) return false;
        Statement statement(raw, sqlite3_finalize);
        const size_t count = (std::min)(values.size(),
            static_cast<size_t>((std::min)(limit, MAX_STORED_SEARCH_HISTORY)));
        for (size_t index = count; index > 0; --index) {
            if (values[index - 1].empty()) continue;
            sqlite3_reset(raw);
            if (!result(sqlite3_bind_text(raw, 1, key.c_str(), -1, SQLITE_TRANSIENT))
                    || !result(sqlite3_bind_text(raw, 2, values[index - 1].c_str(), -1, SQLITE_TRANSIENT))
                    || !result(sqlite3_step(raw))) return false;
        }
        return true;
    }();
    return finishWrite(success);
}

bool FolderStateStore::trimSearchHistory(unsigned limit) {
    if (!initialized) return false;
    Lock lock(*this);
    if (!lock || !execute("BEGIN IMMEDIATE")) return false;
    sqlite3_stmt *raw = nullptr;
    bool success = result(sqlite3_prepare_v2(db, "DELETE FROM search_history WHERE sequence NOT IN "
        "(SELECT recent.sequence FROM search_history recent WHERE recent.key=search_history.key "
        "ORDER BY recent.sequence DESC LIMIT ?1)", -1, &raw, nullptr));
    {
        Statement statement(raw, sqlite3_finalize);
        if (success) success = result(sqlite3_bind_int(raw, 1,
                static_cast<int>((std::min)(limit, MAX_STORED_SEARCH_HISTORY))))
            && result(sqlite3_step(raw));
    }
    return finishWrite(success);
}

#if defined(_WIN32) && !defined(FILESPACER_STORAGE_TEST)
static_assert(settings::MAX_SEARCH_HISTORY_LIMIT == MAX_STORED_SEARCH_HISTORY,
    "The search history preference limit must match the rows kept per folder");
static HRESULT resolveFolderStatePath(std::wstring &path) {
    path.clear();
    CComHeapPtr<wchar_t> local;
    const HRESULT found = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local);
    if (FAILED(found)) return found;
    std::wstring directory = std::wstring(local) + L"\\FileSpacer";
    path = directory + L"\\folder-state.sqlite3";
#ifdef FILESPACER_DEBUG
    if (settings::testMode) path = directory + L"\\folder-state-test.sqlite3";
#endif
    const int status = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    return status == ERROR_SUCCESS || status == ERROR_ALREADY_EXISTS || status == ERROR_FILE_EXISTS
        ? S_OK : HRESULT_FROM_WIN32(status);
}
std::string folderStateDatabasePath() {
    std::wstring path;
    if (FAILED(resolveFolderStatePath(path))) return {};
    return toUtf8(path.c_str());
}
namespace {
// The last failed UI initialization, kept only for its diagnosis: the mutex
// handle a waiting window duplicates, and SQLite's busy report.
std::unique_ptr<FolderStateStore> failedStore;
// UI-only diagnosis; background readers resolve their path into local variables.
std::wstring attemptedPath;
HRESULT pathError = S_OK;
std::unique_ptr<FolderStateStore> &currentStore() {
    static std::unique_ptr<FolderStateStore> store;
    return store;
}
}
FolderStateStore *folderStateStore(bool waitForInitialization) {
    // /test is parsed before the first folder creation. No worker uses this connection.
    auto &store = currentStore();
    static bool attempted = false;
    if (!store) {
        pathError = resolveFolderStatePath(attemptedPath);
        const std::string path = SUCCEEDED(pathError) ? toUtf8(attemptedPath.c_str()) : std::string();
        if (!path.empty() && !store) {
            {
                const bool wait = waitForInitialization || !attempted;
                attempted = true;
                std::unique_ptr<FolderStateStore> candidate(new FolderStateStore(path.c_str(), wait));
                if (candidate->ready() && !store) store = std::move(candidate);
                else if (!candidate->ready()) failedStore = std::move(candidate);
            }
        }
    }
    if (store) failedStore.reset();
    return store.get(); // Only a ready connection is retained; the next request retries a failure.
}
void *folderStateBlockingMutex() {
    return failedStore ? failedStore->blockingMutex() : nullptr;
}
bool folderStateBusy() {
    return failedStore && failedStore->busy();
}
namespace {
std::wstring storageErrorText(const std::string &utf8) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1, nullptr, 0);
    if (!count) return {};
    std::wstring text(static_cast<size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1, &text[0], count)) return {};
    text.pop_back();
    return text;
}
DWORD copyStorageDiagnosis(HWND owner, const std::wstring &diagnosis) {
    const size_t bytes = (diagnosis.size() + 1) * sizeof(wchar_t);
    std::unique_ptr<std::remove_pointer<HGLOBAL>::type, decltype(&GlobalFree)>
        memory(GlobalAlloc(GMEM_MOVEABLE, bytes), GlobalFree);
    if (!memory) return GetLastError();
    void *text = GlobalLock(memory.get());
    if (!text) return GetLastError();
    std::memcpy(text, diagnosis.c_str(), bytes);
    GlobalUnlock(memory.get());
    if (!OpenClipboard(owner)) return GetLastError();
    DWORD error = ERROR_SUCCESS;
    if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory.get())) error = GetLastError();
    else memory.release(); // Clipboard owns the allocation only after successful transfer.
    CloseClipboard();
    return error;
}
DWORD openStorageDirectory(HWND owner, const std::wstring &directory) {
    if (directory.empty()) return ERROR_PATH_NOT_FOUND;
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
    wchar_t windows[MAX_PATH];
    const UINT length = GetSystemWindowsDirectoryW(windows, ARRAYSIZE(windows));
    if (!length) return GetLastError();
    if (length >= ARRAYSIZE(windows)) return ERROR_INSUFFICIENT_BUFFER;
    const std::wstring explorer = std::wstring(windows, length) + L"\\explorer.exe";
    const auto arguments = quoteCommandArgument(directory.c_str());
    SHELLEXECUTEINFOW info = {sizeof(info)};
    info.hwnd = owner;
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpFile = explorer.c_str(); // Never recurse through FileSpacer's default folder association.
    info.lpParameters = arguments.c_str();
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) ? ERROR_SUCCESS : GetLastError();
}
struct StorageErrorDialog {
    std::wstring path, directory, diagnosis;
    bool usersLoaded = false;
};
void loadStorageUsers(StorageErrorDialog &dialog) {
    if (dialog.usersLoaded) return;
    dialog.usersLoaded = true;
    DWORD session = 0;
    wchar_t key[CCH_RM_SESSION_KEY + 1] = {};
    std::wstring users;
    DWORD error = RmStartSession(&session, 0, key);
    if (error == ERROR_SUCCESS) {
        auto endSession = [](DWORD *handle) { RmEndSession(*handle); };
        const std::unique_ptr<DWORD, decltype(endSession)> guard(&session, endSession);
        std::wstring paths[] = {dialog.path, dialog.path + L"-wal", dialog.path + L"-shm", dialog.path + L"-journal"};
        LPCWSTR files[] = {paths[0].c_str(), paths[1].c_str(), paths[2].c_str(), paths[3].c_str()};
        error = dialog.path.empty() ? ERROR_PATH_NOT_FOUND
            : RmRegisterResources(session, ARRAYSIZE(files), files, 0, nullptr, 0, nullptr);
        if (error == ERROR_SUCCESS) {
            UINT required = 0, count = 0;
            DWORD reboot = 0;
            error = RmGetList(session, &required, &count, nullptr, &reboot);
            // A changing process list can require one resize; never spin waiting for it.
            for (int attempt = 0; error == ERROR_MORE_DATA && attempt < 2; ++attempt) {
                std::vector<RM_PROCESS_INFO> processes(required);
                count = required;
                error = RmGetList(session, &required, &count, processes.data(), &reboot);
                if (error == ERROR_SUCCESS) {
                    for (UINT index = 0; index < count; ++index)
                        users += L"\n" + std::wstring(processes[index].strAppName) + L" — PID "
                            + std::to_wstring(processes[index].Process.dwProcessId);
                }
            }
        }
    }
    dialog.diagnosis += L"\n\n";
    dialog.diagnosis += getString(users.empty() ? IDS_STORAGE_USERS_UNAVAILABLE : IDS_STORAGE_USERS);
    dialog.diagnosis += users;
    if (error != ERROR_SUCCESS) dialog.diagnosis += L"\nRestart Manager: " + std::to_wstring(error);
}
HRESULT CALLBACK storageErrorCallback(HWND window, UINT notification, WPARAM button, LPARAM, LONG_PTR data) {
    auto &dialog = *reinterpret_cast<StorageErrorDialog *>(data);
    if (notification == TDN_EXPANDO_BUTTON_CLICKED && button) {
        loadStorageUsers(dialog);
        SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_EXPANDED_INFORMATION,
            reinterpret_cast<LPARAM>(dialog.diagnosis.c_str()));
        return S_OK;
    }
    if (notification == TDN_BUTTON_CLICKED && button == IDC_STORAGE_RESET) {
        int confirmed = IDNO;
        TASKDIALOGCONFIG confirmation = {sizeof(confirmation)};
        confirmation.hwndParent = window;
        confirmation.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        confirmation.pszWindowTitle = getString(IDS_CONFIRM_CAPTION);
        confirmation.pszMainInstruction = getString(IDS_STORAGE_RESET);
        confirmation.pszContent = getString(IDS_STORAGE_RESET_CONFIRM);
        confirmation.pszMainIcon = TD_WARNING_ICON;
        confirmation.dwCommonButtons = TDCBF_NO_BUTTON | TDCBF_YES_BUTTON;
        confirmation.nDefaultButton = IDNO;
        if (FAILED(TaskDialogIndirect(&confirmation, &confirmed, nullptr, nullptr)) || confirmed != IDYES)
            return S_FALSE;
        // Confirmation pumps messages. Recheck the current path and retry native
        // recovery before deciding whether any destructive operation is still needed.
        if (folderStateStore()) {
            SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_FOOTER,
                reinterpret_cast<LPARAM>(getString(IDS_STORAGE_RECOVERED)));
            SendMessageW(window, TDM_ENABLE_BUTTON, IDC_STORAGE_RESET, FALSE);
            return S_FALSE;
        }
        GUID guid;
        wchar_t id[40];
        const bool named = SUCCEEDED(CoCreateGuid(&guid)) && StringFromGUID2(guid, id, ARRAYSIZE(id));
        const std::wstring backup = named ? dialog.path + L".backup-" + id : std::wstring();
        if (!named || dialog.path != attemptedPath || !failedStore || !failedStore->canResetDamagedDatabase()
                || !failedStore->resetDamagedDatabase(toUtf8(dialog.path.c_str()).c_str(), backup.c_str())) {
            const std::wstring reason = failedStore ? storageErrorText(failedStore->error())
                : std::wstring(getString(IDS_STORAGE_UNKNOWN_ERROR));
            const auto message = formatString(IDS_STORAGE_RESET_FAILED, reason.c_str());
            SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, reinterpret_cast<LPARAM>(message.get()));
            return S_FALSE;
        }
        const auto message = formatString(IDS_STORAGE_RESET_DONE, backup.c_str());
        TaskDialog(window, nullptr, getString(IDS_APP_NAME), getString(IDS_STORAGE_RESET),
            message.get(), TDCBF_OK_BUTTON, TD_INFORMATION_ICON, nullptr);
        return S_OK;
    }
    if (notification != TDN_BUTTON_CLICKED
            || (button != IDC_STORAGE_OPEN_DATA && button != IDC_STORAGE_COPY_DIAGNOSIS)) return S_OK;
    if (button == IDC_STORAGE_COPY_DIAGNOSIS) loadStorageUsers(dialog);
    const DWORD error = button == IDC_STORAGE_OPEN_DATA
        ? openStorageDirectory(window, dialog.directory) : copyStorageDiagnosis(window, dialog.diagnosis);
    if (error) {
        const auto message = formatString(IDS_STORAGE_ACTION_ERROR, error, getErrorMessage(error).get());
        SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, reinterpret_cast<LPARAM>(message.get()));
    } else {
        SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_FOOTER,
            reinterpret_cast<LPARAM>(getString(button == IDC_STORAGE_COPY_DIAGNOSIS
                ? IDS_STORAGE_DIAGNOSIS_COPIED : IDS_STORAGE_DATA_OPENED)));
    }
    return S_FALSE; // Auxiliary actions leave the original error and its owned snapshot available.
}
}
void reportFolderStateError(void *owner) {
    // Snapshot before any modal message loop can retry initialization or change the last error.
    const auto store = currentStore() ? currentStore().get() : failedStore.get();
    std::wstring cause = store ? storageErrorText(store->error()) : std::wstring();
    const int code = store ? store->errorCode() : 0;
    if (FAILED(pathError))
        cause = format(L"HRESULT 0x%1!08lX!: %2!s!", static_cast<unsigned long>(pathError),
            getErrorMessage(HRESULT_FACILITY(pathError) == FACILITY_WIN32
                ? HRESULT_CODE(pathError) : static_cast<DWORD>(pathError)).get()).get();
    if (cause.empty()) cause = getString(IDS_STORAGE_UNKNOWN_ERROR);
    std::vector<wchar_t> executable(32768);
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    const std::wstring module = length && length < executable.size()
        ? std::wstring(executable.data(), length) : std::wstring();
    StorageErrorDialog dialog;
    dialog.path = attemptedPath;
    const size_t slash = attemptedPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dialog.directory = attemptedPath.substr(0, slash);
    const auto details = formatString(IDS_STORAGE_DIAGNOSIS, TEXT(FILESPACER_VERSION_STRING), module.c_str(),
        attemptedPath.c_str(), storageErrorText(sqlite3_libversion()).c_str(), code, cause.c_str());
    dialog.diagnosis = std::wstring(details.get()) + L"\n\n" + getString(IDS_STORAGE_RECOVERY_HELP);
    const std::wstring initialDetails = dialog.diagnosis;
    TASKDIALOG_BUTTON buttons[] = {
        {IDC_STORAGE_OPEN_DATA, getString(IDS_STORAGE_OPEN_DATA)},
        {IDC_STORAGE_COPY_DIAGNOSIS, getString(IDS_STORAGE_COPY_DIAGNOSIS)},
        {IDC_STORAGE_RESET, getString(IDS_STORAGE_RESET)}
    };
    TASKDIALOGCONFIG config = {sizeof(config)};
    config.hwndParent = static_cast<HWND>(owner);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = getString(IDS_ERROR_CAPTION);
    config.pszMainIcon = TD_ERROR_ICON;
    config.pszMainInstruction = cause.c_str();
    config.pszContent = getString(IDS_FOLDER_STATE_ERROR);
    config.cButtons = SUCCEEDED(pathError) && store && store->canResetDamagedDatabase()
        ? ARRAYSIZE(buttons) : ARRAYSIZE(buttons) - 1;
    config.pButtons = buttons;
    config.nDefaultButton = IDOK;
    config.pszExpandedControlText = getString(IDS_STORAGE_DETAILS);
    config.pszExpandedInformation = initialDetails.c_str();
    config.pszFooter = L" "; // Reserve the footer for action results.
    config.pfCallback = storageErrorCallback;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(&dialog);
    if (FAILED(TaskDialogIndirect(&config, nullptr, nullptr, nullptr)))
        MessageBoxW(static_cast<HWND>(owner), dialog.diagnosis.c_str(),
            getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
}
#endif

} // namespace

