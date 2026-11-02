#include "WindowResizeGesture.h"
#include <windowsx.h>
#include <algorithm>
#include <cstdlib>

namespace filespacer {

namespace {
UINT buttonDownMask(UINT message, WPARAM wParam) {
    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: return MK_LBUTTON;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: return MK_MBUTTON;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: return MK_RBUTTON;
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MK_XBUTTON1 : MK_XBUTTON2;
    default: return 0;
    }
}
UINT buttonUpMask(UINT message, WPARAM wParam) {
    switch (message) {
    case WM_LBUTTONUP: return MK_LBUTTON;
    case WM_MBUTTONUP: return MK_MBUTTON;
    case WM_RBUTTONUP: return MK_RBUTTON;
    case WM_XBUTTONUP:
        return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MK_XBUTTON1 : MK_XBUTTON2;
    default: return 0;
    }
}
} // namespace

bool WindowResizeGesture::begin(HWND target, HWND control, POINT point, GestureButton trigger, bool moving) {
    if (source || finishingSource || GetCapture() || IsZoomed(target) || IsIconic(target)
            || !(GetWindowLongPtr(target, GWL_STYLE) & WS_THICKFRAME))
        return false;
    RECT bounds = {};
    if (!GetWindowRect(target, &bounds)) return false;
    HCURSOR resizeCursor = LoadCursor(nullptr, moving ? IDC_SIZEALL : IDC_SIZENWSE);
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
    button = trigger;
    move = moving;
    absorbedButtons = 0;
    swallowButtonUps &= ~gestureButtonMask(trigger);
    swallowAltUp = GetKeyState(VK_MENU) < 0;
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
    if (move) {
        if (!checkLE(SetWindowPos(window, nullptr, initialBounds.left + dx, initialBounds.top + dy,
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE))) cancel();
        return;
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
    const bool drain = !restore && !swallowRelease && absorbedButtons && GetCapture() == control;
    // Clear state before calls that synchronously dispatch capture/size events.
    source = nullptr;
    window = nullptr;
    dragging = false;
    previousCursor = nullptr;
    if (swallowRelease) swallowButtonUps |= absorbedButtons | gestureButtonMask(button);
    if (drain) finishingSource = control;
    else absorbedButtons = 0;
    completed = changed;
    if (restore && changed && IsWindow(target))
        SetWindowPos(target, nullptr, bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    if (!drain && GetCapture() == control) ReleaseCapture();
    if (changed) SetCursor(oldCursor);
}

void WindowResizeGesture::releaseFinishingCapture() {
    const HWND control = finishingSource;
    finishingSource = nullptr;
    swallowButtonUps |= absorbedButtons;
    absorbedButtons = 0;
    // ReleaseCapture synchronously sends WM_CAPTURECHANGED, including to us.
    if (control && GetCapture() == control) ReleaseCapture();
}

void WindowResizeGesture::cancel() { finish(true, true); releaseFinishingCapture(); }
void WindowResizeGesture::stop() { finish(false, true); releaseFinishingCapture(); }

bool WindowResizeGesture::handleFinishingMessage(UINT message, WPARAM wParam) {
    if (GetCapture() != finishingSource || message == WM_CAPTURECHANGED
            || message == WM_CANCELMODE || message == WM_KILLFOCUS
            || message == WM_DESTROY || message == WM_NCDESTROY) {
        releaseFinishingCapture();
        return false;
    }
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wParam == VK_ESCAPE) {
        releaseFinishingCapture();
        return true;
    }
    if (message < WM_MOUSEFIRST || message > WM_MOUSELAST) return false;
    const UINT down = buttonDownMask(message, wParam);
    const UINT up = buttonUpMask(message, wParam);
    const bool consumed = down || (up & absorbedButtons) || message == WM_MOUSEMOVE;
    // Only DOWNs actually absorbed by us prolong capture. The native MK_*
    // state also recovers a lost UP on the next mouse message, without a timer.
    // Retain ownership if that UP is merely late; a new DOWN clears it.
    swallowButtonUps |= absorbedButtons & ~(LOWORD(wParam) | up);
    absorbedButtons = (absorbedButtons | down) & LOWORD(wParam) & ~up;
    if (!absorbedButtons) releaseFinishingCapture();
    return consumed;
}

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
    const UINT down = buttonDownMask(message, wParam);
    const UINT up = buttonUpMask(message, wParam);
    // A new DOWN supersedes only that button's stale UP, as in PowerToys.
    const bool swallowUp = (swallowButtonUps & up) != 0;
    swallowButtonUps &= ~(down | up);
    if (finishingSource) return handleFinishingMessage(message, wParam) || swallowUp;
    if (!source) {
        return swallowUp;
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
    if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) {
        swallowButtonUps |= absorbedButtons & ~(LOWORD(wParam) | up);
        absorbedButtons = (absorbedButtons | down) & LOWORD(wParam)
            & ~up & ~gestureButtonMask(button);
    }
    if (message == WM_MOUSEMOVE || message == gestureButtonUp(button)) {
        // A button-up still supplies its final point without the trigger bit.
        // A mouse move without that bit must not update even transiently.
        if (GetCapture() != source || (message == WM_MOUSEMOVE
                && !(wParam & gestureButtonMask(button)))) { cancel(); return true; }
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (ClientToScreen(source, &point)) update(point);
        if (message == gestureButtonUp(button)) finish(false, false);
        return true;
    }
    // Reserve the whole gesture; no partial click or wheel reaches the view.
    return (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
        || (message >= WM_KEYFIRST && message <= WM_KEYLAST);
}

} // namespace filespacer
