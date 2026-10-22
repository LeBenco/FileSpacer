#pragma once
#include <common.h>
#include <shobjidl.h>

namespace filespacer {

const wchar_t APP_ID[] = L"FileSpacer.FileSpacer";

// The main STA exits when windows, COM server locks and Shell references end.
void lockProcess();
void unlockProcess();

// Main STA only. Publish the lifetime reference for this invocation; work that
// acquires it may safely outlive the call and the last application window.
HRESULT invokeShellCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info);

} // namespace
