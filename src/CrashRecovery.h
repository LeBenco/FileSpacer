#pragma once
#include "common.h"
#include <windows.h>

namespace filespacer { namespace recovery {
// Called before COM, settings dialogs and folder windows. Maintenance commands exit here.
// A shared file handle prevents CLI resets while another updated instance is running.
bool handleCommandLine(int argc, wchar_t **argv, int *exitCode);
// Starts the helper process that writes a minidump and a report if this process crashes.
bool start();
bool hasSettingsAccess();
}} // namespace
