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

constexpr LONG WindowPlacementGrid::cellSize;

WindowPlacementGrid::WindowPlacementGrid(RECT workArea) : work(workArea),
        columns((work.right - work.left + cellSize - 1) / cellSize),
        rows((work.bottom - work.top + cellSize - 1) / cellSize),
        counts(static_cast<size_t>(columns) * rows),
        sums((static_cast<size_t>(columns) + 1) * (rows + 1)) {}

RECT WindowPlacementGrid::toCells(const RECT &rect) const {
    const LONG left = (std::max)(rect.left, work.left);
    const LONG top = (std::max)(rect.top, work.top);
    const LONG right = (std::min)(rect.right, work.right);
    const LONG bottom = (std::min)(rect.bottom, work.bottom);
    if (left >= right || top >= bottom) return {};
    return {(left - work.left) / cellSize, (top - work.top) / cellSize,
        (right - work.left + cellSize - 1) / cellSize,
        (bottom - work.top + cellSize - 1) / cellSize};
}

void WindowPlacementGrid::change(const RECT &cells, bool add) {
    for (LONG y = cells.top; y < cells.bottom; ++y) {
        for (LONG x = cells.left; x < cells.right; ++x) {
            auto &count = counts[static_cast<size_t>(y) * columns + x];
            const bool wasOccupied = count != 0;
            if (add) ++count;
            else { assert(count != 0); --count; }
            if (wasOccupied != (count != 0)) dirty = true;
        }
    }
}

void WindowPlacementGrid::changeDifference(const RECT &rect, const RECT &other, bool add) {
    RECT intersection = {};
    if (!IntersectRect(&intersection, &rect, &other)) {
        change(rect, add);
        return;
    }
    change({rect.left, rect.top, rect.right, intersection.top}, add);
    change({rect.left, intersection.bottom, rect.right, rect.bottom}, add);
    change({rect.left, intersection.top, intersection.left, intersection.bottom}, add);
    change({intersection.right, intersection.top, rect.right, intersection.bottom}, add);
}

void WindowPlacementGrid::setWindow(HWND window, const RECT *rect) {
    const RECT next = rect ? toCells(*rect) : RECT{};
    auto found = windows.find(window);
    if (found != windows.end()) {
        const RECT previous = found->second.cells;
        found->second.generation = generation;
        if (EqualRect(&previous, &next)) return;
        changeDifference(previous, next, false);
        changeDifference(next, previous, true);
        if (IsRectEmpty(&next)) windows.erase(found);
        else found->second.cells = next;
    } else if (!IsRectEmpty(&next)) {
        // Allocate the entry before changing counters, so allocation failure
        // cannot leave an unregistered contribution behind.
        windows.emplace(window, Entry{next, generation});
        change(next, true);
    }
}

void WindowPlacementGrid::beginRefresh() { ++generation; }

void WindowPlacementGrid::endRefresh() {
    for (auto entry = windows.begin(); entry != windows.end();) {
        if (entry->second.generation != generation) {
            change(entry->second.cells, false);
            entry = windows.erase(entry);
        } else ++entry;
    }
}

void WindowPlacementGrid::prepareSums() {
    if (!dirty) return;
    const size_t stride = static_cast<size_t>(columns) + 1;
    for (LONG y = 0; y < rows; ++y) {
        uint64_t occupied = 0;
        for (LONG x = 0; x < columns; ++x) {
            if (counts[static_cast<size_t>(y) * columns + x]) ++occupied;
            sums[(static_cast<size_t>(y) + 1) * stride + x + 1]
                = sums[static_cast<size_t>(y) * stride + x + 1] + occupied;
        }
    }
    dirty = false;
}

uint64_t WindowPlacementGrid::occupiedArea(const RECT &rect) {
    prepareSums();
    const size_t stride = static_cast<size_t>(columns) + 1;
    const auto at = [&](LONG x, LONG y) { return sums[static_cast<size_t>(y) * stride + x]; };
    const auto integral = [&](LONG screenX, LONG screenY) {
        const LONG x = (std::max)(work.left, (std::min)(screenX, work.right)) - work.left;
        const LONG y = (std::max)(work.top, (std::min)(screenY, work.bottom)) - work.top;
        const LONG column = x / cellSize, row = y / cellSize;
        const LONG dx = x % cellSize, dy = y % cellSize;
        uint64_t area = at(column, row) * cellSize * cellSize;
        if (dx) area += (at(column + 1, row) - at(column, row)) * cellSize * dx;
        if (dy) area += (at(column, row + 1) - at(column, row)) * cellSize * dy;
        if (dx && dy && counts[static_cast<size_t>(row) * columns + column])
            area += static_cast<uint64_t>(dx) * dy;
        return area;
    };
    if (IsRectEmpty(&rect)) return 0;
    return integral(rect.right, rect.bottom) - integral(rect.left, rect.bottom)
        - integral(rect.right, rect.top) + integral(rect.left, rect.top);
}

POINT WindowPlacementGrid::findPosition(SIZE size, POINT preferred) {
    // Preserve the requested size even when it exceeds the work area. Its
    // upper-left corner remains accessible and the other axis can still fit.
    const LONG maxX = (std::max)(work.left, work.right - size.cx);
    const LONG maxY = (std::max)(work.top, work.bottom - size.cy);
    preferred.x = (std::max)(work.left, (std::min)(preferred.x, maxX));
    preferred.y = (std::max)(work.top, (std::min)(preferred.y, maxY));
    POINT best = preferred;
    uint64_t bestArea = occupiedArea({best.x, best.y, best.x + size.cx, best.y + size.cy});
    uint64_t bestDistance = 0;
    if (!bestArea) return best;
    for (LONG y = work.top;; y = (std::min)(y + cellSize, maxY)) {
        for (LONG x = work.left;; x = (std::min)(x + cellSize, maxX)) {
            const uint64_t area = occupiedArea({x, y, x + size.cx, y + size.cy});
            const int64_t dx = static_cast<int64_t>(x) - preferred.x;
            const int64_t dy = static_cast<int64_t>(y) - preferred.y;
            const uint64_t distance = static_cast<uint64_t>(dx * dx + dy * dy);
            if (area < bestArea || (area == bestArea && distance < bestDistance)) {
                best = {x, y}; bestArea = area; bestDistance = distance;
            }
            if (x == maxX) break;
        }
        if (y == maxY) break;
    }
    return best;
}
