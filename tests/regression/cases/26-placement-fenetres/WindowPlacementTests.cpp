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
bool samePoint(POINT a, POINT b) { return a.x == b.x && a.y == b.y; }
LONG overlap(const RECT &a, const RECT &b) {
    const LONG width = (std::min)(a.right, b.right) - (std::max)(a.left, b.left);
    const LONG height = (std::min)(a.bottom, b.bottom) - (std::max)(a.top, b.top);
    return width > 0 && height > 0 ? width * height : 0;
}
void requireFits(const RECT &work, SIZE size, POINT point) {
    const LONG maxX = (std::max)(work.left, work.right - size.cx);
    const LONG maxY = (std::max)(work.top, work.bottom - size.cy);
    require(point.x >= work.left && point.x <= maxX && point.y >= work.top && point.y <= maxY,
        "placement escaped the permitted origins");
}
// The product passes whatever order EnumWindows yields: a snapshot must place
// identically in any order, and placing twice must give the same origin.
POINT place(const RECT &work, SIZE size, POINT preferred, std::vector<RECT> windows) {
    const POINT point = findPlacementPosition(work, size, preferred, windows);
    require(samePoint(findPlacementPosition(work, size, preferred, windows), point),
        "repeated placement of the same snapshot is unstable");
    std::reverse(windows.begin(), windows.end());
    require(samePoint(findPlacementPosition(work, size, preferred, windows), point),
        "placement depends on the snapshot order");
    return point;
}
void requireFree(const RECT &work, SIZE size, POINT point, const std::vector<RECT> &windows) {
    requireFits(work, size, point);
    const RECT placed = {point.x, point.y, point.x + size.cx, point.y + size.cy};
    for (const auto &window : windows)
        require(overlap(placed, window) == 0, "a free origin existed but the placement overlaps");
}
void testFreeAndBlockedPaths() {
    const RECT work = {0, 0, 200, 150};
    const SIZE size = {40, 30};
    const POINT preferred = {17, 23};
    require(samePoint(place(work, size, preferred, {}), preferred), "empty scene moved a valid preference");
    const RECT left = {0, 0, 100, 150}, right = {100, 0, 200, 150};
    requireFree(work, size, place(work, size, preferred, {left}), {left});
    const POINT blocked = place(work, size, preferred, {left, right});
    requireFits(work, size, blocked);
    require(samePoint(place(work, size, preferred, {left, right}), blocked), "unchanged blocked scene is unstable");
}
void testSnapshots() {
    const RECT work = {0, 0, 200, 150};
    const SIZE size = {30, 25};
    const POINT preferred = {5, 5};
    const RECT original = {0, 0, 100, 80}, moved = {120, 80, 180, 140};
    const POINT avoided = place(work, size, preferred, {original});
    require(!samePoint(avoided, preferred), "an occupied preference was kept");
    requireFree(work, size, avoided, {original});
    require(samePoint(place(work, size, preferred, {moved}), preferred), "a moved window retained its old footprint");
    require(!samePoint(place(work, size, preferred, {original, original}), preferred),
        "two windows with the same geometry were lost");
    require(samePoint(place(work, size, preferred, {}), preferred), "an empty snapshot retained removed geometry");
    const RECT outside = {300, 300, 400, 400};
    require(samePoint(place(work, size, preferred, {outside}), preferred), "an off-monitor window counted as an obstacle");
    const RECT straddling = {-50, -50, 50, 50};
    require(!samePoint(place(work, size, preferred, {straddling}), preferred), "the on-monitor part of a window was ignored");
    require(samePoint(place(work, size, {50, 50}, {straddling}), {50, 50}), "a touching edge counted as an overlap");
    // Each placement sees only the snapshot given to it, never an earlier one.
    const RECT first = {0, 0, 100, 80}, second = {120, 80, 180, 140};
    require(!samePoint(place(work, size, {5, 5}, {first, second}), {5, 5}), "a present window was ignored");
    require(samePoint(place(work, size, {5, 5}, {second}), {5, 5}), "a vanished window was retained");
    require(!samePoint(place(work, size, {125, 90}, {second}), {125, 90}), "a surviving window was lost");
    require(samePoint(place(work, size, {125, 90}, {}), {125, 90}), "an empty snapshot retained geometry");
}
void testBoundsAndSizes() {
    const RECT work = {-200, 40, 0, 140};
    const RECT clipped = {-250, 0, -100, 100};
    for (SIZE size : {SIZE{35, 25}, SIZE{200, 100}, SIZE{300, 200}, SIZE{300, 25}})
        requireFits(work, size, place(work, size, {400, -200}, {clipped}));
    require(samePoint(place(work, {300, 200}, {400, 500}, {clipped}), {-200, 40}),
        "oversized window lost its accessible origin");
    require(samePoint(place({10, 20, 10, 20}, {30, 25}, {0, 0}, {clipped}), {10, 20}),
        "empty work area lost its origin");
    require(samePoint(place(work, {0, 25}, {400, -200}, {clipped}), {-200, 40}),
        "degenerate size lost the work area origin");
    const RECT edge = {0, 0, 103, 97};
    const RECT occupied = {0, 0, 80, 97};
    requireFree(edge, {23, 97}, place(edge, {23, 97}, {0, 0}, {occupied}), {occupied});
}
void testSeveralWindows() {
    const RECT work = {0, 0, 3840, 2160};
    std::vector<RECT> windows;
    for (size_t i = 0; i < 60; ++i) {
        const LONG x = static_cast<LONG>(37 * i % 3200), y = static_cast<LONG>(53 * i % 1650);
        windows.push_back({x, y, x + 400, y + 300});
    }
    for (SIZE size : {SIZE{320, 240}, SIZE{580, 435}, SIZE{960, 720}})
        requireFits(work, size, place(work, size, {0, 0}, windows));
}
}

int main() {
    testFreeAndBlockedPaths(); testSnapshots(); testBoundsAndSizes(); testSeveralWindows();
    std::puts("PASS: hybrid placement over snapshots, order independence, clipping, boundaries and sizes.");
}
