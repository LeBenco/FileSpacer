#pragma once
#include <common.h>

namespace filespacer {

const wchar_t APP_ID[] = L"FileSpacer.FileSpacer";

// The main STA exits when windows, COM server locks and Shell references end.
void lockProcess();
void unlockProcess();

} // namespace
