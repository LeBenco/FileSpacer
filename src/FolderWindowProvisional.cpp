// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#include <common.h>
#include "FolderWindow.h"
#include "FolderWindowInternal.h"
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "ShellViewOperations.h"
#include "ShellViewIcons.h"
#include "ShellViewColumns.h"
#include "MixedFolder.h"
#include "GeomUtils.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "Settings.h"
#include "DPI.h"
#include "UIStrings.h"
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <propkey.h>
#include <propsys.h>
#include <Propvarutil.h>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace filespacer {

thread_local HHOOK FolderWindow::provisionalHook = nullptr;

// Created by the first thread that watches a window, not by every thread: a
// thread_local member with a dynamic initializer would run it, and allocate,
// on each thread the process starts, including one started while the process
// exits and the Debug runtime has already released its heap lock.
std::vector<FolderWindow *> &FolderWindow::provisionalWindowList() {
    static thread_local std::vector<FolderWindow *> windows;
    return windows;
}

bool FolderWindow::watchProvisionalInput(bool watch) {
    auto &provisionalWindows = provisionalWindowList();
    auto found = std::find(provisionalWindows.begin(), provisionalWindows.end(), this);
    if (watch) {
        if (found != provisionalWindows.end()) return true;
        if (!provisionalHook)
            provisionalHook = SetWindowsHookEx(WH_GETMESSAGE, provisionalMessageHook, nullptr, GetCurrentThreadId());
        if (!provisionalHook) return false;
        provisionalWindows.push_back(this);
    } else {
        if (found != provisionalWindows.end()) provisionalWindows.erase(found);
        if (provisionalWindows.empty() && provisionalHook && UnhookWindowsHookEx(provisionalHook))
            provisionalHook = nullptr;
    }
    return true;
}

LRESULT CALLBACK FolderWindow::provisionalMessageHook(int code, WPARAM removed, LPARAM data) {
    if (code < 0) return CallNextHookEx(nullptr, code, removed, data);
    if (code == HC_ACTION && removed == PM_REMOVE) {
        auto msg = reinterpret_cast<MSG *>(data);
        const bool down = msg->message == WM_LBUTTONDOWN || msg->message == WM_RBUTTONDOWN
            || msg->message == WM_MBUTTONDOWN || msg->message == WM_POINTERDOWN;
        const bool key = msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN;
        const bool wheel = msg->message == WM_MOUSEWHEEL;
        const bool up = msg->message == WM_LBUTTONUP;
        ItemWindow *target = (down || key || wheel || up) ? ItemWindow::fromChildWindow(msg->hwnd) : nullptr;
        FolderWindow *window = nullptr;
        for (auto candidate : provisionalWindowList())
            if (static_cast<ItemWindow *>(candidate) == target) { window = candidate; break; }
        if (window && window->isWindowOperational()) {
            CComPtr<ItemWindow> keepAlive(window);
            const bool exit = key && ((msg->wParam == VK_F4 && GetKeyState(VK_MENU) < 0)
                || msg->wParam == VK_F5 || (msg->wParam == VK_UP && GetKeyState(VK_MENU) < 0));
            auto consume = [&]() { msg->message = WM_NULL; msg->hwnd = nullptr; msg->wParam = msg->lParam = 0; };
            const HWND body = window->provisionalViewWindow;
            const bool inView = body && (msg->hwnd == body || IsChild(body, msg->hwnd));
            if ((window->provisionalCaptureBusy || (window->provisionalInputBlocked && inView)) && !exit) {
                // This hook also sees messages retrieved by a nested COM/Shell
                // loop. They must not mutate the view while its baseline is read.
                consume();
            } else if (!exit && window->iconRestoreFailed && !window->provisionalBaseline
                    && !window->restoringState && !window->viewMenuPreview && !window->viewMenuRestore && inView) {
                // Failed observations do not consume the user's input or
                // disable the view. Keep the known reference and warn once.
                if (up) window->scheduleStateSave(true);
                else if (!GetCapture() && (down || (key && (msg->wParam == VK_APPS
                        || (msg->wParam == VK_F10 && GetKeyState(VK_SHIFT) < 0))))) {
                    GUITHREADINFO info = {sizeof(info)};
                    if (GetGUIThreadInfo(GetCurrentThreadId(), &info)
                            && !(info.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) {
                        try { window->observeIconChange(); }
                        catch (...) { window->iconChangeBaseline.reset(); } // Never unwind through User32.
                    }
                }
            } else if (!exit && window->provisionalReady && !window->restoringState
                    && !window->viewMenuPreview && !window->viewMenuRestore) {
                CComPtr<IShellView> source = window->shellView;
                const ULONG_PTR generation = window->viewGeneration;
                const HWND recipient = msg->hwnd;
                if (window->isWindowOperational() && window->shellView == source
                        && window->viewGeneration == generation && body
                        && (recipient == body || IsChild(body, recipient))) {
                    if (up) {
                        window->viewStateDirty(FolderState::Icons);
                        SetTimer(window->hwnd, TIMER_SAVE_STATE, 250, nullptr);
                    } else if (!GetCapture()) {
                        GUITHREADINFO info = {sizeof(info)};
                        if (GetGUIThreadInfo(GetCurrentThreadId(), &info)
                                && !(info.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) {
                            const bool positions = down || (key && (msg->wParam == VK_APPS
                                || (msg->wParam == VK_F10 && GetKeyState(VK_SHIFT) < 0)));
                            bool captured = false;
                            try { captured = window->captureProvisionalCurrent(positions, true); }
                            catch (...) { captured = false; } // Do not unwind through User32.
                            if (!captured || !window->isWindowOperational() || window->shellView != source
                                    || window->viewGeneration != generation || msg->hwnd != recipient
                                    || ItemWindow::fromChildWindow(recipient) != target) {
                                consume();
                                if (window->isWindowOperational() && window->provisionalBaseline) {
                                    window->blockProvisionalInput(true);
                                    SetTimer(window->hwnd, TIMER_SAVE_STATE, 250, nullptr);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return CallNextHookEx(nullptr, code, removed, data);
}

bool FolderWindow::captureProvisionalCurrent(bool icons, bool beforeInput) {
    if (!provisionalBaseline || !provisionalReady || !provisionalCurrent || provisionalCaptureBusy
            || restoringState || viewMenuPreview || viewMenuRestore) return false;
    CComPtr<ItemWindow> keepAlive(this);
    provisionalCaptureBusy = true;
    struct CaptureGuard { bool &busy; ~CaptureGuard() { busy = false; } } guard{provisionalCaptureBusy};
    const HWND owner = hwnd;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration, enumeration = viewHost.enumerationSerial();
    auto current = [&]() {
        return hwnd == owner && source == shellView && viewGeneration == generation
            && enumeration == viewHost.enumerationSerial() && !getFolderStateKey().empty()
            && provisionalBaseline && provisionalCurrent;
    };
    CComQIPtr<IFolderView2> view(source);
    if (!owner || !view || !current()) return false;
    const FolderViewSettings available = captureOptions(view, {});
    if (!current()) return false;
    auto options = provisionalCurrent->view.mergeChanges(provisionalCurrent->view, available);
    if (beforeInput) {
        auto missing = available;
        missing.present &= ~provisionalBaseline->view.present;
        provisionalBaseline->view = provisionalBaseline->view.mergeChanges({}, missing);
    }
    const bool spatial = spatialView(view);
    if (!current()) return false;
    std::vector<uint8_t> captured;
    bool capturedIcons = false;
    if (icons && spatial && searchText.empty()) {
        CComPtr<IShellFolder> identityFolder;
        const HRESULT bound = view->GetFolder(IID_PPV_ARGS(&identityFolder));
        if (FAILED(bound) || !identityFolder || !current()) return false;
        std::vector<uint8_t> visible;
        const HRESULT result = captureIconPositions(view, current, false, {}, visible);
        if (result != S_OK || !current()) return false;
        if (beforeInput) {
            std::vector<uint8_t> extended;
            if (FAILED(extendIconBaseline(provisionalBaseline->icons, visible, extended, identityFolder, current))
                    || !current()) return false;
            provisionalBaseline->icons.swap(extended);
        }
        // An incomplete observation extends the last complete unfiltered one;
        // it never replaces that folder's full position set with a subset.
        if (FAILED(mergeIconChanges(provisionalCurrent->icons, provisionalCurrent->icons,
                visible, enumerationComplete, captured, identityFolder, current))) return false;
        if (!current()) return false;
        capturedIcons = true;
    }
    provisionalCurrent->view = std::move(options);
    if (capturedIcons) {
        provisionalCurrent->icons.swap(captured);
        provisionalIconsComplete = enumerationComplete;
    }
    return true;
}

bool FolderWindow::recoverProvisionalView() {
    if (!provisionalBaseline || !provisionalReady || !provisionalCurrent || !viewReady
            || !enumerationComplete || restoringState || viewMenuPreview || viewMenuRestore) return false;
    CComPtr<ItemWindow> keepAlive(this);
    const HWND owner = hwnd;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto currentView = [&]() { return hwnd == owner && shellView == source && viewGeneration == generation; };
    auto current = [&]() { return currentView() && !getFolderStateKey().empty()
        && provisionalBaseline && provisionalCurrent; };
    CComQIPtr<IFolderView2> view(source);
    if (!owner || !view || !current()) return false;
    CComPtr<IShellFolder> identityFolder;
    const HRESULT bound = view->GetFolder(IID_PPV_ARGS(&identityFolder));
    if (FAILED(bound) || !identityFolder || !current()) return false;
    const bool isClosing = !isWindowOperational();
    if (!isClosing) {
        GUITHREADINFO info = {sizeof(info)};
        if (GetCapture() || !GetGUIThreadInfo(GetCurrentThreadId(), &info)
                || (info.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) return false;
    }
    if (!provisionalRestore) {
        if (!captureProvisionalCurrent(true)) return false;
        if (!current()) return false;
        auto target = std::unique_ptr<ShellViewState>(new ShellViewState);
        target->view = getSavedFolderState().view.mergeChanges(provisionalBaseline->view,
            provisionalCurrent->view, provisionalColumnWidths);
        const bool complete = provisionalIconsComplete && searchText.empty() && spatialView(view);
        if (!current()) return false;
        if (FAILED(mergeIconChanges(getSavedFolderState().icons, provisionalBaseline->icons,
                provisionalCurrent->icons, complete, target->icons, identityFolder, current))) return false;
        if (!isClosing && FAILED(captureReopenSelection(source, current, target->selection))) return false;
        if (!current()) return false;
        provisionalRestore = std::move(target);
    }
    const auto recoveredState = provisionalRestore; // Keep helper arguments alive across expiration callbacks.
    if (!isClosing) {
        blockProvisionalInput(true);
        if (!current() || !isWindowOperational()) return false;
        struct RestoreGuard {
            bool &restoring;
            decltype(currentView) &valid;
            ~RestoreGuard() { if (valid()) restoring = false; }
        } restoreGuard{restoringState, currentView};
        restoringState = true;
        if (!provisionalRestoreOptionsApplied) {
            // Consume before native calls; a refill may dispatch another callback.
            provisionalRestoreOptionsApplied = true;
            int before = -1, after = -1;
            const HRESULT counted = view->ItemCount(SVGIO_ALLVIEW, &before);
            if (!current()) return false;
            const HRESULT restored = restoreOptions(view, recoveredState->view);
            if (!current()) return false;
            if (SUCCEEDED(restored)) applyIconSpacing(source, current);
            if (!current()) return false;
            viewOptionsEnumerationBaseline = viewHost.enumerationSerial();
            const HRESULT recounted = view->ItemCount(SVGIO_ALLVIEW, &after);
            if (!current()) return false;
            restoringState = false;
            if (FAILED(restored)) {
                provisionalRestoreOptionsApplied = false;
                checkHR(restored);
                return false; // Never capture a partly restored fallback over SQL.
            }
            if (FAILED(counted) || FAILED(recounted) || before != after) {
                // Not proven as a refill: keep the window usable until the
                // EnumDone that completes the view. The merge is recomputed
                // then, so interactions made meanwhile are still observed.
                enumerationComplete = false;
                beginLoading();
                provisionalRestore.reset();
                blockProvisionalInput(false);
                return false;
            }
            restoringState = true;
        }
        getSavedFolderState().view = recoveredState->view;
        getSavedFolderState().icons = recoveredState->icons;
        viewStateDirty(FolderState::View | FolderState::Icons);
        if (searchText.empty() && spatialView(view)) {
            if (!current()) return false;
            const ULONG_PTR enumeration = viewHost.enumerationSerial();
            auto iconCurrent = [&]() { return current() && enumerationComplete
                && viewHost.enumerationSerial() == enumeration; };
            std::vector<uint8_t> visible, pruned;
            HRESULT result = captureIconPositions(view, iconCurrent, true, {}, visible);
            if (SUCCEEDED(result)) result = mergeIconChanges(recoveredState->icons, {}, visible, true,
                pruned, identityFolder, iconCurrent);
            if (!iconCurrent()) return false;
            if (SUCCEEDED(result)) result = restoreIconPositions(view, pruned, iconCurrent);
            if (!iconCurrent()) return false;
            // Complete the merge even after a failed application: persistence
            // uses recoveredState, never the partly positioned view.
            iconRestoreResult(result);
            if (!iconCurrent()) return false;
            if (SUCCEEDED(result)) recoveredState->icons.swap(pruned);
        }
        if (!current()) return false;
        const HRESULT selectionResult = restoreReopenSelection(source, recoveredState->selection, current, &selectionClear);
        if (!current()) return false;
        restoringState = false;
        checkHR(selectionResult); // Selection failure cannot invalidate the known state or keep input disabled.
    }
    // Closing merges without replaying the native layout. Later close callbacks
    // must keep the known positions instead of capturing that unreplayed view.
    if (isClosing) iconRestoreFailed = true;
    getSavedFolderState().view = std::move(recoveredState->view);
    getSavedFolderState().icons = std::move(recoveredState->icons);
    viewStateDirty(FolderState::View | FolderState::Icons);
    provisionalRestore.reset();
    provisionalCurrent.reset();
    provisionalBaseline.reset();
    provisionalColumnWidths.clear();
    // A closing window never re-registers with the input hook after onDestroy removed it.
    watchProvisionalInput(!isClosing && iconRestoreFailed);
    if (!isClosing) blockProvisionalInput(false);
    if (!isClosing && currentView()) {
        replayProviderSync(); // Outside this restoration, not from within it.
        resumeColumnSync();
    }
    return currentView();
}

// A provider synchronization deferred by synchronizeViewProvider() runs once
// the step that deferred it is over, from the message queue rather than from
// within that step's callback.
void FolderWindow::blockProvisionalInput(bool blocked) {
    provisionalInputBlocked = blocked; // EnableWindow can dispatch messages.
    const HWND owner = hwnd;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && hwnd == owner && shellView == source
            && viewGeneration == generation && provisionalInputBlocked == blocked;
    };
    if (!current()) return;
    HWND body = nullptr;
    if (source) source->GetWindow(&body);
    if (!current()) return;
    const HWND search = searchBox.windowHandle(), toolbar = getCommandToolbar();
    if (blocked) {
        // EnableWindow(FALSE) sets a contained keyboard focus to NULL (Window
        // Features, Learn): remember which control held it to give it back.
        const HWND focus = GetFocus();
        disabledProvisionalFocus = nullptr;
        if (body && IsWindowEnabled(body)) {
            if (focus && (focus == body || IsChild(body, focus))) disabledProvisionalFocus = body;
            disabledProvisionalView = body;
            disabledProvisionalGeneration = generation;
            EnableWindow(body, FALSE);
            if (!current()) return;
        }
        if (search && IsWindowEnabled(search)) {
            if (focus && (focus == search || IsChild(search, focus))) disabledProvisionalFocus = search;
            disabledProvisionalSearch = search;
            EnableWindow(search, FALSE);
            if (!current()) return;
        }
        if (toolbar && SendMessage(toolbar, TB_ISBUTTONENABLED, IDM_VIEW_MENU, 0)) {
            disabledProvisionalToolbar = toolbar;
            SendMessage(toolbar, TB_ENABLEBUTTON, IDM_VIEW_MENU, MAKELONG(FALSE, 0));
        }
    } else {
        const HWND oldBody = disabledProvisionalView, oldSearch = disabledProvisionalSearch;
        const HWND oldToolbar = disabledProvisionalToolbar;
        const ULONG_PTR oldGeneration = disabledProvisionalGeneration;
        const HWND oldFocus = disabledProvisionalFocus;
        disabledProvisionalFocus = nullptr;
        // Give the focus back only where disabling took it: the window is still
        // active and nothing else received it meanwhile.
        auto focusLost = [&]() { return GetActiveWindow() == owner && GetFocus() == nullptr; };
        if (body == oldBody && generation == oldGeneration && IsWindow(body)) {
            EnableWindow(body, TRUE);
            if (!provisionalInputBlocked && disabledProvisionalView == oldBody
                    && disabledProvisionalGeneration == oldGeneration) disabledProvisionalView = nullptr;
            if (!current()) return;
            if (oldFocus == body && focusLost()) {
                checkHR(source->UIActivate(SVUIA_ACTIVATE_FOCUS));
                if (!current()) return;
            }
        }
        if (search && search == oldSearch && IsWindow(search)) {
            EnableWindow(search, TRUE);
            if (!provisionalInputBlocked && disabledProvisionalSearch == oldSearch) disabledProvisionalSearch = nullptr;
            if (!current()) return;
            if (oldFocus == search && focusLost()) {
                SetFocus(search);
                if (!current()) return;
            }
        }
        if (toolbar && toolbar == oldToolbar && IsWindow(toolbar)) {
            SendMessage(toolbar, TB_ENABLEBUTTON, IDM_VIEW_MENU, MAKELONG(TRUE, 0));
            if (!provisionalInputBlocked && disabledProvisionalToolbar == oldToolbar) disabledProvisionalToolbar = nullptr;
        }
    }
}

bool FolderWindow::captureProvisionalBaseline() {
    if (!provisionalBaseline || provisionalReady) return true;
    if (provisionalCaptureBusy || !watchProvisionalInput(true)) return false;
    CComPtr<ItemWindow> keepAlive(this);
    provisionalCaptureBusy = true;
    struct CaptureGuard { bool &busy; ~CaptureGuard() { busy = false; } } guard{provisionalCaptureBusy};
    const HWND owner = hwnd;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration, enumeration = viewHost.enumerationSerial();
    auto current = [&]() {
        return isWindowOperational() && hwnd == owner && shellView == source
            && viewGeneration == generation && viewHost.enumerationSerial() == enumeration
            && enumerationComplete && viewReady && !getFolderStateKey().empty()
            && provisionalBaseline && !provisionalReady;
    };
    CComQIPtr<IFolderView2> view(source);
    if (!view || !current()) return false;
    ShellViewState observed;
    observed.view = captureOptions(view, {});
    constexpr uint32_t required = FolderViewSettings::Mode | FolderViewSettings::Flags;
    if (!current() || (observed.view.present & required) != required) return false;
    // Hidden columns can be shown and resized in one native dialog. Record
    // their widths before that dialog, without treating them as initially visible.
    std::vector<FolderViewSettings::Column> widths;
    for (const auto &column : observed.view.columns)
        if (column.width <= INT32_MAX) widths.push_back(column);
    CComQIPtr<IColumnManager> manager(view);
    if (!current()) return false;
    UINT count = 0;
    if (manager && SUCCEEDED(manager->GetColumnCount(CM_ENUM_ALL, &count)) && current() && count) {
        std::vector<PROPERTYKEY> keys(count);
        if (SUCCEEDED(manager->GetColumns(CM_ENUM_ALL, keys.data(), count)) && current()) {
            for (const auto &key : keys) {
                CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
                const HRESULT result = manager->GetColumnInfo(key, &info);
                if (!current()) return false;
                // A default/automatic sentinel is not a measured pixel width.
                if (SUCCEEDED(result) && info.uWidth && info.uWidth <= INT32_MAX) {
                    FolderViewSettings::Column column;
                    column.key = viewSettingsKey(key);
                    column.width = static_cast<uint32_t>(invScaleDPI(static_cast<int>(info.uWidth)));
                    if (std::none_of(widths.begin(), widths.end(), [&](const FolderViewSettings::Column &value) {
                            return value.key == column.key; })) widths.push_back(column);
                }
            }
        }
    }
    if (!current()) return false;
    const bool spatial = spatialView(view);
    if (!current()) return false;
    if (spatial) {
        const HRESULT result = captureIconPositions(view, current, true, {}, observed.icons);
        if (result != S_OK || !current()) return false;
    }
    if (!current()) return false;
    provisionalCurrent.reset(new ShellViewState(observed));
    provisionalColumnWidths.swap(widths);
    provisionalIconsComplete = spatial;
    *provisionalBaseline = std::move(observed);
    provisionalReady = true;
    blockProvisionalInput(false);
    return isWindowOperational() && viewGeneration == generation;
}

// Modes and placement flags are prepared in OnViewCreated. Navigation and
// enumeration callbacks can arrive in either order; restore the remaining
// presentation after initial enumeration. A refill still requires a later
// EnumDone before selection, icon positions or persistence can use the view.
void FolderWindow::finishViewInitialization() {
    if (!viewReady || !initialEnumerationReady || !enumerationComplete
            || restoringState || !isWindowOperational()) return;
    debugPrintf(L"NATIVE HWND=%p finishViewInitialization: options=%d initial=%d\n",
        hwnd, viewOptionsApplied, initialStateApplied);
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == view && viewGeneration == generation;
    };
    if (storedViewState) restoreScrollPending = false; // Its transient viewport takes precedence.
    if (!initialStateApplied && searchText.empty()
            && (storedViewState || ((getSavedFolderState().present & FolderState::Icons)
                && !getSavedFolderState().icons.empty()))) {
        // A newly created view can publish an empty completion before its real
        // contents. Do not consume saved positions or a rename snapshot if the native folder
        // proves that this empty view is still transitional. A truly empty
        // folder must remain a valid completed enumeration.
        const ULONG_PTR enumeration = viewHost.enumerationSerial();
        bool pendingContents = false;
        {
            restoringState = true;
            struct ProbeGuard {
                bool &restoring;
                decltype(current) &valid;
                ~ProbeGuard() { if (valid()) restoring = false; }
            } probeGuard{restoringState, current};
            CComQIPtr<IFolderView2> folderView(view);
            CComPtr<IShellItem> target = item;
            int count = -1;
            SFGAOF attributes = SFGAO_FILESYSTEM;
            const HRESULT counted = folderView ? folderView->ItemCount(SVGIO_ALLVIEW, &count) : E_NOINTERFACE;
            if (!current()) return;
            if (SUCCEEDED(counted) && count == 0 && target
                    && SUCCEEDED(target->GetAttributes(SFGAO_FILESYSTEM, &attributes))
                    && (attributes & SFGAO_FILESYSTEM)) {
                if (!current()) return;
                CComPtr<IShellFolder> folder;
                CComHeapPtr<ITEMIDLIST_ABSOLUTE> targetID;
                HRESULT result = SHGetIDListFromObject(target, &targetID);
                if (!current()) return;
                // The merged Desktop has additional view exclusions; its
                // native enumeration is not evidence of visible contents.
                if (SUCCEEDED(result) && !ILIsEmpty(targetID))
                    result = folderView->GetFolder(IID_PPV_ARGS(&folder));
                else result = E_NOINTERFACE;
                if (!current()) return;
                DWORD flags = 0;
                HWND owner = hwnd;
                if (SUCCEEDED(result)) result = nameFilter
                    ? nameFilter->GetEnumFlags(folder, nullptr, &owner, &flags) : E_NOINTERFACE;
                if (!current()) return;
                CComPtr<IEnumIDList> contents;
                if (SUCCEEDED(result)) result = folder->EnumObjects(nullptr, flags & ~static_cast<DWORD>(SHCONTF_ENABLE_ASYNC), &contents);
                if (!current()) return;
                CComHeapPtr<ITEMID_CHILD> child;
                pendingContents = SUCCEEDED(result) && contents && contents->Next(1, &child, nullptr) == S_OK && child;
            }
        }
        if (!current()) return;
        if (pendingContents) {
            if (viewHost.enumerationSerial() == enumeration) {
                viewOptionsEnumerationBaseline = enumeration;
                enumerationComplete = false;
                beginLoading();
            } else if (enumerationComplete) {
                // A newer public completion arrived reentrantly during the
                // probe. Continue outside that call, without inventing one.
                checkLE(PostMessage(hwnd, MSG_FINISH_VIEW_INITIALIZATION, generation, 0));
            }
            return;
        }
    }
    if (provisionalRestore) {
        if (recoverProvisionalView() && current()) scheduleStateSave();
        return;
    }
    if (provisionalBaseline && !provisionalReady) {
        if (loadFolderState()) {
            provisionalBaseline.reset(); // SQL returned before any provisional interaction.
        } else if (!captureProvisionalBaseline()) {
            return;
        }
        if (!current()) return;
    }
    if (provisionalBaseline && !storedViewState) {
        // The observed fallback is ready for use; it is still not a reference
        // to write or to replay over the settings that have not yet been read.
        viewOptionsApplied = true;
        restoreIconsPending = false;
    }
    if (!viewOptionsApplied) {
        viewOptionsApplied = true; // Consume before calls that can dispatch messages.
        restoringState = true;
        FolderViewSettings options;
        if (storedViewState) options = storedViewState->view;
        else if (loadFolderState() && (getSavedFolderState().present & FolderState::View))
            options = getSavedFolderState().view;
        if (!current()) return;
        CComQIPtr<IFolderView2> folderView(view);
        if (!current()) return;
        if (folderView && options.present) {
            int before = -1, after = -1;
            const HRESULT beforeResult = folderView->ItemCount(SVGIO_ALLVIEW, &before);
            if (!current()) return;
            const HRESULT restored = restoreOptions(folderView, options);
            if (!current()) return;
            if (!checkHR(restored) && provisionalInputBlocked) {
                restoringState = false;
                viewOptionsApplied = false;
                return;
            }
            viewOptionsEnumerationBaseline = viewHost.enumerationSerial();
            const HRESULT afterResult = folderView->ItemCount(SVGIO_ALLVIEW, &after);
            if (!current()) return;
            restoringState = false;
            // Counts invalidate an earlier completion; they never establish
            // initial readiness (an enumerated empty folder is valid). Nor do
            // they prove a refill (a change notification dispatched meanwhile
            // also moves them): the window stays usable, and the next EnumDone
            // completes this initialization.
            if (FAILED(beforeResult) || FAILED(afterResult) || before != after) {
                enumerationComplete = false;
                beginLoading();
                if ((!provisionalBaseline || provisionalReady) && provisionalInputBlocked)
                    blockProvisionalInput(false);
                return;
            }
        }
        restoringState = false;
    }
    const bool initial = !initialStateApplied;
    if (initial) {
        restoringState = true;
        const auto spacing = applyIconSpacing(view, current);
        if (!current()) return;
        restoringState = false;
        // A freshly navigated reference can complete after this callback.
        // Give it one dispatch before replaying positions into a live grid.
        if (spacing == ExplorerIconSpacingResult::Pending && !initialIconSpacingDeferred) {
            initialIconSpacingDeferred = true;
            if (PostMessageW(hwnd, MSG_FINISH_VIEW_INITIALIZATION, generation, 0)) return;
        }
        initialStateApplied = true;
        queueExplorerIconSpacing();
        selectionClear.bind(view, current);
        if (!current()) return;
        CComQIPtr<IFolderView> folderView(view);
        if (!folderView || !current()) return;
        spatialLayout = spatialView(folderView);
        arrangementChanged(folderView);
        if (!current()) return;
    }
    if (restoreIconsPending) {
        restoreIconsPending = false;
        const bool pruneMissing = !restoreIconsUnpruned;
        restoreIconsUnpruned = false;
        if (initial && storedViewState) {
            auto state = std::move(storedViewState);
            CComQIPtr<IFolderView2> folderView(view);
            if (folderView) {
                restoringState = true;
                if (!state->icons.empty() && spatialView(folderView)) {
                    const HRESULT result = restoreIconPositions(folderView, state->icons, current);
                    if (!current()) return;
                    iconRestoreResult(result);
                }
                if (!current()) return;
                checkHR(restoreReopenSelection(view, state->selection, current, &selectionClear));
                if (!current()) return;
                restoringState = false;
            }
        } else if (searchText.empty()) {
            checkHR(loadIconPositions(pruneMissing));
        }
    }
    if (!current()) return;
    if (initial) {
        restoreReopenViewState();
        if (!current()) return;
        if (restoreScrollPending) {
            restoreScrollPending = false; // Consume before reentrant Shell calls.
            if (!provisionalBaseline && searchText.empty() && !getFolderStateKey().empty()) {
                std::vector<uint8_t> bytes;
                auto store = folderStateStore();
                if (!store || !store->readViewScroll(getFolderStateKey(), &bytes)) bytes.clear();
                if (!current()) return;
                if (!bytes.empty()) checkHR(restoreViewScroll(view, bytes, current));
                if (!current()) return;
            }
        }
        onViewReady();
    }
    if (initial && viewProfileChanging && current()) {
        viewProfileChanging = false;
        const HWND focus = GetFocus();
        if (GetActiveWindow() == hwnd && (!focus || focus == hwnd)) {
            if (pendingKeyboardView) checkHR(view->UIActivate(SVUIA_ACTIVATE_FOCUS));
            else if (pendingKeyboardControl && IsWindow(pendingKeyboardControl)) SetFocus(pendingKeyboardControl);
        }
        pendingKeyboardControl = nullptr;
        pendingKeyboardView = false;
    }
    if (!current()) return;
    if ((!provisionalBaseline || provisionalReady) && provisionalInputBlocked) {
        blockProvisionalInput(false); // Reference and its eventual refill are now applied.
        if (!current()) return;
    }
    scheduleStateSave();
    updateSelection();
    if (current()) synchronizeColumns();
    if (current()) replayProviderSync(); // Outside this callback, not from within it.
    if (current()) requestViewProfile();
}

} // namespace filespacer
