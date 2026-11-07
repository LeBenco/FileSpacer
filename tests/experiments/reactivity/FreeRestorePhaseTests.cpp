#include <common.h>
#include <Windows.h>
#include <cstdio>
#include <cstring>

extern "C" void restorePhase(const char *name) {
    static LARGE_INTEGER frequency = {}, start = {}, previous = {};
    static bool active = false;
    LARGE_INTEGER now = {}; QueryPerformanceCounter(&now);
    if (std::strcmp(name, "begin") == 0) {
        QueryPerformanceFrequency(&frequency); start = previous = now; active = true;
    }
    if (!active) return;
    const double scale = 1000.0 / static_cast<double>(frequency.QuadPart);
    std::printf("PHASE name=%s delta_ms=%.6f total_ms=%.6f\n", name,
        static_cast<double>(now.QuadPart - previous.QuadPart) * scale,
        static_cast<double>(now.QuadPart - start.QuadPart) * scale);
    previous = now;
    if (std::strcmp(name, "end") == 0) active = false;
}
#include "RestoreAllTests.cpp"
