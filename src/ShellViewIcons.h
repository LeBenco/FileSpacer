#pragma once
#include <common.h>
#include <shobjidl.h>
#include <cstdint>
#include <functional>
#include <vector>

namespace filespacer {
// Existing format: Shell-serialized child PIDL followed by a 96-DPI POINT.
// A filtered view returns S_FALSE with empty output; callers must not save it.
// Caller must exclude auto-arranged views from persistent captures.
// Incomplete enumeration preserves previously saved items not yet visible.
HRESULT captureIconPositions(IFolderView *view, const std::function<bool()> &current,
    bool enumerationComplete, const std::vector<uint8_t> &previous, std::vector<uint8_t> &bytes,
    bool filtered = false);
// Decode the entire blob before moving anything; preserve selection and focus.
HRESULT restoreIconPositions(IFolderView *view, const std::vector<uint8_t> &bytes,
    const std::function<bool()> &current);
} // namespace filespacer
