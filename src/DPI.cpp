#include "DPI.h"

namespace filespacer {

// DPI.h
int systemDPI = USER_DEFAULT_SCREEN_DPI;

void initDPI() {
    if (HDC screen = checkLE(GetDC(nullptr))) {
        systemDPI = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(nullptr, screen);
    }
}

int scaleDPI(int dp) {
    return MulDiv(dp, systemDPI, USER_DEFAULT_SCREEN_DPI);
}

SIZE scaleDPI(SIZE size) {
    return {scaleDPI(size.cx), scaleDPI(size.cy)};
}

POINT scaleDPI(POINT p) {
    return {scaleDPI(p.x), scaleDPI(p.y)};
}

int invScaleDPI(int px) {
    return MulDiv(px, USER_DEFAULT_SCREEN_DPI, systemDPI);
}

SIZE invScaleDPI(SIZE size) {
    return {invScaleDPI(size.cx), invScaleDPI(size.cy)};
}

POINT invScaleDPI(POINT p) {
    return {invScaleDPI(p.x), invScaleDPI(p.y)};
}

} // namespace
