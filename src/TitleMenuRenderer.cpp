// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "TitleMenu.h"
#include <shellapi.h>
#include <commctrl.h>
#include <atlbase.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#undef min
#undef max
#pragma comment(lib, "Msimg32.lib")

namespace filespacer {
namespace {
struct Entry { unsigned id; unsigned type; std::wstring text; };
struct Layer { HWND window; SIZE size; BYTE opacity; std::vector<BYTE> pixels; };
struct Renderer {
    HWND owner = nullptr;
    HMENU menu = nullptr;
    std::wstring directory;
    std::vector<Entry> entries;
    std::vector<Layer> layers;
    bool requested = false, captured = false, nilesoft = false;
    int error = 0;
};
constexpr UINT CAPTURE = WM_APP + 10;

struct Surface {
    BITMAPINFO info = {};
    std::unique_ptr<std::remove_pointer<HDC>::type, decltype(&DeleteDC)> dc{CreateCompatibleDC(nullptr), DeleteDC};
    std::unique_ptr<std::remove_pointer<HBITMAP>::type, decltype(&DeleteObject)> bitmap{nullptr, DeleteObject};
    void *pixels = nullptr;
    HGDIOBJ previous = nullptr;
    Surface(int width, int height) {
        if (!dc || width <= 0 || height <= 0 || width > 2100 || height > 2100) return;
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        info.bmiHeader.biSizeImage = static_cast<DWORD>(width) * static_cast<DWORD>(height) * 4;
        bitmap.reset(CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &pixels, nullptr, 0));
        if (bitmap && pixels) {
            previous = SelectObject(dc.get(), bitmap.get());
            std::memset(pixels, 0, info.bmiHeader.biSizeImage);
        }
    }
    ~Surface() { if (previous) SelectObject(dc.get(), previous); }
    bool valid() const { return pixels && previous; }
};

thread_local Renderer *observedRenderer = nullptr;

BOOL WINAPI observeLayerUpdate(HWND window, HDC destination, POINT *position, SIZE *size,
        HDC source, POINT *origin, COLORREF key, BLENDFUNCTION *blend, DWORD flags) {
    const BOOL result = UpdateLayeredWindow(window, destination, position, size, source, origin, key, blend, flags);
    const DWORD error = GetLastError();
    // Observe only NS's import in this disposable process. Preserve its real API
    // result and last error, and never unwind an exception through NS code.
    if (observedRenderer && source && size && blend && (flags & ULW_ALPHA)
            && blend->AlphaFormat == AC_SRC_ALPHA) try {
        Surface copy(size->cx, size->cy);
        const POINT start = origin ? *origin : POINT{};
        if (copy.valid() && BitBlt(copy.dc.get(), 0, 0, size->cx, size->cy,
                source, start.x, start.y, SRCCOPY)) {
            GdiFlush();
            const auto bytes = static_cast<const BYTE *>(copy.pixels);
            Layer layer = {window, *size, blend->SourceConstantAlpha,
                std::vector<BYTE>(bytes, bytes + copy.info.bmiHeader.biSizeImage)};
            auto found = std::find_if(observedRenderer->layers.begin(), observedRenderer->layers.end(),
                [window](const Layer &value) { return value.window == window; });
            if (found == observedRenderer->layers.end()) observedRenderer->layers.push_back(std::move(layer));
            else *found = std::move(layer);
        }
    } catch (...) { observedRenderer->error = 18; }
    SetLastError(error);
    return result;
}

bool observeLayers(HMODULE module) {
    // The PE import table is read only until the one UpdateLayeredWindow slot is
    // made writable. The DLL file and the User32 export are never patched.
    // Every header field is bounded before it is read: an unexpected image
    // uses the explicit opaque fallback, like a missing import.
    const auto base = reinterpret_cast<BYTE *>(module);
    MEMORY_BASIC_INFORMATION region = {};
    if (VirtualQuery(base, &region, sizeof(region)) != sizeof(region) || region.State != MEM_COMMIT
            || region.Type != MEM_IMAGE || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))
            || region.RegionSize < sizeof(IMAGE_DOS_HEADER)) return false;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < static_cast<LONG>(sizeof(*dos))
            || static_cast<ULONGLONG>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > region.RegionSize) return false;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE
            || static_cast<size_t>(nt->FileHeader.SizeOfOptionalHeader) < sizeof(nt->OptionalHeader)
            || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
            || nt->OptionalHeader.NumberOfRvaAndSizes <= static_cast<DWORD>(IMAGE_DIRECTORY_ENTRY_IMPORT)) return false;
    const DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    const auto entry = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!entry.VirtualAddress || entry.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR)
            || static_cast<ULONGLONG>(entry.VirtualAddress) + entry.Size > imageSize) return false;
    const auto imports = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>(base + entry.VirtualAddress);
    decltype(&UpdateLayeredWindow) original = &UpdateLayeredWindow, replacement = &observeLayerUpdate;
    ULONGLONG address = 0;
    static_assert(sizeof(address) == sizeof(original), "Windows x64 import pointer representation");
    std::memcpy(&address, &original, sizeof(address));
    for (size_t index = 0; index < entry.Size / sizeof(*imports) && imports[index].Name; ++index) {
        const DWORD offset = imports[index].FirstThunk;
        if (!offset || offset >= imageSize) continue;
        const auto slots = reinterpret_cast<IMAGE_THUNK_DATA64 *>(base + offset);
        const size_t count = (imageSize - offset) / sizeof(*slots);
        for (size_t slot = 0; slot < count && slots[slot].u1.Function; ++slot) {
            if (slots[slot].u1.Function != address) continue;
            DWORD protection = 0;
            if (!VirtualProtect(&slots[slot].u1.Function, sizeof(address), PAGE_READWRITE, &protection)) return false;
            std::memcpy(&slots[slot].u1.Function, &replacement, sizeof(address));
            DWORD unused = 0;
            return VirtualProtect(&slots[slot].u1.Function, sizeof(address), protection, &unused) != FALSE;
        }
    }
    return false;
}

bool sameModulePath(const std::wstring &registered, const std::wstring &argument) {
    auto full = [](const std::wstring &path) {
        std::wstring result(GetFullPathNameW(path.c_str(), 0, nullptr, nullptr), L'\0');
        const DWORD length = result.empty() ? 0
            : GetFullPathNameW(path.c_str(), static_cast<DWORD>(result.size()), &result[0], nullptr);
        if (!length || length >= result.size()) return path;
        result.resize(length);
        return result;
    };
    return lstrcmpiW(full(registered).c_str(), full(argument).c_str()) == 0;
}

bool writeBitmap(const std::wstring &path, const BITMAPINFO &info, const void *pixels) {
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb")) return false;
    BITMAPFILEHEADER header = {};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + info.bmiHeader.biSizeImage;
    const bool written = fwrite(&header, sizeof(header), 1, file) == 1
        && fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file) == 1
        && fwrite(pixels, 1, info.bmiHeader.biSizeImage, file) == info.bmiHeader.biSizeImage;
    const int closed = fclose(file);
    return written && closed == 0;
}

void capture(Renderer &renderer, HWND popup) {
    observedRenderer = nullptr;
    MENUBARINFO bar = {sizeof(bar)};
    RECT bounds = {};
    if (!GetMenuBarInfo(popup, OBJID_CLIENT, 0, &bar) || !bar.hMenu || !GetWindowRect(popup, &bounds)) {
        renderer.error = 10; EndMenu(); return;
    }
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0 || width > 2000 || height > 2000) {
        renderer.error = 11; EndMenu(); return;
    }
    const int count = GetMenuItemCount(bar.hMenu);
    bool alpha = false;
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW item = {sizeof(item)}; item.fMask = MIIM_FTYPE;
        if (!GetMenuItemInfoW(bar.hMenu, static_cast<UINT>(i), TRUE, &item)) renderer.error = 13;
        if (renderer.nilesoft && (item.fType & MFT_OWNERDRAW)) alpha = true;
    }
    const Layer *background = nullptr; RECT layerRect = {};
    if (alpha) for (const auto &layer : renderer.layers) {
        RECT rect = {};
        if (layer.window != popup && GetWindowRect(layer.window, &rect)
                && rect.left <= bounds.left && rect.top <= bounds.top
                && rect.right >= bounds.right && rect.bottom >= bounds.bottom) {
            if (!background || layer.size.cx * layer.size.cy > background->size.cx * background->size.cy) {
                background = &layer; layerRect = rect;
            }
        }
    }
    const bool simplified = alpha && !background;
    RECT extent = bounds;
    if (background) UnionRect(&extent, &extent, &layerRect);
    Surface image(extent.right - extent.left, extent.bottom - extent.top);
    Surface foreground(width, height);
    if (!image.valid() || !foreground.valid()) { renderer.error = 12; EndMenu(); return; }
    const int insetX = bounds.left - extent.left, insetY = bounds.top - extent.top;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (background) {
        Surface layer(background->size.cx, background->size.cy);
        if (!layer.valid()) { renderer.error = 12; EndMenu(); return; }
        std::memcpy(layer.pixels, background->pixels.data(), background->pixels.size());
        blend.SourceConstantAlpha = background->opacity;
        if (!AlphaBlend(image.dc.get(), layerRect.left - extent.left, layerRect.top - extent.top,
                background->size.cx, background->size.cy, layer.dc.get(), 0, 0,
                background->size.cx, background->size.cy, blend)) renderer.error = 21;
        blend.SourceConstantAlpha = 255;
    } else if (simplified) {
        RECT area = {0, 0, width, height};
        FillRect(image.dc.get(), &area, GetSysColorBrush(COLOR_MENU));
    } else if (!PrintWindow(popup, image.dc.get(), 0)) renderer.error = 17;
    FILE *rows = nullptr;
    if (_wfopen_s(&rows, (renderer.directory + L"\\frame.tsv").c_str(), L"w, ccs=UTF-8")) renderer.error = 15;
    if (rows) fwprintf_s(rows, L"%ld\t%ld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", extent.right - extent.left,
        extent.bottom - extent.top, alpha && !simplified, insetX, insetY, background != nullptr, width, height, simplified);
    std::vector<int> sources, positions;
    std::vector<int> origins(static_cast<size_t>(count), -1);
    std::vector<unsigned> types(static_cast<size_t>(count), 0);
    std::vector<bool> locked(static_cast<size_t>(count), true);
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW item = {sizeof(item)}; item.fMask = MIIM_ID | MIIM_FTYPE;
        if (!GetMenuItemInfoW(bar.hMenu, static_cast<UINT>(i), TRUE, &item)) { renderer.error = 13; break; }
        types[static_cast<size_t>(i)] = item.fType;
        for (size_t source = 0; source < renderer.entries.size(); ++source)
            if (renderer.entries[source].id == item.wID) {
                origins[static_cast<size_t>(i)] = static_cast<int>(source);
                sources.push_back(static_cast<int>(source)); positions.push_back(i); break;
            }
    }
    // The original visible entries should retain their relative order.
    // Only entries present in every longest increasing subsequence are
    // certainly unaffected by an NS ordering rule. Ambiguous swaps stay locked.
    std::vector<int> before(sources.size(), 1), after(sources.size(), 1);
    int longest = 0;
    for (size_t i = 0; i < sources.size(); ++i) {
        for (size_t j = 0; j < i; ++j)
            if (sources[j] < sources[i]) before[i] = std::max(before[i], before[j] + 1);
        longest = std::max(longest, before[i]);
    }
    for (size_t i = sources.size(); i-- > 0;)
        for (size_t j = i + 1; j < sources.size(); ++j)
            if (sources[i] < sources[j]) after[i] = std::max(after[i], after[j] + 1);
    std::vector<int> choices(static_cast<size_t>(longest + 1), 0);
    for (size_t i = 0; i < sources.size(); ++i)
        if (before[i] + after[i] - 1 == longest) ++choices[static_cast<size_t>(before[i])];
    for (size_t i = 0; i < sources.size(); ++i)
        locked[static_cast<size_t>(positions[i])] = before[i] + after[i] - 1 != longest
            || choices[static_cast<size_t>(before[i])] != 1;
    // NS replaces separator IDs. Recover an unchanged run only when its native
    // and rendered counts agree between the same stable command anchors.
    int previousRow = -1, previousSource = -1;
    for (int next = 0; next <= count; ++next) {
        const int source = next == count ? static_cast<int>(renderer.entries.size()) : origins[static_cast<size_t>(next)];
        if (next != count && (source < 0 || locked[static_cast<size_t>(next)]
                || (types[static_cast<size_t>(next)] & MFT_SEPARATOR))) continue;
        std::vector<int> nativeSeparators, renderedSeparators;
        for (int entry = previousSource + 1; entry < source; ++entry)
            if (renderer.entries[static_cast<size_t>(entry)].type == 1
                    && std::find(origins.begin(), origins.end(), entry) == origins.end())
                nativeSeparators.push_back(entry);
        for (int row = previousRow + 1; row < next; ++row)
            if (origins[static_cast<size_t>(row)] < 0 && (types[static_cast<size_t>(row)] & MFT_SEPARATOR))
                renderedSeparators.push_back(row);
        if (nativeSeparators.size() == renderedSeparators.size())
            for (size_t index = 0; index < nativeSeparators.size(); ++index) {
                const size_t row = static_cast<size_t>(renderedSeparators[index]);
                origins[row] = nativeSeparators[index]; locked[row] = false;
            }
        previousRow = next; previousSource = source;
    }
    for (int i = 0; !renderer.error && i < count; ++i) {
        MENUITEMINFOW item = {sizeof(item)};
        wchar_t text[1024] = {};
        item.fMask = MIIM_ID | MIIM_DATA | MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
        item.dwTypeData = text; item.cch = _countof(text);
        RECT rect = {};
        if (!GetMenuItemInfoW(bar.hMenu, static_cast<UINT>(i), TRUE, &item)
                || !GetMenuItemRect(renderer.owner, bar.hMenu, static_cast<UINT>(i), &rect)) {
            renderer.error = 13; break;
        }
        OffsetRect(&rect, -bounds.left, -bounds.top);
        if (alpha) {
            DRAWITEMSTRUCT draw = {};
            draw.CtlType = ODT_MENU; draw.itemID = item.wID; draw.itemAction = ODA_DRAWENTIRE;
            draw.itemState = (item.fState & (MFS_DISABLED | MFS_GRAYED)) ? ODS_DISABLED : 0;
            draw.hwndItem = reinterpret_cast<HWND>(bar.hMenu); draw.hDC = foreground.dc.get();
            draw.rcItem = rect; draw.itemData = item.dwItemData;
            const int saved = SaveDC(foreground.dc.get());
            SendMessageW(renderer.owner, WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(&draw));
            RestoreDC(foreground.dc.get(), saved);
        }
        const int source = origins[static_cast<size_t>(i)];
        const bool managed = source < 0 || (!(item.fType & MFT_SEPARATOR)
            && renderer.entries[static_cast<size_t>(source)].text != text);
        if (rows) fwprintf_s(rows, L"%u\t%d\t%d\t%d\t%u\t%ld\t%ld\t%ld\t%ld\t%s\n",
            item.wID, source, managed, locked[static_cast<size_t>(i)] ? 1 : 0, item.fType,
            rect.left, rect.top, rect.right, rect.bottom, text);
    }
    if (alpha && !AlphaBlend(image.dc.get(), insetX, insetY, width, height,
            foreground.dc.get(), 0, 0, width, height, blend)) renderer.error = 21;
    if (rows) {
        const bool failed = ferror(rows) != 0;
        const int closed = fclose(rows);
        if (failed || closed) renderer.error = 15;
    }
    GdiFlush();
    if (!writeBitmap(renderer.directory + L"\\frame.bmp", image.info, image.pixels)) renderer.error = 16;
    renderer.captured = renderer.error == 0;
    EndMenu();
}

LRESULT CALLBACK ownerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    Renderer *renderer = reinterpret_cast<Renderer *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        renderer = static_cast<Renderer *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
        renderer->owner = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(renderer));
    }
    if (renderer && message == WM_ENTERIDLE && wParam == MSGF_MENU && !renderer->requested) {
        renderer->requested = true;
        PostMessageW(window, CAPTURE, 0, lParam);
        return 0;
    }
    if (renderer && message == CAPTURE) { capture(*renderer, reinterpret_cast<HWND>(lParam)); return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

int render(const std::wstring &directory, const std::wstring &dll, POINT origin) {
    Renderer renderer; renderer.directory = directory; renderer.nilesoft = !dll.empty();
    FILE *request = nullptr;
    if (_wfopen_s(&request, (directory + L"\\request.tsv").c_str(), L"r, ccs=UTF-8")) return 1;
    wchar_t line[2048] = {};
    while (fgetws(line, _countof(line), request)) {
        wchar_t *tab = wcschr(line, L'\t');
        if (!tab) { fclose(request); return 2; }
        *tab = 0;
        wchar_t *second = wcschr(tab + 1, L'\t');
        if (!second) { fclose(request); return 2; }
        *second = 0;
        std::wstring text(second + 1);
        while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r')) text.pop_back();
        renderer.entries.push_back({static_cast<unsigned>(wcstoul(line, nullptr, 10)),
            static_cast<unsigned>(wcstoul(tab + 1, nullptr, 10)), text});
    }
    fclose(request);
    if (renderer.entries.size() > 100) return 3;
    WNDCLASSW type = {};
    type.lpfnWndProc = ownerProc; type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"FileSpacer Title Menu Renderer";
    if (!RegisterClassW(&type)) return 4;
    // Match the real title-menu owner: a child of FileSpacer Item Window.
    // These windows exist only on the disposable private rendering desktop.
    WNDCLASSW frameType = {};
    frameType.lpfnWndProc = DefWindowProcW; frameType.hInstance = type.hInstance;
    frameType.lpszClassName = L"FileSpacer Item Window";
    if (!RegisterClassW(&frameType)) return 4;
    std::unique_ptr<std::remove_pointer<HWND>::type, decltype(&DestroyWindow)> frame(
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, frameType.lpszClassName, L"",
            WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, type.hInstance, nullptr), DestroyWindow);
    if (!frame) return 5;
    HWND owner = CreateWindowExW(0, type.lpszClassName, L"Private renderer",
        WS_CHILD, 0, 0, 10, 10, frame.get(), nullptr, type.hInstance, &renderer);
    if (!owner) return 5;
    renderer.menu = CreatePopupMenu();
    for (const auto &entry : renderer.entries) {
        MENUITEMINFOW item = {sizeof(item)};
        item.fMask = MIIM_ID | MIIM_FTYPE;
        item.wID = entry.id;
        if (entry.type == 1) item.fType = MFT_SEPARATOR;
        else { item.fMask |= MIIM_STRING; item.dwTypeData = const_cast<wchar_t *>(entry.text.c_str()); }
        if (!InsertMenuItemW(renderer.menu, static_cast<UINT>(-1), TRUE, &item)) renderer.error = 6;
    }
    if (!dll.empty()) {
        // Only the handler registered for the Shell is ever loaded, through COM:
        // the command-line path is checked against that registration, not used.
        CLSID clsid = {}; std::wstring registered;
        if (FAILED(findTitleMenuExtension(&clsid, &registered))) renderer.error = 8;
        else if (!sameModulePath(registered, dll)) renderer.error = 22;
        else {
            CComPtr<IClassFactory> unused;
            HMODULE nilesoftModule = nullptr;
            if (FAILED(CoGetClassObject(clsid, CLSCTX_INPROC_SERVER, nullptr, IID_PPV_ARGS(&unused)))) renderer.error = 7;
            // This reference keeps the module mapped until exit, as the former
            // LoadLibrary did: NS hooks stay loaded until exit. An unavailable
            // layer capture uses the explicit opaque fallback.
            else if (checkLE(GetModuleHandleExW(0, registered.c_str(), &nilesoftModule))) observeLayers(nilesoftModule);
        }
    }
    observedRenderer = &renderer;
    if (!renderer.error) TrackPopupMenuEx(renderer.menu, TPM_LEFTALIGN | TPM_TOPALIGN, origin.x + 48, origin.y + 48, owner, nullptr);
    observedRenderer = nullptr;
    if (renderer.menu && IsMenu(renderer.menu)) DestroyMenu(renderer.menu);
    DestroyWindow(owner);
    return renderer.error ? renderer.error : (renderer.captured ? 0 : 14);
}
}


int renderTitleMenu(const wchar_t *directory, const wchar_t *nilesoft, POINT origin) {
    const HRESULT initialized = OleInitialize(nullptr);
    if (FAILED(initialized)) return 20;
    const int result = render(directory, nilesoft, origin);
    OleUninitialize();
    return result;
}
} // namespace filespacer
