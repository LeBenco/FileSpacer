#pragma once
#include <common.h>
#include "GDIUtils.h"
#include <atlbase.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace filespacer {

// Owns the icon work for one popup only. The worker never accesses a menu or UI COM object.
class ShellMenuIcons {
    struct Icon {
        UINT command;
        HBITMAP bitmap;
    };
    struct State {
        SRWLOCK lock = SRWLOCK_INIT;
        bool cancelled = false;
        HWND owner = nullptr;
        HMENU menu = nullptr;
        UINT message = 0;
        int iconSize = 0;
        UINT firstCommand = 1;
        std::vector<std::wstring> paths;
        std::vector<Icon> pending;
        std::vector<HBITMAP> displayed; // UI thread only; freed after the popup is destroyed.
        ~State() {
            for (const Icon &icon : pending)
                DeleteObject(icon.bitmap);
            for (HBITMAP bitmap : displayed)
                DeleteObject(bitmap);
        }
    };
    std::shared_ptr<State> state = std::make_shared<State>();

    static DWORD WINAPI load(void *parameter) {
        std::unique_ptr<std::shared_ptr<State>> parameterOwner(
            static_cast<std::shared_ptr<State> *>(parameter));
        const auto work = *parameterOwner;
        for (size_t index = 0; index < work->paths.size(); ++index) {
            AcquireSRWLockShared(&work->lock);
            const bool cancelled = work->cancelled;
            ReleaseSRWLockShared(&work->lock);
            if (cancelled)
                break;
            CComPtr<IShellItemImageFactory> factory;
            HBITMAP bitmap = nullptr;
            if (FAILED(SHCreateItemFromParsingName(work->paths[index].c_str(), nullptr,
                    IID_PPV_ARGS(&factory)))
                    || FAILED(factory->GetImage({work->iconSize, work->iconSize},
                        SIIGBF_ICONONLY, &bitmap))) {
                // Use a stock icon only after the specific Shell icon failed to load.
                SHSTOCKICONINFO info = {sizeof(info)};
                if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER,
                        SHGSI_ICON | SHGSI_SMALLICON, &info))) {
                    bitmap = iconToPARGB32Bitmap(info.hIcon, work->iconSize, work->iconSize);
                    DestroyIcon(info.hIcon);
                }
            }
            if (!bitmap)
                continue;
            AcquireSRWLockExclusive(&work->lock);
            if (!work->cancelled) {
                work->pending.push_back({work->firstCommand + static_cast<UINT>(index), bitmap});
                PostMessage(work->owner, work->message, 0, 0);
                bitmap = nullptr; // State owns it even if posting fails.
            }
            ReleaseSRWLockExclusive(&work->lock);
            if (bitmap)
                DeleteObject(bitmap);
        }
        return 0;
    }

    static LRESULT CALLBACK receive(HWND owner, UINT message, WPARAM wParam,
            LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
        State *work = reinterpret_cast<State *>(data);
        if (message == WM_NCDESTROY) {
            AcquireSRWLockExclusive(&work->lock);
            work->cancelled = true;
            work->owner = nullptr;
            ReleaseSRWLockExclusive(&work->lock);
            RemoveWindowSubclass(owner, receive, id);
        } else if (message == work->message) {
            std::vector<Icon> icons;
            AcquireSRWLockExclusive(&work->lock);
            icons.swap(work->pending);
            ReleaseSRWLockExclusive(&work->lock);
            for (const Icon &icon : icons) {
                MENUITEMINFO info = {sizeof(info)};
                info.fMask = MIIM_BITMAP;
                info.hbmpItem = icon.bitmap;
                if (SetMenuItemInfo(work->menu, icon.command, FALSE, &info))
                    work->displayed.push_back(icon.bitmap);
                else
                    DeleteObject(icon.bitmap);
            }
            if (!icons.empty()) {
                // Locate the displayed popup using documented menu geometry and identity.
                RECT rect = {};
                if (GetMenuItemRect(nullptr, work->menu, 0, &rect)) {
                    HWND popup = WindowFromPoint({(rect.left + rect.right) / 2,
                        (rect.top + rect.bottom) / 2});
                    MENUBARINFO info = {sizeof(info)};
                    if (popup && GetMenuBarInfo(popup, OBJID_CLIENT, 0, &info)
                            && info.hMenu == work->menu)
                        RedrawWindow(popup, nullptr, nullptr,
                            RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
                }
            }
            return 0;
        }
        return DefSubclassProc(owner, message, wParam, lParam);
    }

public:
    void start(HWND owner, HMENU menu, int iconSize, std::vector<std::wstring> paths,
            UINT firstCommand = 1) {
        if (paths.empty())
            return;
        state->owner = owner;
        state->menu = menu;
        state->iconSize = iconSize;
        state->firstCommand = firstCommand;
        state->paths = std::move(paths);
        state->message = RegisterWindowMessage(L"FileSpacer.ShellMenuIcons");
        if (!state->message || !SetWindowSubclass(owner, receive,
                reinterpret_cast<UINT_PTR>(state.get()), reinterpret_cast<DWORD_PTR>(state.get()))) {
            state->owner = nullptr;
            return;
        }
        auto parameter = new std::shared_ptr<State>(state);
        if (!SHCreateThread(load, parameter, CTF_COINIT_STA, nullptr)) {
            delete parameter;
            stop();
        }
    }
    void stop() {
        AcquireSRWLockExclusive(&state->lock);
        state->cancelled = true;
        HWND owner = state->owner;
        state->owner = nullptr;
        ReleaseSRWLockExclusive(&state->lock);
        if (owner)
            RemoveWindowSubclass(owner, receive, reinterpret_cast<UINT_PTR>(state.get()));
    }
    ~ShellMenuIcons() { stop(); }
};

} // namespace
