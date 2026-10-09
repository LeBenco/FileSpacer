#pragma once
#include "common.h"
#include "COMUtils.h"
#include "WinUtils.h"

namespace filespacer {

class ItemWindow;

// Owns one folder window's taskbar entry; can be replaced without rebuilding its view.
class TaskbarOwnerWindow : public WindowImpl, public UnknownImpl {
public:
    static void init();
    TaskbarOwnerWindow(ItemWindow *folderWindow, int showCommand = -1);
    ~TaskbarOwnerWindow();
    HWND getWnd();
    void setPreview(HWND newPreview);
    void setText(const wchar_t *text);
    void setIcon(HICON smallIcon, HICON largeIcon);
    void setEnabled(bool enabled);

protected:
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    ItemWindow *folderWindow;
    HWND preview = nullptr;
};

} // namespace
