#pragma once
#include <common.h>

#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace filespacer {

// Keep exact monitor-clipped geometry; build query tables only when placing.
class WindowPlacementState {
public:
    explicit WindowPlacementState(RECT workArea);
    WindowPlacementState(const WindowPlacementState &) = delete;
    WindowPlacementState &operator=(const WindowPlacementState &) = delete;
    const RECT &workArea() const { return work; }
    void setWindow(HWND window, const RECT *rect);
    void beginRefresh();
    void endRefresh();
    POINT findPosition(SIZE size, POINT preferred) const;

private:
    struct Entry { RECT bounds; uint64_t generation; };
    RECT work;
    uint64_t generation = 0;
    std::unordered_map<HWND, Entry> windows;
};

// Local notifications update stored geometry. A placement also reconciles windows
// in other processes, including their provisional replacement withdrawals.
void updateWindowPlacement(HWND window) noexcept;
void clearWindowPlacement();
void removeWindowPlacement(HWND window);
void cancelPlacementWithdrawal(HWND window);
void initializeWindowPlacement(HWND window);
void destroyWindowPlacement(HWND window);
bool findWindowPlacement(HMONITOR monitor, SIZE size, HWND source, RECT &rect);

class PlacementWithdrawal {
public:
    explicit PlacementWithdrawal(HWND source);
    ~PlacementWithdrawal();
    PlacementWithdrawal(const PlacementWithdrawal &) = delete;
    PlacementWithdrawal &operator=(const PlacementWithdrawal &) = delete;
    void commit() { committed = true; }
private:
    HWND window = nullptr;
    bool committed = false;
};

} // namespace
