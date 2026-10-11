#include "Update.h"
#include "Settings.h"
#include "resource.h"
#include <vector>
#include <wininet.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <tchar.h>

namespace filespacer {

const wchar_t UPDATE_URL[] = L"https://github.com/LeBenco/FileSpacer/releases/latest/download/FileSpacer-update.txt";
const int MAX_DOWNLOAD_SIZE = 1024;

static UpdateInfo updateInfo;
static HANDLE updateThread = nullptr;
static std::atomic_bool stoppingUpdates{false};
static std::atomic<HWND> notificationWindow{nullptr};

DWORD WINAPI updateCheckProc(void *);

void autoUpdateCheck() {
    if (!stoppingUpdates.load() && settings::getUpdateCheckEnabled()) {
        LONGLONG lastCheck = settings::getLastUpdateCheck();
        LONGLONG interval = settings::getUpdateCheckRate();
        FILETIME fileTime;
        GetSystemTimeAsFileTime(&fileTime); // https://stackoverflow.com/a/1695332/11525734
        LONGLONG curTime = (LONGLONG)fileTime.dwLowDateTime
            + ((LONGLONG)(fileTime.dwHighDateTime) << 32LL);
        if (curTime - lastCheck > interval) {
            if (updateThread) {
                if (WaitForSingleObject(updateThread, 0) == WAIT_OBJECT_0) {
                    checkLE(CloseHandle(updateThread));
                    updateThread = nullptr;
                } else {
                    return; // still running!
                }
            }
            if (checkLE(SHCreateThreadWithHandle(updateCheckProc, nullptr, CTF_COINIT_STA,
                    nullptr, &updateThread)))
                settings::setLastUpdateCheck(curTime);
        }
    }
}

void stopUpdateChecks() {
    stoppingUpdates.store(true);
    HWND window = notificationWindow.load();
    if (window) PostMessageW(window, WM_CLOSE, 0, 0);
}

void resumeUpdateChecks() { stoppingUpdates.store(false); }

void waitForUpdateThread() {
    if (updateThread) {
        WaitForSingleObject(updateThread, INFINITE);
        checkLE(CloseHandle(updateThread));
        updateThread = nullptr;
    }
}

LRESULT CALLBACK notifyWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_USER) {
        if (LOWORD(lParam) == NIN_BALLOONUSERCLICK) {
            openUpdate(updateInfo);
            PostQuitMessage(0);
        } else if (LOWORD(lParam) == NIN_BALLOONTIMEOUT || LOWORD(lParam) == NIN_BALLOONHIDE) {
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

DWORD WINAPI updateCheckProc(void *) {
    debugPrintf(L"Checking for updates...\n");
    if (checkUpdate(&updateInfo, &stoppingUpdates) || !updateInfo.isNewer
            || stoppingUpdates.load())
        return 0;
    debugPrintf(L"Update available!\n");

    HINSTANCE instance = GetModuleHandle(nullptr);
    WNDCLASS notifyClass = {};
    notifyClass.lpfnWndProc = notifyWindowProc;
    notifyClass.hInstance = instance;
    notifyClass.lpszClassName = L"FileSpacer Update Notify";
    if (!checkLE(RegisterClass(&notifyClass)))
        return 0;
    HWND messageWindow = checkLE(CreateWindow(notifyClass.lpszClassName, L"FileSpacer", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr));
    if (!messageWindow) {
        checkLE(UnregisterClass(notifyClass.lpszClassName, instance));
        return 0;
    }

    notificationWindow.store(messageWindow);
    if (stoppingUpdates.load()) PostMessageW(messageWindow, WM_CLOSE, 0, 0);

    // https://learn.microsoft.com/en-us/windows/win32/shell/notification-area
    NOTIFYICONDATA notify = {sizeof(notify)};
    notify.uFlags = NIF_INFO | NIF_ICON | NIF_MESSAGE;
    notify.uCallbackMessage = WM_USER;
    notify.hWnd = messageWindow;
    notify.dwInfoFlags = NIIF_RESPECT_QUIET_TIME;
    notify.uVersion = NOTIFYICON_VERSION_4;
    LoadString(instance, IDS_UPDATE_NOTIF_INFO, notify.szInfo, _countof(notify.szInfo));
    LoadString(instance, IDS_UPDATE_NOTIF_TITLE, notify.szInfoTitle, _countof(notify.szInfoTitle));
    checkHR(LoadIconMetric(instance, MAKEINTRESOURCE(IDR_APP_ICON), LIM_SMALL, &notify.hIcon));
    if (checkLE(Shell_NotifyIcon(NIM_ADD, &notify))) {
        checkLE(Shell_NotifyIcon(NIM_SETVERSION, &notify));

        // keep thread running to keep message window open
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        checkLE(Shell_NotifyIcon(NIM_DELETE, &notify));
    }
    if (notify.hIcon)
        checkLE(DestroyIcon(notify.hIcon));
    notificationWindow.store(nullptr);
    checkLE(DestroyWindow(messageWindow));
    checkLE(UnregisterClass(notifyClass.lpszClassName, instance));
    return 0;
}

DWORD checkUpdate(UpdateInfo *info, const std::atomic_bool *cancel) {
    if (!info) return ERROR_INVALID_PARAMETER;
    auto cancelled = [cancel]() { return cancel && cancel->load(); };
    if (cancelled()) return ERROR_CANCELLED;
    HINTERNET internet = InternetOpenW(L"FileSpacer/" _T(FILESPACER_VERSION_STRING),
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet) return GetLastError();
    DWORD timeout = 15000;
    InternetSetOptionW(internet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(internet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(internet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    HINTERNET connection = InternetOpenUrlW(internet, UPDATE_URL, nullptr, 0,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_RELOAD, 0);
    DWORD error = connection ? ERROR_SUCCESS : GetLastError();
    std::string data;
    if (connection) {
        DWORD status = 0, size = sizeof(status);
        if (!HttpQueryInfoW(connection, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                &status, &size, nullptr)) error = GetLastError();
        else if (status != HTTP_STATUS_OK) error = status == HTTP_STATUS_NOT_FOUND
            ? ERROR_FILE_NOT_FOUND : ERROR_HTTP_INVALID_SERVER_RESPONSE;
        char buffer[256];
        while (!error) {
            if (cancelled()) { error = ERROR_CANCELLED; break; }
            DWORD received = 0;
            if (!InternetReadFile(connection, buffer, sizeof(buffer), &received)) {
                error = GetLastError(); break;
            }
            if (!received) break;
            if (data.size() + received > MAX_DOWNLOAD_SIZE) {
                error = ERROR_INVALID_DATA; break;
            }
            data.append(buffer, received);
        }
        InternetCloseHandle(connection);
    }
    InternetCloseHandle(internet);
    if (cancelled()) return ERROR_CANCELLED;
    if (error) return error;
    ReleaseManifest manifest;
    if (!parseUpdateManifest(data, &manifest)) return ERROR_INVALID_DATA;
    static_cast<ReleaseManifest &>(*info) = std::move(manifest);
    info->isNewer = info->version > makeVersion(FILESPACER_VERSION);
    return ERROR_SUCCESS;
}

void openUpdate(const UpdateInfo &info) {
    ShellExecuteA(nullptr, "open", info.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace
