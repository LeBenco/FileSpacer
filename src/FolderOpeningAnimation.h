#pragma once
#include <common.h>
#include <memory>
#include <functional>
#include <windows.h>

namespace filespacer {

// A dotted grey outline; the Shell window keeps its final size throughout.
class FolderOpeningAnimation {
public:
    static constexpr UINT durationMs = 100;
    // Warm the shared graphics resources off the UI thread after a folder opens.
    static void prepare();
    static void uninit();

    FolderOpeningAnimation();
    ~FolderOpeningAnimation();
    FolderOpeningAnimation(const FolderOpeningAnimation &) = delete;
    FolderOpeningAnimation &operator=(const FolderOpeningAnimation &) = delete;
    bool begin(HWND window, const RECT &origin, int showCommand);
    void whenFinished(const std::function<void()> &callback);

private:
    struct State;
    std::unique_ptr<State> state;
};

} // namespace filespacer
