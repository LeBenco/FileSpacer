// Seeds a saved free icon layout for every child of a folder into the /test
// folder-state database, in the product's own format (Shell-serialized child
// PIDL followed by a 96-DPI POINT), with Icons mode and no automatic
// arrangement. Prints the seeded layout as "name=x,y" lines in 96-DPI units.
#include "FolderIdentity.h"
#include "FolderStateStore.h"
#include <atlbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")

using namespace filespacer;
namespace {
struct Entry {
    std::wstring name;
    PITEMID_CHILD id = nullptr;
    POINT point = {};
};
struct Layout { int columns, originX, originY, stepX, stepY; };

std::string utf8(const std::wstring &text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    std::vector<char> buffer(static_cast<size_t>(size));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer.data(), size, nullptr, nullptr);
    return buffer.data();
}

int fail(const wchar_t *stage, HRESULT result, int code) {
    std::wcerr << stage << L" failed: 0x" << std::hex << static_cast<unsigned long>(result) << L'\n';
    return code;
}

int seed(const wchar_t *database, const wchar_t *folderPath, const Layout &layout) {
    CComPtr<IShellItem> item;
    HRESULT result = SHCreateItemFromParsingName(folderPath, nullptr, IID_PPV_ARGS(&item));
    if (FAILED(result)) return fail(L"folder item", result, 3);
    const std::wstring key = folderWindowProperty(getFolderIdentity(item));
    if (key.empty()) return fail(L"folder identity", E_FAIL, 4);
    CComPtr<IShellFolder> folder;
    result = item->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&folder));
    if (FAILED(result)) return fail(L"folder binding", result, 5);
    CComPtr<IEnumIDList> children;
    result = folder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, &children);
    if (result != S_OK) return fail(L"folder enumeration", FAILED(result) ? result : E_FAIL, 6);
    std::vector<Entry> entries;
    struct FreeEntries {
        std::vector<Entry> &entries;
        ~FreeEntries() { for (auto &entry : entries) CoTaskMemFree(entry.id); }
    } freeEntries{entries};
    for (;;) {
        PITEMID_CHILD child = nullptr;
        ULONG fetched = 0;
        result = children->Next(1, &child, &fetched);
        if (result != S_OK || fetched != 1 || !child) break;
        Entry entry;
        entry.id = child;
        entries.push_back(entry);
        STRRET displayName = {};
        result = folder->GetDisplayNameOf(child, SHGDN_INFOLDER | SHGDN_FORPARSING, &displayName);
        if (FAILED(result)) return fail(L"child name", result, 7);
        wchar_t *name = nullptr;
        result = StrRetToStrW(&displayName, child, &name);
        if (FAILED(result)) return fail(L"child name conversion", result, 7);
        entries.back().name = name;
        CoTaskMemFree(name);
    }
    if (entries.empty()) return fail(L"empty folder", E_FAIL, 8);
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    if (!stream) return fail(L"memory stream", E_OUTOFMEMORY, 9);
    for (size_t index = 0; index < entries.size(); ++index) {
        auto &entry = entries[index];
        const int column = static_cast<int>(index % static_cast<size_t>(layout.columns));
        const int row = static_cast<int>(index / static_cast<size_t>(layout.columns));
        entry.point.x = layout.originX + column * layout.stepX;
        entry.point.y = layout.originY + row * layout.stepY;
        result = IStream_WritePidl(stream, entry.id);
        if (SUCCEEDED(result)) result = IStream_Write(stream, &entry.point, sizeof(entry.point));
        if (FAILED(result)) return fail(L"layout encoding", result, 9);
    }
    STATSTG size = {};
    result = stream->Stat(&size, STATFLAG_NONAME);
    if (FAILED(result)) return fail(L"layout size", result, 9);
    LARGE_INTEGER beginning = {};
    result = stream->Seek(beginning, STREAM_SEEK_SET, nullptr);
    if (FAILED(result)) return fail(L"layout rewind", result, 9);
    std::vector<uint8_t> bytes(static_cast<size_t>(size.cbSize.QuadPart));
    result = IStream_Read(stream, bytes.data(), static_cast<ULONG>(bytes.size()));
    if (FAILED(result)) return fail(L"layout read-back", result, 9);

    FolderStateStore store(utf8(database).c_str());
    if (!store.ready()) {
        std::cerr << "database initialization failed: " << store.error() << '\n';
        return 10;
    }
    FolderState state;
    state.present = FolderState::View | FolderState::Icons;
    state.view.present = FolderViewSettings::Mode | FolderViewSettings::Flags;
    state.view.mode = FVM_ICON;
    state.view.iconSize = 48;
    state.view.flags = 0; // Neither FWF_AUTOARRANGE nor FWF_SNAPTOGRID.
    state.icons = bytes;
    if (!store.save(utf8(key), state, FolderState::View | FolderState::Icons)) {
        std::cerr << "seed save failed: " << store.error() << '\n';
        return 11;
    }
    for (const auto &entry : entries)
        std::wcout << entry.name << L'=' << entry.point.x << L',' << entry.point.y << L'\n';
    return 0;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
    // argv: executable --seed database folder columns originX originY stepX stepY
    if (argc != 9 || wcscmp(argv[1], L"--seed") != 0) {
        std::wcerr << L"usage: LargeLayoutSeed --seed <database> <folder> <columns> <originX> <originY> <stepX> <stepY>\n";
        return 2;
    }
    const Layout layout = {_wtoi(argv[4]), _wtoi(argv[5]), _wtoi(argv[6]), _wtoi(argv[7]), _wtoi(argv[8])};
    if (layout.columns <= 0 || layout.stepX <= 0 || layout.stepY <= 0) return 2;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return fail(L"COM initialization", initialized, 3);
    const int code = seed(argv[2], argv[3], layout);
    CoUninitialize();
    return code;
}
