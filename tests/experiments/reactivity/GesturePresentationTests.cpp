// Desktop-image observer for an explicitly owned FileSpacer gesture fixture.
#include <common.h>
#include "DPI.h"
#include <atlbase.h>
#include <shlobj.h>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include "PresentationTiming.h"

namespace {
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;
Handle eventHandle(const std::wstring &name) {
    HANDLE value = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, name.c_str());
    presentation::check(value != nullptr, "Cannot open gesture synchronization event");
    return Handle(value, &CloseHandle);
}
void waitEvent(HANDLE event) {
    presentation::check(WaitForSingleObject(event, 120000) == WAIT_OBJECT_0,
        "Gesture synchronization timed out");
}
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 6) return 2;
    try {
        filespacer::initDPI();
        wchar_t *end = nullptr;
        const unsigned long processId = wcstoul(argv[1], &end, 10);
        presentation::check(processId && end && !*end, "Invalid fixture PID");
        const auto windowValue = _wcstoui64(argv[2], &end, 10);
        presentation::check(windowValue && end && !*end, "Invalid fixture HWND");
        const HWND body = reinterpret_cast<HWND>(static_cast<UINT_PTR>(windowValue));
        DWORD ownerId = 0;
        presentation::check(GetWindowThreadProcessId(body, &ownerId) && ownerId == processId,
            "Gesture window owner changed");
        const HWND frame = GetAncestor(body, GA_ROOT);
        wchar_t className[128] = {};
        presentation::check(GetClassNameW(frame, className, _countof(className))
            && wcscmp(className, L"FileSpacer Item Window") == 0, "Not a FileSpacer fixture window");
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId), &CloseHandle);
        presentation::check(process != nullptr, "Cannot inspect fixture executable");
        wchar_t path[32768] = {}; DWORD length = _countof(path);
        presentation::check(QueryFullProcessImageNameW(process.get(), 0, path, &length)
            && _wcsicmp(path, argv[3]) == 0, "Unexpected fixture executable");
        const std::wstring stem = argv[5];
        auto ready = eventHandle(stem + L"-ready");
        auto begin = eventHandle(stem + L"-begin");
        auto started = eventHandle(stem + L"-started");
        auto returned = eventHandle(stem + L"-returned");
        // The existing gesture fixture owns its geometry. Capture its exact
        // rectangle; desktop coverage and final images are still required.
        presentation::Observation observation(body, argv[4], {}, true);
        presentation::check(SetEvent(ready.get()) != FALSE, "Cannot publish observer readiness");
        waitEvent(begin.get());
        presentation::check(IsWindow(body) && GetForegroundWindow() == frame,
            "Fixture no longer foreground at gesture start");
        presentation::requireUncovered(body, observation.region);
        observation.begin();
        std::printf("GESTURE_MARKER qpc=%lld rate=%lld\n", observation.began.load(), observation.rate);
        std::fflush(stdout);
        presentation::check(SetEvent(started.get()) != FALSE, "Cannot acknowledge gesture marker");
        waitEvent(returned.get());
        observation.apiReturned();
        observation.finish();
        RECT actual = {};
        const HWND foreground = GetForegroundWindow();
        const bool unchanged = IsWindow(body) && GetWindowRect(body, &actual)
            && EqualRect(&actual, &observation.region) && foreground == frame;
        if (!unchanged) {
            DWORD foregroundProcess = 0; GetWindowThreadProcessId(foreground, &foregroundProcess);
            std::fprintf(stderr, "OBSERVATION frame=%p foreground=%p pid=%lu expected=(%ld,%ld,%ld,%ld) actual=(%ld,%ld,%ld,%ld)\n",
                static_cast<void *>(frame), static_cast<void *>(foreground), foregroundProcess,
                observation.region.left, observation.region.top, observation.region.right, observation.region.bottom,
                actual.left, actual.top, actual.right, actual.bottom);
        }
        presentation::check(unchanged,
            "Gesture window moved or lost foreground during observation");
        presentation::check(observation.quiet, "Gesture image did not settle");
        const auto images = presentation::boundaries(observation.images, observation.began.load());
        presentation::check(images.firstChange && !images.returnedToBaseline,
            "No lasting visible selection change");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
