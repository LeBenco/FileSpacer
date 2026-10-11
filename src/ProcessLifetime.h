#pragma once
#include <common.h>
#include <windows.h>

namespace filespacer {

const wchar_t PROCESS_CONTROL_CLASS[] = L"FileSpacer Process Control";
constexpr UINT PROCESS_CANCEL_QUIT = WM_APP + 32;
constexpr ULONG_PTR PROCESS_SAVE_AND_QUIT = 0x46535351; // WM_COPYDATA operation.
// Main STA only; the endpoint itself does not hold an application lifetime lock.
bool initProcessLifetime(void (*onQuit)(), void (*onResume)(),
    bool (*onSaveSession)(const wchar_t *));
void uninitProcessLifetime();
bool isProcessQuitRequested();

} // namespace filespacer
