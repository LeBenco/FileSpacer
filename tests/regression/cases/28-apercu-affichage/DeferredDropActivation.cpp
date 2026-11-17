#include <common.h>
#include <windows.h>

// Reach one real activation boundary deterministically, without a timing race.
#pragma data_seg(".shared")
HWND activationTarget = nullptr;
UINT placementMessage = 0;
LONG sawRelease = 0, fired = 0, activationProof = 0;
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

static HMODULE module = nullptr;
static HHOOK hook = nullptr;

static LRESULT CALLBACK observe(int code, WPARAM removed, LPARAM data) {
    if (code == HC_ACTION && removed == PM_REMOVE) {
        const auto message = reinterpret_cast<const MSG *>(data);
        if (message->message == WM_LBUTTONUP) InterlockedExchange(&sawRelease, 1);
        if (message->message == placementMessage && message->lParam == 0 && sawRelease
                && !GetCapture() && InterlockedCompareExchange(&fired, 1, 0) == 0) {
            const BOOL accepted = SetForegroundWindow(activationTarget);
            const DWORD result = (accepted ? 1u : 0u)
                | (GetForegroundWindow() == activationTarget ? 2u : 0u);
            InterlockedExchange(&activationProof, static_cast<LONG>(result));
        }
    }
    return CallNextHookEx(nullptr, code, removed, data);
}

extern "C" __declspec(dllexport) BOOL WINAPI installActivationHook(DWORD thread, HWND target, UINT message) {
    if (hook || !target || !message) return FALSE;
    activationTarget = target; placementMessage = message;
    sawRelease = fired = activationProof = 0;
    hook = SetWindowsHookExW(WH_GETMESSAGE, observe, module, thread);
    return hook != nullptr;
}

extern "C" __declspec(dllexport) DWORD WINAPI readActivationProof() {
    return (InterlockedCompareExchange(&sawRelease, 0, 0) ? 1u : 0u)
        | (InterlockedCompareExchange(&fired, 0, 0) ? 2u : 0u)
        | (static_cast<DWORD>(InterlockedCompareExchange(&activationProof, 0, 0)) << 2);
}

extern "C" __declspec(dllexport) BOOL WINAPI removeActivationHook() {
    if (!hook) return TRUE;
    const BOOL result = UnhookWindowsHookEx(hook); hook = nullptr; return result;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) module = instance;
    return TRUE;
}
