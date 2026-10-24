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
struct Canvas {
    std::unique_ptr<std::remove_pointer_t<HDC>, decltype(&DeleteDC)> dc{CreateCompatibleDC(nullptr), DeleteDC};
    std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype(&DeleteObject)> bitmap{nullptr, DeleteObject};
    HGDIOBJ previous = nullptr;
    uint32_t *pixels = nullptr;
    int size;
    explicit Canvas(int value) : size(value) {
        BITMAPINFO info = {{sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB}};
        void *bits = nullptr;
        bitmap.reset(CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0));
        require(dc && bitmap && bits, "native offscreen canvas creation");
        pixels = static_cast<uint32_t *>(bits);
        previous = SelectObject(dc.get(), bitmap.get());
        clear();
    }
    ~Canvas() { if (previous) SelectObject(dc.get(), previous); }
    void clear() { for (int i = 0; i < size*size; ++i) pixels[i] = 0xFFFF00FF; }
    void verify(bool monochrome, bool white) {
        GdiFlush();
        int colored = 0, foreground = 0;
        for (int i = 0; i < size*size; ++i) {
            const uint32_t rgb = pixels[i] & 0x00FFFFFF;
            const unsigned r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
            if (r > 180 && g > 60 && b < 80) ++colored;
            if (rgb == (white ? 0x00FFFFFFu : 0u)) ++foreground;
        }
        if (monochrome) require(foreground > 2 && colored == 0, "high contrast must use only the selected system foreground");
        else require(colored > 2, "native icon must retain the chosen orange/yellow padlock");
        require((pixels[0] & 0x00FFFFFF) == 0x00FF00FF, "transparent corner must preserve the underlying icon");
    }
};
int main() {
    try {
        for (int size : {12, 18, 24, 36, 48}) {
            Canvas canvas(size);
            for (bool highContrast : {false, true}) {
                for (bool white : {false, true}) {
                    canvas.clear();
                    const COLORREF color = white ? RGB(255,255,255) : RGB(0,0,0);
                    require(drawLockBadge(canvas.dc.get(),0,0,size,color,highContrast), "draw real embedded ICO badge");
                    canvas.verify(highContrast, white);
                }
            }
        }
        std::cout << "PASS: native ICO rendering, transparency, DPI sizes and high-contrast colors.\n";
    } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
    return 0;
}
