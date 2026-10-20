#include "FolderIdentity.h"
#include <shlobj.h>
#include <iostream>

int wmain(int argc, wchar_t **argv) {
    if (argc != 2)
        return 2;
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
        return 3;
    IShellItem *item = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(argv[1], nullptr, IID_PPV_ARGS(&item));
    std::wstring identity;
    if (SUCCEEDED(hr)) {
        identity = filespacer::getFolderIdentity(item);
        item->Release();
    }
    std::wstring property = filespacer::folderWindowProperty(identity);
    // The property is ASCII even when the parsing name contains non-ASCII characters.
    std::wcout << property << L'\n';
    std::wcout << (identity.compare(0, 5, L"ntfs:") == 0 ? L"ntfs" :
        identity.compare(0, 5, L"path:") == 0 ? L"path" : L"shell") << L'\n';
    CoUninitialize();
    return property.empty() ? 4 : 0;
}
