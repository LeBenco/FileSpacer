// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#include <common.h>
#include "FolderWindow.h"
#include "FolderWindowInternal.h"
#include "FolderWindowViewMenuPreview.h"
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "ShellViewOperations.h"
#include "ShellViewIcons.h"
#include "ShellViewColumns.h"
#include "Settings.h"
#include "DPI.h"
#include "UIStrings.h"
#include "WinUtils.h"
#include <windowsx.h>
#include <VersionHelpers.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace filespacer {

static bool isViewMenuMode(UINT command) {
    return command >= IDM_VIEW_MODE_FIRST && command <= IDM_VIEW_MODE_LAST;
}

static bool isPlacementCommand(UINT command) {
    return command == IDM_AUTO_ARRANGE_ICONS || command == IDM_KEEP_ICONS_ALIGNED
        || command == IDM_FREE_PLACEMENT_ICONS;
}



static bool appendPlacementMenu(HMENU parent, UINT mode, DWORD flags) {
    auto destroy = [](HMENU handle) { DestroyMenu(handle); };
    std::unique_ptr<std::remove_pointer<HMENU>::type, decltype(destroy)> menu(CreatePopupMenu(), destroy);
    if (!menu) return false;
    const bool enabled = iconPlacementMode(mode);
    const UINT selected = !enabled || (flags & FWF_AUTOARRANGE) ? IDM_AUTO_ARRANGE_ICONS
        : (flags & FWF_SNAPTOGRID) ? IDM_KEEP_ICONS_ALIGNED : IDM_FREE_PLACEMENT_ICONS;
    const UINT commands[] = {IDM_AUTO_ARRANGE_ICONS, IDM_KEEP_ICONS_ALIGNED, IDM_FREE_PLACEMENT_ICONS};
    const UINT labels[] = {IDS_AUTO_ARRANGE_ICONS, IDS_KEEP_ICONS_ALIGNED, IDS_FREE_PLACEMENT_ICONS};
    for (UINT i = 0; i < _countof(commands); ++i) {
        MENUITEMINFOW entry = {sizeof(entry)};
        entry.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE;
        entry.wID = commands[i];
        entry.dwTypeData = const_cast<wchar_t *>(getString(labels[i]));
        entry.fType = MFT_RADIOCHECK;
        entry.fState = commands[i] == selected ? MFS_CHECKED : MFS_UNCHECKED;
        if (!InsertMenuItemW(menu.get(), i, TRUE, &entry)) return false;
    }
    MENUITEMINFOW entry = {sizeof(entry)};
    entry.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_SUBMENU;
    entry.wID = IDM_ICON_PLACEMENT_MENU;
    entry.dwTypeData = const_cast<wchar_t *>(getString(IDS_ICON_PLACEMENT));
    entry.hSubMenu = menu.get();
    entry.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
    if (!InsertMenuItemW(parent, GetMenuItemCount(parent), TRUE, &entry)) return false;
    static_cast<void>(menu.release()); // Owned by the parent menu.
    return true;
}

static HRESULT invokeDisplayCommand(IFolderView2 *view, UINT command) {
    if (!view) return E_NOINTERFACE;
    if (isViewMenuMode(command)) {
        const auto &mode = VIEW_PRESETS[command - IDM_VIEW_MODE_FIRST];
        return view->SetViewModeAndIconSize(mode.mode, mode.size);
    }
    if (!isPlacementCommand(command)) return E_INVALIDARG;
    FOLDERVIEWMODE mode = FVM_AUTO; int size = 0; DWORD flags = 0;
    HRESULT read = view->GetViewModeAndIconSize(&mode, &size);
    if (FAILED(read)) return read;
    if (!iconPlacementMode(mode)) return E_INVALIDARG;
    read = view->GetCurrentFolderFlags(&flags);
    if (FAILED(read)) return read;
    const DWORD placement = command == IDM_AUTO_ARRANGE_ICONS ? FWF_AUTOARRANGE
        : command == IDM_KEEP_ICONS_ALIGNED ? FWF_SNAPTOGRID : 0;
    const DWORD mask = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
    return (flags & mask) == placement ? S_FALSE : view->SetCurrentFolderFlags(mask, placement);
}

static void toggleGlobalThumbnails() {
    SHELLSTATE state = {};
    SHGetSetSettings(&state, SSF_ICONSONLY, FALSE);
    state.fIconsOnly = !state.fIconsOnly;
    // The Shell publishes its normal ShellState setting-change notification.
    SHGetSetSettings(&state, SSF_ICONSONLY, TRUE);
}

HRESULT FolderWindow::ViewMenuPreview::preparePositionChunks() {
        positionChunks.clear();
        nextPositionChunk = 0;
        if (initial.icons.empty()) return S_OK;
        if (initial.icons.size() > INT32_MAX) return E_INVALIDARG;
        CComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(initial.icons.data(), static_cast<UINT>(initial.icons.size())));
        if (!stream) return E_OUTOFMEMORY;
        size_t offset = 0, first = 0, count = 0;
        while (offset < initial.icons.size()) {
            CComHeapPtr<ITEMID_CHILD> id;
            POINT point = {};
            HRESULT result = IStream_ReadPidl(stream, &id);
            if (FAILED(result) || !id) return FAILED(result) ? result : E_INVALIDARG;
            result = IStream_Read(stream, &point, sizeof(point));
            if (FAILED(result)) return result;
            ULARGE_INTEGER at = {};
            LARGE_INTEGER zero = {};
            result = stream->Seek(zero, STREAM_SEEK_CUR, &at);
            if (FAILED(result) || at.QuadPart > initial.icons.size()) return FAILED(result) ? result : E_INVALIDARG;
            offset = static_cast<size_t>(at.QuadPart);
            // Measured compromise: fewer repeated Shell layouts, while still yielding to the menu.
            if (++count == 128 || offset == initial.icons.size()) {
                positionChunks.emplace_back(initial.icons.data() + first, initial.icons.data() + offset);
                first = offset;
                count = 0;
            }
        }
        return S_OK;
    }

void FolderWindow::ViewMenuPreview::advancePositions() {
        if (busy || !current() || currentCommand || nextPositionChunk >= positionChunks.size()) return;
        busy = true;
        auto valid = [&]() { return current(); };
        ++owner.navigationDepth;
        const HRESULT result = restoreIconPositions(view, positionChunks[nextPositionChunk++], valid);
        owner.resumeDeferredClose();
        checkHR(result);
        busy = false;
        if (!current() || nextPositionChunk == positionChunks.size()) {
            positionChunks.clear();
            nextPositionChunk = 0;
        } else if (tracking) PostMessage(owner.hwnd, WM_NULL, 0, 0);
    }

void FolderWindow::ViewMenuPreview::finishPositions() {
        if (!busy && !tracking && current() && !currentCommand && nextPositionChunk < positionChunks.size()) {
            // Closing already drains every remaining chunk synchronously. Combine
            // only those records to avoid repeating the Shell layout at each step.
            std::vector<uint8_t> remaining;
            remaining.reserve(initial.icons.size());
            for (size_t i = nextPositionChunk; i < positionChunks.size(); ++i)
                remaining.insert(remaining.end(), positionChunks[i].begin(), positionChunks[i].end());
            positionChunks.resize(nextPositionChunk + 1);
            positionChunks[nextPositionChunk].swap(remaining);
        }
        while (!busy && current() && !currentCommand && nextPositionChunk < positionChunks.size()) advancePositions();
    }

FolderWindow::ViewMenuPreview::ViewMenuPreview(FolderWindow &window, HMENU popup, IContextMenu *handler,
            IFolderView2 *folderView) : owner(window), menu(popup), context(handler),
        source(window.shellView), view(folderView), generation(window.viewGeneration),
        serial(++window.viewMenuPreviewSerial),
        enumerationBaseline(window.viewHost.enumerationSerial()) {
        GetCursorPos(&lastPoint);
    }

bool FolderWindow::ViewMenuPreview::current() const {
        return owner.isWindowOperational() && owner.shellView == source
            && owner.viewGeneration == generation;
    }

bool FolderWindow::ViewMenuPreview::prepare() {
        if (prepared) return current();
        ++owner.navigationDepth;
        struct Resume {
            FolderWindow &owner;
            ~Resume() { owner.resumeDeferredClose(); }
        } resume{owner};
        // Capture only in memory, on the first actual preview or command.
        // Persistence belongs to changes and closure, not menu preparation.
        if (!current() || owner.deferredCloseMessage) return false;
        initial.view = captureOptions(view, {});
        if (!current() || !(initial.view.present & FolderViewSettings::Mode)) return false;
        if (owner.columnFillSet) {
            initial.view.present |= FolderViewSettings::ColumnFill;
            initial.view.fillColumns = owner.fillColumns;
        }
        owner.captureColumnWeights(initial.view);
        if (!current()) return false;
        auto &saved = owner.getSavedFolderState();
        auto options = saved.view.mergeChanges(saved.view, initial.view);
        owner.captureColumnWeights(options);
        if (!(saved.present & FolderState::View) || !sameView(options, saved.view)) {
            saved.view = std::move(options);
            saved.present |= FolderState::View;
            owner.viewStateDirty(FolderState::View);
        }
        auto valid = [&]() { return current(); };
        HRESULT result = captureReopenSelection(source, valid, initial.selection);
        if (FAILED(result) || !current()) return false;
        captureViewScroll(source, valid, initial.scroll);
        if (!current()) return false;
        if (gridPlacement(view) && current()) {
            if (FAILED(view->GetSpacing(&initialSpacing)) || !current()) return false;
        }
        if (owner.spatialView(view) && current()) {
            result = captureIconPositions(view, valid, owner.enumerationComplete,
                owner.getSavedFolderState().icons, initial.icons, !owner.searchText.empty());
            if (FAILED(result) || !current()) return false;
            capturedIcons = result == S_OK;
            if (capturedIcons) {
                if (!(saved.present & FolderState::Icons) || saved.icons != initial.icons) {
                    saved.icons = initial.icons;
                    saved.present |= FolderState::Icons;
                    owner.viewStateDirty(FolderState::Icons);
                }
            }
        }
        if (!current() || owner.deferredCloseMessage) return false;
        prepared = true;
        KillTimer(owner.hwnd, TIMER_SAVE_STATE);
        return true;
    }

bool FolderWindow::ViewMenuPreview::capture() {
        if (context && !prepare()) return false;
        for (int i = 0, count = GetMenuItemCount(menu); i < count; ++i) {
            MENUITEMINFO info = {sizeof(info)};
            info.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
            if (!GetMenuItemInfo(menu, i, TRUE, &info) || info.hSubMenu
                    || (info.fType & MFT_SEPARATOR) || (info.fState & (MFS_DISABLED | MFS_GRAYED))) continue;
            if (!context) {
                if (!isViewMenuMode(info.wID)) continue;
                if (info.fState & MFS_CHECKED) initialCommand = info.wID;
                // Content needs its full-width renderer. Commit it after the
                // menu closes instead of previewing the classic Tiles alias.
                if (info.wID == IDM_VIEW_MODE_LAST) continue;
                entries.push_back({info.wID});
                continue;
            }
            if (info.wID < IDM_SHELL_FIRST || info.wID > IDM_SHELL_LAST) continue;
            wchar_t verb[64] = {};
            if (FAILED(context->GetCommandString(info.wID - IDM_SHELL_FIRST, GCS_VERBW,
                    nullptr, reinterpret_cast<char *>(verb), _countof(verb))) || !current()) continue;
            // Canonical verbs observed on the actual Shell; never match translated labels.
            if (_wcsnicmp(verb, L"viewlogical", 11) != 0) continue;
            entries.push_back({info.wID});
            if (info.fState & MFS_CHECKED) initialCommand = info.wID;
        }
        return current() && !entries.empty();
    }

LRESULT CALLBACK FolderWindow::ViewMenuPreview::filter(int code, WPARAM wParam, LPARAM lParam) {
        if (code == MSGF_MENU && active) {
            const MSG &message = *reinterpret_cast<const MSG *>(lParam);
            if (message.message == WM_MOUSEMOVE || message.message == WM_NCMOUSEMOVE) {
                POINT point = {};
                if (GetCursorPos(&point)) active->pointAt(point);
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

void FolderWindow::ViewMenuPreview::pointAt(POINT point) {
        if (!hoverEnabled || !tracking || !current() || !displayedMenu) return;
        // Mouse messages also arrive while the cursor stands still (the view
        // changes under the menu, the menu loop idles). Only a displacement is
        // a hover; otherwise a keyboard highlight would be cancelled at once.
        if (point.x == lastPoint.x && point.y == lastPoint.y) return;
        lastPoint = point;
        const int position = MenuItemFromPoint(nullptr, displayedMenu, point);
        const UINT pointed = position >= 0 ? GetMenuItemID(displayedMenu, position) : 0;
        UINT command = 0;
        for (const auto &entry : entries) {
            if (entry.command == pointed) {
                command = entry.command == initialCommand ? 0 : entry.command;
                break;
            }
        }
        wantedCommand = command;
    }

void FolderWindow::ViewMenuPreview::highlight(UINT command, HMENU popup) {
        if (!hoverEnabled || !tracking || !current()) return;
        displayedMenu = popup;
        // Like pointAt: an entry outside the modes returns to the initial mode.
        UINT wanted = 0;
        for (const auto &entry : entries) {
            if (entry.command == command) {
                wanted = command == initialCommand ? 0 : command;
                break;
            }
        }
        wantedCommand = wanted;
    }

void FolderWindow::ViewMenuPreview::restoreDetails() {
        if (!prepared || !current() || (tracking && currentCommand && initialSpacing.x <= 0)) return;
        auto valid = [&]() { return current(); };
        ++owner.navigationDepth;
        if (currentCommand && !initial.icons.empty() && initialSpacing.x > 0 && initialSpacing.y > 0
                && gridPlacement(view) && current()) {
            checkHR(restoreIconGridPositions(view, initial.icons, initialSpacing, valid));
        } else if (!currentCommand && !initial.icons.empty() && owner.spatialView(view) && current()) {
            if (tracking) {
                checkHR(preparePositionChunks());
                if (!positionChunks.empty()) PostMessage(owner.hwnd, WM_NULL, 0, 0);
            } else checkHR(restoreIconPositions(view, initial.icons, valid));
        }
        if (current()) {
            auto selection = initial.selection;
            checkHR(restoreReopenSelection(source, selection, valid, &owner.selectionClear));
        }
        owner.resumeDeferredClose();
    }

void FolderWindow::ViewMenuPreview::apply(UINT command) {
        if (busy || !current() || command == currentCommand) return;
        busy = true;
        const bool ready = prepare();
        busy = false;
        if (!ready || !current()) return;
        positionChunks.clear();
        nextPositionChunk = 0;
        busy = true;
        int before = -1, after = -1;
        const HRESULT counted = view->ItemCount(SVGIO_ALLVIEW, &before);
        if (!current()) { busy = false; return; }
        const ULONG_PTR baseline = owner.viewHost.enumerationSerial();
        HRESULT result;
        if (command) {
            if (context) {
                auto info = owner.makeInvokeInfo(static_cast<int>(command - IDM_SHELL_FIRST), POINT{});
                info.hwnd = owner.hwnd;
                result = owner.invokeViewCommand(context, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info));
            } else {
                ++owner.navigationDepth;
                result = invokeDisplayCommand(view, command);
                owner.resumeDeferredClose();
            }
        } else {
            FolderViewSettings presentation = initial.view;
            presentation.present &= FolderViewSettings::Mode | FolderViewSettings::Flags;
            ++owner.navigationDepth; // Restoring options can refill and dispatch like a verb.
            result = restoreOptions(view, presentation);
            owner.resumeDeferredClose();
        }
        if (SUCCEEDED(result)) currentCommand = command;
        if (current()) {
            const HRESULT recounted = view->ItemCount(SVGIO_ALLVIEW, &after);
            if (current()) {
                // As in finishViewInitialization, a changed count only invalidates
                // the completed enumeration; nothing waits on the count itself.
                // The refill's EnumDone completes the view again and replays the
                // restoration made here, so a transient view is never persisted.
                if (FAILED(counted) || FAILED(recounted) || before != after) {
                    owner.enumerationComplete = false;
                    owner.beginLoading();
                    owner.viewOptionsEnumerationBaseline = baseline;
                }
                restoreDetails();
            }
        }
        checkHR(result);
        busy = false;
        if (SUCCEEDED(result) && tracking && wantedCommand != currentCommand && !posted && current())
            posted = PostMessage(owner.hwnd, MSG_VIEW_MENU_PREVIEW, serial, 0) != FALSE;
    }

void FolderWindow::ViewMenuPreview::update() {
        posted = false;
        if (tracking) apply(wantedCommand);
    }

bool FolderWindow::ViewMenuPreview::isEntry(UINT command) const {
        for (const auto &entry : entries)
            if (entry.command == command) return true;
        return false;
    }

bool FolderWindow::ViewMenuPreview::invokePending() {
        const UINT command = pendingCommand;
        pendingCommand = 0;
        if (!command || !current()) return false;
        if (command == IDM_TOGGLE_THUMBNAILS) {
            toggleGlobalThumbnails();
            return true;
        }
        if (command == IDM_FILL_COLUMNS) {
            owner.toggleFillColumns();
            return true;
        }
        if (command == IDM_REALIGN_ICONS) {
            owner.realignIcons();
            return true;
        }
        if (!context || isPlacementCommand(command)) {
            ++owner.navigationDepth;
            const HRESULT result = invokeDisplayCommand(view, command);
            checkHR(result);
            owner.resumeDeferredClose();
            return SUCCEEDED(result) && result != S_FALSE;
        }
        auto info = owner.makeInvokeInfo(static_cast<int>(command - IDM_SHELL_FIRST), POINT{});
        info.hwnd = owner.hwnd;
        const HRESULT result = owner.invokeViewCommand(context, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info));
        checkHR(result);
        return SUCCEEDED(result);
    }

void FolderWindow::ViewMenuPreview::enumerated(ULONG_PTR enumeration) {
        if (!prepared || busy || enumeration <= enumerationBaseline || !current()) return;
        restoreDetails();
    }

void FolderWindow::ViewMenuPreview::replay(ULONG_PTR enumeration) {
        FolderWindow &window = owner;
        if (busy || enumeration <= enumerationBaseline || !window.enumerationComplete || !current()) return;
        if (chosenCommand != currentCommand) apply(chosenCommand);
        else restoreDetails();
        if (!current()) return;
        // Releasing the record may destroy this session: only the window is used after it.
        if (chosenCommand != currentCommand) {
            window.viewMenuRestore.reset();
            window.replayProviderSync();
            window.resumeColumnSync();
            return;
        }
        if (window.enumerationComplete) window.viewMenuRestore.reset(); // Else another refill was observed.
        window.scheduleStateSave();
        if (!window.viewMenuRestore) {
            window.replayProviderSync();
            window.resumeColumnSync();
        }
    }

void FolderWindow::ViewMenuPreview::finish(UINT command) {
        if (!prepared) {
            if (command && command != initialCommand) {
                busy = true;
                const bool ready = prepare();
                busy = false;
                if (!ready || !current()) {
                    if (owner.viewMenuPreview.get() == this) owner.viewMenuPreview.reset();
                    return;
                }
            } else {
                // Nothing was previewed. Escape/outside click has no snapshot
                // to restore and must not perform a synchronous database write.
                tracking = false;
                if (owner.viewMenuPreview.get() == this) owner.viewMenuPreview.reset();
                if (current()) {
                    owner.replayProviderSync();
                    owner.resumeColumnSync();
                }
                return;
            }
        }
        tracking = false;
        // An outside click / Escape retains the original mode. A chosen entry
        // already previewed is committed without another round trip through it.
        // An entry outside the modes (sorting, auto arrange...) is not a mode:
        // the preview is cancelled first, then the entry runs the ordinary way.
        const bool mode = !command || isEntry(command);
        chosenCommand = mode && command != initialCommand ? command : 0;
        if (!chosenCommand) finishPositions();
        const bool applying = chosenCommand != currentCommand;
        const ULONG_PTR enumerationBefore = owner.viewHost.enumerationSerial();
        apply(chosenCommand);
        if (!current()) return;
        if (!mode) pendingCommand = command;
        // Returning to the initial mode already restored its details in apply().
        // Only a committed preview still needs its selection/focus restored here.
        if (!applying && chosenCommand) restoreDetails();
        if (!current()) return;
        const bool pendingChanged = invokePending();
        if (!current()) return;
        // The session stops holding back saves now. The restoration is final once
        // the view is complete; a refill observed by apply() is still running and
        // its EnumDone replays it. A failed cancellation is retried there as well,
        // and the preview still showing is not persisted meanwhile.
        FolderWindow &window = owner;
        const bool settled = chosenCommand == currentCommand;
        if (window.viewMenuPreview.get() == this && (!settled || !window.enumerationComplete)) {
            window.viewMenuRestore = window.viewMenuPreview;
            if (!settled && window.enumerationComplete
                    && window.viewHost.enumerationSerial() == enumerationBefore)
                restoreFailed = true;
        }
        // The trusted in-memory layout may be newer than SQLite. Once the
        // chosen mode is nonspatial, ordinary capture cannot write those icons.
        // Flush this changed reference while the preview still owns it, after
        // the menu has closed; opening an unchanged menu remains read-only.
        if (capturedIcons && window.viewStateIsDirty(FolderState::Icons)) {
            window.persistViewState();
            if (!current()) return;
        }
        if (settled && chosenCommand && initialSpacing.x > 0 && gridPlacement(view) && current()) {
            window.arrangementChanged(view); // The committed grid was already mapped above.
            if (!current()) return;
            window.viewStateDirty(FolderState::Icons);
        }
        window.viewMenuPreview.reset();
        if (settled && (chosenCommand || pendingChanged)) window.scheduleStateSave();
        // A preference changed while the menu was open: synchronized now that
        // it is closed, unless a refill still running replays it on completion.
        if (!window.viewMenuRestore) {
            window.replayProviderSync();
            window.resumeColumnSync();
        }
    }

thread_local FolderWindow::ViewMenuPreview *FolderWindow::ViewMenuPreview::active = nullptr;

HMENU findViewMenu(IContextMenu *const contextMenu, HMENU popupMenu) {
    if (!IsWindows8OrGreater())
        return GetSubMenu(popupMenu, 0);
    for (int i = 0, count = GetMenuItemCount(popupMenu); i < count; i++) {
        MENUITEMINFO itemInfo = {sizeof(itemInfo)};
        itemInfo.fMask = MIIM_ID | MIIM_SUBMENU;
        if (!checkLE(GetMenuItemInfo(popupMenu, i, TRUE, &itemInfo)))
            continue;
        if (!itemInfo.hSubMenu || itemInfo.wID <= 0)
            continue;
        wchar_t verb[64];
        verb[0] = 0;
        if (SUCCEEDED(contextMenu->GetCommandString(itemInfo.wID - IDM_SHELL_FIRST, GCS_VERBW,
                nullptr, (char*)verb, _countof(verb))) && lstrcmpi(verb, L"view") == 0) {
            return itemInfo.hSubMenu;
        }
    }
    return nullptr;
}

void FolderWindow::openViewMenu(POINT point) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational() || provisionalInputBlocked || provisionalCaptureBusy)
        return;
    ++viewMenuDepth;
    struct MenuScope {
        FolderWindow &owner;
        ~MenuScope() { --owner.viewMenuDepth; owner.requestViewProfile(); }
    } menuScope{*this};
    if (provisionalBaseline && !captureProvisionalCurrent(true, true)) return;
    if (!isWindowOperational()) return;
    if (hybridViewEligible) {
        // This popup contains application commands. Opening it never queries
        // the folder's context-menu extensions or recreates its Shell view.
        CComPtr<IShellView> source = shellView;
        const ULONG_PTR generation = viewGeneration;
        CComQIPtr<IFolderView2> view(source);
        FOLDERVIEWMODE mode = FVM_AUTO; int size = 0; DWORD flags = 0;
        if (!view || FAILED(view->GetViewModeAndIconSize(&mode, &size))
                || FAILED(view->GetCurrentFolderFlags(&flags)) || !isWindowOperational()
                || shellView != source || viewGeneration != generation) return;
        auto destroy = [](HMENU handle) { DestroyMenu(handle); };
        std::unique_ptr<std::remove_pointer<HMENU>::type, decltype(destroy)> menu(CreatePopupMenu(), destroy);
        if (!menu) return;
        int checked = -1, distance = INT_MAX;
        for (int i = 0; i < static_cast<int>(_countof(VIEW_PRESETS)); ++i) {
            const auto &choice = VIEW_PRESETS[i];
            if (choice.mode != mode) continue;
            const int difference = choice.size < 0 ? 0 : std::abs(size - choice.size);
            if (difference < distance) { checked = i; distance = difference; }
        }
        for (UINT i = 0; i < _countof(VIEW_PRESETS); ++i) {
            MENUITEMINFOW entry = {sizeof(entry)};
            entry.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE;
            entry.wID = IDM_VIEW_MODE_FIRST + i;
            entry.dwTypeData = const_cast<wchar_t *>(getString(IDS_VIEW_MODE_FIRST + i));
            entry.fType = MFT_RADIOCHECK;
            entry.fState = static_cast<int>(i) == checked ? MFS_CHECKED : MFS_UNCHECKED;
            if (!InsertMenuItemW(menu.get(), i, TRUE, &entry)) return;
        }
        if (!AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr)) return;
        if (!appendPlacementMenu(menu.get(), mode, flags)) return;
        // The registered extension already styles and modifies local title and
        // toolbar menus. Keep the same Win32 tracking and COM activation path.
        if (!viewMenuExtension) activateTitleMenuExtension(&viewMenuExtension);
        if (isWindowOperational() && shellView == source && viewGeneration == generation)
            openBackgroundSubMenu(nullptr, menu.get(), point);
        return;
    }
    HMENU popupMenu = nullptr;
    CComPtr<IContextMenu> contextMenu = queryBackgroundMenu(&popupMenu);
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        menu(popupMenu, destroyMenu);
    if (!contextMenu || !isWindowOperational())
        return;
    HMENU viewMenu = findViewMenu(contextMenu, popupMenu);
    if (isWindowOperational() && viewMenu)
        openBackgroundSubMenu(contextMenu, viewMenu, point);
}

void FolderWindow::openBackgroundSubMenu(IContextMenu *const contextMenu, HMENU subMenu,
        POINT point) {
    // The parent menu and this object are owned by openViewMenu.
    if (!isWindowOperational())
        return;
    if (columnLayout) columnLayout->suspend(ShellViewColumns::Hold::Menu, true);
    struct ColumnsResume {
        std::shared_ptr<ShellViewColumns> layout;
        ~ColumnsResume() { if (layout) layout->suspend(ShellViewColumns::Hold::Menu, false); }
    } columnsResume{columnLayout};
    viewMenuRestore.reset(); // A new menu supersedes a previous preview's pending replay.
    const bool previewEnabled = settings::getViewMenuPreviewEnabled();
    // Keep any pending layout in memory. Opening the popup never requires
    // a database flush, including on the native-provider path.
    if (!isWindowOperational()) return;
    CComPtr<IFolderView2> folderView;
    auto detachSite = [](IContextMenu *handler) { checkHR(IUnknown_SetSite(handler, nullptr)); };
    std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
    if (checkHR(viewHost.currentView(IID_PPV_ARGS(&folderView))) && isWindowOperational()
            && contextMenu && checkHR(IUnknown_SetSite(contextMenu, folderView)))
        site.reset(contextMenu);
    if (!isWindowOperational()) return;
    if (contextMenu) {
        for (int i = GetMenuItemCount(subMenu) - 1; i >= 0; --i) {
            MENUITEMINFOW entry = {sizeof(entry)};
            entry.fMask = MIIM_ID;
            if (!GetMenuItemInfoW(subMenu, i, TRUE, &entry)
                    || entry.wID < IDM_SHELL_FIRST || entry.wID > IDM_SHELL_LAST) continue;
            wchar_t verb[64] = {};
            if (SUCCEEDED(contextMenu->GetCommandString(entry.wID - IDM_SHELL_FIRST,
                    GCS_VERBW, nullptr, reinterpret_cast<char *>(verb), _countof(verb)))
                    && (lstrcmpiW(verb, L"arrangeauto") == 0 || lstrcmpiW(verb, L"arrangeautogrid") == 0))
                checkLE(DeleteMenu(subMenu, i, MF_BYPOSITION));
        }
        FOLDERVIEWMODE mode = FVM_AUTO; int size = 0; DWORD flags = 0;
        if (folderView && SUCCEEDED(folderView->GetViewModeAndIconSize(&mode, &size))
                && SUCCEEDED(folderView->GetCurrentFolderFlags(&flags)) && isWindowOperational())
            checkLE(appendPlacementMenu(subMenu, mode, flags));
    }
    const int menuCount = GetMenuItemCount(subMenu);
    if (menuCount < 0 || !isWindowOperational()) return;
    int thumbnailPosition = menuCount;
    for (int i = 0; i < menuCount; ++i) {
        MENUITEMINFOW menuItem = {sizeof(menuItem)};
        menuItem.fMask = MIIM_FTYPE;
        if (GetMenuItemInfoW(subMenu, i, TRUE, &menuItem) && (menuItem.fType & MFT_SEPARATOR)) {
            thumbnailPosition = i;
            break;
        }
    }
    SHELLSTATE thumbnailState = {};
    SHGetSetSettings(&thumbnailState, SSF_ICONSONLY, FALSE);
    MENUITEMINFO thumbnailItem = {sizeof(thumbnailItem)};
    thumbnailItem.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
    thumbnailItem.wID = IDM_TOGGLE_THUMBNAILS;
    thumbnailItem.dwTypeData = const_cast<wchar_t *>(getString(IDS_GLOBAL_THUMBNAILS));
    thumbnailItem.fState = thumbnailState.fIconsOnly ? MFS_UNCHECKED : MFS_CHECKED;
    checkLE(InsertMenuItemW(subMenu, thumbnailPosition, TRUE, &thumbnailItem));
    MENUITEMINFOW realignItem = {sizeof(realignItem)};
    realignItem.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
    realignItem.wID = IDM_REALIGN_ICONS;
    realignItem.dwTypeData = const_cast<wchar_t *>(getString(IDS_REALIGN_ICONS));
    realignItem.fState = folderView && spatialView(folderView) && searchText.empty()
        ? MFS_ENABLED : MFS_DISABLED;
    if (!isWindowOperational()) return;
    checkLE(InsertMenuItemW(subMenu, GetMenuItemCount(subMenu), TRUE, &realignItem));
    if (settings::getDetailsColumnsEnabled()) {
        MENUITEMINFOW columnsItem = {sizeof(columnsItem)};
        columnsItem.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
        columnsItem.wID = IDM_FILL_COLUMNS;
        columnsItem.dwTypeData = const_cast<wchar_t *>(getString(IDS_FILL_COLUMNS));
        columnsItem.fState = fillColumns ? MFS_CHECKED : MFS_UNCHECKED;
        checkLE(InsertMenuItemW(subMenu, GetMenuItemCount(subMenu), TRUE, &columnsItem));
    }
    std::shared_ptr<ViewMenuPreview> preview;
    HHOOK hook = nullptr;
    ViewMenuPreview *previous = ViewMenuPreview::active;
    if (folderView && viewReady && initialStateApplied && enumerationComplete && !restoringState) {
        preview = std::make_shared<ViewMenuPreview>(*this, subMenu, contextMenu, folderView);
        if (preview->capture()) {
            if (previewEnabled) {
                hook = SetWindowsHookEx(WH_MSGFILTER, ViewMenuPreview::filter, nullptr, GetCurrentThreadId());
                checkLE(hook);
            }
            preview->hoverEnabled = hook != nullptr;
            if (hook) ViewMenuPreview::active = preview.get();
            viewMenuPreview = preview;
            if (contextMenu) KillTimer(hwnd, TIMER_SAVE_STATE);
        }
    }
    const UINT trackingFlags = TPM_RETURNCMD | TPM_RIGHTBUTTON | (contextMenu ? 0u : TPM_NOANIMATION);
    const int cmd = isWindowOperational() ? TrackPopupMenuEx(subMenu, trackingFlags,
        point.x, point.y, hwnd, nullptr) : 0;
    if (hook) {
        UnhookWindowsHookEx(hook);
        ViewMenuPreview::active = previous;
    }
    if (preview && viewMenuPreview == preview) {
        preview->finish((cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) || isViewMenuMode(cmd)
            || isPlacementCommand(static_cast<UINT>(cmd))
            || cmd == IDM_TOGGLE_THUMBNAILS || cmd == IDM_FILL_COLUMNS || cmd == IDM_REALIGN_ICONS
            ? static_cast<UINT>(cmd) : 0);
    } else if (isWindowOperational() && cmd == IDM_REALIGN_ICONS) {
        realignIcons();
    } else if (isWindowOperational() && cmd == IDM_FILL_COLUMNS) {
        toggleFillColumns();
    } else if (isWindowOperational() && cmd == IDM_TOGGLE_THUMBNAILS) {
        toggleGlobalThumbnails();
    } else if (isWindowOperational() && (!contextMenu || isPlacementCommand(static_cast<UINT>(cmd))) && cmd) {
        // Preserve free coordinates in memory before a native mode change.
        // Save the resulting state through the ordinary change notification.
        std::vector<uint8_t> icons;
        CComPtr<IShellView> source = shellView;
        const ULONG_PTR generation = viewGeneration;
        const auto current = [&]() { return isWindowOperational() && shellView == source && viewGeneration == generation; };
        if (isViewMenuMode(static_cast<UINT>(cmd)) && folderView && spatialView(folderView)) {
            const HRESULT result = captureIconPositions(folderView, current, enumerationComplete,
                getSavedFolderState().icons, icons, !searchText.empty());
            if (!current() || FAILED(result)) return;
            if (result == S_OK && icons != getSavedFolderState().icons) {
                getSavedFolderState().icons = std::move(icons);
                getSavedFolderState().present |= FolderState::Icons;
                viewStateDirty(FolderState::Icons);
            }
        }
        if (!current()) return;
        ++navigationDepth;
        checkHR(invokeDisplayCommand(folderView, static_cast<UINT>(cmd)));
        resumeDeferredClose();
        if (isWindowOperational()) scheduleStateSave();
    } else if (isWindowOperational() && viewHost && cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) {
        auto info = makeInvokeInfo(cmd - IDM_SHELL_FIRST, point);
        info.hwnd = hwnd;
        checkHR(invokeViewCommand(contextMenu, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info)));
    }
}
} // namespace filespacer
