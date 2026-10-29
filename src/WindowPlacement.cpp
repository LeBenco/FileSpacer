#include <common.h>
#include "WindowPlacement.h"
#include "Settings.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <dwmapi.h>

#pragma comment(lib, "Dwmapi.lib")

namespace filespacer {
namespace {
using Area = uint64_t;
struct PlacementQuery {
    RECT work;
    SIZE size;
    POINT preferred;
    std::vector<RECT> windows;
};
struct PlacementChoice { POINT point; Area area; Area distance; };

LONG clamp(LONG value, LONG low, LONG high) {
    return (std::max)(low, (std::min)(value, high));
}
RECT footprint(POINT point, SIZE size) {
    return {point.x, point.y, point.x + size.cx, point.y + size.cy};
}
bool intersects(const RECT &a, const RECT &b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}
Area distance(POINT a, POINT b) {
    const int64_t dx = static_cast<int64_t>(a.x) - b.x;
    const int64_t dy = static_cast<int64_t>(a.y) - b.y;
    return static_cast<Area>(dx * dx + dy * dy);
}
bool better(const PlacementChoice &a, const PlacementChoice &b) {
    if (a.area != b.area) return a.area < b.area;
    if (a.distance != b.distance) return a.distance < b.distance;
    return a.point.y < b.point.y || (a.point.y == b.point.y && a.point.x < b.point.x);
}
void unique(std::vector<LONG> &values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
size_t indexOf(const std::vector<LONG> &values, LONG value) {
    return static_cast<size_t>(std::lower_bound(values.begin(), values.end(), value) - values.begin());
}
struct Probe { size_t cell; LONG offset; };
Probe locate(const std::vector<LONG> &cuts, LONG value) {
    const auto upper = std::upper_bound(cuts.begin(), cuts.end(), value);
    const size_t cell = static_cast<size_t>(upper - cuts.begin() - 1);
    return {cell, value - cuts[cell]};
}
std::vector<LONG> candidates(const PlacementQuery &scene, bool horizontal) {
    const LONG start = horizontal ? scene.work.left : scene.work.top;
    const LONG extent = horizontal ? scene.size.cx : scene.size.cy;
    const LONG end = (horizontal ? scene.work.right : scene.work.bottom) - extent;
    std::vector<LONG> result = {start, end, horizontal ? scene.preferred.x : scene.preferred.y};
    for (const auto &rect : scene.windows) {
        const LONG first = horizontal ? rect.left : rect.top;
        const LONG last = horizontal ? rect.right : rect.bottom;
        for (LONG value : {first, last, first - extent, last - extent})
            if (value >= start && value <= end) result.push_back(value);
    }
    unique(result);
    return result;
}

// Compress the true obstacle edges, build union counts
// by 2D differences, then integrate occupied widths for each candidate x.
// Both preprocessing and all candidate queries are O(n^2), not O(n^2 log n).
class ExactSearch {
public:
    explicit ExactSearch(const PlacementQuery &scene) : scene(scene) {}
    PlacementChoice find() {
        if (!ready) prepare();
        const size_t preferredX = indexOf(cx, scene.preferred.x), preferredY = indexOf(cy, scene.preferred.y);
        const Area preferredArea = integral(preferredX, bottoms[preferredY]) - integral(preferredX, tops[preferredY]);
        if (!preferredArea) return {scene.preferred, 0, 0};
        PlacementChoice best = {scene.preferred, (std::numeric_limits<Area>::max)(), 0};
        for (size_t x = 0; x < cx.size(); ++x) {
            for (size_t y = 0; y < cy.size(); ++y) {
                const POINT point = {cx[x], cy[y]};
                const PlacementChoice next = {point, integral(x, bottoms[y]) - integral(x, tops[y]),
                    distance(point, scene.preferred)};
                if (better(next, best)) best = next;
            }
        }
        return best;
    }
private:
    const PlacementQuery &scene;
    bool ready = false;
    std::vector<LONG> cx, cy, ys, widths;
    std::vector<Area> prefixes;
    std::vector<Probe> tops, bottoms;
    Area integral(size_t x, Probe y) const {
        Area value = prefixes[x * ys.size() + y.cell];
        if (y.offset) value += static_cast<Area>(widths[x * (ys.size() - 1) + y.cell]) * y.offset;
        return value;
    }
    void prepare() {
        cx = candidates(scene, true); cy = candidates(scene, false);
        std::vector<LONG> xs = {scene.work.left, scene.work.right};
        ys = {scene.work.top, scene.work.bottom};
        for (const auto &rect : scene.windows) {
            xs.push_back(rect.left); xs.push_back(rect.right);
            ys.push_back(rect.top); ys.push_back(rect.bottom);
        }
        unique(xs); unique(ys);
        const size_t columns = xs.size(), rows = ys.size();
        std::vector<int> counts(columns * rows);
        for (const auto &rect : scene.windows) {
            const size_t left = indexOf(xs, rect.left), right = indexOf(xs, rect.right);
            const size_t top = indexOf(ys, rect.top), bottom = indexOf(ys, rect.bottom);
            ++counts[top * columns + left]; --counts[top * columns + right];
            --counts[bottom * columns + left]; ++counts[bottom * columns + right];
        }
        for (size_t y = 0; y < rows; ++y) {
            int line = 0;
            for (size_t x = 0; x < columns; ++x) {
                line += counts[y * columns + x];
                counts[y * columns + x] = line + (y ? counts[(y - 1) * columns + x] : 0);
            }
        }
        widths.resize(cx.size() * (rows - 1));
        prefixes.assign(cx.size() * rows, 0);
        std::vector<Probe> lefts, rights;
        for (LONG x : cx) { lefts.push_back(locate(xs, x)); rights.push_back(locate(xs, x + scene.size.cx)); }
        std::vector<LONG> line(columns);
        for (size_t y = 0; y + 1 < rows; ++y) {
            line[0] = 0;
            for (size_t x = 0; x + 1 < columns; ++x)
                line[x + 1] = line[x] + (counts[y * columns + x] ? xs[x + 1] - xs[x] : 0);
            const auto at = [&](Probe p) {
                return line[p.cell] + (p.offset && counts[y * columns + p.cell] ? p.offset : 0);
            };
            for (size_t x = 0; x < cx.size(); ++x) {
                const LONG occupied = at(rights[x]) - at(lefts[x]);
                widths[x * (rows - 1) + y] = occupied;
                prefixes[x * rows + y + 1] = prefixes[x * rows + y]
                    + static_cast<Area>(occupied) * (ys[y + 1] - ys[y]);
            }
        }
        for (LONG y : cy) { tops.push_back(locate(ys, y)); bottoms.push_back(locate(ys, y + scene.size.cy)); }
        ready = true;
    }
};

// Range-add/minimum tree over integer y-position bands. Search zero-covered
// predecessors/successors without materializing the union's polygon boundary.
class CoverageTree {
public:
    explicit CoverageTree(size_t leaves) : leaves(leaves), minimum(4 * leaves), lazy(4 * leaves) {}
    void add(size_t first, size_t last, int delta) { add(1, 0, leaves, first, last, delta); }
    int firstZero(size_t from) { return zero(1, 0, leaves, from, leaves, false); }
    int lastZero(size_t to) { return zero(1, 0, leaves, 0, to + 1, true); }
private:
    size_t leaves;
    std::vector<int> minimum, lazy;
    void apply(size_t node, int delta) { minimum[node] += delta; lazy[node] += delta; }
    void push(size_t node) {
        if (lazy[node]) { apply(2 * node, lazy[node]); apply(2 * node + 1, lazy[node]); lazy[node] = 0; }
    }
    void add(size_t node, size_t left, size_t right, size_t first, size_t last, int delta) {
        if (first >= right || last <= left) return;
        if (first <= left && right <= last) { apply(node, delta); return; }
        push(node);
        const size_t middle = (left + right) / 2;
        add(2 * node, left, middle, first, last, delta);
        add(2 * node + 1, middle, right, first, last, delta);
        minimum[node] = (std::min)(minimum[2 * node], minimum[2 * node + 1]);
    }
    int zero(size_t node, size_t left, size_t right, size_t first, size_t last, bool reverse) {
        if (minimum[node] || first >= right || last <= left) return -1;
        if (right - left == 1) return static_cast<int>(left);
        push(node);
        const size_t middle = (left + right) / 2;
        const int result = reverse ? zero(2 * node + 1, middle, right, first, last, reverse)
            : zero(2 * node, left, middle, first, last, reverse);
        if (result >= 0) return result;
        return reverse ? zero(2 * node, left, middle, first, last, reverse)
            : zero(2 * node + 1, middle, right, first, last, reverse);
    }
};

bool findFree(const PlacementQuery &scene, PlacementChoice &best) {
    const RECT preferred = footprint(scene.preferred, scene.size);
    if (std::none_of(scene.windows.begin(), scene.windows.end(),
            [&](const RECT &r) { return intersects(r, preferred); })) {
        best = {scene.preferred, 0, 0}; return true;
    }
    struct Event { LONG x, first, last; int delta; };
    const LONG maxX = scene.work.right - scene.size.cx, maxY = scene.work.bottom - scene.size.cy;
    std::vector<Event> events = {{scene.work.left, 0, 0, 0}, {maxX + 1, 0, 0, 0}};
    std::vector<LONG> ys = {scene.work.top, maxY + 1};
    for (const auto &r : scene.windows) {
        // Integer origins: touching edges are allowed, positive-area overlap is not.
        const LONG left = (std::max)(scene.work.left, r.left - scene.size.cx + 1);
        const LONG right = (std::min)(maxX, r.right - 1);
        const LONG top = (std::max)(scene.work.top, r.top - scene.size.cy + 1);
        const LONG bottom = (std::min)(maxY, r.bottom - 1);
        if (left > right || top > bottom) continue;
        events.push_back({left, top, bottom + 1, 1});
        events.push_back({right + 1, top, bottom + 1, -1});
        ys.push_back(top); ys.push_back(bottom + 1);
    }
    unique(ys);
    std::sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.x < b.x; });
    CoverageTree tree(ys.size() - 1);
    const size_t preferredBand = locate(ys, scene.preferred.y).cell;
    best = {scene.preferred, 1, 0};
    size_t event = 0;
    while (event < events.size()) {
        const LONG x = events[event].x;
        do {
            const auto &e = events[event++];
            if (e.delta) tree.add(indexOf(ys, e.first), indexOf(ys, e.last), e.delta);
        } while (event < events.size() && events[event].x == x);
        if (x > maxX || event == events.size()) break;
        const LONG nearestX = clamp(scene.preferred.x, x, events[event].x - 1);
        for (int band : {tree.lastZero(preferredBand), tree.firstZero(preferredBand)}) {
            if (band < 0) continue;
            const LONG y = clamp(scene.preferred.y, ys[band], ys[band + 1] - 1);
            const POINT point = {nearestX, y};
            const PlacementChoice next = {point, 0, distance(point, scene.preferred)};
            if (better(next, best)) best = next;
        }
    }
    return best.area == 0;
}


} // namespace

POINT findPlacementPosition(RECT work, SIZE size, POINT preferred, const std::vector<RECT> &windows) {
    if (IsRectEmpty(&work) || size.cx <= 0 || size.cy <= 0) return {work.left, work.top};
    // Only the part on this monitor participates when a requested size is larger
    // than its work area; requestedRect still preserves the full window size.
    size.cx = (std::min)(size.cx, work.right - work.left);
    size.cy = (std::min)(size.cy, work.bottom - work.top);
    preferred.x = clamp(preferred.x, work.left, work.right - size.cx);
    preferred.y = clamp(preferred.y, work.top, work.bottom - size.cy);
    PlacementQuery query = {work, size, preferred, {}};
    query.windows.reserve(windows.size());
    for (const auto &window : windows) {
        RECT bounds = {};
        if (IntersectRect(&bounds, &window, &work)) query.windows.push_back(bounds);
    }
    PlacementChoice free = {};
    if (findFree(query, free)) return free.point;
    return ExactSearch(query).find().point;
}

namespace {
const wchar_t withdrawalProperty[] = L"FileSpacer.PlacementWithdrawal";
const wchar_t testProperty[] = L"FileSpacer.PlacementTest";

bool placementRect(HWND window, RECT &rect) {
    wchar_t name[64] = {};
    if (!IsWindowVisible(window) || IsIconic(window)
            || GetPropW(window, withdrawalProperty)
            || !GetClassNameW(window, name, ARRAYSIZE(name))
            || lstrcmpW(name, L"FileSpacer Item Window") != 0)
        return false;
    bool testWindow = false;
#ifdef FILESPACER_DEBUG
    testWindow = settings::testMode;
#endif
    if ((GetPropW(window, testProperty) != nullptr) != testWindow) return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
            && cloaked)
        return false;
    // Keep the same DPI-virtualized outer coordinates as requestedRect and
    // MONITORINFO.rcWork; do not mix them with DWM's physical frame bounds.
    return GetWindowRect(window, &rect) && !IsRectEmpty(&rect);
}
}

void removeWindowPlacement(HWND window) {
    // Destruction can reenter Shell code before WM_NCDESTROY. Keep the closing
    // window excluded from any intervening cross-process snapshot.
    SetPropW(window, withdrawalProperty, reinterpret_cast<HANDLE>(1));
}

void cancelPlacementWithdrawal(HWND window) {
    RemovePropW(window, withdrawalProperty);
}

void initializeWindowPlacement(HWND window) {
#ifdef FILESPACER_DEBUG
    if (settings::testMode) SetPropW(window, testProperty, reinterpret_cast<HANDLE>(1));
#else
    (void)window;
#endif
}

void destroyWindowPlacement(HWND window) {
    RemovePropW(window, withdrawalProperty);
    RemovePropW(window, testProperty);
}

bool findWindowPlacement(HMONITOR monitor, SIZE size, HWND source, RECT &rect) try {
    MONITORINFO info = {sizeof(info)};
    if (!monitor || size.cx <= 0 || size.cy <= 0 || !GetMonitorInfoW(monitor, &info)
            || IsRectEmpty(&info.rcWork)) return false;
    std::vector<RECT> windows;
    const auto collect = [](HWND window, LPARAM data) -> BOOL {
        try {
            RECT bounds = {};
            if (placementRect(window, bounds))
                reinterpret_cast<std::vector<RECT> *>(data)->push_back(bounds);
            return TRUE;
        } catch (...) {
            // Do not let allocation failures escape through a Win32 callback.
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    };
    if (!EnumWindows(collect, reinterpret_cast<LPARAM>(&windows))) return false;
    POINT preferred = {info.rcWork.left, info.rcWork.top};
    RECT origin = {};
    if (source && GetWindowRect(source, &origin)) preferred = {origin.left, origin.top};
    const POINT position = findPlacementPosition(info.rcWork, size, preferred, windows);
    rect = {position.x, position.y, position.x + size.cx, position.y + size.cy};
    return true;
} catch (...) {
    // Leave the caller's native Windows placement available if tables cannot be built.
    return false;
}

PlacementWithdrawal::PlacementWithdrawal(HWND source) {
    if (source && !GetPropW(source, withdrawalProperty)
            && SetPropW(source, withdrawalProperty, reinterpret_cast<HANDLE>(this)))
        window = source;
}

PlacementWithdrawal::~PlacementWithdrawal() {
    if (!committed && window
            && GetPropW(window, withdrawalProperty) == reinterpret_cast<HANDLE>(this))
        RemovePropW(window, withdrawalProperty);
}

} // namespace
