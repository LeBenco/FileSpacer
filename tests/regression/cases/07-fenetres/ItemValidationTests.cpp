#include <common.h>
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <atlbase.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace filespacer {
// Only the callback's owner declaration is replaced. Its request, persistence
// helper, native resolution and callback body come from ItemWindow.cpp.
class ItemWindow {
public:
    static UINT itemValidatedMessage;
    static VOID CALLBACK validateItemPath(PTP_CALLBACK_INSTANCE instance, void *context);
};
UINT ItemWindow::itemValidatedMessage = 0;
#include "ItemValidationProduction.inc"

namespace {
void require(bool passed, const char *message) {
    if (!passed) throw std::runtime_error(message);
}
struct Received {
    UINT token = 0;
    DWORD error = ERROR_SUCCESS;
    unsigned count = 0;
};
LRESULT CALLBACK receive(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (message == ItemWindow::itemValidatedMessage) {
        auto result = reinterpret_cast<Received *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        result->token = static_cast<UINT>(lParam);
        result->error = static_cast<DWORD>(wParam);
        ++result->count;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
struct Invocation {
    std::shared_ptr<ItemValidation> *context;
    HANDLE finished;
    DWORD worker = 0;
};
VOID CALLBACK invoke(PTP_CALLBACK_INSTANCE instance, void *context) {
    auto &invocation = *static_cast<Invocation *>(context);
    invocation.worker = GetCurrentThreadId();
    ItemWindow::validateItemPath(instance, invocation.context);
    SetEvent(invocation.finished);
}
void runWorker(const std::shared_ptr<ItemValidation> &pending) {
    const std::unique_ptr<void, decltype(&CloseHandle)> finished(CreateEventW(nullptr, TRUE, FALSE, nullptr), &CloseHandle);
    require(!!finished, "Completion event unavailable");
    Invocation invocation{new std::shared_ptr<ItemValidation>(pending), finished.get()};
    if (!TrySubmitThreadpoolCallback(invoke, &invocation, nullptr)) {
        delete invocation.context;
        throw std::runtime_error("Pool submission failed");
    }
    const ULONGLONG deadline = GetTickCount64() + 15000;
    bool done = false;
    while (!done && GetTickCount64() < deadline) {
        HANDLE handle = finished.get();
        const DWORD wait = MsgWaitForMultipleObjects(1, &handle, FALSE, 100, QS_ALLINPUT);
        done = wait == WAIT_OBJECT_0;
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    // A timeout fails the process: the worker must not outlive stack storage.
    if (!done) {
        std::fputs("FAIL: validation worker exceeded its watchdog\n", stderr);
        ExitProcess(1);
    }
    require(invocation.worker != GetCurrentThreadId(), "Validation ran on the receiving thread");
}
void checkCase(const std::wstring &root, unsigned index, bool moved, bool otherParent) {
    const std::wstring parent = root + L"\\validation-" + std::to_wstring(index);
    const std::wstring destination = parent + L"\\destination";
    const std::wstring before = parent + L"\\before";
    const std::wstring after = moved ? (otherParent ? destination : parent) + L"\\after" : before;
    require(!!CreateDirectoryW(parent.c_str(), nullptr), "Case directory unavailable");
    require(!!CreateDirectoryW(destination.c_str(), nullptr), "Destination directory unavailable");
    require(!!CreateDirectoryW(before.c_str(), nullptr), "Source directory unavailable");
    try {
        CComHeapPtr<ITEMIDLIST> id;
        require(SUCCEEDED(SHParseDisplayName(before.c_str(), nullptr, &id, 0, nullptr)), "Source PIDL unavailable");
        CComPtr<IShellLinkW> original;
        require(SUCCEEDED(original.CoCreateInstance(CLSID_ShellLink))
            && SUCCEEDED(original->SetIDList(id)), "Original Shell link unavailable");
        if (moved) require(!!MoveFileW(before.c_str(), after.c_str()), "Fixture move failed");
        Received received;
        const std::unique_ptr<std::remove_pointer<HWND>::type, decltype(&DestroyWindow)> window(
            CreateWindowExW(0, L"FileSpacer.ValidationTest", L"", 0, 0, 0, 0, 0,
                HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), &received), &DestroyWindow);
        require(!!window, "Message receiver unavailable");
        auto pending = std::make_shared<ItemValidation>();
        pending->window = window.get();
        pending->token = 100 + index;
        pending->path = before;
        pending->idList.Attach(reinterpret_cast<ITEMIDLIST *>(ILCloneFull(id)));
        require(!!pending->idList, "PIDL copy failed");
        // Save AFTER the move, as activation does: only the retained original
        // link can still provide its former target's tracking information.
        require(SUCCEEDED(saveValidationLink(original, pending->linkState)), "Link snapshot failed");
        require(!pending->linkState.empty(), "Empty link snapshot");
        runWorker(pending);
        require(received.count == 1 && received.token == pending->token, "Wrong or missing result token");
        require(moved ? received.error == ERROR_FILE_NOT_FOUND || received.error == ERROR_PATH_NOT_FOUND
                      : received.error == ERROR_SUCCESS, "Unexpected path validation result");
        CComHeapPtr<ITEMIDLIST> target;
        target.Attach(static_cast<ITEMIDLIST *>(InterlockedExchangePointer(&pending->target, nullptr)));
        if (moved) {
            require(!!target, "Worker lost the moved target: a PIDL-only link is insufficient");
            CComPtr<IShellItem> found;
            CComHeapPtr<wchar_t> path;
            require(SUCCEEDED(SHCreateItemFromIDList(target, IID_PPV_ARGS(&found)))
                && SUCCEEDED(found->GetDisplayName(SIGDN_FILESYSPATH, &path)), "Resolved path unavailable");
            require(_wcsicmp(path, after.c_str()) == 0, "Worker resolved a different folder");
        } else require(!target, "Accessible folder unexpectedly retargeted");
        std::printf("PASS: production worker %s, exact target and posted token checked\n",
            !moved ? "accessible path" : otherParent ? "moved folder" : "renamed folder");
    } catch (...) {
        RemoveDirectoryW(after.c_str());
        RemoveDirectoryW(destination.c_str());
        RemoveDirectoryW(parent.c_str());
        throw;
    }
    require(!!RemoveDirectoryW(after.c_str()), "Target cleanup failed");
    require(!!RemoveDirectoryW(destination.c_str()), "Destination cleanup failed");
    require(!!RemoveDirectoryW(parent.c_str()), "Parent cleanup failed");
}
}
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 2 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    int result = 0;
    try {
        filespacer::ItemWindow::itemValidatedMessage = RegisterWindowMessageW(L"FileSpacer.ValidationTest.Result");
        filespacer::require(filespacer::ItemWindow::itemValidatedMessage != 0, "Result message unavailable");
        WNDCLASSW type = {};
        type.lpfnWndProc = filespacer::receive;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"FileSpacer.ValidationTest";
        filespacer::require(RegisterClassW(&type) != 0, "Receiver class unavailable");
        filespacer::checkCase(argv[1], 0, false, false);
        filespacer::checkCase(argv[1], 1, true, false);
        filespacer::checkCase(argv[1], 2, true, true);
        filespacer::require(!!UnregisterClassW(type.lpszClassName, type.hInstance), "Receiver class cleanup failed");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        result = 1;
    }
    CoUninitialize();
    return result;
}
