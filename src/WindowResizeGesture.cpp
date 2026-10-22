#include "WindowResizeGesture.h"
#include <windowsx.h>
#include <algorithm>
#include <cstdlib>

namespace filespacer {

bool WindowResizeGesture::begin(HWND target, HWND control, POINT point) {
    if (source || GetCapture() || IsZoomed(target) || IsIconic(target)
            || !(GetWindowLongPtr(target, GWL_STYLE) & WS_THICKFRAME))
        return false;
    RECT bounds = {};
    if (!GetWindowRect(target, &bounds)) return false;
    HCURSOR resizeCursor = LoadCursor(nullptr, IDC_SIZENWSE);
    if (!resizeCursor) return false;
    MINMAXINFO tracking = {};
    // FileSpacer is system-DPI-aware, like its existing frame/layout code.
    // Populate the documented defaults before allowing the host to override.
    tracking.ptMinTrackSize = {GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK)};
    tracking.ptMaxTrackSize = {GetSystemMetrics(SM_CXMAXTRACK), GetSystemMetrics(SM_CYMAXTRACK)};
    SendMessage(target, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&tracking));
    if (!IsWindow(target) || !IsWindow(control)) return false;
    window = target;
    source = control;
    origin = point;
    initialBounds = bounds;
    limits = tracking;
    cursor = resizeCursor;
    dragging = false;
    swallowRightUp = false;
    swallowAltUp = true;
    SetCapture(control);
    if (GetCapture() != control) {
        source = nullptr;
        window = nullptr;
        swallowAltUp = false;
        return false;
    }
    return true;
}

void WindowResizeGesture::update(POINT point) {
    const LONG dx = point.x - origin.x;
    const LONG dy = point.y - origin.y;
    if (!dragging) {
        if (std::abs(dx) <= std::abs(GetSystemMetrics(SM_CXDRAG))
                && std::abs(dy) <= std::abs(GetSystemMetrics(SM_CYDRAG)))
            return;
        dragging = true;
        previousCursor = SetCursor(cursor);
    } else {
        SetCursor(cursor);
    }
    const LONG width = (std::max)(limits.ptMinTrackSize.x,
        (std::min)(limits.ptMaxTrackSize.x, initialBounds.right - initialBounds.left + dx));
    const LONG height = (std::max)(limits.ptMinTrackSize.y,
        (std::min)(limits.ptMaxTrackSize.y, initialBounds.bottom - initialBounds.top + dy));
    // WM_SIZE/WM_WINDOWPOSCHANGED continue through the host's normal layout
    // and geometry tracking. No artificial enter/exit-size-loop messages.
    if (!checkLE(SetWindowPos(window, nullptr, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)))
        cancel();
}

void WindowResizeGesture::finish(bool restore, bool swallowRelease) {
    if (!source) return;
    const HWND target = window;
    const HWND control = source;
    const RECT bounds = initialBounds;
    const bool changed = dragging;
    const HCURSOR oldCursor = previousCursor;
    // Clear state before calls that synchronously dispatch capture/size events.
    source = nullptr;
    window = nullptr;
    dragging = false;
    previousCursor = nullptr;
    swallowRightUp = swallowRelease;
    completed = changed;
    if (restore && changed && IsWindow(target))
        SetWindowPos(target, nullptr, bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    if (GetCapture() == control) ReleaseCapture();
    if (changed) SetCursor(oldCursor);
}

void WindowResizeGesture::cancel() { finish(true, true); }
void WindowResizeGesture::stop() { finish(false, true); }

bool WindowResizeGesture::takeCompleted() {
    const bool result = completed;
    completed = false;
    return result;
}

bool WindowResizeGesture::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    // Alt belongs to the completed gesture too: releasing it must not activate
    // the menu after the right button has already been released.
    if (message == WM_SYSKEYDOWN && wParam == VK_MENU) swallowAltUp = false;
    if (message == WM_SYSKEYUP && wParam == VK_MENU && swallowAltUp) {
        swallowAltUp = false;
        return true;
    }
    if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK)
        swallowRightUp = false;
    if (!source) {
        if (swallowRightUp && message == WM_RBUTTONUP) {
            swallowRightUp = false;
            return true;
        }
        return false;
    }
    if (message == WM_DESTROY || message == WM_NCDESTROY) {
        stop();
        return false;
    }
    if (message == WM_CAPTURECHANGED || message == WM_CANCELMODE || message == WM_KILLFOCUS) {
        cancel();
        return false; // The control must still receive lifecycle/focus messages.
    }
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wParam == VK_ESCAPE) {
        cancel();
        return true;
    }
    if (message == WM_SETCURSOR && dragging) {
        SetCursor(cursor);
        return true;
    }
    if (message == WM_MOUSEMOVE || message == WM_RBUTTONUP) {
        if (GetCapture() != source) { cancel(); return true; }
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (ClientToScreen(source, &point)) update(point);
        if (message == WM_RBUTTONUP) finish(false, false);
        else if (!(wParam & MK_RBUTTON)) cancel();
        return true;
    }
    // Reserve the whole gesture; no partial click or wheel reaches the view.
    return (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
        || (message >= WM_KEYFIRST && message <= WM_KEYLAST);
}

} // namespace filespacer
