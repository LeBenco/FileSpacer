#pragma once
#include <common.h>
#include <shobjidl.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace filespacer {
// Item count from which the accelerated selection, clearing, capture and
// restoration paths apply. Below it the ordinary Shell calls are used alone.
constexpr int SELECTION_BULK_THRESHOLD = 128;

// Nonblocking startup. Public operations are available immediately; a qualified
// private capability may be published once without changing an in-flight call.
void initializeShellSelectionClear(const wchar_t *cacheRoot = nullptr, bool background = true);
// Same thread as the initialization, before the process returns from its entry
// point: stops the background resolution and waits until its thread has exited.
// The resolution observes the request in each of its waits. No effect when none
// was started.
void stopShellSelectionClear();

// Bind once when a new view's initial presentation is ready. An unbound or
// unsupported view uses the public API. A call retains its binding through
// reentrant callbacks; reset detaches before releasing the COM references.
// A qualified classic view also handles native global-selection notifications
// in linear time; ordinary selection and unsupported contexts remain native.
class ShellSelectionClear {
public:
    void bind(IShellView *view, const std::function<bool()> &current);
    void reset();
    // Attempt a native complete-set selection. S_FALSE leaves unsupported
    // views to the caller's per-item restoration; backend choice is internal.
    HRESULT selectAll(IShellView *view, const std::function<bool()> &current) const;
    // Qualification/diagnostic observations only; not operation dispatch.
    bool accelerated() const;
    bool acceleratesSelectAll() const;
    uint64_t acceleratedSelectAllCount() const;
    bool backingBufferReleased(bool &known) const;
    // E_PENDING: called from within a global selection or deselection that
    // this binding is handling. Nothing is cleared and nothing is deferred.
    HRESULT clear(IShellView *view, const std::function<bool()> &current,
        bool *applied = nullptr) const;
private:
    struct Binding;
    std::shared_ptr<Binding> binding;
};

// The UI observes readiness without depending on discovery/cache internals.
UINT shellViewOperationsReadyMessage();
int runShellViewOperationsWorker(const wchar_t *cacheRoot = nullptr);

// Complete grid operation: an optional qualified calculation and the public
// native fallback have the same selection/focus/flag preservation contract.
HRESULT alignIconsToGrid(IShellView *view, bool selectedOnly,
    const std::function<bool()> &current);
// Read-only rounding shared by the drag prediction and grid operation.
bool alignIconToGrid(POINT &point, const POINT &spacing, int size);
} // namespace filespacer
