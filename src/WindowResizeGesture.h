#pragma once
#include <common.h>
#include <windows.h>

namespace filespacer {

// Application-managed right-button gesture. It deliberately does not enter
// Windows' left-button modal sizing loop or synthesize mouse input.
class WindowResizeGesture {
public:
    bool begin(HWND window, HWND source, POINT screenPoint);
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
    bool swallowRightUp = false;
    bool swallowAltUp = false;
};

} // namespace filespacer
