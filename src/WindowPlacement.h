#pragma once
#include <common.h>

#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace filespacer {

// Ten screen-coordinate pixels per cell, independent of saved folder geometry.
class WindowPlacementGrid {
public:
    explicit WindowPlacementGrid(RECT workArea);
    WindowPlacementGrid(const WindowPlacementGrid &) = delete;
    WindowPlacementGrid &operator=(const WindowPlacementGrid &) = delete;
    const RECT &workArea() const { return work; }
    void setWindow(HWND window, const RECT *rect);
    void beginRefresh();
    void endRefresh();
    POINT findPosition(SIZE size, POINT preferred);
    uint64_t occupiedArea(const RECT &rect);

private:
    static constexpr LONG cellSize = 10;
    struct Entry { RECT cells; uint64_t generation; };
    RECT work;
    LONG columns, rows;
    uint64_t generation = 0;
    bool dirty = true;
    std::unordered_map<HWND, Entry> windows;
    std::vector<uint32_t> counts;
    std::vector<uint64_t> sums;
    RECT toCells(const RECT &rect) const;
    void change(const RECT &cells, bool add);
    void changeDifference(const RECT &rect, const RECT &other, bool add);
    void prepareSums();
};

// Local notifications update existing grids. A placement also reconciles windows
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
