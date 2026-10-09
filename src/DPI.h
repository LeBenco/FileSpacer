#pragma once
#include <common.h>

#include <windows.h>

namespace filespacer {

extern int systemDPI;

// to be used in SetViewModeAndIconSize (does not scale with DPI)
const int SHELL_SMALL_ICON = 16;

void initDPI();

int scaleDPI(int dp);
SIZE scaleDPI(SIZE size);
POINT scaleDPI(POINT p);
int invScaleDPI(int px);
SIZE invScaleDPI(SIZE size);
POINT invScaleDPI(POINT p);

} // namespace
