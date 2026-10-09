#pragma once
#include "common.h"
#include <windows.h>
#include <commctrl.h>

namespace filespacer { namespace recovery {
// Called before COM, settings dialogs and folder windows. Maintenance commands exit here.
// A shared file handle prevents CLI resets while another updated instance is running.
bool handleCommandLine(int argc, wchar_t **argv, int *exitCode);
bool start(bool restarted);
bool canUseFullNames();
bool hasSettingsAccess();
void blockFullNames();

struct LabelDraw {
    DWORD stage;
    DWORD itemType;
    UINT state;
    DWORD_PTR item;
    HDC dc;
};
LabelDraw readLabelDraw(const NMLVCUSTOMDRAW *draw);
DWORD labelView(HWND control);
BOOL labelRect(HWND control, int item, RECT *rect);
COLORREF labelBackground(HWND control);
void fillLabel(HDC dc, const RECT *rect, COLORREF color);
}} // namespace
