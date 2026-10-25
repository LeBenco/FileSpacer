#pragma once
#include <common.h>
#include <windows.h>
#include "WindowShortcuts.h"

namespace filespacer {

// Shared capture for resizing with any button and moving with middle/right.
// Left-button movement retains Windows' native modal move loop.
class WindowResizeGesture {
public:
    bool begin(HWND window, HWND source, POINT screenPoint,
        GestureButton button = GestureButton::Right, bool move = false);
    bool handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void cancel();
    void stop(); // Teardown commits the current size without resizing a dying window.
    bool takeCompleted();

private:
    void update(POINT point);
    void finish(bool restore, bool swallowRelease);
    HWND window = nullptr;
    HWND source = nullptr;
    POINT origin = {};
    RECT initialBounds = {};
    MINMAXINFO limits = {};
    HCURSOR cursor = nullptr;
    HCURSOR previousCursor = nullptr;
    bool dragging = false;
    bool completed = false;
    GestureButton button = GestureButton::Right;
    bool move = false;
    bool swallowButtonUp = false;
    bool swallowAltUp = false;
};

} // namespace filespacer
