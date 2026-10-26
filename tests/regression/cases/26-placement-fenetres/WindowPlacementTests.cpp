#include <common.h>
#include "WindowPlacement.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace filespacer;

namespace {
void require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
HWND identity(unsigned value) { return reinterpret_cast<HWND>(static_cast<uintptr_t>(value)); }
uint32_t randomState = 91273;
unsigned randomValue(unsigned limit) {
    randomState = randomState * 1664525U + 1013904223U;
    return randomState % limit;
}
void testCounters() {
    WindowPlacementGrid grid({0, 0, 200, 100});
    const RECT first = {0, 0, 60, 60}, second = {30, 30, 90, 90};
    grid.setWindow(identity(1), &first);
    grid.setWindow(identity(2), &second);
    require(grid.occupiedArea({0, 0, 200, 100}) == 6300, "overlaps counted more than once");
    grid.setWindow(identity(1), nullptr);
    grid.setWindow(identity(1), nullptr);
    require(grid.occupiedArea({0, 0, 200, 100}) == 3600, "idempotent withdrawal lost another window");
    const RECT third = {100, 0, 130, 30};
    grid.setWindow(identity(3), &third);
    grid.setWindow(identity(1), &first);
    require(grid.occupiedArea({0, 0, 200, 100}) == 7200, "rollback discarded intervening changes");
    grid.beginRefresh(); grid.setWindow(identity(2), &second); grid.endRefresh();
    require(grid.occupiedArea({0, 0, 200, 100}) == 3600, "refresh retained vanished windows");
}
void testPlacement() {
    WindowPlacementGrid grid({-200, 40, 0, 140});
    const RECT left = {-200, 40, -100, 140};
    grid.setWindow(identity(1), &left);
    const POINT free = grid.findPosition({100, 100}, {-200, 40});
    require(free.x == -100 && free.y == 40, "free space on a negative-coordinate monitor missed");
    grid.setWindow(identity(1), nullptr);
    const POINT preferred = grid.findPosition({35, 25}, {-173, 57});
    require(preferred.x == -173 && preferred.y == 57, "free preferred position moved");
    const RECT full = {-200, 40, 0, 140};
    grid.setWindow(identity(2), &full);
    const POINT tied = grid.findPosition({35, 25}, {-173, 57});
    require(tied.x == -173 && tied.y == 57, "equal coverage ignored preferred position");
    const POINT large = grid.findPosition({300, 200}, {400, 500});
    require(large.x == -200 && large.y == 40, "oversized window lost its accessible origin");
    WindowPlacementGrid edge({0, 0, 103, 97});
    const RECT occupied = {0, 0, 80, 97};
    edge.setWindow(identity(1), &occupied);
    const POINT fit = edge.findPosition({23, 97}, {0, 0});
    require(fit.x == 80 && fit.y == 0, "non-multiple screen edge missed");
    const RECT all = {0, 0, 103, 97};
    edge.setWindow(identity(2), &all);
    require(edge.occupiedArea(all) == 103U * 97U, "partial boundary cells overcounted");
    require(edge.occupiedArea({3, 7, 26, 34}) == 23U * 27U, "partial-cell query incorrect");
}
void testUpdatesAgainstRebuild() {
    const RECT work = {-113, 27, 120, 202};
    WindowPlacementGrid incremental(work);
    std::vector<RECT> rectangles(20);
    std::vector<bool> active(20);
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        const unsigned index = randomValue(20);
        active[index] = randomValue(5) != 0;
        RECT &rect = rectangles[index];
        rect.left = -150 + static_cast<LONG>(randomValue(300));
        rect.top = static_cast<LONG>(randomValue(220));
        rect.right = rect.left + 1 + static_cast<LONG>(randomValue(150));
        rect.bottom = rect.top + 1 + static_cast<LONG>(randomValue(130));
        incremental.setWindow(identity(index + 1), active[index] ? &rect : nullptr);
        WindowPlacementGrid rebuilt(work);
        for (unsigned item = 0; item < rectangles.size(); ++item)
            if (active[item]) rebuilt.setWindow(identity(item + 1), &rectangles[item]);
        for (LONG y = work.top; y < work.bottom; y += 10) {
            for (LONG x = work.left; x < work.right; x += 10) {
                const RECT cell = {x, y, x + 10, y + 10};
                require(incremental.occupiedArea(cell) == rebuilt.occupiedArea(cell),
                    "incremental movement/resize differs from full reconstruction");
                bool occupied = false;
                RECT clippedCell = {};
                IntersectRect(&clippedCell, &cell, &work);
                for (unsigned item = 0; item < rectangles.size(); ++item) {
                    RECT overlap = {};
                    if (active[item] && IntersectRect(&overlap, &clippedCell, &rectangles[item])) occupied = true;
                }
                const LONG width = (std::min)(10L, work.right - x);
                const LONG height = (std::min)(10L, work.bottom - y);
                require(incremental.occupiedArea(cell) == (occupied ? static_cast<uint64_t>(width) * height : 0),
                    "occupation differs from independently intersected rectangles");
            }
        }
    }
}

void measurePlacement() {
    LARGE_INTEGER frequency = {}, start = {}, finish = {};
    QueryPerformanceFrequency(&frequency);
    for (unsigned count : {100U, 1000U}) {
        WindowPlacementGrid grid({0, 0, 3840, 2160});
        for (unsigned index = 0; index < count; ++index) {
            const LONG x = static_cast<LONG>(randomValue(3441));
            const LONG y = static_cast<LONG>(randomValue(1861));
            const RECT rect = {x, y, x + 400, y + 300};
            grid.setWindow(identity(index + 1), &rect);
        }
        QueryPerformanceCounter(&start);
        for (unsigned query = 0; query < 10; ++query) {
            const POINT result = grid.findPosition({400, 300}, {0, 0});
            require(result.x >= 0 && result.x <= 3440 && result.y >= 0 && result.y <= 1860,
                "measured placement escaped the work area");
        }
        QueryPerformanceCounter(&finish);
        const double milliseconds = 1000.0 * static_cast<double>(finish.QuadPart - start.QuadPart)
            / static_cast<double>(frequency.QuadPart) / 10.0;
        std::printf("OBSERVATION: 4K grid, %u rectangles, %.3f ms per placement (native test build).\n", count, milliseconds);
    }
}
}

int main() {
    testCounters(); testPlacement(); testUpdatesAgainstRebuild(); measurePlacement();
    std::puts("PASS: union counters, idempotent withdrawal/rollback, reconciliation, placement, boundaries and 1000 incremental scenes.");
}
