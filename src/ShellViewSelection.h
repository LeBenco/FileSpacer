#pragma once
#include <common.h>
#include <shobjidl.h>
#include <cstdint>
#include <functional>
#include <vector>

namespace filespacer {
// current is synchronous and must reject a closed or replaced view. The
// existing version-1 reopen blob stays readable, including its legacy anchor.
HRESULT captureReopenSelection(IShellView *view, const std::function<bool()> &current,
    std::vector<uint8_t> &bytes);
// Consumes pending before calling the Shell, including on failure/cancellation.
// Empty input returns S_FALSE without changing the current selection.
HRESULT restoreReopenSelection(IShellView *view, std::vector<uint8_t> &pending,
    const std::function<bool()> &current);
} // namespace filespacer
