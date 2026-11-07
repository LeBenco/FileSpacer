#include "common.h"
#include <windows.h>
#include <cstdint>
#include <vector>
#include <cstring>
#include <memory>
#include <type_traits>
#include <iostream>
#include <stdexcept>
#include "resource.h"
namespace filespacer {
#include "LockBadge-under-test.inc"
}
using namespace filespacer;
static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
enum class Palette : uint8_t { Orange, Gray };
static SIZE badgeSize(int resource) {
    std::unique_ptr<std::remove_pointer_t<HICON>, decltype(&DestroyIcon)> icon(
        static_cast<HICON>(LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(resource),
            IMAGE_ICON, 0, 0, 0)), DestroyIcon);
    ICONINFO info = {};
    require(icon && GetIconInfo(icon.get(), &info), "load native badge dimensions");
    using Bitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype(&DeleteObject)>;
    Bitmap mask(info.hbmMask, DeleteObject), source(info.hbmColor, DeleteObject);
    BITMAP bitmap = {};
    require(source && GetObject(source.get(), sizeof(bitmap), &bitmap)
        && bitmap.bmWidth > 0 && bitmap.bmHeight > 0, "read native badge dimensions");
    return {bitmap.bmWidth, bitmap.bmHeight};
}
struct Canvas {
    std::unique_ptr<std::remove_pointer_t<HDC>, decltype(&DeleteDC)> dc{CreateCompatibleDC(nullptr), DeleteDC};
    std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype(&DeleteObject)> bitmap{nullptr, DeleteObject};
    HGDIOBJ previous = nullptr;
    uint32_t *pixels = nullptr;
    SIZE size;
    explicit Canvas(SIZE value) : size(value) {
        BITMAPINFO info = {{sizeof(BITMAPINFOHEADER), size.cx, -size.cy, 1, 32, BI_RGB}};
        void *bits = nullptr;
        bitmap.reset(CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0));
        require(dc && bitmap && bits, "native offscreen canvas creation");
        pixels = static_cast<uint32_t *>(bits);
        previous = SelectObject(dc.get(), bitmap.get());
        clear();
    }
    ~Canvas() { if (previous) SelectObject(dc.get(), previous); }
    void clear() { for (int i = 0; i < size.cx*size.cy; ++i) pixels[i] = 0xFFFF00FF; }
    void verify(bool monochrome, bool white, Palette palette) {
        GdiFlush();
        int colored = 0, gray = 0, foreground = 0;
        for (int i = 0; i < size.cx*size.cy; ++i) {
            const uint32_t rgb = pixels[i] & 0x00FFFFFF;
            const unsigned r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
            if (r > 180 && g > 60 && b < 80) ++colored;
            if (r == g && g == b && r >= 128 && r < 240) ++gray;
            if (rgb == (white ? 0x00FFFFFFu : 0u)) ++foreground;
        }
        if (monochrome) require(foreground > 2 && colored == 0, "high contrast must use only the selected system foreground");
        else if (palette == Palette::Orange)
            require(colored > 2, "locked icon must retain the chosen orange/yellow padlock");
        else require(gray > 2 && colored == 0, "unlocked icon must retain the chosen gray padlock");
        require((pixels[0] & 0x00FFFFFF) == 0x00FF00FF, "transparent corner must preserve the underlying icon");
    }
};
int main() {
    try {
        const SIZE reference = badgeSize(IDR_LOCK_BADGE_ICON);
        for (int resource : {IDR_LOCK_BADGE_ICON, IDR_UNLOCK_BADGE_ICON}) {
            const SIZE native = badgeSize(resource);
            require(native.cx > native.cy, "badge canvas must preserve space for the open shackle");
            require(native.cx*reference.cy == native.cy*reference.cx, "badge states must share their canvas proportions");
            const Palette palette = resource == IDR_LOCK_BADGE_ICON ? Palette::Orange : Palette::Gray;
            for (int height : {12, 18, 24, 36, 48}) {
                const SIZE size = {MulDiv(height, native.cx, native.cy), height};
                Canvas canvas(size);
                for (bool highContrast : {false, true}) {
                    for (bool white : {false, true}) {
                        canvas.clear();
                        const COLORREF color = white ? RGB(255,255,255) : RGB(0,0,0);
                        require(drawLockBadge(resource,canvas.dc.get(),0,0,size,color,highContrast), "draw real embedded ICO badge");
                        canvas.verify(highContrast, white, palette);
                    }
                }
            }
        }
        std::cout << "PASS: both native ICO states, shared rectangular proportions, transparency, DPI sizes and high-contrast colors.\n";
    } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
    return 0;
}
