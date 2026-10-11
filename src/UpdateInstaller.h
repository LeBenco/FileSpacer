#pragma once
#include <common.h>
#include <windows.h>

namespace filespacer {

// Owns the update button, status label and asynchronous jobs on the About page.
void initUpdateControls(HWND page);
bool handleUpdateDialogMessage(MSG *message);
void closeUpdateDialogs();
// Called before COM/recovery initialization; internal relay runs from a temp copy.
bool handleUpdateCommandLine(int argc, wchar_t **argv, int *exitCode);
bool isUpdateInstallationRunning();

} // namespace filespacer
