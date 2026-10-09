#include "TaskbarOwnerWindow.h"
#include "ItemWindow.h"
#include <shellapi.h>
#include <propkey.h>

namespace filespacer {

const wchar_t TASKBAR_OWNER_CLASS[] = L"FileSpacer Taskbar Owner";

void TaskbarOwnerWindow::init() {
    WNDCLASS ownerClass = {};
    ownerClass.lpszClassName = TASKBAR_OWNER_CLASS;
    ownerClass.lpfnWndProc = windowProc;
    ownerClass.hInstance = GetModuleHandle(nullptr);
    RegisterClass(&ownerClass);
}

TaskbarOwnerWindow::TaskbarOwnerWindow(ItemWindow *folder, int showCommand)
        : folderWindow(folder) {
    debugPrintf(L"Create taskbar owner\n");
    HWND window = checkLE(CreateWindowEx(0,
        TASKBAR_OWNER_CLASS, nullptr, WS_POPUP, 0, 0, 0, 0,
        nullptr, nullptr, GetModuleHandle(nullptr), (WindowImpl *)this));
    if (showCommand != -1)
        ShowWindow(window, showCommand); // show in taskbar
}

TaskbarOwnerWindow::~TaskbarOwnerWindow() {
    debugPrintf(L"Destroy taskbar owner\n");
    setPreview(nullptr);
    CComPtr<IPropertyStore> propStore;
    if (checkHR(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&propStore)))) {
        PROPVARIANT empty = {VT_EMPTY};
        checkHR(propStore->SetValue(PKEY_AppUserModel_ID, empty));
        checkHR(propStore->SetValue(PKEY_AppUserModel_PreventPinning, empty));
        checkHR(propStore->SetValue(PKEY_AppUserModel_RelaunchCommand, empty));
        checkHR(propStore->SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource, empty));
        checkHR(propStore->SetValue(PKEY_AppUserModel_RelaunchIconResource, empty));
    }
    DestroyWindow(hwnd);
}

HWND TaskbarOwnerWindow::getWnd() {
    return hwnd;
}


void TaskbarOwnerWindow::setPreview(HWND newPreview) {
    CComPtr<ITaskbarList4> taskbar;
    if (!checkHR(taskbar.CoCreateInstance(__uuidof(TaskbarList))))
        return;

    if (preview) {
        checkHR(taskbar->UnregisterTab(preview));
    }
    preview = newPreview;
    if (preview) {
        checkHR(taskbar->RegisterTab(preview, hwnd));
        checkHR(taskbar->SetTabOrder(preview, nullptr));
        checkHR(taskbar->SetTabProperties(preview, STPF_USEAPPPEEKALWAYS));
    }
}

void TaskbarOwnerWindow::setText(const wchar_t *text) {
    SetWindowText(hwnd, text);
}

void TaskbarOwnerWindow::setIcon(HICON smallIcon, HICON largeIcon) {
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)largeIcon);
}

void TaskbarOwnerWindow::setEnabled(bool enabled) {
    EnableWindow(folderWindow->hwnd, enabled);
}

LRESULT TaskbarOwnerWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
            // default behavior is to destroy owned windows without calling WM_CLOSE.
            // Forward the request so the folder follows its normal persistence/close path.
            debugPrintf(L"Close taskbar owner\n");
            folderWindow->close();
            return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace
