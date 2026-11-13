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
// Signaled while an update installs: every process of the installed image has
// to leave then, including one that cannot link the function above.
const wchar_t UPDATE_INSTALL_EVENT[] = L"Local\\FileSpacer.UpdateInstall";

} // namespace filespacer
