#include "main.h"
#include "FolderWindow.h"
#include "ShellUtils.h"
#include "CreateItemWindow.h"
#include "ClosedWindows.h"
#include "Settings.h"
#include "CrashRecovery.h"
#include "SettingsDialog.h"
#include "Update.h"
#include "UpdateInstaller.h"
#include "UpdateSession.h"
#include "ProcessLifetime.h"
#include "ExecuteCommand.h"
#include "DPI.h"
#include "UIStrings.h"
#include "WinUtils.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <cwchar>
#include <new>
#include <cstdlib>

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "Shlwapi.lib")

using namespace filespacer;

const wchar_t SHELL_PREFIX[] = L"shell:";

enum LaunchType {LAUNCH_FAIL, LAUNCH_HEADLESS, LAUNCH_AUTO, LAUNCH_FOUND, LAUNCH_REOPEN, LAUNCH_RESTORE_UPDATE};

#ifdef FILESPACER_DEBUG
int main(int, char**) {
    return wWinMain(nullptr, nullptr, nullptr, SW_SHOWNORMAL);
}

bool logHRESULT(long hr, const char *file, int line, const char *expr) {
    if (SUCCEEDED(hr))
        return true;
    local_wstr_ptr message = getErrorMessage(hr);
    debugPrintf(L"Error 0x%lX: %s\n    in %S (%S:%d)\n",
        static_cast<unsigned long>(hr), message.get(), expr, file, line);
    return false;
}

void logLastError(const char *file, int line, const char *expr) {
    DWORD error = GetLastError();
    local_wstr_ptr message = getErrorMessage(error);
    debugPrintf(L"Error %lu: %s\n    in %S (%S:%d)\n", error, message.get(), expr, file, line);
}
#endif

LaunchType parseStartingFolder(int argc, wchar_t **argv, wstr_ptr *folder);
LaunchType createStartingWindow(wchar_t *path, int showCommand);
DWORD WINAPI checkLastVersion(void *);
void showWelcomeDialog();
HRESULT WINAPI welcomeDialogCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR data);
DWORD WINAPI updateJumpList(void *);

namespace {

LONG processLockCount = 0;
SRWLOCK processLock = SRWLOCK_INIT;
HANDLE processIdleEvent = nullptr;

bool isProcessIdle() {
    AcquireSRWLockShared(&processLock);
    bool idle = processLockCount == 0;
    ReleaseSRWLockShared(&processLock);
    return idle;
}

bool getProcessMessage(MSG &msg, int &exitCode) {
    for (;;) {
        // Dispatch STA calls while Shell work outlives the application windows.
        DWORD wait = MsgWaitForMultipleObjectsEx(1, &processIdleEvent, INFINITE,
            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_FAILED) {
            (void)checkLE(FALSE);
            // A broken wait cannot safely tear down a still-referenced STA.
            std::abort();
        }
        if (wait == WAIT_OBJECT_0) {
            // A new window/reference may have invalidated an earlier idle wakeup.
            if (isProcessIdle())
                return false;
            continue;
        }
        if (!PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            continue;
        if (msg.message == WM_QUIT) {
            exitCode = static_cast<int>(msg.wParam);
            if (isProcessIdle())
                return false;
            continue;
        }
        return true;
    }
}

// The initial reference belongs to main and does not keep the message loop alive.
// Each additional Shell reference shares the existing application lifetime count.
class ShellThreadReference : public UnknownImpl {
public:
    STDMETHODIMP_(ULONG) AddRef() override {
        lockProcess();
        return UnknownImpl::AddRef();
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG references = UnknownImpl::Release();
        if (references != 0)
            unlockProcess();
        return references;
    }
};

// Borrowed from main's initial reference, which remains alive until process idle.
// Accessed only by commands dispatched on the main STA.
ShellThreadReference *shellCommandReference = nullptr;

class ScopedShellThreadReference {
public:
    explicit ScopedShellThreadReference(IUnknown *reference) : current(reference) {
        result = SHGetThreadRef(&previous);
        if (SUCCEEDED(result) || result == E_NOINTERFACE) {
            result = SHSetThreadRef(current);
            installed = SUCCEEDED(result);
        }
    }

    ~ScopedShellThreadReference() {
        // Unpublishing does not release references already acquired by workers.
        // Restore on this STA, even when InvokeCommand closes the last window.
        if (installed && !checkHR(SHSetThreadRef(previous)))
            std::abort(); // A dangling TLS reference would make teardown unsafe.
    }

    HRESULT status() const { return result; }
    ScopedShellThreadReference(const ScopedShellThreadReference &) = delete;
    ScopedShellThreadReference &operator=(const ScopedShellThreadReference &) = delete;

private:
    CComPtr<IUnknown> current; // Also keeps an active invocation in the process count.
    CComPtr<IUnknown> previous;
    HRESULT result = E_UNEXPECTED;
    bool installed = false;
};

} // namespace

HRESULT filespacer::invokeShellCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info) {
    if (!menu || !info)
        return E_POINTER;
    if (!shellCommandReference)
        return E_UNEXPECTED;
    // Publish only during explicit invocation, not view creation, icon lookup,
    // QueryContextMenu or TrackPopupMenu. Nested commands restore their caller.
    ScopedShellThreadReference reference(shellCommandReference);
    if (FAILED(reference.status()))
        return reference.status();
    return menu->InvokeCommand(info);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int showCommand) {
    // Try the Windows display language, then fall back to English.
    ULONG languageCount = 0;
    ULONG languageBufferSize = 0;
    std::wstring uiLanguages;
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &languageCount,
            nullptr, &languageBufferSize) && languageBufferSize > 1) {
        uiLanguages.resize(languageBufferSize);
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &languageCount,
                &uiLanguages[0], &languageBufferSize)) {
            // The first preferred UI language is the Windows display language.
            uiLanguages.resize(lstrlen(uiLanguages.c_str()) + 1);
        } else {
            uiLanguages.clear();
        }
    }
    uiLanguages.append(L"en-US", 6); // Include the language's terminating null.
    uiLanguages.push_back(L'\0'); // Terminate the language list.

    checkLE(SetProcessPreferredUILanguages(MUI_LANGUAGE_NAME, uiLanguages.c_str(), nullptr));
    checkLE(SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, uiLanguages.c_str(), nullptr));

    int maintenanceExit = 0, recoveryArgc = 0;
    wchar_t **recoveryArgv = CommandLineToArgvW(GetCommandLineW(), &recoveryArgc);
    if (!recoveryArgv) return 1;
    const bool maintenance = handleUpdateCommandLine(recoveryArgc, recoveryArgv, &maintenanceExit)
        || recovery::handleCommandLine(recoveryArgc, recoveryArgv, &maintenanceExit);
    bool restarted = false;
    for (int i = 1; i < recoveryArgc; ++i) {
        restarted |= lstrcmpiW(recoveryArgv[i], L"/recovery-restarted") == 0;
    }
    wstr_ptr startingFolder;
    std::wstring updateSession;
    if (!maintenance && recoveryArgc == 3 && lstrcmpiW(recoveryArgv[1], L"/restore-update") == 0)
        updateSession = recoveryArgv[2];
    const LaunchType requestedLaunch = maintenance ? LAUNCH_FAIL : !updateSession.empty()
        ? LAUNCH_RESTORE_UPDATE : parseStartingFolder(recoveryArgc, recoveryArgv, &startingFolder);
    LocalFree(recoveryArgv);
    if (maintenance) return maintenanceExit;
    // Leave immediately so a new launch cannot keep the installed image mapped.
    if (isUpdateInstallationRunning()) return 0;
    if (requestedLaunch == LAUNCH_FAIL) return 1;
    // Resolve relative arguments first. Neither the application nor its recovery
    // helper may keep a user folder locked as the process working directory.
    // Set this once, before COM or application worker threads are started.
    wchar_t workingDirectory[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(workingDirectory, _countof(workingDirectory));
    if (!length || length >= _countof(workingDirectory)
            || !checkLE(SetCurrentDirectoryW(workingDirectory))) return 1;
    if (!recovery::start(restarted, startingFolder.get())) {
        if (!recovery::hasSettingsAccess()) {
            MessageBoxW(nullptr, getString(IDS_SETTINGS_ACCESS_FAILED), getString(IDS_APP_NAME),
                MB_OK | MB_ICONERROR);
            return 3;
        }
        // Keep the ordinary Shell view available even when crash protection cannot start.
        recovery::blockFullNames();
        MessageBoxW(nullptr, getString(IDS_RECOVERY_UNAVAILABLE), getString(IDS_APP_NAME),
            MB_OK | MB_ICONWARNING);
    }

#ifdef FILESPACER_MEMLEAKS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
    debugPrintf(L"Compiled with memory leak detection\n");
#endif

    if (!checkHR(OleInitialize(0))) // needed for drag/drop
        return 0;

    // Auto-reset notification: a modal loop cannot consume this wakeup.
    // Keep it alive until every Shell/application Release has finished signaling.
    CHandle idleEvent(checkLE(CreateEvent(nullptr, FALSE, FALSE, nullptr)));
    if (!idleEvent) {
        OleUninitialize();
        return 1;
    }
    processIdleEvent = idleEvent;
    CComPtr<ShellThreadReference> threadReference;
    threadReference.Attach(new (std::nothrow) ShellThreadReference());
    if (!threadReference) {
        threadReference.Release();
        processIdleEvent = nullptr;
        OleUninitialize();
        return 1;
    }

    INITCOMMONCONTROLSEX controls = {sizeof(controls)};
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_USEREX_CLASSES
        | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&controls);

    if (!initProcessLifetime([]() {
            stopUpdateChecks();
            closeUpdateDialogs();
            requestSettingsDialogClose();
            // Headless COM servers can already have no references to release.
            SetEvent(processIdleEvent);
        }, resumeUpdateChecks, saveUpdateWindows)) {
        threadReference.Release();
        processIdleEvent = nullptr;
        OleUninitialize();
        return 1;
    }

    if (settings::getAdminWarning()) {
        TOKEN_ELEVATION elevation = {};
        HANDLE procToken;
        if (checkLE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &procToken))) {
            DWORD size = sizeof(elevation);
            checkLE(GetTokenInformation(procToken, TokenElevation,
                &elevation, sizeof(elevation), &size));
            CloseHandle(procToken);
        }
        if (elevation.TokenIsElevated) {
            TASKDIALOGCONFIG config = {sizeof(config)};
            config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
            config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
            config.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_NAME);
            config.pszMainIcon = TD_WARNING_ICON;
            config.pszContent = MAKEINTRESOURCE(IDS_ADMIN_WARNING);
            config.nDefaultButton = IDNO;
            config.pszVerificationText = MAKEINTRESOURCE(IDS_DONT_ASK);
            config.pfCallback = [](HWND hwnd, UINT msg, WPARAM, LPARAM, LONG_PTR) -> HRESULT {
                if (msg == TDN_CREATED) {
                    SendMessage(hwnd, WM_SETICON, ICON_BIG, 0); // this is what TaskDialog does
                    SendMessage(hwnd, WM_SETICON, ICON_SMALL, 0);
                }
                return S_OK;
            };
            int result = 0;
            BOOL dontShowAgain = false;
            checkHR(TaskDialogIndirect(&config, &result, nullptr, &dontShowAgain));
            if (result != IDYES || isProcessQuitRequested()) {
                uninitProcessLifetime();
                threadReference.Release();
                processIdleEvent = nullptr;
                OleUninitialize();
                return 0;
            }
            if (dontShowAgain) // only counts if user chose yes!
                settings::setAdminWarning(false);
        }
    }

    // Do not publish this anchor for passive view/icon-handler caches.
    shellCommandReference = threadReference;
    initDPI();
    ItemWindow::init();
    FolderWindow::init();

    // https://docs.microsoft.com/en-us/windows/win32/shell/appids
    checkHR(SetCurrentProcessExplicitAppUserModelID(APP_ID));

    debugPrintf(L"%s\n", GetCommandLine());
    LaunchType type;
    if (requestedLaunch == LAUNCH_REOPEN || requestedLaunch == LAUNCH_RESTORE_UPDATE) {
        const FolderOpenResult opened = requestedLaunch == LAUNCH_REOPEN ? reopenLastWindow(nullptr)
            : restoreUpdateWindows(updateSession.c_str());
        type = opened == FolderOpenResult::Created ? LAUNCH_AUTO
            : (opened == FolderOpenResult::Activated || opened == FolderOpenResult::OpenedExternally
                ? LAUNCH_FOUND : LAUNCH_FAIL);
    } else {
        type = requestedLaunch == LAUNCH_HEADLESS ? LAUNCH_HEADLESS
            : createStartingWindow(startingFolder.get(), showCommand);
    }
    const bool runServer = type != LAUNCH_FAIL && type != LAUNCH_FOUND;

    FSExecuteFactory executeFactory;
    HRESULT regHR = E_FAIL;
    DWORD regCookie = 0;
    // https://devblogs.microsoft.com/oldnewthing/20100503-00/?p=14183
    if (runServer) {
        regHR = CoRegisterClassObject(CLSID_FSExecute, &executeFactory,
            CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &regCookie);
        checkHR(regHR);
    }

    HANDLE jumpListThread = nullptr, versionThread = nullptr;
    if (runServer) {
        SHCreateThreadWithHandle(updateJumpList, nullptr, CTF_COINIT_STA, nullptr, &jumpListThread);
        SHCreateThreadWithHandle(checkLastVersion, nullptr, 0, nullptr, &versionThread);
    }

    autoUpdateCheck();

    MSG msg = {};
    int exitCode = 0;
    while (runServer || !isProcessIdle()) {
        if (!getProcessMessage(msg, exitCode)) {
            if (SUCCEEDED(regHR)) {
                checkHR(CoRevokeClassObject(regCookie));
                regHR = E_FAIL;
            }
            // Revoking the COM factory can dispatch an already pending request.
            if (isProcessIdle())
                break;
            continue;
        }
#ifdef FILESPACER_DEBUG
        ULONGLONG tick = GetTickCount64();
#endif
        if (handleUpdateDialogMessage(&msg)) continue;
        {
            // A nested command can close or replace the globally active window.
            CComPtr<ItemWindow> activeWindow = ItemWindow::activeWindow;
            if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) {
                // The initial press can target an inactive folder window.
                if (ItemWindow *target = ItemWindow::fromChildWindow(msg.hwnd))
                    activeWindow = target;
            }
            if (activeWindow && activeWindow->handleTopLevelMessage(&msg))
                continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
#ifdef FILESPACER_DEBUG
        ULONGLONG time = GetTickCount64() - tick;
        if (time > 100 && msg.message != WM_NCLBUTTONDOWN) {
            wchar_t className[64] = L"Unknown Class";
            GetClassName(msg.hwnd, className, _countof(className));
            debugPrintf(L"%s took %llu ms to handle 0x%04X!\n", className, time, msg.message);
        }
#endif
    }

    if (SUCCEEDED(regHR))
        checkHR(CoRevokeClassObject(regCookie));

    if (runServer) {
        WaitForSingleObject(jumpListThread, INFINITE);
        WaitForSingleObject(versionThread, INFINITE);
        checkLE(CloseHandle(jumpListThread));
        checkLE(CloseHandle(versionThread));
    }
    stopUpdateChecks();
    waitForUpdateThread();
    uninitProcessLifetime();

    ItemWindow::uninit();
    shellCommandReference = nullptr;
    threadReference.Release();
    processIdleEvent = nullptr;
    OleUninitialize();

#ifdef FILESPACER_MEMLEAKS
    if (MEMLEAK_COUNT) {
        auto message = format(L"Detected memory leak (count: %1!d!)", MEMLEAK_COUNT);
        MessageBox(nullptr, message.get(), L"Memory leak!", MB_ICONERROR);
    }
#endif
    debugPrintf(L"Goodbye\n");
    return exitCode;
}

LaunchType parseStartingFolder(int argc, wchar_t **argv, wstr_ptr *folder) {
    wstr_ptr pathAlloc;
    wchar_t *path = nullptr;
    bool reopen = false;
    for (int i = 1; i < argc; i++) {
        wchar_t *arg = argv[i];
        if (lstrcmpi(arg, L"/embedding") == 0 || lstrcmpi(arg, L"-embedding") == 0) {
            // https://devblogs.microsoft.com/oldnewthing/20100503-00/?p=14183
            debugPrintf(L"Started as local COM server\n");
            return LAUNCH_HEADLESS;
        } else if (lstrcmpiW(arg, L"/reopen-last") == 0) {
            reopen = true;
        } else if (lstrcmpiW(arg, L"/recovery-restarted") == 0) {
            // Internal one-shot restart marker, not a folder path.
            continue;
        } else if (lstrcmpi(arg, L"/test") == 0) {
#ifdef FILESPACER_DEBUG
            settings::testMode = true;
#else
            return LAUNCH_FAIL;
#endif
        } else if (!path) {
            int argLen = lstrlen(arg);
            if (argLen > 0 && arg[argLen - 1] == '"')
                arg[argLen - 1] = '\\'; // fix weird CommandLineToArgvW behavior with \"

            if (PathIsURLW(arg) || (argLen >= 1 && arg[0] == ':') || (argLen >= _countof(SHELL_PREFIX) - 1
                    && _wcsnicmp(arg, SHELL_PREFIX, _countof(SHELL_PREFIX) - 1) == 0)) {
                path = arg; // assume desktop absolute parsing name
            } else { // assume relative file system path
                // Shell relaunch commands can contain an absolute path longer
                // than MAX_PATH. Ask Win32 for the required buffer size.
                DWORD capacity = GetFullPathName(arg, 0, nullptr, nullptr);
                path = arg;
                if (capacity) {
                    pathAlloc = wstr_ptr(new wchar_t[capacity]);
                    DWORD length = GetFullPathName(arg, capacity, pathAlloc.get(), nullptr);
                    if (length && length < capacity)
                        path = pathAlloc.get();
                }
            }
        }
    }

    if (reopen) return path ? LAUNCH_FAIL : LAUNCH_REOPEN;
    if (!path) {
        pathAlloc = settings::getStartingFolder();
        path = pathAlloc.get();
    }

    if (path == pathAlloc.get()) {
        *folder = std::move(pathAlloc);
    } else {
        folder->reset(new wchar_t[lstrlenW(path) + 1]);
        lstrcpyW(folder->get(), path);
    }
    return LAUNCH_AUTO;
}

LaunchType createStartingWindow(wchar_t *path, int showCommand) {
    CComPtr<IShellItem> startItem = itemFromPath(path);
    if (!startItem)
        return LAUNCH_FAIL;
    CComPtr<IShellItem> resolved = resolveLink(startItem);
    SFGAOF attributes = 0;
    HRESULT attributesResult = resolved->GetAttributes(SFGAO_FOLDER, &attributes);
    if (!checkHR(attributesResult)) {
        recordFolderPathFailure(path, attributesResult);
        return LAUNCH_FAIL;
    }
    if (!(attributes & SFGAO_FOLDER))
        return invokeDefaultVerb(startItem, nullptr, showCommand) ? LAUNCH_FOUND : LAUNCH_FAIL;
    startItem = resolved;

    FolderOpenResult result = openFolderWindow(startItem, nullptr, showCommand);
    if (result == FolderOpenResult::Created)
        return LAUNCH_AUTO;
    if (result == FolderOpenResult::Activated || result == FolderOpenResult::Pending
            || result == FolderOpenResult::OpenedExternally)
        return LAUNCH_FOUND;
    return LAUNCH_FAIL;
}

DWORD WINAPI checkLastVersion(void *) {
    DWORD lastVersion = settings::getLastOpenedVersion();
    DWORD curVersion = makeVersion(FILESPACER_VERSION);
    if (lastVersion != curVersion) {
        settings::setLastOpenedVersion(curVersion);
        if (lastVersion == settings::DEFAULT_LAST_OPENED_VERSION)
            showWelcomeDialog();
    }
    return 0;
}

void showWelcomeDialog() {
    TASKDIALOGCONFIG config = {sizeof(config)};
    config.hInstance = GetModuleHandle(nullptr);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS;
    config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    config.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_NAME);
    config.pszMainIcon = MAKEINTRESOURCE(IDR_APP_ICON);
    config.pszMainInstruction = MAKEINTRESOURCE(IDS_WELCOME_HEADER);
    config.pszContent = MAKEINTRESOURCE(IDS_WELCOME_BODY);
    TASKDIALOG_BUTTON buttons[] = {
        // {IDS_WELCOME_TUTORIAL, MAKEINTRESOURCE(IDS_WELCOME_TUTORIAL)}, // Deferred help.
        {IDS_WELCOME_BROWSER, MAKEINTRESOURCE(IDS_WELCOME_BROWSER)}};
    config.cButtons = _countof(buttons);
    config.pButtons = buttons;
    config.nDefaultButton = IDCLOSE;
    config.pszVerificationText = MAKEINTRESOURCE(IDS_WELCOME_UPDATE);
    config.pfCallback = welcomeDialogCallback;

    BOOL autoUpdateChecked;
    if (checkHR(TaskDialogIndirect(&config, nullptr, nullptr, &autoUpdateChecked)))
        settings::setUpdateCheckEnabled(autoUpdateChecked);

}

HRESULT WINAPI welcomeDialogCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM, LONG_PTR) {
    if (msg == TDN_CREATED && isProcessQuitRequested()) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return S_OK;
    }
#if 0 // Deferred until FileSpacer has its own public services.
    if (msg == TDN_BUTTON_CLICKED && wParam == IDS_WELCOME_TUTORIAL) {
        ShellExecute(nullptr, L"open", L"https://github.com/vanjac/chromafiler/wiki/Tutorial",
            nullptr, nullptr, SW_SHOWNORMAL);
        return S_FALSE;
    } else
#endif
    if (msg == TDN_BUTTON_CLICKED && wParam == IDS_WELCOME_BROWSER) {

        HINSTANCE instance = GetModuleHandle(nullptr);
        if (!settings::supportsDefaultBrowser()) {
            TaskDialog(hwnd, instance, MAKEINTRESOURCE(IDS_BROWSER_SET_FAILED),
                nullptr, MAKEINTRESOURCE(IDS_REQUIRE_CONTEXT), 0, TD_ERROR_ICON, nullptr);
        } else {
            int result = 0;
            TaskDialog(hwnd, instance, MAKEINTRESOURCE(IDS_CONFIRM_CAPTION),
                nullptr, MAKEINTRESOURCE(IDS_BROWSER_SET_CONFIRM),
                TDCBF_YES_BUTTON | TDCBF_CANCEL_BUTTON, TD_WARNING_ICON, &result);
            if (result == IDYES) {
                changeDefaultBrowser(GetParent(hwnd), true);
            }
        }
        return S_FALSE;
    }
    return S_OK;
}

DWORD WINAPI updateJumpList(void *) {
    updateReopenJumpLists();
    return 0;
}

namespace filespacer {    
    // main.h
#ifdef FILESPACER_MEMLEAKS
    long MEMLEAK_COUNT = 0;
#endif

    void lockProcess() {
        AcquireSRWLockExclusive(&processLock);
        ++processLockCount;
        ReleaseSRWLockExclusive(&processLock);
    }

    void unlockProcess() {
        AcquireSRWLockExclusive(&processLock);
        if (--processLockCount == 0)
            checkLE(SetEvent(processIdleEvent));
        // Serialize the idle check with signaling, including cross-thread Release.
        ReleaseSRWLockExclusive(&processLock);
    }
}
