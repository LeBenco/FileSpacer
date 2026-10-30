#include <common.h>
#include "MixedFolder.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <cwchar>

// Published 1.2.2 contract, used only to verify rejection at the DLL boundary.
MIDL_INTERFACE("A6AD59F1-76BE-48F4-A141-B4CB8C25E241") LegacyMixedFolder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE NativeItem(PCUITEMID_CHILD child, REFIID iid, void **out) = 0;
    virtual HRESULT STDMETHODCALLTYPE NativeChild(PCUITEMID_CHILD child, PITEMID_CHILD *out) = 0;
    virtual HRESULT STDMETHODCALLTYPE ViewChild(PCUITEMID_CHILD child, PITEMID_CHILD *out) = 0;
};

void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
void checkProvider(const wchar_t *path, const wchar_t *directory, bool modern) {
    std::unique_ptr<std::remove_pointer<HMODULE>::type, decltype(&FreeLibrary)>
        module(LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH), &FreeLibrary);
    require(module != nullptr, "Provider DLL failed to load");
    using Create = HRESULT (WINAPI *)(BOOL, IShellItem *, IShellItem **);
    const auto create = reinterpret_cast<Create>(GetProcAddress(module.get(), "CreateMixedFolderItem"));
    require(create != nullptr, "Factory export missing");
    CComPtr<IShellItem> native, mixed;
    require(SUCCEEDED(SHCreateItemFromParsingName(directory, nullptr, IID_PPV_ARGS(&native))), "Native fixture unavailable");
    const HRESULT created = create(TRUE, native, &mixed);
    std::printf("Factory=0x%08lX\n", static_cast<unsigned long>(created));
    require(SUCCEEDED(created), "Provider factory failed");
    CComPtr<IShellFolder> folder;
    require(SUCCEEDED(mixed->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&folder))), "Provider folder binding failed");
    CComPtr<filespacer::IMixedFolder> current;
    CComPtr<LegacyMixedFolder> legacy;
    const HRESULT currentResult = folder->QueryInterface(IID_PPV_ARGS(&current));
    const HRESULT legacyResult = folder->QueryInterface(IID_PPV_ARGS(&legacy));
    std::printf("new IID=0x%08lX; published IID=0x%08lX\n",
        static_cast<unsigned long>(currentResult), static_cast<unsigned long>(legacyResult));
    require(modern ? currentResult == S_OK && legacyResult == E_NOINTERFACE && !legacy
                   : legacyResult == S_OK && currentResult == E_NOINTERFACE && !current,
        "Provider did not distinguish the two interface versions");
    CComPtr<IEnumIDList> enumeration;
    require(SUCCEEDED(folder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &enumeration))
        && enumeration, "Provider enumeration unavailable");
    CComHeapPtr<ITEMID_CHILD> child;
    require(enumeration->Next(1, &child, nullptr) == S_OK, "Native child missing");
    CComPtr<IShellItem> converted;
    require(SUCCEEDED(modern ? current->NativeItem(child, IID_PPV_ARGS(&converted))
                             : legacy->NativeItem(child, IID_PPV_ARGS(&converted))), "Native identity conversion failed");
    CComPtr<IShellItem> expected;
    const std::wstring expectedPath = std::wstring(directory) + L"\\native.txt";
    require(SUCCEEDED(SHCreateItemFromParsingName(expectedPath.c_str(), nullptr, IID_PPV_ARGS(&expected))), "Expected native item unavailable");
    int comparison = 1;
    require(SUCCEEDED(converted->Compare(expected, SICHINT_CANONICAL, &comparison)) && comparison == 0,
        "Converted child has the wrong canonical identity");
    if (!modern) {
        CComHeapPtr<ITEMID_CHILD> nativeChild, viewChild;
        require(SUCCEEDED(legacy->NativeChild(child, &nativeChild)), "Legacy NativeChild slot failed");
        require(SUCCEEDED(legacy->ViewChild(nativeChild, &viewChild)), "Legacy ViewChild slot failed");
        require(ILIsEqual(child, viewChild) != FALSE, "Legacy child round-trip failed");
    }
    std::puts(modern ? "PASS: new provider rejects the published IID and restores the exact native Shell identity."
                     : "PASS: legacy provider rejects the new IID and preserves its three-method vtable.");
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 4 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    int result = 0;
    try { checkProvider(argv[1], argv[2], std::wcscmp(argv[3], L"new") == 0); }
    catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    CoUninitialize();
    return result;
}
