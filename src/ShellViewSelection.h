#pragma once
#include <common.h>
#include <shobjidl.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace filespacer {
// Nonblocking startup. Public operations are available immediately; a qualified
// private capability may be published once without changing an in-flight call.
void initializeShellSelectionClear(const wchar_t *cacheRoot = nullptr, bool background = true);

// Bind once when a new view's initial presentation is ready. An unbound or
// unsupported view uses the public API. A call retains its binding through
// reentrant callbacks; reset detaches before releasing the COM references.
// A qualified classic view also handles native global-selection notifications
// in linear time; ordinary selection and unsupported contexts remain native.
class ShellSelectionClear {
public:
    void bind(IShellView *view, const std::function<bool()> &current);
    void reset();
    bool accelerated() const;
    uint64_t acceleratedSelectAllCount() const;
    bool backingBufferReleased(bool &known) const;
    HRESULT clear(IShellView *view, const std::function<bool()> &current,
        bool *applied = nullptr) const;
private:
    struct Binding;
    std::shared_ptr<Binding> binding;
};

// current is synchronous and must reject a closed or replaced view. The
// existing version-1 reopen blob stays readable, including its legacy anchor.
HRESULT captureReopenSelection(IShellView *view, const std::function<bool()> &current,
    std::vector<uint8_t> &bytes);
// Consumes pending before calling the Shell, including on failure/cancellation.
// Empty input returns S_FALSE without changing the current selection.
HRESULT restoreReopenSelection(IShellView *view, std::vector<uint8_t> &pending,
    const std::function<bool()> &current, const ShellSelectionClear *clear = nullptr);
} // namespace filespacer
