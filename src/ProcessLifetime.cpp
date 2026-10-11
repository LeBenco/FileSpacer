#include "ProcessLifetime.h"
#include <atomic>
#include <objbase.h>

namespace filespacer {
namespace {
std::atomic_bool quitting{false};
HWND controlWindow = nullptr;
void (*quitCallback)() = nullptr;
void (*resumeCallback)() = nullptr;

BOOL CALLBACK closeWindow(HWND window, LPARAM) {
    DWORD process = 0;
    const DWORD thread = GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId()) return TRUE;
    wchar_t name[64] = {};
    if (GetClassNameW(window, name, ARRAYSIZE(name))
            && (lstrcmpW(name, L"FileSpacer Item Window") == 0
                || (thread != GetCurrentThreadId() && lstrcmpW(name, L"#32770") == 0)))
        PostMessageW(window, WM_CLOSE, 0, 0);
    return TRUE;
}

LRESULT CALLBACK controlProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == PROCESS_CANCEL_QUIT) {
        if (quitting.exchange(false)) {
            CoResumeClassObjects();
            if (resumeCallback) resumeCallback();
        }
        return 1;
    }
    if (message == WM_CLOSE) {
        if (!quitting.exchange(true)) {
            CoSuspendClassObjects();
            if (quitCallback) quitCallback();
        }
        // Repeated requests also close windows published by an earlier queued call.
        EnumWindows(closeWindow, 0);
        return 1;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

bool initProcessLifetime(void (*onQuit)(), void (*onResume)()) {
    quitCallback = onQuit;
    resumeCallback = onResume;
    WNDCLASSW cls = {};
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpfnWndProc = controlProc;
    cls.lpszClassName = PROCESS_CONTROL_CLASS;
    if (!RegisterClassW(&cls)) return false;
    // A hidden top-level endpoint is discoverable across application processes.
    controlWindow = CreateWindowExW(0, cls.lpszClassName, L"", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, cls.hInstance, nullptr);
    if (!controlWindow) UnregisterClassW(cls.lpszClassName, cls.hInstance);
    return controlWindow != nullptr;
}

void uninitProcessLifetime() {
    if (controlWindow) DestroyWindow(controlWindow);
    controlWindow = nullptr;
    UnregisterClassW(PROCESS_CONTROL_CLASS, GetModuleHandleW(nullptr));
    quitCallback = nullptr;
    resumeCallback = nullptr;
}

bool isProcessQuitRequested() { return quitting.load(); }

} // namespace filespacer
