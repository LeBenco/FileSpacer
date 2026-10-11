#pragma once
#include <common.h>
#include <windows.h>

namespace filespacer {

const wchar_t PROCESS_CONTROL_CLASS[] = L"FileSpacer Process Control";
constexpr UINT PROCESS_CANCEL_QUIT = WM_APP + 32;
// Main STA only; the endpoint itself does not hold an application lifetime lock.
bool initProcessLifetime(void (*onQuit)(), void (*onResume)());
void uninitProcessLifetime();
bool isProcessQuitRequested();

} // namespace filespacer
