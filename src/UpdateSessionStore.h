#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace filespacer {

// A temporary database, independent of folder state and closed-window history.
// Paths use UTF-8 Shell parsing names. Functions return SQLite result codes.
int createUpdateSession(const char *filename);
int saveUpdateSession(const char *filename, uint32_t process,
    const std::vector<std::string> &paths);
int readUpdateSession(const char *filename, std::vector<std::string> *paths);

} // namespace filespacer
