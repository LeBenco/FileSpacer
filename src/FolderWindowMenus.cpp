// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#include <common.h>
#include "FolderWindow.h"
#include "main.h"
#include "CreateItemWindow.h"
#include "ClosedWindows.h"
#include "ShellMenuIcons.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "Settings.h"
#include "UIStrings.h"
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace filespacer {

FolderWindow::QuickAccessIconThread::QuickAccessIconThread(HWND ownerWindow, int imageSize)
        : owner(ownerWindow), iconSize(imageSize) {}

FolderWindow::QuickAccessIconThread::~QuickAccessIconThread() {
    if (bitmap)
        DeleteObject(bitmap);
}

HBITMAP FolderWindow::QuickAccessIconThread::takeBitmap() {
    AcquireSRWLockExclusive(&stopLock);
    HBITMAP result = bitmap;
    bitmap = nullptr;
    ReleaseSRWLockExclusive(&stopLock);
    return result;
}

void FolderWindow::QuickAccessIconThread::run() {
    // Extract the namespace's own icon, independently of the UI thread's COM objects.
    // Quick Access resolves to Home on Shells exposing that namespace. Use
    // the Shell's legacy Favorites image for this pinned-folders-only menu.
    // These namespace identities are empirical, not SDK known-folder IDs;
    // extraction still uses public APIs and no DLL resource position.
    const wchar_t *iconNamespace = L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}";
    CComPtr<IShellItem> home;
    SFGAOF attributes = 0;
    if (SUCCEEDED(SHCreateItemFromParsingName(
            L"shell:::{F874310E-B6B7-47DC-BC84-B9E6B38F5903}", nullptr,
            IID_PPV_ARGS(&home))) && SUCCEEDED(home->GetAttributes(SFGAO_FOLDER, &attributes))
            && (attributes & SFGAO_FOLDER))
        iconNamespace = L"shell:::{323CA680-C24D-4099-B94D-446DD2D7249E}";
    CComPtr<IShellItem> quickAccess;
    HBITMAP image = nullptr;
    if (FAILED(SHCreateItemFromParsingName(
            iconNamespace, nullptr, IID_PPV_ARGS(&quickAccess)))
            || FAILED(quickAccess->GetAttributes(SFGAO_FOLDER, &attributes))
            || !(attributes & SFGAO_FOLDER))
        return;
    CComHeapPtr<ITEMIDLIST> idList;
    SHFILEINFO fileInfo = {};
    if (SUCCEEDED(SHGetIDListFromObject(quickAccess, &idList))) {
        SHGetFileInfoW(reinterpret_cast<LPCWSTR>(static_cast<ITEMIDLIST *>(idList)),
            0, &fileInfo, sizeof(fileInfo), SHGFI_PIDL | SHGFI_ICON | SHGFI_SMALLICON);
        if (fileInfo.hIcon) {
            image = iconToPARGB32Bitmap(fileInfo.hIcon, iconSize, iconSize);
            DestroyIcon(fileInfo.hIcon);
        }
    }
    if (!image) {
        CComQIPtr<IShellItemImageFactory> factory(quickAccess);
        if (factory && FAILED(factory->GetImage({iconSize, iconSize}, SIIGBF_ICONONLY, &image))) {
            if (image) DeleteObject(image);
            return;
        }
    }
    if (!image)
        return;
    GdiFlush(); // Complete GDI writes before the UI thread reads the bitmap pixels.
    AcquireSRWLockExclusive(&stopLock);
    if (!isStopped()) {
        bitmap = image;
        image = nullptr;
        PostMessage(owner, MSG_QUICK_ACCESS_ICON, 0, 0);
    }
    ReleaseSRWLockExclusive(&stopLock);
    if (image)
        DeleteObject(image);
}

CComPtr<IContextMenu> FolderWindow::queryBackgroundMenu(HMENU *popupMenu) {
    // newItem/openViewMenu pin the window through this helper and its cleanup.
    *popupMenu = nullptr;
    if (!isWindowOperational() || !shellView)
        return nullptr;
    CComPtr<IShellView> view = shellView;
    CComPtr<IContextMenu> contextMenu;
    if (!isWindowOperational()
            || !checkHR(view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&contextMenu)))
            || !isWindowOperational())
        return nullptr;
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        menu(CreatePopupMenu(), destroyMenu);
    if (!menu || !checkHR(contextMenu->QueryContextMenu(menu.get(), 0, IDM_SHELL_FIRST,
            IDM_SHELL_LAST, CMF_OPTIMIZEFORINVOKE)) || !isWindowOperational())
        return nullptr;
    *popupMenu = menu.release(); // The caller owns the complete menu tree.
    return contextMenu;
}

void FolderWindow::newItem(const char *verb) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    HMENU popupMenu = nullptr;
    CComPtr<IContextMenu> contextMenu = queryBackgroundMenu(&popupMenu);
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        menu(popupMenu, destroyMenu);
    if (!contextMenu || !isWindowOperational() || !viewHost)
        return;
    CComPtr<IFolderView2> folderView;
    auto detachSite = [](IContextMenu *handler) { checkHR(IUnknown_SetSite(handler, nullptr)); };
    std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
    // Own the installed site, not an additional reference to the menu handler.
    if (isWindowOperational() && checkHR(viewHost.currentView(IID_PPV_ARGS(&folderView)))
            && isWindowOperational()) {
        // Preserve the Shell site's selection/rename support for the new item.
        if (checkHR(IUnknown_SetSite(contextMenu, folderView)))
            site.reset(contextMenu);
    }
    if (!isWindowOperational())
        return;
    CMINVOKECOMMANDINFO info = {sizeof(info)};
    info.hwnd = hwnd;
    info.lpVerb = verb;
    checkHR(invokeViewCommand(contextMenu, &info));
}

void FolderWindow::trackContextMenu(POINT pos) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational() || !shellView)
        return;
    CComPtr<IShellView> view = shellView;
    if (!isWindowOperational())
        return;
    UINT contextFlags = CMF_CANRENAME | CMF_NODEFAULT;
    if (GetKeyState(VK_SHIFT) < 0)
        contextFlags |= CMF_EXTENDEDVERBS;
    CComPtr<IContextMenu> contextMenu;
    // The application menu belongs to the folder background, regardless of selection.
    if (!checkHR(view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&contextMenu))))
        return;
    if (!isWindowOperational())
        return;
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        menu(CreatePopupMenu(), destroyMenu);
    if (menu && checkHR(contextMenu->QueryContextMenu(menu.get(), 0, IDM_SHELL_FIRST,
            IDM_SHELL_LAST, contextFlags)) && isWindowOperational()) {
        contextMenu2 = contextMenu;
        if (isWindowOperational())
            contextMenu3 = contextMenu;
        HWND menuOwner = getToolbarMenuOwner();
        if (!menuOwner) {
            menuOwner = hwnd;
            HWND shellViewHwnd = nullptr;
            if (isWindowOperational() && SUCCEEDED(view->GetWindow(&shellViewHwnd)) && shellViewHwnd)
                menuOwner = shellViewHwnd;
        }
        int cmd = 0;
        if (isWindowOperational()) {
            cmd = ItemWindow::trackContextMenu(pos, menu.get(), menuOwner);
        }
        contextMenu2 = nullptr;
        contextMenu3 = nullptr;
        if (isWindowOperational() && cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) {
            // The site and all COM temporaries finish before the final save check.
            auto invoke = [&]() {
                auto info = makeInvokeInfo(cmd - IDM_SHELL_FIRST, pos);
                CComHeapPtr<wchar_t> path;
                checkHR(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &path));
                if (!isWindowOperational() || !viewHost)
                    return;
                info.lpDirectoryW = path;
                int pathASize = WideCharToMultiByte(CP_ACP, 0, path, -1, nullptr, 0, nullptr, nullptr);
                std::unique_ptr<char[]> pathA;
                if (checkLE(pathASize)) {
                    pathA.reset(new char[pathASize]);
                    checkLE(WideCharToMultiByte(CP_ACP, 0, path, -1,
                        pathA.get(), pathASize, nullptr, nullptr));
                    info.lpDirectory = pathA.get();
                }
                CComPtr<IFolderView2> folderView;
                auto detachSite = [](IContextMenu *handler) {
                    checkHR(IUnknown_SetSite(handler, nullptr));
                };
                std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
                if (isWindowOperational()
                        && checkHR(viewHost.currentView(IID_PPV_ARGS(&folderView)))
                        && isWindowOperational()) {
                    if (checkHR(IUnknown_SetSite(contextMenu, folderView)))
                        site.reset(contextMenu);
                }
                if (isWindowOperational()) {
                    info.hwnd = hwnd;
                    checkHR(invokeViewCommand(contextMenu, (CMINVOKECOMMANDINFO *)&info));
                }
            };
            invoke();
        }
    }
    menu.reset();
    contextMenu.Release();
    view.Release();
    if (isWindowOperational())
        scheduleStateSave();
}

void FolderWindow::openQuickAccessMenu(POINT point) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    // The Explorer namespace and property are resolved at runtime by
    // enumerateQuickAccessFolders; fail visibly if either is unavailable.
    auto unavailable = [this](UINT stage, HRESULT error) {
        if (!isWindowOperational())
            return;
        MessageBox(hwnd, formatString(IDS_QUICK_ACCESS_UNAVAILABLE,
            getString(stage), (DWORD)error).get(), getString(IDS_QUICK_ACCESS),
            MB_OK | MB_ICONWARNING);
    };
    std::vector<CComPtr<IShellItem>> folders;
    UINT stage = 0;
    HRESULT result = enumerateQuickAccessFolders(&folders, &stage, true);
    if (!isWindowOperational())
        return;
    if (FAILED(result)) {
        unavailable(stage, result);
        return;
    }
    struct Entry {
        std::wstring name;
        std::wstring path;
    };
    std::vector<Entry> entries;
    for (const auto &entry : folders) {
        // Use the enumerated Shell item; resolve the destination only after selection.
        CComHeapPtr<wchar_t> name, path;
        result = entry->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
        if (SUCCEEDED(result) && isWindowOperational())
            result = entry->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &path);
        if (FAILED(result)) {
            unavailable(IDS_QUICK_ACCESS_TARGET, result);
            return;
        }
        if (!isWindowOperational())
            return;
        std::wstring label;
        for (const wchar_t *character = name; *character; ++character) {
            label += *character;
            if (*character == L'&')
                label += L'&'; // A folder name must not become a menu mnemonic.
        }
        entries.push_back({label, std::wstring(path)});
    }

    if (!isWindowOperational())
        return;
    const int iconSize = GetSystemMetrics(SM_CXSMICON);
    auto deleteBitmap = [](HBITMAP bitmap) { DeleteObject(bitmap); };
    std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype(deleteBitmap)>
        ownedEmptyIcon(nullptr, deleteBitmap);
    auto destroyMenu = [](HMENU handle) { DestroyMenu(handle); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        ownedMenu(CreatePopupMenu(), destroyMenu);
    HMENU menu = ownedMenu.get();
    if (!menu) {
        unavailable(IDS_QUICK_ACCESS_MENU, HRESULT_FROM_WIN32(GetLastError()));
        return;
    }
    // Use the same native column for checkmarks and item bitmaps.
    MENUINFO menuInfo = {sizeof(menuInfo)};
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_CHECKORBMP;
    if (!SetMenuInfo(menu, &menuInfo)) {
        result = HRESULT_FROM_WIN32(GetLastError());
        ownedMenu.reset();
        unavailable(IDS_QUICK_ACCESS_MENU, result);
        return;
    }
    // Give every entry its final bitmap layout before displaying the popup.
    // One transparent bitmap is shared until specific Shell icons arrive.
    HBITMAP emptyIcon = nullptr;
    if (!entries.empty()) {
        BITMAPINFO bitmapInfo = {{sizeof(BITMAPINFOHEADER), iconSize, -iconSize, 1, 32, BI_RGB}};
        void *pixels = nullptr;
        emptyIcon = CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS,
            &pixels, nullptr, 0);
        ownedEmptyIcon.reset(emptyIcon);
        if (!emptyIcon) {
            result = HRESULT_FROM_WIN32(GetLastError());
            ownedMenu.reset();
            unavailable(IDS_QUICK_ACCESS_MENU, result);
            return;
        }
        ZeroMemory(pixels, static_cast<size_t>(iconSize) * static_cast<size_t>(iconSize)
            * sizeof(DWORD));
    }
    for (size_t index = 0; index < entries.size(); ++index) {
        const UINT command = static_cast<UINT>(index + 1);
        MENUITEMINFO imageInfo = {sizeof(imageInfo)};
        imageInfo.fMask = MIIM_BITMAP;
        imageInfo.hbmpItem = emptyIcon;
        if (!AppendMenu(menu, MF_STRING, command, entries[index].name.c_str())
                || !SetMenuItemInfo(menu, command, FALSE, &imageInfo)) {
            result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }
    }
    if (entries.empty() && !AppendMenu(menu, MF_STRING | MF_GRAYED, 0,
            getString(IDS_QUICK_ACCESS_EMPTY)))
        result = HRESULT_FROM_WIN32(GetLastError());
    int command = 0;
    bool closeSource = false;
    // Track the item's native menu recursively so this destination popup stays
    // visible and resumes unchanged when the context menu is cancelled.
    struct ContextTarget {
        FolderWindow &owner;
        HMENU menu;
        const std::vector<CComPtr<IShellItem>> &folders;
        CComPtr<IContextMenu> handler;
        int command = 0;
        POINT point = {};
        bool tracking = false;
    } contextTarget{*this, menu, folders};
    const auto menuProc = [](HWND window, UINT message, WPARAM wParam, LPARAM lParam,
            UINT_PTR, DWORD_PTR data) -> LRESULT {
        auto &target = *reinterpret_cast<ContextTarget *>(data);
        if (message == WM_MENURBUTTONUP && reinterpret_cast<HMENU>(lParam) == target.menu
                && wParam < target.folders.size()) {
            POINT position = {};
            if (!target.tracking && GetCursorPos(&position)) {
                target.tracking = true;
                CComPtr<IContextMenu> handler;
                const int selected = target.owner.trackQuickAccessContextMenu(
                    target.folders[wParam], position, handler);
                target.tracking = false;
                if (selected >= IDM_SHELL_FIRST && selected <= IDM_SHELL_LAST) {
                    target.handler = handler;
                    target.command = selected;
                    target.point = position;
                    EndMenu(); // A validated command closes the complete menu chain.
                }
            }
            return 0;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    };
    ShellMenuIcons icons;
    if (SUCCEEDED(result) && isWindowOperational()) {
        std::vector<std::wstring> paths;
        for (const Entry &entry : entries)
            paths.push_back(entry.path);
        icons.start(hwnd, menu, iconSize, std::move(paths));
        const UINT_PTR subclassId = reinterpret_cast<UINT_PTR>(&contextTarget);
        const bool subclassed = isWindowOperational() && checkLE(SetWindowSubclass(
            hwnd, menuProc, subclassId, reinterpret_cast<DWORD_PTR>(&contextTarget)));
        if (isWindowOperational())
            command = TrackPopupMenuEx(menu, TPM_RETURNCMD,
                point.x, point.y, hwnd, nullptr);
        if (subclassed) RemoveWindowSubclass(hwnd, menuProc, subclassId);
        // Sample Ctrl at validation (mouse or Enter), before any Shell work.
        closeSource = settings::getKeepSourceWindowOpen() == (GetKeyState(VK_CONTROL) < 0);
    }
    icons.stop();
    ownedMenu.reset();
    ownedEmptyIcon.reset();
    if (!isWindowOperational())
        return;
    if (FAILED(result)) {
        unavailable(IDS_QUICK_ACCESS_MENU, result);
        return;
    }
    if (contextTarget.handler) {
        auto info = makeInvokeInfo(contextTarget.command - IDM_SHELL_FIRST, contextTarget.point);
        checkHR(invokeShellCommand(contextTarget.handler,
            reinterpret_cast<CMINVOKECOMMANDINFO *>(&info)));
        if (isWindowOperational()) refreshReopenJumpLists();
        return;
    }
    if (command > 0 && static_cast<size_t>(command) <= entries.size()) {
        CComPtr<IShellItem> target;
        result = SHCreateItemFromParsingName(entries[command - 1].path.c_str(), nullptr,
            IID_PPV_ARGS(&target));
        if (!isWindowOperational())
            return;
        if (FAILED(result)) {
            recordFolderPathFailure(entries[command - 1].path.c_str(), result);
            unavailable(IDS_QUICK_ACCESS_TARGET, result);
            return;
        }
        RECT origin = {};
        if (FolderOpeningAnimation::isEnabled()) {
            const HWND toolbar = getQuickAccessToolbar();
            if (toolbar && IsWindowVisible(toolbar)) origin = windowRect(toolbar);
        }
        openChild(target, closeSource, IsRectEmpty(&origin) ? nullptr : &origin);
    }
}

int FolderWindow::trackQuickAccessContextMenu(IShellItem *target, POINT point,
        CComPtr<IContextMenu> &handler) {
    // Keep the namespace item: reparsing its destination can lose Shell verbs.
    if (!checkHR(target->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&handler)))
            || !isWindowOperational()) return 0;
    auto destroyMenu = [](HMENU menu) { DestroyMenu(menu); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        menu(CreatePopupMenu(), destroyMenu);
    UINT flags = CMF_NORMAL | CMF_ITEMMENU;
    if (GetKeyState(VK_SHIFT) < 0) flags |= CMF_EXTENDEDVERBS;
    if (!menu || !checkHR(handler->QueryContextMenu(menu.get(), 0,
            IDM_SHELL_FIRST, IDM_SHELL_LAST, flags)) || !isWindowOperational()) return 0;
    CComPtr<IContextMenu2> previousMenu2 = contextMenu2;
    CComPtr<IContextMenu3> previousMenu3 = contextMenu3;
    contextMenu2 = handler;
    if (isWindowOperational()) contextMenu3 = handler;
    int command = 0;
    if (isWindowOperational())
        command = TrackPopupMenuEx(menu.get(), TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_RECURSE,
            point.x, point.y, hwnd, nullptr);
    contextMenu2 = previousMenu2;
    contextMenu3 = previousMenu3;
    return isWindowOperational() ? command : 0;
}

} // namespace filespacer
