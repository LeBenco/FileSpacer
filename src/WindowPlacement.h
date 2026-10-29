#pragma once
#include <common.h>

#include <windows.h>
#include <vector>

namespace filespacer {

// Exact placement over the windows of one work area: a free origin found by
// sweep, else the origin with the least overlap. Pure over its inputs; only the
// part of each window inside the work area counts.
POINT findPlacementPosition(RECT work, SIZE size, POINT preferred, const std::vector<RECT> &windows);

// A placement snapshots the visible item windows of every process at the time
// of the call. Two window properties survive between placements: the provisional
// withdrawal of a replaced source and the test-mode marker.
void cancelPlacementWithdrawal(HWND window);
void initializeWindowPlacement(HWND window);
void removeWindowPlacement(HWND window);
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
