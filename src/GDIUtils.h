#pragma once
#include <common.h>

#include <windows.h>

namespace filespacer {

void makeBitmapOpaque(HDC hdc, const RECT &rect);
HBITMAP iconToPARGB32Bitmap(HICON icon, int width, int height);

// Cached normal/disabled navigation glyph colors sampled from the themed Back button.
COLORREF getNavigationIconColor(bool disabled, COLORREF background);
void invalidateNavigationIconColors();

} // namespace
