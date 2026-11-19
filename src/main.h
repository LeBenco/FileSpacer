#pragma once
#include <common.h>
#include <shobjidl.h>
#include "Settings.h"

namespace filespacer {

// AppUserModelID of the process and base of its windows' IDs. The /test mode has
// its own: a taskbar group and a Jump List belong to an AppUserModelID.
inline const wchar_t *appID() {
    return settings::testMode ? L"FileSpacer.FileSpacer.Test" : L"FileSpacer.FileSpacer";
}

// The main STA exits when windows, COM server locks and Shell references end.
void lockProcess();
void unlockProcess();

// Main STA only. Publish the lifetime reference for this invocation; work that
// acquires it may safely outlive the call and the last application window.
HRESULT invokeShellCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info);

} // namespace
