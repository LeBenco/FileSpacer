#include <common.h>
#include "WindowResizeGesture.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace filespacer;

namespace {
int failures = 0;
void expect(bool condition, const char *message) {
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
struct Host {
    WindowResizeGesture gesture;
    std::vector<RECT> positions;
    int contextMenus = 0;
    bool reenterOnCaptureChange = false;
};
LRESULT CALLBACK hostProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto creation = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    }
    const auto host = reinterpret_cast<Host *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (host) {
        if (host->gesture.handleMessage(message, wParam, lParam)) return 0;
        if (message == WM_CAPTURECHANGED && host->reenterOnCaptureChange) {
            host->reenterOnCaptureChange = false;
            host->gesture.cancel(); host->gesture.stop();
        }
        if (message == WM_WINDOWPOSCHANGED) {
            RECT bounds = {}; GetWindowRect(window, &bounds); host->positions.push_back(bounds);
        }
        if (message == WM_CONTEXTMENU) { ++host->contextMenus; return 0; }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
RECT bounds(HWND window) { RECT result = {}; GetWindowRect(window, &result); return result; }
POINT point(HWND window, int dx = 0, int dy = 0) {
    POINT result = {dx, dy}; ClientToScreen(window, &result); return result;
}
void sendAt(HWND window, UINT message, WPARAM keys, POINT screen) {
    ScreenToClient(window, &screen);
    SendMessageW(window, message, keys, MAKELPARAM(screen.x, screen.y));
}
RECT start(Host &host, HWND window, GestureButton button, bool moving, POINT &origin) {
    host.gesture.stop(); host.gesture.takeCompleted();
    SetWindowPos(window, nullptr, 50, 50, 400, 300, SWP_NOZORDER | SWP_NOACTIVATE);
    host.positions.clear();
    host.contextMenus = 0;
    origin = point(window);
    expect(host.gesture.begin(window, window, origin, button, moving), "begin native capture");
    return bounds(window);
}
void lostRelease(Host &host, HWND window, bool moving) {
    const GestureButton button = moving ? GestureButton::Middle : GestureButton::Right;
    const UINT up = gestureButtonUp(button);
    POINT origin = {};
    RECT before = start(host, window, button, moving, origin);
    sendAt(window, WM_MOUSEMOVE, 0, {origin.x + 80, origin.y + 60});
    const bool pendingCompleted = host.gesture.takeCompleted();
    const RECT pendingBounds = bounds(window);
    std::printf("OBS pending-lost moving=%d changes=%zu completed=%d captured=%d\n",
        moving, host.positions.size(), pendingCompleted, GetCapture() != nullptr);
    expect(host.positions.empty(), "pending gesture cannot change geometry after a lost release");
    expect(!pendingCompleted && GetCapture() == nullptr && EqualRect(&before, &pendingBounds),
        "pending lost release ends capture without completing a drag");

    before = start(host, window, button, moving, origin);
    sendAt(window, WM_MOUSEMOVE, gestureButtonMask(button), origin);
    expect(host.positions.empty() && GetCapture() == window && !host.gesture.takeCompleted(),
        "valid pending movement retains capture without completing a drag");
    sendAt(window, up, 0, origin);
    expect(host.positions.empty() && !host.gesture.takeCompleted() && GetCapture() == nullptr,
        "stationary button-up remains a click");

    before = start(host, window, button, moving, origin);
    sendAt(window, up, 0, {origin.x + 80, origin.y + 60});
    RECT expected = before;
    if (moving) { expected.left += 80; expected.top += 60; }
    expected.right += 80; expected.bottom += 60;
    const RECT released = bounds(window);
    expect(EqualRect(&expected, &released) && host.gesture.takeCompleted() && GetCapture() == nullptr,
        "button-up applies the final point even without an intermediate mouse move");

    before = start(host, window, button, moving, origin);
    sendAt(window, WM_MOUSEMOVE, gestureButtonMask(button), {origin.x + 40, origin.y + 30});
    host.positions.clear();
    sendAt(window, WM_MOUSEMOVE, 0, {origin.x + 140, origin.y + 90});
    bool onlyRestore = !host.positions.empty();
    for (const auto &position : host.positions) if (!EqualRect(&position, &before)) onlyRestore = false;
    std::printf("OBS active-lost moving=%d changes=%zu onlyRestore=%d\n", moving, host.positions.size(), onlyRestore);
    expect(onlyRestore && host.gesture.takeCompleted() && GetCapture() == nullptr,
        "active lost release restores directly without applying the invalid pointer position");
}
RECT beginFinishing(Host &host, HWND window) {
    POINT origin = {};
    start(host, window, GestureButton::Middle, true, origin);
    sendAt(window, WM_MOUSEMOVE, MK_MBUTTON, {origin.x + 40, origin.y + 30});
    sendAt(window, WM_RBUTTONDOWN, MK_MBUTTON | MK_RBUTTON, {origin.x + 40, origin.y + 30});
    sendAt(window, WM_MBUTTONUP, MK_RBUTTON, {origin.x + 80, origin.y + 60});
    expect(GetCapture() == window && host.gesture.takeCompleted(), "primary up commits a drag while keeping only the secondary capture");
    host.positions.clear();
    return bounds(window);
}
void secondaryRelease(Host &host, HWND &window, Host &other, HWND otherWindow) {
    struct Button { UINT down, up, mask, extra; };
    const Button buttons[] = {
        {WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON, 0},
        {WM_MBUTTONDOWN, WM_MBUTTONUP, MK_MBUTTON, 0},
        {WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON, 0},
        {WM_XBUTTONDOWN, WM_XBUTTONUP, MK_XBUTTON1, XBUTTON1},
        {WM_XBUTTONDOWN, WM_XBUTTONUP, MK_XBUTTON2, XBUTTON2}
    };
    // Preserve the original cross-window/orphan-menu assertion for every button,
    // both before and after the threshold. Routing follows the actual capture.
    for (const auto &secondary : buttons) for (unsigned drag = 0; drag != 2; ++drag) {
        const GestureButton primary = secondary.mask == MK_MBUTTON ? GestureButton::Right : GestureButton::Middle;
        POINT origin = {};
        const RECT before = start(host, window, primary, primary == GestureButton::Middle, origin);
        other.contextMenus = 0;
        const POINT destination = {origin.x + (drag ? 40 : 0), origin.y + (drag ? 30 : 0)};
        sendAt(window, WM_MOUSEMOVE, gestureButtonMask(primary), destination);
        sendAt(window, secondary.down, MAKEWPARAM(gestureButtonMask(primary) | secondary.mask, secondary.extra), destination);
        sendAt(window, gestureButtonUp(primary), secondary.mask, destination);
        const RECT committed = bounds(window);
        expect(GetCapture() == window, "an absorbed secondary down keeps capture after the primary up");
        expect(host.gesture.takeCompleted() == (drag != 0), "secondary draining cannot turn a click into a drag");
        host.positions.clear();
        const POINT outside = point(otherWindow, 50, 50);
        sendAt(window, WM_MOUSEMOVE, secondary.mask, outside);
        const HWND releaseTarget = GetCapture() ? GetCapture() : otherWindow;
        sendAt(releaseTarget, secondary.up, MAKEWPARAM(0, secondary.extra), outside);
        const RECT after = bounds(window);
        std::printf("OBS secondary mask=%u drag=%u otherWindow=%d menusOrigin=%d menusOther=%d\n",
            secondary.mask, drag, releaseTarget == otherWindow, host.contextMenus, other.contextMenus);
        expect(host.contextMenus == 0 && other.contextMenus == 0,
            "secondary release consumed with its down must not generate an orphan context menu");
        expect(host.positions.empty() && EqualRect(&committed, &after) && GetCapture() == nullptr,
            "draining outside the window preserves final geometry and releases capture");
        expect(!host.gesture.takeCompleted(), "secondary up does not complete the gesture again");
        if (!drag) expect(EqualRect(&before, &after), "pending secondary sequence preserves initial geometry");
        sendAt(window, WM_RBUTTONDOWN, MK_RBUTTON, point(window, 20, 20));
        sendAt(window, WM_RBUTTONUP, 0, point(window, 20, 20));
        expect(host.contextMenus == 1, "the next independent right click remains native");
    }

    // An UP already received before the primary UP must not prolong capture.
    POINT origin = {};
    start(host, window, GestureButton::Middle, true, origin);
    sendAt(window, WM_MOUSEMOVE, MK_MBUTTON | MK_RBUTTON, {origin.x + 40, origin.y + 30});
    sendAt(window, WM_MBUTTONUP, MK_RBUTTON, {origin.x + 40, origin.y + 30});
    expect(GetCapture() == nullptr && host.gesture.takeCompleted(),
        "a button already held before the gesture cannot prolong capture without an absorbed down");

    start(host, window, GestureButton::Middle, true, origin);
    sendAt(window, WM_RBUTTONDOWN, MK_MBUTTON | MK_RBUTTON, origin);
    sendAt(window, WM_RBUTTONUP, MK_MBUTTON, origin);
    sendAt(window, WM_MBUTTONUP, 0, origin);
    expect(GetCapture() == nullptr && host.contextMenus == 0, "secondary up before primary up needs no finishing capture");

    // Multiple secondaries and a new DOWN during draining each retain their UP.
    beginFinishing(host, window);
    sendAt(window, WM_LBUTTONDOWN, MK_RBUTTON | MK_LBUTTON, point(window));
    sendAt(window, WM_RBUTTONUP, MK_LBUTTON, point(window));
    expect(GetCapture() == window && host.contextMenus == 0, "another absorbed down remains paired during draining");
    sendAt(window, WM_LBUTTONUP, 0, point(window));
    expect(GetCapture() == nullptr, "last paired secondary up frees capture");

    // Recover a lost secondary UP from native MK_* state; no timer or geometry.
    beginFinishing(host, window);
    sendAt(window, WM_MOUSEMOVE, 0, point(otherWindow));
    expect(GetCapture() == nullptr && host.positions.empty(), "lost secondary up is recovered on mouse movement without geometry changes");
    sendAt(window, WM_RBUTTONUP, 0, point(window));
    expect(host.contextMenus == 0, "a late secondary up after recovery is still consumed exactly once");

    const UINT cancellations[] = {WM_CANCELMODE, WM_KILLFOCUS, WM_KEYDOWN};
    for (const UINT message : cancellations) {
        const RECT committed = beginFinishing(host, window);
        host.reenterOnCaptureChange = true;
        SendMessageW(window, message, message == WM_KEYDOWN ? VK_ESCAPE : 0, 0);
        host.gesture.cancel(); host.gesture.stop();
        const RECT after = bounds(window);
        expect(GetCapture() == nullptr && EqualRect(&committed, &after) && host.positions.empty(),
            "finishing cancellation/reentrancy frees capture without rolling back committed geometry");
        sendAt(window, WM_LBUTTONDOWN, MK_LBUTTON, point(window));
        sendAt(window, WM_RBUTTONUP, 0, point(window));
        expect(host.contextMenus == 0, "unrelated down cannot erase a pending secondary up after cancellation");
        expect(!host.gesture.takeCompleted(), "cancellation after completion cannot complete again");
    }

    beginFinishing(host, window);
    host.reenterOnCaptureChange = true;
    SetCapture(otherWindow);
    host.gesture.cancel(); host.gesture.stop();
    expect(GetCapture() == otherWindow, "capture loss and repeated teardown never release or reclaim foreign capture");
    ReleaseCapture();

    // A cancelled button's pending UP survives an unrelated new gesture.
    beginFinishing(host, window); host.gesture.cancel();
    expect(host.gesture.begin(window, window, point(window), GestureButton::Middle, true), "new gesture after finishing cancellation");
    sendAt(window, WM_LBUTTONDOWN, MK_MBUTTON | MK_RBUTTON | MK_LBUTTON, point(window));
    sendAt(window, WM_MBUTTONUP, MK_RBUTTON | MK_LBUTTON, point(window));
    host.contextMenus = 0;
    sendAt(window, WM_RBUTTONUP, MK_LBUTTON, point(window));
    expect(GetCapture() == window && host.contextMenus == 0, "pending cancelled UP remains paired during a later finishing sequence");
    sendAt(window, WM_LBUTTONUP, 0, point(window));
    expect(GetCapture() == nullptr, "later finishing sequence also drains normally");

    beginFinishing(host, window);
    host.reenterOnCaptureChange = true;
    DestroyWindow(window); window = nullptr;
    host.gesture.cancel(); host.gesture.stop();
    expect(GetCapture() == nullptr, "window destruction leaves no finishing capture");
}
} // namespace

bool logHRESULT(long result, const char *, int, const char *) { return SUCCEEDED(result); }
void logLastError(const char *, int, const char *expression) { expect(false, expression); }

int main(int argc, char **argv) {
    WNDCLASSW definition = {}; definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpszClassName = L"FileSpacer gesture input regression"; definition.lpfnWndProc = hostProc;
    if (!RegisterClassW(&definition)) return 2;
    Host first, second;
    HWND window = CreateWindowExW(0, definition.lpszClassName, L"first", WS_OVERLAPPEDWINDOW,
        50, 50, 400, 300, nullptr, nullptr, definition.hInstance, &first);
    if (!window) return 2;
    if (argc == 1) { lostRelease(first, window, false); lostRelease(first, window, true); }
    else if (argc == 2 && std::strcmp(argv[1], "--secondary") == 0) {
        HWND other = CreateWindowExW(0, definition.lpszClassName, L"second", WS_OVERLAPPEDWINDOW,
            600, 50, 400, 300, nullptr, nullptr, definition.hInstance, &second);
        if (!other) return 2;
        secondaryRelease(first, window, second, other);
        second.gesture.stop(); DestroyWindow(other);
    } else return 2;
    first.gesture.stop(); if (window) DestroyWindow(window);
    std::printf("%s: controlled native HWND input sequences, %d failure(s); no real Shell claim\n",
        failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
