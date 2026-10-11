#include "UpdateSessionStore.h"
#include <sqlite3.h>
#include <memory>
#include <utility>

namespace filespacer {
namespace {
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
constexpr size_t MAX_WINDOWS = 4096;
constexpr size_t MAX_PATH_BYTES = 131072;
constexpr size_t MAX_SESSION_BYTES = 16ULL * 1024 * 1024;

int openDatabase(const char *filename, int flags, Database &database) {
    sqlite3 *raw = nullptr;
    const int error = sqlite3_open_v2(filename, &raw, flags, nullptr);
    database.reset(raw);
    if (!error) sqlite3_busy_timeout(raw, 1000);
    return error;
}

bool validPath(const std::string &path) {
    return !path.empty() && path.size() <= MAX_PATH_BYTES
        && path.find('\0') == std::string::npos;
}
} // namespace

int createUpdateSession(const char *filename) {
    Database database(nullptr, sqlite3_close);
    const int error = openDatabase(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, database);
    if (error) return error;
    return sqlite3_exec(database.get(),
        "CREATE TABLE windows(process INTEGER NOT NULL, position INTEGER NOT NULL,"
        " path TEXT NOT NULL, PRIMARY KEY(process, position))", nullptr, nullptr, nullptr);
}

int saveUpdateSession(const char *filename, uint32_t process,
        const std::vector<std::string> &paths) {
    if (paths.size() > MAX_WINDOWS) return SQLITE_TOOBIG;
    size_t bytes = 0;
    for (const auto &path : paths) {
        if (!validPath(path)) return SQLITE_MISMATCH;
        bytes += path.size();
        if (bytes > MAX_SESSION_BYTES) return SQLITE_TOOBIG;
    }
    Database database(nullptr, sqlite3_close);
    int error = openDatabase(filename, SQLITE_OPEN_READWRITE, database);
    if (error) return error;
    error = sqlite3_exec(database.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    if (error) return error;
    sqlite3_stmt *raw = nullptr;
    error = sqlite3_prepare_v2(database.get(),
        "SELECT count(*), coalesce(sum(length(CAST(path AS BLOB))),0) FROM windows WHERE process!=?",
        -1, &raw, nullptr);
    Statement statement(raw, sqlite3_finalize);
    if (!error) error = sqlite3_bind_int64(statement.get(), 1, process);
    if (!error) {
        error = sqlite3_step(statement.get());
        if (error == SQLITE_ROW) {
            error = sqlite3_column_int64(statement.get(), 0) + static_cast<sqlite3_int64>(paths.size())
                    > static_cast<sqlite3_int64>(MAX_WINDOWS)
                || sqlite3_column_int64(statement.get(), 1) + static_cast<sqlite3_int64>(bytes)
                    > static_cast<sqlite3_int64>(MAX_SESSION_BYTES) ? SQLITE_TOOBIG : SQLITE_OK;
        }
    }
    statement.reset();
    if (!error) {
        raw = nullptr;
        error = sqlite3_prepare_v2(database.get(), "DELETE FROM windows WHERE process=?", -1, &raw, nullptr);
        statement.reset(raw);
    }
    if (!error) error = sqlite3_bind_int64(statement.get(), 1, process);
    if (!error) {
        error = sqlite3_step(statement.get());
        if (error == SQLITE_DONE) error = SQLITE_OK;
    }
    statement.reset();
    if (!error) {
        raw = nullptr;
        error = sqlite3_prepare_v2(database.get(), "INSERT INTO windows VALUES(?,?,?)", -1, &raw, nullptr);
        statement.reset(raw);
    }
    for (size_t index = 0; !error && index < paths.size(); ++index) {
        error = sqlite3_bind_int64(statement.get(), 1, process);
        if (!error) error = sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(index));
        if (!error) error = sqlite3_bind_text(statement.get(), 3, paths[index].data(),
            static_cast<int>(paths[index].size()), SQLITE_TRANSIENT);
        if (!error) {
            error = sqlite3_step(statement.get());
            if (error == SQLITE_DONE) error = sqlite3_reset(statement.get());
        }
    }
    statement.reset();
    if (!error) error = sqlite3_exec(database.get(), "COMMIT", nullptr, nullptr, nullptr);
    // Closing a failed transaction rolls it back; a previous process snapshot survives.
    return error;
}

int readUpdateSession(const char *filename, std::vector<std::string> *paths) {
    Database database(nullptr, sqlite3_close);
    int error = openDatabase(filename, SQLITE_OPEN_READONLY, database);
    if (error) return error;
    sqlite3_stmt *raw = nullptr;
    error = sqlite3_prepare_v2(database.get(), "SELECT path FROM windows ORDER BY rowid", -1, &raw, nullptr);
    Statement statement(raw, sqlite3_finalize);
    if (error) return error;
    std::vector<std::string> result;
    size_t bytes = 0;
    while ((error = sqlite3_step(statement.get())) == SQLITE_ROW) {
        const int length = sqlite3_column_bytes(statement.get(), 0);
        if (sqlite3_column_type(statement.get(), 0) != SQLITE_TEXT || length <= 0
                || static_cast<size_t>(length) > MAX_PATH_BYTES) return SQLITE_MISMATCH;
        bytes += static_cast<size_t>(length);
        if (result.size() >= MAX_WINDOWS || bytes > MAX_SESSION_BYTES) return SQLITE_TOOBIG;
        const auto text = reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), 0));
        if (!text) return SQLITE_NOMEM;
        std::string path(text, static_cast<size_t>(length));
        if (!validPath(path)) return SQLITE_MISMATCH;
        result.push_back(std::move(path));
    }
    if (error != SQLITE_DONE) return error;
    *paths = std::move(result); // Leave the caller's data intact on failure.
    return SQLITE_OK;
}

} // namespace filespacer
