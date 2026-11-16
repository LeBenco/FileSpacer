#pragma once
#include <common.h>
#include <shobjidl.h>
#include "ShellViewOperations.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace filespacer {
// current is synchronous and must reject a closed or replaced view. The
// existing version-1/2 reopen blobs stay readable. New snapshots use the shared
// viewport capture, with the legacy anchor fallback on unsupported renderers.
HRESULT captureReopenSelection(IShellView *view, const std::function<bool()> &current,
    std::vector<uint8_t> &bytes);
// Public snapshot of large selections in ungrouped free Icon/SmallIcon views.
// S_FALSE leaves unsupported contexts to GetSelection; E_ABORT rejects a stale view.
HRESULT captureSpatialSelection(IFolderView2 *view, const std::function<bool()> &current,
    IShellItemArray **out);
// Consumes pending before calling the Shell, including on failure/cancellation.
// Empty input returns S_FALSE without changing the current selection.
HRESULT restoreReopenSelection(IShellView *view, std::vector<uint8_t> &pending,
    const std::function<bool()> &current, const ShellSelectionClear *clear = nullptr);

// A small viewport-only snapshot: no selection, focus or item enumeration.
// S_FALSE leaves unavailable capabilities and mismatched presentations native.
HRESULT captureViewScroll(IShellView *view, const std::function<bool()> &current,
    std::vector<uint8_t> &bytes);
HRESULT restoreViewScroll(IShellView *view, const std::vector<uint8_t> &bytes,
    const std::function<bool()> &current);
} // namespace filespacer
