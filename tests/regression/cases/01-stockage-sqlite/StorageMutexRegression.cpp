#include <windows.h>
#include "FolderStateStore.h"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <map>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")

using namespace filespacer;
namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
struct CloseFixtureHandle { void operator()(void *handle) const { ::CloseHandle(handle); } };
using Handle = std::unique_ptr<void, CloseFixtureHandle>;
std::wstring wide(const std::string &text) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, nullptr, 0);
    require(size > 0, "UTF-8 conversion size");
    std::vector<wchar_t> buffer(static_cast<size_t>(size));
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, buffer.data(), size) != 0,
        "UTF-8 conversion");
    return buffer.data();
}
Handle event(const std::wstring &prefix, const wchar_t *suffix, bool reset = true) {
    Handle result(CreateEventW(nullptr, reset ? TRUE : FALSE, FALSE, (prefix + suffix).c_str()));
    require(result != nullptr, "create event");
    return result;
}
Handle databaseMutex(const std::string &path) {
    const HANDLE raw = CreateFileW(wide(path).c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(raw != INVALID_HANDLE_VALUE, "open database identity");
    Handle file(raw);
    FILE_ID_INFO info = {};
    require(GetFileInformationByHandleEx(file.get(), FileIdInfo, &info, sizeof(info)) != 0, "database identity");
    std::wstring name = L"Global\\FileSpacer.FolderState." + std::to_wstring(info.VolumeSerialNumber) + L".";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (const auto byte : info.FileId.Identifier) {
        name.push_back(hex[byte >> 4]); name.push_back(hex[byte & 15]);
    }
    Handle result(CreateMutexExW(nullptr, name.c_str(), 0, SYNCHRONIZE | MUTEX_MODIFY_STATE));
    require(result != nullptr, "open database mutex");
    return result;
}
void acquire(HANDLE mutex) {
    const DWORD result = WaitForSingleObject(mutex, 10000);
    require(result == WAIT_OBJECT_0 || result == WAIT_ABANDONED, "fixture mutex acquisition");
}
FolderState fixture() {
    FolderState state;
    state.present = FolderState::All;
    state.x = 241; state.y = 317; state.width = 902; state.height = 650;
    state.maximized = true; state.lastSuccessAt = 100000;
    state.view.present = FolderViewSettings::Mode | FolderViewSettings::Flags;
    state.view.mode = 4; state.view.iconSize = 48; state.view.flags = 17;
    state.icons.resize(32000);
    for (size_t i = 0; i < state.icons.size(); ++i) state.icons[i] = static_cast<uint8_t>(i * 7);
    return state;
}
void check(FolderStateStore &store, int x = 241) {
    FolderState state;
    const bool read = store.read("known", &state);
    if (!read) throw std::runtime_error(store.error());
    require(state.present == FolderState::All && state.x == x && state.y == 317 && state.width == 902
        && state.height == 650 && state.maximized && state.view.mode == 4 && state.view.iconSize == 48
        && state.view.flags == 17 && state.icons == fixture().icons && state.lastSuccessAt == 100000,
        "known settings replaced or damaged");
    std::vector<std::string> history;
    require(store.readSearchHistory("known", 20, &history) && history == std::vector<std::string>({"alpha", "beta"}),
        "search history replaced");
    ClosedWindow closed;
    require(store.lastClosedWindow(&closed) && closed.key == "known" && closed.parsingName == "known-folder"
        && closed.viewState == std::vector<uint8_t>({1, 2, 3}), "closed-window history replaced");
}
struct Child {
    std::wstring prefix;
    Handle ready, release, step, start, process;
    bool finished = false;
    Child(const std::string &path, const std::string &mode) {
        static unsigned sequence = 0;
        prefix = L"Local\\FileSpacer.MutexTest." + std::to_wstring(GetCurrentProcessId()) + L"."
            + std::to_wstring(++sequence);
        ready = event(prefix, L".ready"); release = event(prefix, L".release");
        step = event(prefix, L".step", false); start = event(prefix, L".start");
        std::vector<wchar_t> executable(32768);
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        require(length && length < executable.size(), "test executable path");
        const std::wstring command = L"\"" + std::wstring(executable.data()) + L"\" --child \"" + wide(path)
            + L"\" \"" + prefix + L"\" " + wide(mode);
        std::vector<wchar_t> buffer(command.begin(), command.end()); buffer.push_back(0);
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION child = {};
        require(CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
            nullptr, nullptr, &startup, &child) != 0, "start fixture child");
        process.reset(child.hProcess);
        ::CloseHandle(child.hThread);
        HANDLE waits[] = {ready.get(), process.get()};
        if (WaitForMultipleObjects(2, waits, FALSE, 20000) != WAIT_OBJECT_0) {
            SetEvent(release.get()); SetEvent(start.get());
            if (WaitForSingleObject(process.get(), 30000) != WAIT_OBJECT_0)
                std::cerr << "Cleanup failure after fixture startup failure\n";
            throw std::runtime_error("fixture child did not become ready");
        }
    }
    ~Child() {
        if (!finished && process) {
            SetEvent(release.get()); SetEvent(start.get());
            if (WaitForSingleObject(process.get(), 30000) != WAIT_OBJECT_0)
                std::fputs("Cleanup failure: fixture child did not exit\n", stderr);
        }
    }
    void finish() {
        require(SetEvent(release.get()) != 0, "release fixture child");
        require(WaitForSingleObject(process.get(), 30000) == WAIT_OBJECT_0, "fixture child exit timeout");
        DWORD code = 1;
        require(GetExitCodeProcess(process.get(), &code) && code == 0, "fixture child failed");
        finished = true;
    }
};
void sql(sqlite3 *db, const char *command) {
    const int result = sqlite3_exec(db, command, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
}
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3 *db, const char *command) {
    sqlite3_stmt *raw = nullptr;
    const int result = sqlite3_prepare_v2(db, command, -1, &raw, nullptr);
    Statement statement(raw, sqlite3_finalize);
    require(result == SQLITE_OK, "fixture SQL preparation");
    return statement;
}
void verifyProductPositions(const char *path, const char *key, const char *directory, const char *expectedPath) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    require(SUCCEEDED(initialized), "verification COM apartment");
    struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
    sqlite3 *raw = nullptr;
    const int opened = sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, nullptr);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    require(opened == SQLITE_OK, "read-only snapshot open");
    auto integrity = prepare(db.get(), "PRAGMA integrity_check");
    require(sqlite3_step(integrity.get()) == SQLITE_ROW, "snapshot integrity result");
    const auto integrityText = reinterpret_cast<const char *>(sqlite3_column_text(integrity.get(), 0));
    require(integrityText && std::string(integrityText) == "ok", "snapshot integrity");
    auto history = prepare(db.get(), "SELECT query FROM search_history WHERE key=?1 ORDER BY sequence DESC");
    require(sqlite3_bind_text(history.get(), 1, key, -1, SQLITE_TRANSIENT) == SQLITE_OK, "history key");
    std::vector<std::string> queries;
    int code = SQLITE_OK;
    while ((code = sqlite3_step(history.get())) == SQLITE_ROW) {
        const auto query = reinterpret_cast<const char *>(sqlite3_column_text(history.get(), 0));
        require(query != nullptr, "snapshot history text");
        queries.emplace_back(query);
    }
    require(code == SQLITE_DONE && queries == std::vector<std::string>({"alpha", "beta"}), "post-close history");
    auto statement = prepare(db.get(), "SELECT icons FROM folder_state WHERE key=?1");
    require(sqlite3_bind_text(statement.get(), 1, key, -1, SQLITE_TRANSIENT) == SQLITE_OK
        && sqlite3_step(statement.get()) == SQLITE_ROW, "snapshot icon row");
    const int length = sqlite3_column_bytes(statement.get(), 0);
    const auto bytes = static_cast<const BYTE *>(sqlite3_column_blob(statement.get(), 0));
    require(length > 0 && bytes, "snapshot icon blob");
    if (!directory) {
        require(std::string(reinterpret_cast<const char *>(bytes), static_cast<size_t>(length))
            == std::string("\xff", 1), "failed restoration overwrote the known icon blob");
        std::cout << "{\"integrity\":true,\"knownHistoryPreserved\":true,\"invalidReferencePreserved\":true}\n";
        return;
    }
    CComPtr<IShellItem> item;
    CComPtr<IShellFolder> folder;
    require(SUCCEEDED(SHCreateItemFromParsingName(wide(directory).c_str(), nullptr, IID_PPV_ARGS(&item)))
        && SUCCEEDED(item->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&folder))), "bind canonical fixture");
    std::ifstream input(wide(expectedPath));
    std::string expected;
    unsigned dpi = 0;
    input >> dpi;
    require(dpi > 0 && bool(input >> expected), "expected position file");
    struct Order {
        IShellFolder *folder;
        bool operator()(PCUIDLIST_RELATIVE a, PCUIDLIST_RELATIVE b) const {
            const HRESULT result = folder->CompareIDs(SHCIDS_CANONICALONLY, a, b);
            require(SUCCEEDED(result), "canonical snapshot comparison");
            return static_cast<short>(HRESULT_CODE(result)) < 0;
        }
    };
    std::map<PCUIDLIST_RELATIVE, POINT, Order> wanted(Order{folder});
    struct FreePidl { void operator()(ITEMIDLIST *id) const { CoTaskMemFree(id); } };
    std::vector<std::unique_ptr<ITEMIDLIST, FreePidl>> ids;
    std::istringstream entries(expected);
    std::string entry;
    while (std::getline(entries, entry, ';')) {
        if (entry.empty()) continue;
        const auto equal = entry.find('=');
        const auto comma = entry.find(',', equal);
        require(equal != std::string::npos && comma != std::string::npos, "expected position syntax");
        std::wstring name = wide(entry.substr(0, equal));
        CComHeapPtr<ITEMIDLIST> id;
        ULONG eaten = 0; SFGAOF attributes = 0;
        require(SUCCEEDED(folder->ParseDisplayName(nullptr, nullptr, &name[0], &eaten, &id, &attributes)), "expected identity");
        const POINT point = {std::stoi(entry.substr(equal + 1, comma - equal - 1)), std::stoi(entry.substr(comma + 1))};
        require(wanted.emplace(id, point).second, "duplicate expected canonical identity");
        ids.emplace_back(id.Detach());
    }
    CComPtr<IStream> stream; stream.Attach(SHCreateMemStream(bytes, static_cast<UINT>(length)));
    require(stream != nullptr, "snapshot icon stream");
    const size_t expectedCount = wanted.size();
    size_t count = 0;
    for (;;) {
        LARGE_INTEGER zero = {}; ULARGE_INTEGER offset = {};
        require(SUCCEEDED(stream->Seek(zero, STREAM_SEEK_CUR, &offset)), "snapshot stream position");
        if (offset.QuadPart == static_cast<ULONGLONG>(length)) break;
        CComHeapPtr<ITEMIDLIST> id; POINT point = {};
        require(SUCCEEDED(IStream_ReadPidl(stream, &id)) && id
            && SUCCEEDED(IStream_Read(stream, &point, sizeof(point))), "snapshot PIDL/point decoding");
        const auto match = wanted.find(id);
        require(match != wanted.end(), "unknown or duplicate saved canonical identity");
        require(std::abs(MulDiv(point.x, static_cast<int>(dpi), 96) - match->second.x) <= 1
            && std::abs(MulDiv(point.y, static_cast<int>(dpi), 96) - match->second.y) <= 1,
            "canonical post-close position mismatch");
        wanted.erase(match); ++count;
    }
    require(count == expectedCount && wanted.empty(), "post-close identity set changed");
    std::cout << "{\"integrity\":true,\"knownHistoryPreserved\":true,\"canonicalPositions\":"
        << count << ",\"mismatches\":0}\n";
}

int childMain(const std::string &path, const std::wstring &prefix, const std::string &mode) {
    Handle ready = event(prefix, L".ready"), release = event(prefix, L".release");
    if (mode == "failed-close") {
        Handle start = event(prefix, L".start"), step = event(prefix, L".step", false);
        SetEvent(ready.get());
        {
            FolderStateStore unavailable(path.c_str());
            require(!unavailable.ready(), "constructor unexpectedly acquired the held mutex");
            require(SetEvent(start.get()) != 0, "signal failed constructor");
        }
        require(SetEvent(step.get()) != 0, "signal failed connection destruction");
        require(WaitForSingleObject(release.get(), 10000) == WAIT_OBJECT_0, "failed-close fixture release");
        return 0;
    }
    if (mode == "hot1" || mode == "hot3") {
        const unsigned batch = mode == "hot3" ? 3u : 1u;
        FolderStateStore store(path.c_str()); require(store.ready(), "hot client initialization");
        Handle mutex = databaseMutex(path), step = event(prefix, L".step", false), start = event(prefix, L".start");
        SetEvent(ready.get());
        require(WaitForSingleObject(start.get(), 10000) == WAIT_OBJECT_0, "hot client start");
        const ULONGLONG deadline = GetTickCount64() + 45000;
        unsigned count = 0;
        FolderState state = fixture();
        while (WaitForSingleObject(release.get(), 0) == WAIT_TIMEOUT && GetTickCount64() < deadline) {
            if (batch > 1) acquire(mutex.get());
            bool success = true;
            for (unsigned i = 0; i < batch; ++i) {
                state.x = static_cast<int>(++count);
                if (!store.save("hot", state, FolderState::All)) { success = false; break; }
            }
            if (batch > 1) require(ReleaseMutex(mutex.get()) != 0, "release grouped fixture");
            if (!success) throw std::runtime_error(store.error());
            SetEvent(step.get());
        }
        require(WaitForSingleObject(release.get(), 0) == WAIT_OBJECT_0, "hot client watchdog expired");
        std::cout << "HOT batch=" << batch << " commits=" << count << std::endl;
        return 0;
    }
    Handle mutex;
    if (mode != "external") { mutex = databaseMutex(path); acquire(mutex.get()); }
    sqlite3 *db = nullptr;
    if (mode == "external" || mode == "abandon") {
        require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "raw fixture database");
        sql(db, "BEGIN IMMEDIATE; UPDATE folder_state SET x=-999 WHERE key='known'; DELETE FROM search_history; DELETE FROM closed_windows;");
        require(sqlite3_db_cacheflush(db) == SQLITE_OK, "flush uncommitted fixture pages");
    }
    SetEvent(ready.get());
    require(WaitForSingleObject(release.get(), mode == "hold-long" ? 120000 : 30000) == WAIT_OBJECT_0,
        "fixture release timeout");
    if (mode == "abandon") ExitProcess(0); // Deliberately abandon this fixture's mutex and uncommitted WAL.
    if (db) { sql(db, "ROLLBACK"); require(sqlite3_close(db) == SQLITE_OK, "raw fixture close"); }
    if (mutex) require(ReleaseMutex(mutex.get()) != 0, "fixture mutex release");
    return 0;
}
void fairness(FolderStateStore &store, const std::string &path, const char *mode) {
    Child hot(path, mode);
    SetEvent(hot.start.get());
    std::vector<int64_t> waits;
    for (unsigned request = 0; request < 40; ++request) {
        HANDLE progress[] = {hot.step.get(), hot.process.get()};
        require(WaitForMultipleObjects(2, progress, FALSE, 10000) == WAIT_OBJECT_0, "hot client stopped making progress");
        const auto before = std::chrono::steady_clock::now();
        FolderState state;
        const bool read = store.read("known", &state);
        if (!read) throw std::runtime_error(store.error());
        state.x = 241;
        const bool saved = store.save("known", state, FolderState::Position);
        if (!saved) throw std::runtime_error(store.error());
        waits.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - before).count());
    }
    hot.finish();
    std::sort(waits.begin(), waits.end());
    std::cout << "FAIRNESS mode=" << mode << " requests=" << waits.size() << " median_us=" << waits[20]
        << " p95_us=" << waits[37] << " max_us=" << waits.back() << std::endl;
    check(store);
}
} // namespace

int main(int argc, char **argv) {
    try {
        require(argc >= 2, "database path required");
        if (argc == 6 && std::string(argv[1]) == "--verify-product") {
            verifyProductPositions(argv[2], argv[3], argv[4], argv[5]);
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--verify-invalid-product") {
            verifyProductPositions(argv[2], argv[3], nullptr, nullptr);
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--invalidate-product-icons") {
            require(wide(argv[2]).find(L"folder-state-test.sqlite3") != std::wstring::npos, "test database required");
            sqlite3 *raw = nullptr;
            const int opened = sqlite3_open_v2(argv[2], &raw, SQLITE_OPEN_READWRITE, nullptr);
            std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
            require(opened == SQLITE_OK, "test database open");
            auto update = prepare(db.get(), "UPDATE folder_state SET icons=?2 WHERE key=?1");
            require(sqlite3_bind_text(update.get(), 1, argv[3], -1, SQLITE_TRANSIENT) == SQLITE_OK
                && sqlite3_bind_blob(update.get(), 2, "\xff", 1, SQLITE_STATIC) == SQLITE_OK
                && sqlite3_step(update.get()) == SQLITE_DONE && sqlite3_changes(db.get()) == 1, "invalidate test icon blob");
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "--child")
            return childMain(argv[2], wide(argv[3]), argv[4]);
        if (argc == 4 && std::string(argv[1]) == "--seed-product") {
            FolderStateStore store(argv[2]); require(store.ready(), "product fixture initialization");
            FolderState state = fixture(); state.x = 137; state.y = 163; state.width = 743; state.height = 511;
            state.maximized = false; state.view.present = FolderViewSettings::Mode;
            state.view.mode = 4; state.view.iconSize = 16; state.icons.clear();
            require(store.save(argv[3], state, FolderState::All & ~FolderState::Icons), "seed product settings");
            require(store.saveSearchHistory(argv[3], {"alpha", "beta"}, 20), "seed product history");
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--inspect-product") {
            FolderStateStore store(argv[2]); require(store.ready(), "inspect product database");
            FolderState state; require(store.read(argv[3], &state), "inspect product settings");
            std::vector<std::string> history;
            require(store.readSearchHistory(argv[3], 20, &history), "inspect product history");
            std::cout << "{\"present\":" << state.present << ",\"x\":" << state.x << ",\"y\":" << state.y
                << ",\"width\":" << state.width << ",\"height\":" << state.height
                << ",\"knownHistoryPreserved\":" << (history == std::vector<std::string>({"alpha", "beta"}) ? "true" : "false") << "}\n";
            return 0;
        }
        // argv: executable --child database prefix mode (five arguments including executable).
        const std::string path = argv[1];
        require(GetFileAttributesW(wide(path).c_str()) == INVALID_FILE_ATTRIBUTES, "refuse existing fixture database");
        FolderStateStore store(path.c_str()); require(store.ready(), "fixture initialization");
        const FolderState original = fixture();
        require(store.save("known", original, FolderState::All), "seed known state");
        require(store.saveSearchHistory("known", {"alpha", "beta"}, 20), "seed search history");
        ClosedWindow closed; closed.key = "known"; closed.parsingName = "known-folder"; closed.viewState = {1, 2, 3};
        require(store.rememberClosedWindow(closed), "seed closed window");
        {
            Child held(path, "hold");
            Handle started = event(held.prefix, L".started"), done = event(held.prefix, L".done");
            bool read = false;
            FolderState value;
            std::thread request([&]() { SetEvent(started.get()); read = store.read("known", &value); SetEvent(done.get()); });
            const bool began = WaitForSingleObject(started.get(), 10000) == WAIT_OBJECT_0;
            const bool blocked = WaitForSingleObject(done.get(), 150) == WAIT_TIMEOUT;
            try { held.finish(); }
            catch (...) { SetEvent(held.release.get()); request.join(); throw; }
            request.join();
            require(began && blocked && read && value.icons == original.icons, "blocking/release lost state or did not block");
        }
        std::cout << "PASS blocking and controlled release\n";
        {
            Child held(path, "hold");
            FolderState defaults;
            const bool saved = store.save("known", defaults, FolderState::All);
            const std::string error = store.error();
            held.finish();
            require(!saved && error.find("1460") != std::string::npos, "mutex deadline was not reported");
            check(store);
            require(store.save("known", original, FolderState::Position), "same-connection write retry");
            check(store);
        }
        std::cout << "PASS timed-out save, preservation and same-connection recovery\n";
        {
            Child held(path, "hold");
            std::unique_ptr<FolderStateStore> failed(new FolderStateStore(path.c_str()));
            const bool rejected = !failed->ready();
            held.finish();
            require(rejected && !failed->ready(), "initialization deadline not observed");
            FolderState sentinel = original;
            require(!failed->read("known", &sentinel) && sentinel.icons == original.icons, "failed initialization exposed defaults");
            FolderStateStore recovered(path.c_str()); require(recovered.ready(), "new-connection recovery");
            check(recovered);
            std::cout << "LIMIT initialization_failure_persists_in_same_instance=true new_instance_recovers=true singleton_not_exercised=true\n";
        }
        {
            Child abandoned(path, "abandon"); abandoned.finish();
            check(store);
            require(store.save("known", original, FolderState::Position), "write after abandoned owner");
            check(store);
        }
        std::cout << "PASS abandoned owner, uncommitted WAL recovery and preserved histories\n";
        {
            Child held(path, "hold");
            FolderStateStore other((path + ".other").c_str());
            const bool independent = other.ready() && other.save("independent", original, FolderState::All);
            held.finish(); require(independent, "independent databases share a mutex");
        }
        std::cout << "PASS independent database isolation\n";
        {
            Child holder(path, "hold");
            Child unavailable(path, "failed-close");
            const DWORD constructed = WaitForSingleObject(unavailable.start.get(), 10000);
            const DWORD destroyed = constructed == WAIT_OBJECT_0
                ? WaitForSingleObject(unavailable.step.get(), 5000) : WAIT_FAILED;
            // Release before reporting a failure, so the old destructor can also exit.
            holder.finish();
            unavailable.finish();
            require(constructed == WAIT_OBJECT_0, "failed constructor was not observed");
            require(destroyed == WAIT_OBJECT_0, "failed unopened connection waited for the held mutex during destruction");
            check(store);
        }
        std::cout << "PASS failed unopened connection closes before the blocking mutex is released\n";
        {
            Child held(path, "hold");
            const ULONGLONG started = GetTickCount64();
            bool rejected = true;
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                FolderStateStore retry(path.c_str(), false);
                FolderState sentinel = original;
                rejected = rejected && !retry.ready() && retry.error().find("1460") != std::string::npos
                    && !retry.read("known", &sentinel) && sentinel.icons == original.icons;
            }
            const ULONGLONG elapsed = GetTickCount64() - started;
            held.finish();
            require(rejected && elapsed < 1500, "non-waiting mutex initialization waited or exposed defaults");
            std::cout << "OBS non_waiting_mutex_init_attempts=3 elapsed_ms=" << elapsed << '\n';
        }
        {
            Child external(path, "external");
            const ULONGLONG started = GetTickCount64();
            std::unique_ptr<FolderStateStore> failed(new FolderStateStore(path.c_str(), false));
            const bool rejected = !failed->ready() && failed->error().find("locked") != std::string::npos;
            const ULONGLONG elapsed = GetTickCount64() - started;
            // A failed SQLite handle must already be closed under its original
            // constructor lock, even if another client subsequently owns it.
            Child held(path, "hold");
            Handle done = event(held.prefix, L".sql-closed");
            std::thread closing([&]() { failed.reset(); SetEvent(done.get()); });
            const bool closedBeforeRelease = WaitForSingleObject(done.get(), 1500) == WAIT_OBJECT_0;
            try { held.finish(); }
            catch (...) { SetEvent(held.release.get()); closing.join(); throw; }
            closing.join();
            external.finish();
            require(rejected && elapsed < 1500 && closedBeforeRelease,
                "failed non-waiting SQLite initialization waited again during open or destruction");
            check(store);
            std::cout << "OBS non_waiting_sqlite_init_ms=" << elapsed << '\n';
        }
        {
            FolderStateStore recovered(path.c_str(), false);
            require(recovered.ready(), "non-waiting initialization did not recover after release");
            check(recovered);
            {
                Child held(path, "hold");
                FolderState sentinel = original;
                const ULONGLONG started = GetTickCount64();
                const bool read = recovered.read("known", &sentinel);
                const ULONGLONG elapsed = GetTickCount64() - started;
                held.finish();
                require(!read && elapsed >= 2500 && sentinel.icons == original.icons,
                    "ready connection did not retain its ordinary mutex deadline");
            }
            {
                Child external(path, "external");
                const ULONGLONG started = GetTickCount64();
                const bool saved = recovered.save("known", {}, FolderState::All);
                const ULONGLONG elapsed = GetTickCount64() - started;
                const std::string error = recovered.error();
                external.finish();
                require(!saved && elapsed >= 2500 && error.find("locked") != std::string::npos,
                    "ready connection did not restore its ordinary SQLite busy timeout");
                check(recovered);
            }
        }
        std::cout << "PASS non-waiting initialization retries, serialized failed cleanup, recovery and restored deadlines\n";
        {
            Child external(path, "external");
            FolderState defaults;
            const bool saved = store.save("known", defaults, FolderState::All);
            const std::string error = store.error();
            external.finish();
            require(!saved && error.find("locked") != std::string::npos, "external SQLite writer was not diagnosed");
            check(store);
            require(store.save("known", original, FolderState::Position), "retry after external writer");
            check(store);
        }
        std::cout << "PASS external SQLite writer, rollback, preservation and retry\n";
        fairness(store, path, "hot1"); fairness(store, path, "hot3");
        sqlite3 *db = nullptr;
        require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "integrity open");
        sqlite3_stmt *statement = nullptr;
        require(sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &statement, nullptr) == SQLITE_OK, "integrity prepare");
        require(sqlite3_step(statement) == SQLITE_ROW && std::string(reinterpret_cast<const char *>(sqlite3_column_text(statement, 0))) == "ok",
            "database integrity");
        sqlite3_finalize(statement); require(sqlite3_close(db) == SQLITE_OK, "integrity close");
        std::cout << "PASS mutex protocol and integrity; LIMIT raw-store fixture does not exercise the product singleton\n";
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("FAIL: unknown exception\n", stderr);
        return 1;
    }
}
