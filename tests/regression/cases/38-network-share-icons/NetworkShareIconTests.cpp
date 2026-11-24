#include <common.h>
// Exercise the production extractor without a machine-specific SMB server or
// registration. The local fixture uses the same Shell customization contract.
#include "MixedFolderProvider.cpp"
#include <cstdio>
#include <future>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
void writeSettings(const std::wstring &folder, const wchar_t *settings) {
    const std::wstring ini = folder + L"\\desktop.ini";
    SetFileAttributesW(ini.c_str(), FILE_ATTRIBUTE_NORMAL);
    const std::unique_ptr<void, decltype(&CloseHandle)> file(CreateFileW(ini.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr), &CloseHandle);
    require(file.get() != INVALID_HANDLE_VALUE, "desktop.ini creation failed");
    const WORD bom = 0xfeff;
    DWORD written = 0;
    require(WriteFile(file.get(), &bom, sizeof(bom), &written, nullptr) && written == sizeof(bom), "BOM write failed");
    const DWORD bytes = static_cast<DWORD>(wcslen(settings) * sizeof(wchar_t));
    require(WriteFile(file.get(), settings, bytes, &written, nullptr) && written == bytes, "INI write failed");
    require(SetFileAttributesW(ini.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM) != FALSE, "INI attributes failed");
}
void run(const wchar_t *directory, const wchar_t *icon) {
    const std::wstring folder = std::wstring(directory) + L"\\personnalisation-\x00e9";
    require(CreateDirectoryW(folder.c_str(), nullptr) != FALSE, "Fixture creation failed");
    const std::wstring resource = folder + L"\\custom.exe";
    require(CopyFileW(icon, resource.c_str(), FALSE) != FALSE, "Icon fixture copy failed");
    writeSettings(folder, L"[.ShellClassInfo]\r\nIconFile=custom.exe\r\nIconIndex=0\r\n");
    require(SetFileAttributesW(folder.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE, "Folder attributes failed");
    filespacer::ShareIconLocation legacy;
    legacy.load(folder.c_str());
    require(legacy.file == resource && legacy.index == 0, "IconFile/IconIndex was not resolved by the Shell");

    writeSettings(folder, L"[.ShellClassInfo]\r\nIconResource=%SystemRoot%\\System32\\shell32.dll,-4\r\n");
    legacy.load(folder.c_str());
    require(legacy.file == resource, "A cached result must not reread desktop.ini");
    filespacer::ShareIconLocation refreshed;
    refreshed.load(folder.c_str());
    wchar_t windows[MAX_PATH];
    require(GetWindowsDirectoryW(windows, ARRAYSIZE(windows)) != 0, "Windows directory unavailable");
    require(_wcsicmp(refreshed.file.c_str(), (std::wstring(windows) + L"\\System32\\shell32.dll").c_str()) == 0
        && refreshed.index == -4, "IconResource, environment expansion or negative ID failed");

    require(SetFileAttributesW(folder.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE, "Folder attribute reset failed");
    filespacer::ShareIconLocation plain;
    plain.load(folder.c_str());
    require(plain.file.empty(), "An unmarked folder must keep its native icon");
    require(SetFileAttributesW(folder.c_str(), FILE_ATTRIBUTE_SYSTEM) != FALSE, "System folder attribute failed");
    plain.load(folder.c_str());
    require(plain.file.empty(), "A negative result must remain cached until refresh");
    filespacer::ShareIconLocation system;
    system.load(folder.c_str());
    require(!system.file.empty(), "System attribute must enable customization");

    writeSettings(folder, L"[.ShellClassInfo]\r\nIconFile=missing.ico\r\nIconIndex=0\r\n");
    filespacer::ShareIconLocation missing;
    missing.load(folder.c_str());
    require(missing.file.empty(), "Missing icon must use the native fallback");
    writeSettings(folder, L"[.ShellClassInfo]\r\nIconResource=custom.exe,0\r\n");
    CComPtr<IShellItem> native;
    require(SUCCEEDED(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&native))), "Fixture Shell item failed");
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> id;
    require(SUCCEEDED(SHGetIDListFromObject(native, &id)), "Fixture PIDL failed");
    auto cached = std::make_shared<filespacer::ShareIconLocation>();
    CComPtr<IExtractIconW> extractor;
    auto *object = new filespacer::ShareIcon(id, folder.c_str(), cached, GetCurrentThreadId());
    extractor.Attach(object);
    wchar_t output[MAX_PATH]; int index = 123; UINT flags = 123;
    require(extractor->GetIconLocation(GIL_ASYNC, output, ARRAYSIZE(output), &index, &flags) == E_PENDING,
        "Asynchronous requests must defer to a Shell worker");
    require(extractor->GetIconLocation(GIL_DEFAULTICON, output, ARRAYSIZE(output), &index, &flags) == S_FALSE,
        "Default icon request must not perform I/O");
    require(extractor->GetIconLocation(0, output, ARRAYSIZE(output), &index, &flags) == S_FALSE,
        "Synchronous view-thread request must not perform I/O");
    require(!cached->loaded, "View-thread requests read customization");
    auto worker = std::async(std::launch::async, [&] {
        require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "Worker COM initialization failed");
        const HRESULT hr = extractor->GetIconLocation(GIL_FORSHELL, output, ARRAYSIZE(output), &index, &flags);
        HICON large = nullptr, smallIcon = nullptr;
        const HRESULT extracted = hr == S_OK ? extractor->Extract(output, static_cast<UINT>(index), &large,
            &smallIcon, MAKELONG(48, 16)) : hr;
        const bool valid = extracted == S_OK && large && smallIcon;
        if (large) DestroyIcon(large);
        if (smallIcon) DestroyIcon(smallIcon);
        CoUninitialize();
        return hr == S_OK && std::wstring(output) == resource && index == 0 && valid;
    });
    require(worker.get(), "Shell worker did not extract the customized icon");
    require(cached->loaded, "The worker did not populate the shared cache");
    CComPtr<IStream> stream;
    require(SUCCEEDED(CoMarshalInterThreadInterfaceInStream(IID_IExtractIconW, extractor, &stream)),
        "Icon extractor cannot be marshaled to a Shell worker");
    IStream *transfer = stream.Detach();
    auto marshaledWorker = std::async(std::launch::async, [transfer] {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(com)) { transfer->Release(); return false; }
        CComPtr<IExtractIconW> copy;
        const HRESULT unmarshaled = CoGetInterfaceAndReleaseStream(transfer, IID_PPV_ARGS(&copy));
        wchar_t file[MAX_PATH]; int iconIndex = 0; UINT iconFlags = 0;
        const bool ok = SUCCEEDED(unmarshaled)
            && copy->GetIconLocation(GIL_FORSHELL, file, ARRAYSIZE(file), &iconIndex, &iconFlags) == S_OK;
        copy.Release();
        CoUninitialize();
        return ok;
    });
    const ULONGLONG deadline = GetTickCount64() + 5000;
    while (marshaledWorker.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready
            && GetTickCount64() < deadline) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        Sleep(1);
    }
    require(marshaledWorker.get(), "Marshaled extraction returned to the view thread instead of reading in the worker");
    std::puts("PASS: official folder settings, attributes, resource paths, refresh boundaries and asynchronous extraction.");
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 3 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    int result = 0;
    try { run(argv[1], argv[2]); }
    catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    CoUninitialize();
    return result;
}
