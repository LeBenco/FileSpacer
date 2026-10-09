#include "GDIUtils.h"
#include "GeomUtils.h"
#include <windowsx.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <map>

#pragma comment(lib, "UxTheme.lib")

namespace filespacer {

void makeBitmapOpaque(HDC hdc, const RECT &rect) {
    // https://devblogs.microsoft.com/oldnewthing/20210915-00/?p=105687
    // thank you Raymond Chen :)
    BITMAPINFO bitmapInfo = {{sizeof(BITMAPINFOHEADER), 1, 1, 1, 32, BI_RGB}};
    RGBQUAD bitmapBits = { 0x00, 0x00, 0x00, 0xFF };
    StretchDIBits(hdc, rect.left, rect.top, rectWidth(rect), rectHeight(rect),
                  0, 0, 1, 1, &bitmapBits, &bitmapInfo,
                  DIB_RGB_COLORS, SRCPAINT);
}

HBITMAP iconToPARGB32Bitmap(HICON icon, int width, int height) {
    HDC hdcMem = CreateCompatibleDC(nullptr);
    BITMAPINFO bitmapInfo = {{sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB}};
    HBITMAP bitmap = nullptr;
    if ((bitmap = checkLE(CreateDIBSection(hdcMem, &bitmapInfo, DIB_RGB_COLORS,
                                           nullptr, nullptr, 0))) != nullptr) {
        SelectBitmap(hdcMem, bitmap);
        checkLE(DrawIconEx(hdcMem, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL));
        // TODO convert to premultiplied alpha?
    }
    DeleteDC(hdcMem);
    return bitmap;
}

static COLORREF navigationIconColors[2];
static bool navigationIconColorsValid = false;
static COLORREF navigationIconBackground = CLR_INVALID;

// HACK: the Back button's colors can be baked into its theme image rather
// than exposed as COLOR properties. Render NAV_BACKBUTTON in its normal/disabled
// states on our toolbar background, then reuse the most frequent non-background
// RGB value for the Lucide icons. This is a pixel heuristic, not a theme color
// contract: a multicolored/framed image may yield a different dominant color.
// No .msstyles parsing, resource IDs, atlas slicing or fixed sampling point.
static bool sampleNavigationIconColor(HTHEME theme, int state,
        COLORREF background, COLORREF &color) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc)
        return false;
    SIZE size = {};
    if (FAILED(GetThemePartSize(theme, dc, NAV_BACKBUTTON, state,
            nullptr, TS_TRUE, &size)) || size.cx <= 0 || size.cy <= 0) {
        DeleteDC(dc);
        return false;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) {
        DeleteDC(dc);
        return false;
    }
    HGDIOBJ previous = SelectObject(dc, bitmap);
    if (!previous || previous == HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(dc);
        return false;
    }

    DWORD *pixels = static_cast<DWORD *>(bits);
    const int pixelCount = (int)(size.cx * size.cy);
    // A 32-bit BI_RGB DIB stores BGR, not COLORREF's RGB byte order.
    const DWORD backgroundPixel = ((DWORD)GetRValue(background) << 16)
        | ((DWORD)GetGValue(background) << 8) | GetBValue(background);
    for (int i = 0; i < pixelCount; ++i)
        pixels[i] = backgroundPixel;
    RECT rect = {0, 0, size.cx, size.cy};
    const HRESULT hr = DrawThemeBackground(theme, dc, NAV_BACKBUTTON,
        state, &rect, nullptr);
    // Synchronize GDI before reading the DIB's memory directly.
    const BOOL flushed = GdiFlush();
    unsigned bestCount = 0;
    DWORD bestPixel = 0;
    if (SUCCEEDED(hr) && flushed) {
        std::map<DWORD, unsigned> counts;
        for (int i = 0; i < pixelCount; ++i) {
            const DWORD pixel = pixels[i] & 0x00FFFFFF;
            if (pixel == backgroundPixel)
                continue;
            const unsigned count = ++counts[pixel];
            if (count > bestCount) {
                bestCount = count;
                bestPixel = pixel;
            }
        }
    }
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    if (!bestCount)
        return false;
    color = RGB((bestPixel >> 16) & 0xFF, (bestPixel >> 8) & 0xFF, bestPixel & 0xFF);
    return true;
}

COLORREF getNavigationIconColor(bool disabled, COLORREF background) {
    if (!navigationIconColorsValid || background != navigationIconBackground) {
        navigationIconBackground = background;
        navigationIconColors[0] = GetSysColor(COLOR_BTNTEXT);
        navigationIconColors[1] = GetSysColor(COLOR_GRAYTEXT);
        // Keep the user's system colors in high contrast or without visual styles.
        HIGHCONTRAST contrast = {sizeof(contrast)};
        if (SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &contrast, 0)
                && !(contrast.dwFlags & HCF_HIGHCONTRASTON)) {
            HTHEME theme = OpenThemeData(nullptr, L"Navigation");
            if (theme) {
                // Aero-like fallback if this theme's image cannot be sampled.
                navigationIconColors[0] = RGB(128, 128, 128);
                navigationIconColors[1] = RGB(191, 191, 191);
                sampleNavigationIconColor(theme, NAV_BB_NORMAL,
                    background, navigationIconColors[0]);
                sampleNavigationIconColor(theme, NAV_BB_DISABLED,
                    background, navigationIconColors[1]);
                CloseThemeData(theme);
            }
        }
        navigationIconColorsValid = true;
    }
    return navigationIconColors[disabled ? 1 : 0];
}

void invalidateNavigationIconColors() {
    navigationIconColorsValid = false;
}

} // namespace
