#include "COMUtils.h"
#include <atlbase.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <functional>

using namespace filespacer;
#include "ShellLifetimeProduction.inc"

namespace {
int cases = 0;
void require(bool condition, const char *description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::abort();
    }
}

class Fixture {
public:
    Fixture() {
        require(isProcessIdle(), "no leaked process locks before test");
        processIdleEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        require(processIdleEvent != nullptr, "create idle event");
        reference.Attach(new ShellThreadReference());
        require(shellCommandReference == nullptr, "no borrowed anchor before fixture");
        shellCommandReference = reference;
        requireUnpublished();
        require(isProcessIdle(), "main anchor alone does not keep the process alive");
    }
    ~Fixture() {
        require(isProcessIdle(), "all external references/locks released");
        shellCommandReference = nullptr;
        reference.Release();
        IUnknown *missing = nullptr;
        require(SHGetThreadRef(&missing) == E_NOINTERFACE && missing == nullptr,
            "thread reference unregistered on the creating STA");
        require(CloseHandle(processIdleEvent) != FALSE, "close idle event after release");
        processIdleEvent = nullptr;
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {}
        ++cases;
    }
    IUnknown *acquire() {
        IUnknown *result = nullptr;
        {
            ScopedShellThreadReference scope(reference);
            require(SUCCEEDED(scope.status()), "publish command reference temporarily");
            require(SUCCEEDED(SHGetThreadRef(&result)) && result != nullptr,
                "real SHGetThreadRef acquires reference");
        }
        requireUnpublished();
        require(!isProcessIdle(), "Shell reference holds the process");
        return result;
    }
    static void requireUnpublished() {
        CComPtr<IUnknown> missing;
        require(SHGetThreadRef(&missing) == E_NOINTERFACE && !missing,
            "no reference published outside a command");
    }
private:
    CComPtr<ShellThreadReference> reference;
};

// Exercise the real invocation helper with native COM/SHGetThreadRef calls.
class CommandProbe : public IContextMenu, public UnknownImpl {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid != IID_IUnknown && iid != IID_IContextMenu) return E_NOINTERFACE;
        *object = static_cast<IContextMenu *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return UnknownImpl::AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override { return UnknownImpl::Release(); }
    HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU, UINT, UINT, UINT, UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR, UINT, UINT *, LPSTR, UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE InvokeCommand(CMINVOKECOMMANDINFO *info) override {
        require(GetCurrentThreadId() == ownerThread, "invoke on creating STA");
        CComPtr<IUnknown> published;
        require(SUCCEEDED(SHGetThreadRef(&published)), "command sees a native thread reference");
        require(published.p == shellCommandReference, "command sees main-owned anchor");
        require(!isProcessIdle(), "active invocation retains the process");
        ++calls;
        return invoke ? invoke(info) : S_OK;
    }
    std::function<HRESULT(CMINVOKECOMMANDINFO *)> invoke;
    int calls = 0;
private:
    DWORD ownerThread = GetCurrentThreadId();
};

// The standard IStream marshaler supplies a real cross-apartment STA call.
class StaProbe : public IStream, public UnknownImpl {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid != IID_IUnknown && iid != IID_IStream && iid != IID_ISequentialStream)
            return E_NOINTERFACE;
        *object = static_cast<IStream *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return UnknownImpl::AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override { return UnknownImpl::Release(); }
    HRESULT STDMETHODCALLTYPE Read(void *, ULONG, ULONG *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Write(const void *, ULONG, ULONG *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyTo(IStream *, ULARGE_INTEGER, ULARGE_INTEGER *, ULARGE_INTEGER *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Revert() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG *stat, DWORD) override {
        require(GetCurrentThreadId() == ownerThread, "marshaled method executes on main STA");
        if (!stat) return E_POINTER;
        *stat = {};
        stat->type = STGTY_STREAM;
        ++calls;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Clone(IStream **) override { return E_NOTIMPL; }
    unsigned calls = 0;
private:
    DWORD ownerThread = GetCurrentThreadId();
};

int pumpUntilIdle() {
    MSG msg = {};
    int exitCode = 0;
    while (getProcessMessage(msg, exitCode)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    require(isProcessIdle(), "message pump exits only at idle");
    return exitCode;
}

constexpr UINT releaseMessage = WM_APP + 37;
int dispatched = 0;
LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == releaseMessage) {
        ++dispatched;
        unlockProcess();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

struct NativeJob {
    HWND window;
    HANDLE proceed;
    HANDLE done;
};
DWORD WINAPI nativeJob(void *data) {
    NativeJob *job = static_cast<NativeJob *>(data);
    require(WaitForSingleObject(job->proceed, 5000) == WAIT_OBJECT_0, "start native job");
    SendMessageW(job->window, releaseMessage, 0, 0);
    SetEvent(job->done);
    return 0;
}
} // namespace

int main() {
    require(SUCCEEDED(OleInitialize(nullptr)), "initialize real main STA");
    WNDCLASSW cls = {};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"FileSpacer.ShellLifetime.Test";
    require(RegisterClassW(&cls) != 0, "register test window class");
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, cls.hInstance, nullptr);
    require(window != nullptr, "create message-only test window");
    // Watchdog terminates this test executable only if its STA stops pumping.
    HANDLE finished = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    require(finished != nullptr, "create watchdog event");
    std::thread watchdog([finished] {
        if (WaitForSingleObject(finished, 20000) != WAIT_OBJECT_0) {
            std::fprintf(stderr, "FAIL: native lifetime test timed out\n");
            TerminateProcess(GetCurrentProcess(), 2);
        }
    });
    {
        Fixture test;
        CommandProbe command;
        CMINVOKECOMMANDINFO info = {sizeof(info)};
        info.fMask = CMIC_MASK_ASYNCOK;
        require(invokeShellCommand(&command, &info) == S_OK, "successful invocation result");
        require(command.calls == 1 && info.fMask == CMIC_MASK_ASYNCOK,
            "invoke exactly once without altering command flags");
        Fixture::requireUnpublished();
        require(isProcessIdle(), "synchronous command releases its temporary lock");
        command.invoke = [](CMINVOKECOMMANDINFO *) { return E_FAIL; };
        require(invokeShellCommand(&command, &info) == E_FAIL, "preserve handler failure");
        Fixture::requireUnpublished();
        require(isProcessIdle(), "failed command restores publication and lock count");
    }
    {
        Fixture test;
        CComPtr<UnknownImpl> previous;
        previous.Attach(new UnknownImpl());
        require(SUCCEEDED(SHSetThreadRef(previous)), "install a distinct previous reference");
        CommandProbe outer, inner;
        CMINVOKECOMMANDINFO info = {sizeof(info)};
        outer.invoke = [&](CMINVOKECOMMANDINFO *command) {
            require(invokeShellCommand(&inner, command) == S_OK, "nested invocation succeeds");
            CComPtr<IUnknown> restored;
            require(SUCCEEDED(SHGetThreadRef(&restored)) && restored.p == shellCommandReference,
                "nested invocation restores the outer registration");
            return E_FAIL;
        };
        require(invokeShellCommand(&outer, &info) == E_FAIL, "outer failure preserved");
        {
            CComPtr<IUnknown> restored;
            require(SUCCEEDED(SHGetThreadRef(&restored)) && restored.p == previous.p,
                "outer invocation restores the distinct original reference");
        }
        require(outer.calls == 1 && inner.calls == 1, "both nested commands invoked once");
        require(SUCCEEDED(SHSetThreadRef(nullptr)), "remove test-owned previous registration");
        Fixture::requireUnpublished();
        require(isProcessIdle(), "nested commands balance every acquired reference");
    }
    {
        Fixture test;
        CommandProbe command;
        IUnknown *workerReference = nullptr;
        CMINVOKECOMMANDINFO info = {sizeof(info)};
        info.fMask = CMIC_MASK_ASYNCOK;
        lockProcess(); // Simulated last application window.
        command.invoke = [&](CMINVOKECOMMANDINFO *) {
            require(SUCCEEDED(SHGetThreadRef(&workerReference)), "asynchronous command takes reference");
            unlockProcess(); // The command reentrantly closes its owner.
            require(!isProcessIdle(), "command survives last-window closure");
            return S_OK;
        };
        require(invokeShellCommand(&command, &info) == S_OK, "launch asynchronous command");
        Fixture::requireUnpublished();
        require(!isProcessIdle(), "worker still holds STA after return and unpublication");
        std::thread worker([workerReference] {
            Fixture::requireUnpublished(); // Publication is local to the invoking STA.
            workerReference->Release();
        });
        pumpUntilIdle();
        worker.join();
    }
    {
        Fixture test;
        try {
            ScopedShellThreadReference scope(shellCommandReference);
            require(SUCCEEDED(scope.status()), "publish before exceptional unwind");
            throw 1;
        } catch (int) {}
        Fixture::requireUnpublished();
        require(isProcessIdle(), "scope restores publication during C++ unwinding");
    }
    {
        Fixture test;
        lockProcess(); unlockProcess();
        require(pumpUntilIdle() == 0, "ordinary last-window closure exits");
    }
    {
        Fixture test;
        IUnknown *reference = test.acquire();
        void *identity = nullptr;
        require(SUCCEEDED(reference->QueryInterface(IID_IUnknown, &identity)), "COM identity");
        static_cast<IUnknown *>(identity)->Release();
        reference->Release();
        require(pumpUntilIdle() == 0, "QueryInterface reference balanced");
    }
    {
        Fixture test;
        lockProcess();
        IUnknown *reference = test.acquire();
        unlockProcess();
        require(!isProcessIdle(), "last window closed but Shell still running");
        std::thread worker([reference] { reference->Release(); });
        require(pumpUntilIdle() == 0, "worker Release wakes STA");
        worker.join();
    }
    {
        Fixture test;
        IUnknown *a = test.acquire(), *b = test.acquire();
        a->Release();
        require(!isProcessIdle(), "second command remains active");
        b->Release();
        require(pumpUntilIdle() == 0, "all commands released");
    }
    {
        Fixture test;
        lockProcess(); unlockProcess(); // stale idle notification
        lockProcess();
        require(PostMessageW(window, releaseMessage, 0, 0) != FALSE, "queue new window closure");
        const int before = dispatched;
        pumpUntilIdle();
        require(dispatched == before + 1, "stale notification does not close a new window");
    }
    {
        Fixture test;
        IUnknown *reference = test.acquire();
        lockProcess();
        reference->Release(); // reopened window outlives command
        require(!isProcessIdle(), "reopened window keeps STA running");
        PostMessageW(window, releaseMessage, 0, 0);
        pumpUntilIdle();
    }
    {
        Fixture test;
        IUnknown *reference = test.acquire();
        std::thread worker([reference] { reference->Release(); });
        worker.join();
        // A nested/modal loop processes queued messages, not the kernel event.
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        pumpUntilIdle();
        require(isProcessIdle(), "idle wakeup survives a nested message loop");
    }
    {
        Fixture test;
        lockProcess();
        require(PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 7, 0) != FALSE,
            "inject an explicitly queued WM_QUIT before the close message");
        PostMessageW(window, releaseMessage, 0, 0);
        require(pumpUntilIdle() == 7, "WM_QUIT does not bypass live references");
    }
    {
        Fixture test;
        lockProcess();
        std::thread worker([window] {
            SendMessageW(window, releaseMessage, 0, 0);
        });
        const int before = dispatched;
        pumpUntilIdle();
        worker.join();
        require(dispatched == before + 1, "sent messages serviced while waiting");
    }
    {
        Fixture test;
        HANDLE proceed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE thread = nullptr;
        require(proceed != nullptr && done != nullptr, "create native job barriers");
        NativeJob job = {window, proceed, done};
        lockProcess();
        {
            ScopedShellThreadReference scope(shellCommandReference);
            require(SUCCEEDED(scope.status()), "publish for native worker creation");
            require(SHCreateThreadWithHandle(nativeJob, &job,
                CTF_THREAD_REF | CTF_COINIT_STA, nullptr, &thread) != FALSE,
                "Shell creates a real thread retaining the caller reference");
        }
        Fixture::requireUnpublished();
        unlockProcess();
        require(!isProcessIdle(), "native Shell thread keeps STA alive without windows");
        // The sent message releases this extra simulated window/server lock.
        lockProcess();
        SetEvent(proceed);
        pumpUntilIdle();
        require(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0, "native Shell thread finished");
        require(WaitForSingleObject(done, 0) == WAIT_OBJECT_0, "native job callback completed");
        CloseHandle(thread); CloseHandle(done); CloseHandle(proceed);
    }
    {
        Fixture test;
        IUnknown *reference = test.acquire();
        CComPtr<StaProbe> probe;
        probe.Attach(new StaProbe());
        IStream *packet = nullptr;
        require(SUCCEEDED(CoMarshalInterThreadInterfaceInStream(IID_IStream,
            static_cast<IStream *>(probe.p), &packet)), "marshal probe from main STA");
        std::thread worker([reference, packet] {
            require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "initialize worker MTA");
            IStream *proxy = nullptr;
            require(SUCCEEDED(CoGetInterfaceAndReleaseStream(packet, IID_IStream,
                reinterpret_cast<void **>(&proxy))), "unmarshal worker proxy");
            STATSTG stat = {};
            require(SUCCEEDED(proxy->Stat(&stat, STATFLAG_NONAME)), "worker COM callback completes");
            require(stat.type == STGTY_STREAM, "callback result returned to worker");
            proxy->Release();
            CoUninitialize();
            reference->Release();
        });
        pumpUntilIdle();
        worker.join();
        require(probe->calls == 1, "main STA dispatched the real COM call exactly once");
        probe.Release();
    }
    {
        Fixture test;
        IUnknown *refs[12] = {};
        for (auto &reference : refs) reference = test.acquire();
        std::thread workers[12];
        for (size_t i = 0; i < 12; ++i)
            workers[i] = std::thread([reference = refs[i]] { reference->Release(); });
        pumpUntilIdle();
        for (auto &worker : workers) worker.join();
    }
    SetEvent(finished); watchdog.join(); CloseHandle(finished);
    require(DestroyWindow(window) != FALSE, "destroy test window");
    require(UnregisterClassW(cls.lpszClassName, cls.hInstance) != FALSE, "unregister test class");
    OleUninitialize();
    std::printf("PASS: %d native Shell/STA lifetime cases.\n", cases);
    return 0;
}

