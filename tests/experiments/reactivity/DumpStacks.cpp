#include <common.h>
#include <dbgeng.h>
#include <dbghelp.h>
#include <atlbase.h>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstring>

namespace {
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;
void need(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
struct OutputSink : IDebugOutputCallbacks {
    LONG references = 1;
    STDMETHODIMP QueryInterface(REFIID id, void **out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (id != IID_IUnknown && id != __uuidof(IDebugOutputCallbacks)) return E_NOINTERFACE;
        *out = static_cast<IDebugOutputCallbacks *>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override { const ULONG n = InterlockedDecrement(&references); if (!n) delete this; return n; }
    STDMETHODIMP Output(ULONG, PCSTR text) override { std::fputs(text, stdout); return S_OK; }
};
void manifest(const wchar_t *path) {
    Handle file(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr), &CloseHandle);
    need(file.get() != INVALID_HANDLE_VALUE, "Cannot open dump");
    LARGE_INTEGER size = {}; need(GetFileSizeEx(file.get(), &size) != FALSE, "Cannot size dump");
    Handle map(CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr), &CloseHandle);
    need(!!map, "Cannot map dump");
    const auto bytes = static_cast<const unsigned char *>(MapViewOfFile(map.get(), FILE_MAP_READ, 0, 0, 0));
    need(!!bytes, "Cannot read dump");
    std::unique_ptr<const void, decltype(&UnmapViewOfFile)> unmap(bytes, &UnmapViewOfFile);
    MINIDUMP_DIRECTORY *directory = nullptr; void *stream = nullptr; ULONG length = 0;
    need(MiniDumpReadDumpStream(const_cast<unsigned char *>(bytes), ModuleListStream, &directory, &stream, &length) != FALSE, "No module stream");
    const auto modules = static_cast<const MINIDUMP_MODULE_LIST *>(stream);
    need(length >= sizeof(ULONG32) && modules->NumberOfModules <= (length - sizeof(ULONG32)) / sizeof(MINIDUMP_MODULE), "Invalid module list");
    for (ULONG32 i = 0; i < modules->NumberOfModules; ++i) {
        const auto &module = modules->Modules[i];
        const ULONG rva = module.CvRecord.Rva, count = module.CvRecord.DataSize;
        if (count <= 24 || static_cast<ULONGLONG>(rva) + count > static_cast<ULONGLONG>(size.QuadPart)) continue;
        const auto cv = bytes + rva; if (std::memcmp(cv, "RSDS", 4) != 0) continue;
        GUID guid = {}; ULONG age = 0; std::memcpy(&guid, cv + 4, sizeof(guid)); std::memcpy(&age, cv + 20, sizeof(age));
        const auto name = reinterpret_cast<const char *>(cv + 24);
        const auto end = static_cast<const char *>(std::memchr(name, 0, count - 24)); if (!end) continue;
        std::string pdb(name, end); const auto slash = pdb.find_last_of("\\/"); if (slash != std::string::npos) pdb.erase(0, slash + 1);
        std::printf("%s\t%08lX%04hX%04hX", pdb.c_str(), guid.Data1, guid.Data2, guid.Data3);
        for (const auto value : guid.Data4) std::printf("%02X", value);
        std::printf("%lX\n", age);
    }
}
void stack(const wchar_t *path, ULONG thread, const wchar_t *symbols) {
    CComPtr<IDebugClient> client; need(SUCCEEDED(DebugCreate(__uuidof(IDebugClient), reinterpret_cast<void **>(&client))), "DebugCreate failed");
    CComPtr<OutputSink> output; output.Attach(new OutputSink);
    need(SUCCEEDED(client->SetOutputCallbacks(output)), "Cannot install output");
    CComQIPtr<IDebugClient4> files(client); CComQIPtr<IDebugControl> control(client);
    CComQIPtr<IDebugSystemObjects> system(client); CComQIPtr<IDebugSymbols3> names(client);
    need(files && control && system && names, "Debugger interfaces unavailable");
    need(SUCCEEDED(names->SetSymbolPathWide(symbols)), "Cannot set symbol path");
    need(SUCCEEDED(names->SetImagePathWide(symbols)), "Cannot set image path");
    need(SUCCEEDED(files->OpenDumpFileWide(path, 0)), "Cannot open debugger target");
    need(control->WaitForEvent(0, 30000) == S_OK, "Dump event unavailable");
    ULONG id = 0; need(SUCCEEDED(system->GetThreadIdBySystemId(thread, &id)) && SUCCEEDED(system->SetCurrentThreadId(id)), "UI thread absent from dump");
    DEBUG_STACK_FRAME frames[96] = {}; ULONG count = 0;
    need(SUCCEEDED(control->GetStackTrace(0, 0, 0, frames, 96, &count)) && count, "No usable stack");
    need(SUCCEEDED(control->OutputStackTrace(DEBUG_OUTCTL_THIS_CLIENT, frames, count, DEBUG_STACK_FRAME_ADDRESSES | DEBUG_STACK_SOURCE_LINE | DEBUG_STACK_FRAME_NUMBERS)), "Cannot render stack");
    for (ULONG i = 0; i < count; ++i) {
        ULONG index = 0; ULONG64 base = 0; DEBUG_MODULE_PARAMETERS parameters = {};
        if (SUCCEEDED(names->GetModuleByOffset(frames[i].InstructionOffset, 0, &index, &base))
                && SUCCEEDED(names->GetModuleParameters(1, &base, 0, &parameters)))
            std::printf("FRAME %lu address=%016llX module=%016llX symbol_type=%lu\n", i, frames[i].InstructionOffset, base, parameters.SymbolType);
    }
    client->EndSession(DEBUG_END_PASSIVE);
    client->SetOutputCallbacks(nullptr);
}
}
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc == 3 && std::wcscmp(argv[1], L"manifest") == 0) manifest(argv[2]);
        else if (argc == 4) stack(argv[1], std::wcstoul(argv[2], nullptr, 10), argv[3]);
        else return 2;
    } catch (const std::exception &e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
    return 0;
}
