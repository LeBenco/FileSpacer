#include <common.h>
#include "WindowPlacement.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace filespacer;

namespace {
void require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
int tokens[64] = {};
HWND identity(size_t index) { return reinterpret_cast<HWND>(&tokens[index]); }
bool samePoint(POINT a, POINT b) { return a.x == b.x && a.y == b.y; }
void requireFits(const RECT &work, SIZE size, POINT point) {
    const LONG maxX = (std::max)(work.left, work.right - size.cx);
    const LONG maxY = (std::max)(work.top, work.bottom - size.cy);
    require(point.x >= work.left && point.x <= maxX && point.y >= work.top && point.y <= maxY,
        "placement escaped the permitted origins");
}
void testFreeAndBlockedPaths() {
    const RECT work = {0, 0, 200, 150};
    const SIZE size = {40, 30};
    const POINT preferred = {17, 23};
    WindowPlacementState state(work);
    require(samePoint(state.findPosition(size, preferred), preferred), "empty scene moved a valid preference");
    const RECT left = {0, 0, 100, 150}, right = {100, 0, 200, 150};
    state.setWindow(identity(0), &left);
    requireFits(work, size, state.findPosition(size, preferred));
    state.setWindow(identity(1), &right);
    const POINT blocked = state.findPosition(size, preferred);
    requireFits(work, size, blocked);
    require(samePoint(state.findPosition(size, preferred), blocked), "unchanged blocked scene is unstable");
}
void testUpdatesAndWithdrawal() {
    const RECT work = {0, 0, 200, 150};
    const SIZE size = {30, 25};
    const POINT preferred = {5, 5};
    const RECT original = {0, 0, 100, 80}, moved = {120, 80, 180, 140};
    WindowPlacementState state(work);
    state.setWindow(identity(0), &original);
    require(!samePoint(state.findPosition(size, preferred), preferred), "new geometry was not recorded");
    state.setWindow(identity(0), &moved);
    require(samePoint(state.findPosition(size, preferred), preferred), "movement retained the old footprint");
    state.setWindow(identity(0), &original);
    state.setWindow(identity(1), &original);
    state.setWindow(identity(0), nullptr);
    state.setWindow(identity(0), nullptr);
    require(!samePoint(state.findPosition(size, preferred), preferred), "withdrawal lost another window");
    state.setWindow(identity(1), nullptr);
    require(samePoint(state.findPosition(size, preferred), preferred), "withdrawal retained removed geometry");
    state.setWindow(identity(0), &original);
    requireFits(work, size, state.findPosition(size, preferred));
    const RECT outside = {300, 300, 400, 400};
    state.setWindow(identity(0), &outside);
    require(samePoint(state.findPosition(size, preferred), preferred), "off-monitor movement retained geometry");
}
void testRefresh() {
    WindowPlacementState state({0, 0, 200, 150});
    const RECT first = {0, 0, 100, 80}, second = {120, 80, 180, 140};
    state.setWindow(identity(0), &first); state.setWindow(identity(1), &second);
    state.beginRefresh(); state.setWindow(identity(1), &second); state.endRefresh();
    require(samePoint(state.findPosition({30, 25}, {5, 5}), {5, 5}), "refresh retained a vanished window");
    require(!samePoint(state.findPosition({30, 25}, {125, 90}), {125, 90}), "refresh lost a surviving window");
    state.beginRefresh(); state.endRefresh();
    require(samePoint(state.findPosition({30, 25}, {125, 90}), {125, 90}), "empty refresh retained geometry");
}
void testBoundsAndSizes() {
    const RECT work = {-200, 40, 0, 140};
    WindowPlacementState state(work);
    const RECT clipped = {-250, 0, -100, 100};
    state.setWindow(identity(0), &clipped);
    for (SIZE size : {SIZE{35, 25}, SIZE{200, 100}, SIZE{300, 200}, SIZE{300, 25}})
        requireFits(work, size, state.findPosition(size, {400, -200}));
    require(samePoint(state.findPosition({300, 200}, {400, 500}), {-200, 40}),
        "oversized window lost its accessible origin");
    WindowPlacementState edge({0, 0, 103, 97});
    const RECT occupied = {0, 0, 80, 97};
    edge.setWindow(identity(0), &occupied);
    requireFits(edge.workArea(), {23, 97}, edge.findPosition({23, 97}, {0, 0}));
}
void testSeveralWindows() {
    const RECT work = {0, 0, 3840, 2160};
    WindowPlacementState state(work);
    for (size_t i = 0; i < 60; ++i) {
        const LONG x = static_cast<LONG>(37 * i % 3200), y = static_cast<LONG>(53 * i % 1650);
        const RECT rect = {x, y, x + 400, y + 300};
        state.setWindow(identity(i), &rect);
    }
    for (SIZE size : {SIZE{320, 240}, SIZE{580, 435}, SIZE{960, 720}})
        requireFits(work, size, state.findPosition(size, {0, 0}));
}
}

int main() {
    testFreeAndBlockedPaths(); testUpdatesAndWithdrawal(); testRefresh();
    testBoundsAndSizes(); testSeveralWindows();
    std::puts("PASS: hybrid placement, geometry updates, withdrawal/reintegration, refresh, boundaries and sizes.");
}
