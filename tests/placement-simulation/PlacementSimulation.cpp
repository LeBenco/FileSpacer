#include <common.h>
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Generated from the production geometry only; no window enumeration or access.
#include "Grids.inc"

namespace filespacer {
namespace simulation {
using Clock = std::chrono::steady_clock;
using Area = uint64_t;
struct Scene {
    RECT work;
    SIZE size;
    POINT preferred;
    std::vector<RECT> windows;
};
struct Choice { POINT point; Area area; Area distance; };
volatile Area checksum = 0;

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
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
bool better(const Choice &a, const Choice &b) {
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
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

// Independent area oracle: clip, split into horizontal strips, merge x intervals.
Area unionArea(const Scene &scene, POINT point) {
    const RECT target = footprint(point, scene.size);
    std::vector<RECT> clipped;
    std::vector<LONG> ys;
    for (const auto &rect : scene.windows) {
        RECT r = {(std::max)(rect.left, target.left), (std::max)(rect.top, target.top),
            (std::min)(rect.right, target.right), (std::min)(rect.bottom, target.bottom)};
        if (r.left >= r.right || r.top >= r.bottom) continue;
        clipped.push_back(r); ys.push_back(r.top); ys.push_back(r.bottom);
    }
    unique(ys);
    Area area = 0;
    for (size_t y = 1; y < ys.size(); ++y) {
        std::vector<std::pair<LONG, LONG>> intervals;
        for (const auto &r : clipped)
            if (r.top <= ys[y - 1] && r.bottom >= ys[y]) intervals.emplace_back(r.left, r.right);
        std::sort(intervals.begin(), intervals.end());
        LONG end = target.left, width = 0;
        for (const auto &interval : intervals) {
            width += (std::max)(0L, interval.second - (std::max)(end, interval.first));
            end = (std::max)(end, interval.second);
        }
        area += static_cast<Area>(width) * (ys[y] - ys[y - 1]);
    }
    return area;
}

struct Probe { size_t cell; LONG offset; };
Probe locate(const std::vector<LONG> &cuts, LONG value) {
    const auto upper = std::upper_bound(cuts.begin(), cuts.end(), value);
    const size_t cell = static_cast<size_t>(upper - cuts.begin() - 1);
    return {cell, value - cuts[cell]};
}
std::vector<LONG> candidates(const Scene &scene, bool horizontal) {
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

// Exact O(n^2) prototype. Compress the true obstacle edges, build union counts
// by 2D differences, then integrate occupied widths for each candidate x.
// Both preprocessing and all candidate queries are O(n^2), not O(n^2 log n).
class ExactSearch {
public:
    explicit ExactSearch(const Scene &scene) : scene(scene) {}
    Choice find() {
        if (!ready) prepare();
        const size_t preferredX = indexOf(cx, scene.preferred.x), preferredY = indexOf(cy, scene.preferred.y);
        const Area preferredArea = integral(preferredX, bottoms[preferredY]) - integral(preferredX, tops[preferredY]);
        if (!preferredArea) return {scene.preferred, 0, 0};
        Choice best = {scene.preferred, (std::numeric_limits<Area>::max)(), 0};
        for (size_t x = 0; x < cx.size(); ++x) {
            for (size_t y = 0; y < cy.size(); ++y) {
                const POINT point = {cx[x], cy[y]};
                const Choice next = {point, integral(x, bottoms[y]) - integral(x, tops[y]),
                    distance(point, scene.preferred)};
                if (better(next, best)) best = next;
            }
        }
        return best;
    }
    size_t bytes() const {
        return (cx.capacity() + cy.capacity() + ys.capacity() + widths.capacity()) * sizeof(LONG)
            + prefixes.capacity() * sizeof(Area) + (tops.capacity() + bottoms.capacity()) * sizeof(Probe);
    }
private:
    const Scene &scene;
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
        // Includes the transient counts and probes, for a conservative peak estimate.
        peakBytes = counts.capacity() * sizeof(int) + xs.capacity() * sizeof(LONG)
            + (lefts.capacity() + rights.capacity()) * sizeof(Probe) + line.capacity() * sizeof(LONG);
        ready = true;
    }
    size_t peakBytes = 0;
public:
    size_t peak() const { return bytes() + peakBytes; }
};

// Range-add/minimum tree over integer y-position bands. Search zero-covered
// predecessors/successors without materializing the union's polygon boundary.
class CoverageTree {
public:
    explicit CoverageTree(size_t leaves) : leaves(leaves), minimum(4 * leaves), lazy(4 * leaves) {}
    void add(size_t first, size_t last, int delta) { add(1, 0, leaves, first, last, delta); }
    int firstZero(size_t from) { return zero(1, 0, leaves, from, leaves, false); }
    int lastZero(size_t to) { return zero(1, 0, leaves, 0, to + 1, true); }
    size_t bytes() const { return (minimum.capacity() + lazy.capacity()) * sizeof(int); }
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

bool findFree(const Scene &scene, Choice &best, const char *&path, size_t *temporaryBytes = nullptr) {
    if (temporaryBytes) *temporaryBytes = 0;
    const RECT preferred = footprint(scene.preferred, scene.size);
    if (std::none_of(scene.windows.begin(), scene.windows.end(),
            [&](const RECT &r) { return intersects(r, preferred); })) {
        best = {scene.preferred, 0, 0}; path = "preferred-free"; return true;
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
    if (temporaryBytes) *temporaryBytes = events.capacity() * sizeof(Event) + ys.capacity() * sizeof(LONG) + tree.bytes();
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
            const Choice next = {point, 0, distance(point, scene.preferred)};
            if (better(next, best)) best = next;
        }
    }
    path = best.area ? "positive-exact" : "zero-sweep";
    return best.area == 0;
}

Choice bruteForce(const Scene &scene) {
    Choice best = {scene.preferred, (std::numeric_limits<Area>::max)(), 0};
    // Deliberately simple independent pixel oracle, used only on small scenes.
    for (LONG y = scene.work.top; y <= scene.work.bottom - scene.size.cy; ++y) {
        for (LONG x = scene.work.left; x <= scene.work.right - scene.size.cx; ++x) {
            Area area = 0;
            for (LONG py = y; py < y + scene.size.cy; ++py) {
                for (LONG px = x; px < x + scene.size.cx; ++px) {
                    const bool occupied = std::any_of(scene.windows.begin(), scene.windows.end(),
                        [&](const RECT &r) { return px >= r.left && px < r.right && py >= r.top && py < r.bottom; });
                    if (occupied) ++area;
                }
            }
            const Choice next = {{x, y}, area, distance({x, y}, scene.preferred)};
            if (better(next, best)) best = next;
        }
    }
    return best;
}
LONG randomBetween(std::mt19937 &random, LONG low, LONG high) {
    return low + static_cast<LONG>(random() % static_cast<uint32_t>(high - low + 1));
}
void verify() {
    std::mt19937 random(91273);
    for (unsigned trial = 0; trial < 600; ++trial) {
        Scene scene = {{-3, 2, 18, 19}, {randomBetween(random, 1, 10), randomBetween(random, 1, 9)}, {0, 0}, {}};
        scene.preferred = {randomBetween(random, scene.work.left, scene.work.right - scene.size.cx),
            randomBetween(random, scene.work.top, scene.work.bottom - scene.size.cy)};
        for (unsigned i = 0, count = random() % 10; i < count; ++i) {
            const LONG x = randomBetween(random, scene.work.left, scene.work.right - 1);
            const LONG y = randomBetween(random, scene.work.top, scene.work.bottom - 1);
            scene.windows.push_back({x, y, randomBetween(random, x + 1, scene.work.right),
                randomBetween(random, y + 1, scene.work.bottom)});
        }
        const Choice oracle = bruteForce(scene);
        ExactSearch exact(scene);
        const Choice result = exact.find();
        require(result.area == oracle.area && result.distance == oracle.distance, "exact solver disagrees with brute force");
        require(unionArea(scene, result.point) == oracle.area, "strip union oracle disagrees with pixel oracle");
        Choice free = {};
        const char *path = nullptr;
        const bool exists = findFree(scene, free, path);
        require(exists == (oracle.area == 0), "free-space sweep missed or invented a free position");
        if (exists) require(free.distance == oracle.distance && unionArea(scene, free.point) == 0,
            "free-space sweep failed nearest-position tie break");
        grid1::WindowPlacementGrid grid(scene.work);
        for (size_t i = 0; i < scene.windows.size(); ++i)
            grid.setWindow(reinterpret_cast<HWND>(&scene.windows[i]), &scene.windows[i]);
        const POINT pixel = grid.findPosition(scene.size, scene.preferred);
        require(unionArea(scene, pixel) == oracle.area && distance(pixel, scene.preferred) == oracle.distance,
            "production pixel grid disagrees with brute force");
    }
    // A free region only one integer origin wide must survive open-edge handling.
    Scene tight = {{0, 0, 100, 80}, {23, 80}, {0, 0}, {{0, 0, 37, 80}, {60, 0, 100, 80}}};
    Choice free = {}; const char *path = nullptr;
    require(findFree(tight, free, path) && free.point.x == 37, "exact-fit corridor missed");
    std::cout << "VERIFY PASS: 600 exhaustive random scenes + exact-fit corridor\n";
}

Scene generate(LONG width, LONG height, const std::string &pattern, unsigned count, unsigned seed) {
    std::mt19937 random(seed);
    Scene scene = {{0, 0, width, height}, {580, 435}, {0, 0}, {}};
    // Normal folder windows: varied sizes near the current default, no tiny slivers.
    if (seed % 3 == 1) scene.size = {randomBetween(random, 480, 800), randomBetween(random, 360, 650)};
    const LONG gapX = width - scene.size.cx - 13;
    for (unsigned i = 0; i < count; ++i) {
        LONG w = randomBetween(random, 420, 920), h = randomBetween(random, 320, 760);
        LONG x = 0, y = 0;
        if (pattern == "scattered") {
            x = randomBetween(random, 0, width - w); y = randomBetween(random, 0, height - h);
        } else if (pattern == "clustered") {
            x = clamp(width / 3 - w / 2 + randomBetween(random, -180, 180), 0, width - w);
            y = clamp(height / 3 - h / 2 + randomBetween(random, -130, 130), 0, height - h);
        } else if (pattern == "aligned") {
            w = 580; h = 435;
            x = static_cast<LONG>(i % 4) * (width - w) / 3;
            y = static_cast<LONG>((i / 4) % 3) * (height - h) / 2;
            if (i >= 12 && i % 3) { x += randomBetween(random, -25, 25); y += randomBetween(random, -25, 25); }
            x = clamp(x, 0, width - w); y = clamp(y, 0, height - h);
        } else {
            // Preserve a wide free strip; the scattered scene is allowed to fill up.
            w = (std::min)(w, gapX);
            x = randomBetween(random, 0, gapX - w); y = randomBetween(random, 0, height - h);
        }
        scene.windows.push_back({x, y, x + w, y + h});
    }
    // Include source-like preferred origins as well as the usual upper-left corner.
    if (seed % 3 == 2) scene.preferred = {scene.windows[0].left, scene.windows[0].top};
    scene.preferred.x = clamp(scene.preferred.x, 0, width - scene.size.cx);
    scene.preferred.y = clamp(scene.preferred.y, 0, height - scene.size.cy);
    return scene;
}

struct Measurement {
    double setup = 0, cold = 0, warm = 0, update = 0, afterUpdate = 0;
    size_t bytes = 0;
    POINT point = {};
    const char *path = "grid";
};
RECT movedFirst(const Scene &scene) {
    RECT moved = scene.windows.front();
    const LONG dx = moved.right + 17 <= scene.work.right ? 17 : -17;
    const LONG dy = moved.bottom + 11 <= scene.work.bottom ? 11 : -11;
    moved.left += dx; moved.right += dx; moved.top += dy; moved.bottom += dy;
    // Keep all generated geometry inside the work area.
    if (moved.left < scene.work.left) { moved.right -= moved.left - scene.work.left; moved.left = scene.work.left; }
    if (moved.top < scene.work.top) { moved.bottom -= moved.top - scene.work.top; moved.top = scene.work.top; }
    return moved;
}
template<class Grid>
Measurement measureGrid(const Scene &scene, unsigned step) {
    Measurement m;
    auto start = Clock::now();
    auto grid = std::make_unique<Grid>(scene.work);
    for (size_t i = 0; i < scene.windows.size(); ++i)
        grid->setWindow(reinterpret_cast<HWND>(const_cast<RECT *>(&scene.windows[i])), &scene.windows[i]);
    m.setup = elapsed(start);
    start = Clock::now();
    POINT point = grid->findPosition(scene.size, scene.preferred);
    m.cold = elapsed(start); checksum ^= static_cast<Area>(point.x + point.y);
    start = Clock::now();
    point = grid->findPosition(scene.size, scene.preferred);
    m.warm = elapsed(start); checksum ^= static_cast<Area>(point.x + point.y);
    const RECT moved = movedFirst(scene);
    start = Clock::now();
    for (unsigned i = 0; i < 65; ++i)
        grid->setWindow(reinterpret_cast<HWND>(const_cast<RECT *>(&scene.windows.front())), i % 2 ? &scene.windows.front() : &moved);
    m.update = elapsed(start) / 65;
    start = Clock::now();
    m.point = grid->findPosition(scene.size, scene.preferred);
    m.afterUpdate = elapsed(start);
    const size_t columns = static_cast<size_t>((scene.work.right - scene.work.left + step - 1) / step);
    const size_t rows = static_cast<size_t>((scene.work.bottom - scene.work.top + step - 1) / step);
    m.bytes = columns * rows * sizeof(uint32_t) + (columns + 1) * (rows + 1) * sizeof(Area);
    return m;
}
Measurement measureExact(const Scene &scene, bool fastFree) {
    Measurement m;
    m.path = "exact";
    auto start = Clock::now();
    Scene state = scene;
    m.setup = elapsed(start);
    std::unique_ptr<ExactSearch> cache;
    const auto find = [&]() {
        Choice choice = {};
        size_t temporaryBytes = 0;
        const bool free = fastFree && findFree(state, choice, m.path, &temporaryBytes);
        m.bytes = (std::max)(m.bytes, temporaryBytes + (cache ? cache->bytes() : 0));
        if (free) return choice;
        if (!cache) cache = std::make_unique<ExactSearch>(state);
        return cache->find();
    };
    start = Clock::now();
    Choice choice = find();
    m.cold = elapsed(start); checksum ^= choice.area;
    start = Clock::now();
    choice = find();
    m.warm = elapsed(start); checksum ^= choice.area;
    const RECT moved = movedFirst(scene);
    start = Clock::now();
    for (unsigned i = 0; i < 65; ++i) {
        state.windows.front() = i % 2 ? scene.windows.front() : moved;
        checksum ^= static_cast<Area>(state.windows.front().left);
    }
    m.update = elapsed(start) / 65;
    // The lazy record update invalidates the query cache; disposal is charged to
    // the next placement rather than silently disappearing from the timings.
    start = Clock::now();
    cache.reset();
    choice = find();
    m.point = choice.point;
    m.afterUpdate = elapsed(start);
    if (cache) m.bytes = (std::max)(m.bytes, cache->peak());
    require(unionArea(state, choice.point) == choice.area, "large-scene exact score failed independent union oracle");
    return m;
}
double median(std::vector<double> values) {
    std::sort(values.begin(), values.end()); return values[values.size() / 2];
}
Measurement aggregate(const std::vector<Measurement> &samples) {
    Measurement m = samples.front();
    std::vector<double> setup, cold, warm, update, after;
    for (const auto &s : samples) {
        setup.push_back(s.setup); cold.push_back(s.cold); warm.push_back(s.warm);
        update.push_back(s.update); after.push_back(s.afterUpdate);
    }
    m.setup = median(setup); m.cold = median(cold); m.warm = median(warm);
    m.update = median(update); m.afterUpdate = median(after);
    return m;
}
void benchmark(const std::string &output, unsigned seeds, unsigned repetitions, bool quick) {
    std::ofstream results(output + "/measurements.csv"), scenes(output + "/scenes.csv"), windows(output + "/windows.csv");
    require(results.good() && scenes.good() && windows.good(), "cannot create output CSV files");
    results << "scene,width,height,pattern,n,seed,method,step,setup_us,cold_us,warm_us,update_us,after_update_us,estimated_peak_bytes,x,y,area,optimal_area,extra_area,missed_free,path\n";
    results << std::fixed << std::setprecision(4);
    scenes << "scene,width,height,pattern,n,seed,target_width,target_height,preferred_x,preferred_y\n";
    windows << "scene,index,left,top,right,bottom\n";
    const std::vector<unsigned> counts = quick ? std::vector<unsigned>{5, 60} : std::vector<unsigned>{1, 5, 10, 20, 40, 60, 100, 200, 400};
    const std::vector<SIZE> screens = quick ? std::vector<SIZE>{{1920, 1080}} : std::vector<SIZE>{{1920, 1080}, {2560, 1440}, {3840, 2160}};
    unsigned sceneId = 0;
    for (const auto &screen : screens) for (const std::string pattern : {"scattered", "clustered", "aligned", "free-strip"}) {
        for (unsigned count : counts) {
            for (unsigned sample = 0; sample < seeds; ++sample) {
                const unsigned seed = 81723 + sample * 104729 + count * 97;
                Scene scene = generate(screen.cx, screen.cy, pattern, count, seed);
                ++sceneId;
                scenes << sceneId << ',' << screen.cx << ',' << screen.cy << ',' << pattern << ',' << count << ',' << seed
                    << ',' << scene.size.cx << ',' << scene.size.cy << ',' << scene.preferred.x << ',' << scene.preferred.y << '\n';
                for (size_t i = 0; i < scene.windows.size(); ++i) {
                    const auto &r = scene.windows[i];
                    windows << sceneId << ',' << i << ',' << r.left << ',' << r.top << ',' << r.right << ',' << r.bottom << '\n';
                }
                std::vector<std::vector<Measurement>> measurements(6);
                for (unsigned repeat = 0; repeat < repetitions; ++repeat) {
                    // Rotate method order to reduce systematic thermal/cache bias.
                    for (unsigned order = 0; order < 6; ++order) {
                        const unsigned method = (order + repeat + sceneId) % 6;
                        Measurement m;
                        switch (method) {
                        case 0: m = measureGrid<grid10::WindowPlacementGrid>(scene, 10); break;
                        case 1: m = measureGrid<grid5::WindowPlacementGrid>(scene, 5); break;
                        case 2: m = measureGrid<grid2::WindowPlacementGrid>(scene, 2); break;
                        case 3: m = measureGrid<grid1::WindowPlacementGrid>(scene, 1); break;
                        case 4: m = measureExact(scene, false); break;
                        default: m = measureExact(scene, true); break;
                        }
                        checksum ^= static_cast<Area>(m.point.x + m.point.y);
                        measurements[method].push_back(m);
                    }
                }
                scene.windows.front() = movedFirst(scene);
                const auto exact = aggregate(measurements[4]);
                const Area optimum = unionArea(scene, exact.point);
                const char *names[] = {"grid10", "grid5", "grid2", "grid1", "exact", "exact-free"};
                const unsigned steps[] = {10, 5, 2, 1, 0, 0};
                for (unsigned method = 0; method < 6; ++method) {
                    const auto m = aggregate(measurements[method]);
                    const Area area = unionArea(scene, m.point);
                    require(area >= optimum, "approximate method beat the exact solver");
                    if (method >= 3) require(area == optimum, "exact methods disagree on optimum area");
                    results << sceneId << ',' << screen.cx << ',' << screen.cy << ',' << pattern << ',' << count << ',' << seed
                        << ',' << names[method] << ',' << steps[method] << ',' << m.setup << ',' << m.cold << ',' << m.warm
                        << ',' << m.update << ',' << m.afterUpdate << ',' << m.bytes << ',' << m.point.x << ',' << m.point.y
                        << ',' << area << ',' << optimum << ',' << area - optimum << ',' << (optimum == 0 && area > 0 ? 1 : 0)
                        << ',' << m.path << '\n';
                }
            }
            results.flush();
            std::cout << screen.cx << 'x' << screen.cy << ' ' << pattern << " n=" << count << " complete\n" << std::flush;
        }
    }
    std::cout << "BENCHMARK PASS: " << sceneId << " scenes, 6 methods, checksum=" << checksum << '\n';
}
} // namespace simulation
} // namespace filespacer

#ifndef FILESPACER_SIMULATION_LIBRARY
int main(int argc, char **argv) {
    try {
        filespacer::simulation::verify();
        if (argc == 2 && std::string(argv[1]) == "--verify-only") return 0;
        if (argc < 4 || argc > 5) throw std::runtime_error("Usage: PlacementSimulation output-directory seeds repetitions [--quick]");
        const unsigned seeds = static_cast<unsigned>(std::stoul(argv[2]));
        const unsigned repetitions = static_cast<unsigned>(std::stoul(argv[3]));
        if (!seeds || !repetitions || seeds > 100 || repetitions > 100) throw std::runtime_error("Seeds/repetitions must be in 1..100");
        filespacer::simulation::benchmark(argv[1], seeds, repetitions, argc == 5 && std::string(argv[4]) == "--quick");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
#endif
