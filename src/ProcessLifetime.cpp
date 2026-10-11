#include "ProcessLifetime.h"
#include <atomic>
#include <algorithm>
#include <objbase.h>

namespace filespacer {
namespace {
std::atomic_bool quitting{false};
HWND controlWindow = nullptr;
void (*quitCallback)() = nullptr;
void (*resumeCallback)() = nullptr;
bool (*saveSessionCallback)(const wchar_t *) = nullptr;

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
    bool saved = false;
    if (message == WM_COPYDATA) {
        const auto data = reinterpret_cast<const COPYDATASTRUCT *>(lParam);
        if (!data || data->dwData != PROCESS_SAVE_AND_QUIT || !data->lpData
                || data->cbData < sizeof(wchar_t) || data->cbData > 65536
                || data->cbData % sizeof(wchar_t) != 0 || !saveSessionCallback) return FALSE;
        const auto filename = static_cast<const wchar_t *>(data->lpData);
        const size_t count = data->cbData / sizeof(wchar_t);
        if (filename[count - 1] != L'\0'
                || std::find(filename, filename + count - 1, L'\0') != filename + count - 1
                || quitting.exchange(true)) return FALSE;
        CoSuspendClassObjects(); // Capture cannot admit new/reentrant folder opens.
        try { saved = saveSessionCallback(filename); }
        catch (...) { saved = false; }
        if (!saved) {
            quitting.store(false);
            CoResumeClassObjects();
            return FALSE; // A failed save never closes this process's windows.
        }
    }
    if (message == WM_CLOSE || saved) {
        if (saved || !quitting.exchange(true)) {
            if (!saved) CoSuspendClassObjects();
            if (quitCallback) quitCallback();
        }
        // Repeated requests also close windows published by an earlier queued call.
        EnumWindows(closeWindow, 0);
        return 1;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

bool initProcessLifetime(void (*onQuit)(), void (*onResume)(),
        bool (*onSaveSession)(const wchar_t *)) {
    quitCallback = onQuit;
    resumeCallback = onResume;
    saveSessionCallback = onSaveSession;
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
    saveSessionCallback = nullptr;
}

bool isProcessQuitRequested() { return quitting.load(); }

} // namespace filespacer
