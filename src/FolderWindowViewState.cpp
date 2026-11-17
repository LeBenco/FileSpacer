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
#include "FolderWindowViewMenuPreview.h"
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

bool gridPlacement(IFolderView *folderView) {
    CComQIPtr<IFolderView2> view(folderView);
    DWORD flags = 0; UINT mode = static_cast<UINT>(FVM_AUTO);
    return view && SUCCEEDED(view->GetCurrentViewMode(&mode)) && iconPlacementMode(mode)
        && SUCCEEDED(view->GetCurrentFolderFlags(&flags))
        && (flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID)) == FWF_SNAPTOGRID;
}

bool FolderWindow::spatialView(IFolderView *const folderView) const {
    if (hybridViewEligible && viewProfile == FVO_DEFAULT) return false;
    UINT viewMode;
    return folderView->GetAutoArrange() == S_FALSE
        && checkHR(folderView->GetCurrentViewMode(&viewMode))
        && viewMode != FVM_DETAILS && viewMode != FVM_LIST && viewMode != FVM_CONTENT;
}

void FolderWindow::replayProviderSync() {
    if (!providerSyncPending || !isWindowOperational()) return;
    providerSyncPending = false;
    checkLE(PostMessage(hwnd, MSG_SYNCHRONIZE_VIEW_PROVIDER, 0, 0));
}

// A column pass refused while a preview, its pending replay or the provisional
// state was in progress is asked again once that blocker is lifted.
void FolderWindow::resumeColumnSync() {
    if (isWindowOperational() && (columnLayout || columnFillActive()))
        checkLE(PostMessageW(hwnd, MSG_COLUMNS_CHANGED, viewGeneration, 0));
}

FOLDERSETTINGS FolderWindow::folderSettings() const {
    FOLDERSETTINGS settings = {};
    SFGAOF attributes = 0;
    const HRESULT result = item->GetAttributes(SFGAO_FILESYSTEM, &attributes);
    if (SUCCEEDED(result) && (attributes & SFGAO_FILESYSTEM)) {
        settings.ViewMode = FVM_DETAILS;
        settings.fFlags = FWF_AUTOARRANGE | FWF_NOWEBVIEW | FWF_NOHEADERINALLVIEWS;
    } else {
        // Virtual folders can provide their own presentation instead of Details.
        settings.ViewMode = static_cast<UINT>(FVM_AUTO);
    }
    debugPrintf(L"NATIVE initial settings: attributes=0x%08lX HRESULT=0x%08lX mode=%u flags=0x%08X\n",
        attributes, static_cast<unsigned long>(result), settings.ViewMode, settings.fFlags);
    return settings;
}

// Records the mode and icon size of the view. True when they differ from the
// ones recorded before: the Shell has then laid the icons out again.
bool FolderWindow::arrangementChanged(IFolderView *const folderView) {
    CComQIPtr<IFolderView2> sized(folderView);
    FOLDERVIEWMODE mode = FVM_AUTO;
    int size = -1;
    if (!sized || FAILED(sized->GetViewModeAndIconSize(&mode, &size))) return false;
    const bool changed = arrangedKnown
        && (arrangedMode != static_cast<UINT>(mode) || arrangedSize != size);
    arrangedMode = static_cast<UINT>(mode);
    arrangedSize = size;
    arrangedKnown = true;
    return changed;
}

void FolderWindow::scheduleStateSave(bool icons) {
    if (restoringState || !viewReady || viewMenuPreview) return;
    if (iconRealignPending && !viewMenuRestore && enumerationComplete) realignIcons();
    if (initialStateApplied) {
        CComPtr<ItemWindow> keepAlive(this);
        CComPtr<IFolderView> view;
        const ULONG_PTR generation = viewGeneration;
        if (FAILED(viewHost.currentView(IID_PPV_ARGS(&view))) || !isWindowOperational()
                || generation != viewGeneration) return;
        const bool spatial = spatialView(view);
        if (!isWindowOperational() || generation != viewGeneration) return;
        const bool resized = arrangementChanged(view);
        if (!isWindowOperational() || generation != viewGeneration) return;
        const bool grid = spatial && (resized || spatial != spatialLayout) && gridPlacement(view);
        if (!isWindowOperational() || generation != viewGeneration) return;
        if (grid && resized) {
            // Reapply the Shell's grid for the current renderer and icon size.
            // Old free coordinates must not override its new cell geometry.
            iconRealignPending = true;
            if (!viewMenuRestore && enumerationComplete) realignIcons();
            if (!isWindowOperational() || generation != viewGeneration) return;
        } else if (spatial && spatial == spatialLayout && resized && !viewStateIsDirty(FolderState::Icons)) {
            // Only free placement retains exact coordinates across a resize.
            // Never replace unsaved moves with older positions.
            restoreIconsPending = searchText.empty();
            restoreIconsUnpruned = restoreIconsPending;
            finishViewInitialization();
            if (!isWindowOperational() || generation != viewGeneration) return;
        }
        if (spatial != spatialLayout) {
            spatialLayout = spatial;
            // Changing mode can discard the Shell's free positions even when
            // the provider stays the same. Restore once, before any capture.
            if (spatial && !grid) {
                restoreIconsPending = searchText.empty();
                // No EnumDone establishes this observation's completeness: a
                // saved position absent from the view is kept, never pruned.
                restoreIconsUnpruned = restoreIconsPending;
                finishViewInitialization(); // Otherwise the next EnumDone finishes it.
                if (!isWindowOperational() || generation != viewGeneration) return;
            }
        }
    }
    if (icons) viewStateDirty(FolderState::Icons);
    // Coalesce related native notifications; no polling and no database write per mouse move.
    SetTimer(hwnd, TIMER_SAVE_STATE, 250, nullptr);
}

FOLDERVIEWOPTIONS FolderWindow::profileForMode(UINT mode) const {
    return hybridViewEligible && (mode == FVM_LIST || mode == FVM_CONTENT || (mode == FVM_DETAILS && !columnFillActive()))
        ? FVO_DEFAULT : static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING);
}

void FolderWindow::requestViewProfile() {
    if (!hybridViewEligible || !isWindowOperational()) return;
    viewProfilePending = true;
    if (viewProfilePosted) return;
    viewProfilePosted = PostMessageW(hwnd, MSG_SYNCHRONIZE_VIEW_PROFILE, 0, 0) != FALSE;
}

void FolderWindow::synchronizeViewProfile() {
    viewProfilePosted = false;
    if (!hybridViewEligible || !isWindowOperational() || !viewReady || !initialStateApplied
            || !enumerationComplete || restoringState || viewProfileChanging || navigationDepth
            || viewMenuDepth || viewMenuPreview || viewMenuRestore || provisionalBaseline) {
        return;
    }
    GUITHREADINFO gui = {sizeof(gui)};
    if (!GetGUIThreadInfo(GetCurrentThreadId(), &gui)
            || (gui.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) return;
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() { return isWindowOperational() && shellView == source && viewGeneration == generation; };
    HWND body = nullptr;
    if (source) source->GetWindow(&body);
    if (!current()) return;
    // This runs from a posted message, possibly replayed by an unrelated event:
    // never destroy the view under one of its own mouse interactions (selection
    // rectangle, drag, scroll bar, column header) or a native rename.
    const HWND capture = GetCapture();
    const HWND focus = GetFocus();
    bool interacting = body && capture && (capture == body || IsChild(body, capture));
    if (!interacting && body && focus && (focus == body || IsChild(body, focus))) {
        const LRESULT code = SendMessage(focus, WM_GETDLGCODE, 0, 0);
        if (!current()) return;
        interacting = (code & DLGC_HASSETSEL) != 0; // Native rename/text editing.
    }
    if (interacting) {
        // Ask again after this native interaction, at the next state capture.
        return;
    }
    viewProfilePending = false;
    ++navigationDepth;
    struct Navigation {
        FolderWindow &owner;
        ~Navigation() { owner.resumeDeferredClose(); }
    } navigation{*this};
    CComPtr<IFolderView2> view;
    if (FAILED(viewHost.currentView(IID_PPV_ARGS(&view))) || !current()) return;
    // The mode alone decides the profile: the complete capture below is only
    // needed for an actual change.
    UINT mode = 0;
    if (FAILED(view->GetCurrentViewMode(&mode)) || !current()) return;
    const auto desired = profileForMode(mode);
    if (desired == viewProfile) return;
    auto state = std::make_unique<ShellViewState>();
    state->view = captureOptions(view, getSavedFolderState().view);
    if (columnFillSet) {
        state->view.present |= FolderViewSettings::ColumnFill;
        state->view.fillColumns = fillColumns;
    }
    captureColumnWeights(state->view);
    if (!current() || !(state->view.present & FolderViewSettings::Mode)) return;
    // Reuse the complete capture path, including protection of positions that
    // could not yet be restored. A profile change must not trust a partial grid.
    viewStateDirty(FolderState::Icons);
    persistViewState();
    if (!current() || deferredCloseMessage) return;
    state->icons = getSavedFolderState().icons;
    if (FAILED(captureReopenSelection(source, current, state->selection)) || !current() || deferredCloseMessage) return;
    pendingKeyboardView = body && focus && (focus == body || IsChild(body, focus));
    pendingKeyboardControl = pendingKeyboardView ? nullptr : focus;
    if (!sameView(state->view, getSavedFolderState().view)) {
        getSavedFolderState().view = state->view;
        getSavedFolderState().present |= FolderState::View;
        viewStateDirty(FolderState::View);
    }
    storedViewState = std::move(state);
    viewProfileChanging = true;
    restoringState = true;
    KillTimer(hwnd, TIMER_SAVE_STATE);
    KillTimer(hwnd, TIMER_UPDATE_SELECTION);
    viewReady = false;
    checkHR(viewHost.stopObserving());
    // Nothing stays attached to the view about to be destroyed, as on the
    // other paths that release it (onItemChanged, onDestroy).
    if (columnLayout) columnLayout->stop();
    columnLayout.reset();
    selectionClear.reset();
    shellView.Release();
    checkHR(viewHost.destroy());
    if (!isWindowOperational() || deferredCloseMessage) return;
    viewProfile = desired;
    RECT bounds = windowBody();
    if (hasStatusText()) bounds.bottom -= scaleDPI(BOTTOM_STATUS_HEIGHT_DIP);
    // Same initial settings as a direct opening: only the mode and the
    // placement flags come from the captured state.
    FOLDERSETTINGS initial = folderSettings();
    initial.ViewMode = static_cast<UINT>(storedViewState->view.mode);
    if (storedViewState->view.present & FolderViewSettings::Flags) {
        const DWORD placementFlags = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
        initial.fFlags = (initial.fFlags & ~placementFlags) | (storedViewState->view.flags & placementFlags);
    }
    if (!isWindowOperational()) return;
    HRESULT result = viewHost.initialize(hwnd, bounds, initial, static_cast<IServiceProvider *>(this), this, viewProfile);
    if (SUCCEEDED(result) && isWindowOperational()) result = viewHost.setFilter(nameFilter);
    if (SUCCEEDED(result) && isWindowOperational()) result = browseToItem();
    if (!isWindowOperational()) return;
    if (SUCCEEDED(result)) result = viewHost.lockNavigation(true);
    if (!checkHR(result)) {
        viewProfileChanging = false;
        restoringState = false;
        setStatusText(getString(IDS_FOLDER_ERROR));
    }
}

// A changed preference recreates the view through the other provider; the
// view's identifiers are the native ones in both, so the existing transfer
// carries positions, selection and focus across whatever the layout.
void FolderWindow::synchronizeViewProvider() {
    if (!isWindowOperational()) return;
    const bool differs = settings::getMixedFolderSorting() != viewHost.mixedSortingPreference();
    if (!viewReady || !initialStateApplied || restoringState || viewMenuPreview || navigationDepth) {
        // Replayed when the initialization or restoration in progress completes.
        providerSyncPending = differs;
        return;
    }
    providerSyncPending = false;
    if (!differs) return;
    CComPtr<ItemWindow> keepAlive(this);
    viewStateDirty(FolderState::Icons);
    persistViewState();
    if (isWindowOperational()) onItemChanged(); // Existing canonical positions/selection transfer.
}

uint32_t FolderWindow::captureViewState(uint32_t mask) {
    uint32_t captured = ItemWindow::captureViewState(mask);
    const auto &preview = viewMenuPreview && viewMenuPreview->prepared ? viewMenuPreview : viewMenuRestore;
    if (!isWindowOperational() && preview && !preview->restoreFailed) closingScroll = preview->initial.scroll;
    if (provisionalBaseline) {
        if (!recoverProvisionalView()) return captured;
        // Closing deliberately merges without replaying the native view. Do
        // not immediately recapture its fallback over the merged reference.
        return captured | FolderState::View | FolderState::Icons;
    }
    if (viewMenuPreview && viewMenuPreview->prepared) {
        // A close during preview saves the trusted original state held in
        // memory, never the transient preview or an older disk reference.
        if (viewStateIsDirty(FolderState::View)) captured |= mask & FolderState::View;
        if (viewMenuPreview->capturedIcons && viewStateIsDirty(FolderState::Icons))
            captured |= mask & FolderState::Icons;
        return captured;
    }
    if (!viewHost || !viewReady || !initialStateApplied || restoringState
            || viewProfileChanging
            || getSavedFolderState().firstFailedAt) return captured;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    const HWND owner = hwnd;
    auto current = [&]() { return hwnd == owner && shellView == source && viewGeneration == generation; };
    CComPtr<IFolderView2> view;
    if (FAILED(viewHost.currentView(IID_PPV_ARGS(&view))) || !current()) return captured;
    if (!isWindowOperational() && closingScroll.empty() && !preview && enumerationComplete && searchText.empty()) {
        std::vector<uint8_t> bytes;
        const HRESULT result = captureViewScroll(source, current, bytes);
        if (!current()) return captured;
        if (result == S_OK) closingScroll.swap(bytes);
    } else if (!isWindowOperational() && !preview && enumerationComplete && !searchText.empty()) {
        // The filtered view has no position to keep, and the stored one is older
        // than what the user last saw unfiltered: it must not be replayed.
        closingScrollStale = true;
    }
    FolderViewSettings options = captureOptions(view, getSavedFolderState().view);
    if (columnFillSet) {
        options.present |= FolderViewSettings::ColumnFill;
        options.fillColumns = fillColumns;
    }
    captureColumnWeights(options);
    if (!current()) return captured;
    const bool profileMismatch = (options.present & FolderViewSettings::Mode)
        && profileForMode(static_cast<UINT>(options.mode)) != viewProfile;
    if (profileMismatch) requestViewProfile();
    if (options.present && (!(getSavedFolderState().present & FolderState::View)
            || !sameView(options, getSavedFolderState().view) || (mask & FolderState::View))) {
        getSavedFolderState().view = std::move(options);
        viewStateDirty(FolderState::View);
        captured |= FolderState::View;
    }
    // A filtered subset must never replace the complete folder's icon positions.
    if ((mask & FolderState::Icons) && !profileMismatch && !restoreIconsPending && spatialView(view)) {
        if (iconRestoreFailed) {
            captureIconChange();
            if (!current()) return captured;
            // Retry only the trusted reference, including an earlier dirty
            // composition; never recapture the partly applied native layout.
            return captured | (getSavedFolderState().present & FolderState::Icons);
        }
        if (!current()) return captured;
        std::vector<uint8_t> bytes;
        const HRESULT result = captureIconPositions(view, current, enumerationComplete,
            getSavedFolderState().icons, bytes, !searchText.empty());
        if (checkHR(result) && result == S_OK) {
            if (!(getSavedFolderState().present & FolderState::Icons) || bytes != getSavedFolderState().icons) {
                getSavedFolderState().icons = std::move(bytes);
                captured |= FolderState::Icons;
            } else {
                viewStateClean(FolderState::Icons);
            }
        }
    }
    return captured;
}

void FolderWindow::iconPositionError() {
    if (iconPositionErrorShown) return;
    iconPositionErrorShown = true;
    MessageBoxW(hwnd, getString(IDS_ICON_POSITION_ERROR), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONWARNING);
}

void FolderWindow::iconRestoreResult(HRESULT result) {
    iconRestoreFailed = result != S_OK;
    iconChangeBaseline.reset();
    if (!iconRestoreFailed) iconPositionErrorShown = false;
    watchProvisionalInput(provisionalBaseline || iconRestoreFailed);
    if (FAILED(result)) iconPositionError();
}

// Observe before input, never use the fallback itself as a saved reference.
// The existing hook suppresses nested input only while this snapshot is read.
bool FolderWindow::observeIconChange() {
    if (!iconRestoreFailed || provisionalCaptureBusy || GetCapture()) return false;
    captureIconChange(); // Finish the preceding interaction before rebasing.
    iconChangeBaseline.reset();
    if (!viewReady || !enumerationComplete || !searchText.empty()) return false;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration, enumeration = viewHost.enumerationSerial();
    auto current = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation
            && viewHost.enumerationSerial() == enumeration && enumerationComplete;
    };
    provisionalCaptureBusy = true;
    struct CaptureGuard { bool &busy; ~CaptureGuard() { busy = false; } } guard{provisionalCaptureBusy};
    CComQIPtr<IFolderView> view(source);
    if (!current() || !view || !spatialView(view) || !current()) return false;
    auto baseline = std::make_unique<std::vector<uint8_t>>();
    const HRESULT result = captureIconPositions(view, current, true, {}, *baseline);
    if (!current()) return false;
    if (result != S_OK) { iconPositionError(); return false; }
    iconChangeBaseline = std::move(baseline);
    iconChangeEnumeration = enumeration;
    viewStateDirty(FolderState::Icons);
    return true;
}

bool FolderWindow::captureIconChange() {
    if (!iconChangeBaseline || provisionalCaptureBusy || GetCapture()) return false;
    // A failed or incomplete observation cannot be attributed to a later input.
    auto baseline = std::move(iconChangeBaseline);
    if (viewHost.enumerationSerial() != iconChangeEnumeration) {
        iconPositionError();
        return false;
    }
    const HWND owner = hwnd;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration, enumeration = viewHost.enumerationSerial();
    auto current = [&]() {
        return hwnd == owner && shellView == source && viewGeneration == generation
            && viewHost.enumerationSerial() == enumeration && enumerationComplete;
    };
    provisionalCaptureBusy = true;
    struct CaptureGuard { bool &busy; ~CaptureGuard() { busy = false; } } guard{provisionalCaptureBusy};
    CComQIPtr<IFolderView> view(source);
    if (!view || !current() || !searchText.empty() || !spatialView(view) || !current()) return false;
    CComPtr<IShellFolder> folder;
    HRESULT result = view->GetFolder(IID_PPV_ARGS(&folder));
    if (!current()) return false;
    std::vector<uint8_t> observed, merged;
    if (SUCCEEDED(result)) result = captureIconPositions(view, current, true, {}, observed);
    if (!current()) return false;
    if (result == S_OK) result = mergeIconChanges(getSavedFolderState().icons,
        *baseline, observed, true, merged, folder, current);
    if (!current()) return false;
    if (result != S_OK) { iconPositionError(); return false; }
    getSavedFolderState().icons.swap(merged);
    viewStateDirty(FolderState::Icons);
    return true;
}

HRESULT FolderWindow::loadIconPositions(bool pruneMissing) {
    if (provisionalBaseline) return S_FALSE;
    if (!viewHost || !loadFolderState()) return E_PENDING;
    if (!(getSavedFolderState().present & FolderState::Icons) || getSavedFolderState().icons.empty()) {
        iconRestoreResult(S_OK);
        return S_OK;
    }
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration, enumeration = viewHost.enumerationSerial();
    auto sameView = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation;
    };
    auto current = [&]() { return sameView() && enumerationComplete
        && viewHost.enumerationSerial() == enumeration; };
    // Keep the old reference protected even if a same-view refill cancels us.
    iconRestoreFailed = true;
    watchProvisionalInput(true);
    CComPtr<IFolderView> view;
    HRESULT result = viewHost.currentView(IID_PPV_ARGS(&view));
    if (!current()) return E_ABORT;
    if (SUCCEEDED(result) && spatialView(view) && current()) {
        restoringState = true;
        struct RestoreGuard {
            bool &restoring;
            std::function<bool()> valid;
            ~RestoreGuard() { if (valid()) restoring = false; }
        } restoreGuard{restoringState, sameView};
        CComPtr<IShellFolder> folder;
        result = view->GetFolder(IID_PPV_ARGS(&folder));
        if (!current()) return E_ABORT;
        std::vector<uint8_t> visible, wanted;
        if (SUCCEEDED(result)) result = captureIconPositions(view, current,
            enumerationComplete, {}, visible, !searchText.empty());
        if (!current()) return E_ABORT;
        // A vanished item is normal, not a failed native restore. Only a full,
        // unfiltered observation may remove it from the known reference.
        if (result == S_OK) result = mergeIconChanges(getSavedFolderState().icons,
            {}, visible, enumerationComplete && pruneMissing, wanted, folder, current);
        if (!current()) return E_ABORT;
        if (result == S_OK) {
            if (wanted != getSavedFolderState().icons) {
                getSavedFolderState().icons = std::move(wanted);
                viewStateDirty(FolderState::Icons);
            }
            result = restoreIconPositions(view, getSavedFolderState().icons, current);
        }
        if (!current()) return E_ABORT;
        restoringState = false;
        iconRestoreResult(result);
    } else if (FAILED(result)) iconRestoreResult(result);
    return result;
}

std::vector<uint8_t> FolderWindow::captureReopenViewState() {
    const auto &session = viewMenuPreview ? viewMenuPreview : viewMenuRestore;
    if (session && session->prepared && !session->restoreFailed) return session->initial.selection;
    // During a profile change the recreated view is not restored yet: the
    // selection and viewport still belong to the transient snapshot.
    if (viewProfileChanging && storedViewState) return storedViewState->selection;
    // Closing is already set: use HWND/view identity rather than isWindowOperational.
    const HWND owner = hwnd;
    CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto valid = [&]() { return hwnd == owner && shellView == view && viewGeneration == generation; };
    if (!owner || !view) return {};
    std::vector<uint8_t> bytes;
    if (!checkHR(captureReopenSelection(view, valid, bytes))) return {};
    return bytes;
}

void FolderWindow::restoreReopenViewState() {
    if (!viewReady || !enumerationComplete || !reopeningWindow.sequence || !isWindowOperational()) return;
    // Consume once, before calling Shell APIs that can reenter enumeration callbacks.
    auto bytes = std::move(reopeningWindow.viewState);
    reopeningWindow = {};
    if (bytes.empty()) return;
    restoreScrollPending = false; // The explicit history snapshot owns this viewport.
    CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto valid = [&]() { return isWindowOperational() && shellView == view && viewGeneration == generation; };
    checkHR(restoreReopenSelection(view, bytes, valid, &selectionClear));
    if (valid()) updateSelection();
}

void FolderWindow::changeViewMode(int amount, bool wheel) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational() || !viewReady || !initialStateApplied || !enumerationComplete
            || restoringState || viewProfileChanging || navigationDepth || viewMenuDepth
            || viewMenuPreview || viewMenuRestore || provisionalInputBlocked || provisionalCaptureBusy) return;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation;
    };
    CComPtr<IFolderView2> view;
    const HRESULT queried = viewHost.currentView(IID_PPV_ARGS(&view));
    if (!current() || FAILED(queried) || !view) return;
    struct Mode { FOLDERVIEWMODE mode; int size; };
    const Mode modes[] = {
        {FVM_ICON, 256}, {FVM_ICON, 96}, {FVM_ICON, 48},
        {FVM_SMALLICON, 16}, {FVM_LIST, -1}, {FVM_DETAILS, -1}, {FVM_TILE, -1}, {FVM_CONTENT, -1}
    };
    FOLDERVIEWMODE mode;
    int size;
    if (!checkHR(view->GetViewModeAndIconSize(&mode, &size)) || !current()) return;
    Mode target = {mode, size};
    if (wheel) {
        // The steps of Explorer's own Ctrl+wheel, measured at 96 DPI on Windows 10
        // and 11: Content, Tiles, Details, List, the small icons, then the icons.
        // Among the small icons Explorer does not take the same sizes both ways.
        static const int icons[] = {33, 35, 38, 41, 44, 47, 50, 54, 58, 62, 67, 72, 77, 83, 89, 95,
            102, 110, 118, 126, 136, 145, 156, 168, 180, 193, 207, 223, 239, 256};
        static const int smallLarger[] = {16, 18, 20, 22, 23, 25, 27, 29, 31};
        static const int smallSmaller[] = {16, 17, 18, 19, 20, 22, 23, 25, 27, 29, 31};
        const bool larger = amount > 0;
        // The first size past the one reached, walking the scale in the steps' direction.
        auto beyond = [&](const int *sizes, int count) {
            for (int i = 0; i < count; ++i) {
                const int candidate = sizes[larger ? i : count - 1 - i];
                if (larger ? candidate > target.size : candidate < target.size) return candidate;
            }
            return 0;
        };
        for (int remaining = larger ? amount : -amount; remaining > 0; --remaining) {
            const Mode from = target;
            if (from.mode == FVM_ICON || from.mode == FVM_SMALLICON) {
                const int icon = larger || from.mode == FVM_ICON ? beyond(icons, _countof(icons)) : 0;
                if (larger) {
                    const int smallIcon = from.mode == FVM_SMALLICON ? beyond(smallLarger, _countof(smallLarger)) : 0;
                    if (smallIcon) target = {FVM_SMALLICON, smallIcon};
                    else if (icon) target = {FVM_ICON, icon};
                } else {
                    const int smallIcon = beyond(smallSmaller, _countof(smallSmaller));
                    if (icon) target = {FVM_ICON, icon};
                    else if (smallIcon) target = {FVM_SMALLICON, smallIcon};
                    else target = {FVM_LIST, -1};
                }
            } else if (from.mode == FVM_LIST) {
                target = larger ? Mode{FVM_SMALLICON, smallLarger[0]} : Mode{FVM_DETAILS, -1};
            } else if (from.mode == FVM_TILE) {
                target = larger ? Mode{FVM_DETAILS, -1} : Mode{FVM_CONTENT, -1};
            } else if (from.mode == FVM_CONTENT) {
                if (larger) target = {FVM_TILE, -1};
            } else {
                // Details, and the modes of other providers, which start from Details.
                target = larger ? Mode{FVM_LIST, -1} : Mode{FVM_TILE, -1};
            }
            if (target.mode == from.mode && target.size == from.size) break; // An end of the scale.
        }
    } else if (amount >= 0 && amount < static_cast<int>(_countof(modes))) {
        target = modes[amount];
    } else {
        return;
    }
    if (mode == target.mode && (target.size < 0 || size == target.size)) return;
    POINT previousSpacing = {};
    const bool wasGrid = gridPlacement(view);
    if (!current()) return;
    if (wasGrid && (FAILED(view->GetSpacing(&previousSpacing)) || !current())) return;
    viewStateDirty(FolderState::Icons);
    persistViewState();
    if (!current()) return;
    restoringState = true; // Ignore notifications from an intermediate native layout.
    ++navigationDepth;
    const HRESULT result = view->SetViewModeAndIconSize(target.mode, target.size);
    resumeDeferredClose();
    if (!current()) return;
    restoringState = false;
    if (checkHR(result)) {
        const bool grid = gridPlacement(view);
        if (!current()) return;
        if (grid && wasGrid && enumerationComplete && searchText.empty()) {
            restoringState = true;
            checkHR(restoreIconGridPositions(view, getSavedFolderState().icons, previousSpacing, current));
            if (!current()) return;
            restoringState = false;
            viewStateDirty(FolderState::Icons);
        } else if (grid) iconRealignPending = true;
        else {
            // Some native mode changes discard free positions without changing
            // the spatial flag. Replay them before another capture.
            restoreIconsPending = searchText.empty();
            restoreIconsUnpruned = restoreIconsPending;
            finishViewInitialization();
            if (!current()) return;
        }
        arrangementChanged(view); // The resize was handled above, not again by the save.
        if (!current()) return;
        scheduleStateSave();
        requestViewProfile();
    }
}

void FolderWindow::restoreReferenceLayout() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational() || !viewReady || !enumerationComplete || restoringState || viewMenuPreview) return;
    FolderState reference;
    auto store = folderStateStore();
    if (!store || !store->read(getFolderStateKey(), &reference)) { stateStorageError(); return; }
    if (!reference.locked || !settings::getWindowStateLockEnabled()) return;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation;
    };
    CComQIPtr<IFolderView2> view(source);
    if (!view || !current()) return;
    getSavedFolderState() = reference;
    // The column fill must neither veto nor reinterpret the widths written
    // below as a user gesture: detach it, and take the folder's fill choice
    // from the reference like the other frozen settings.
    if (columnLayout) columnLayout->stop();
    columnLayout.reset();
    if (reference.present & FolderState::View) {
        columnFillSet = (reference.view.present & FolderViewSettings::ColumnFill) != 0;
        fillColumns = !columnFillSet || reference.view.fillColumns;
        columnFillLoaded = true;
    }
    restoringState = true;
    auto apply = [&]() {
        const RECT rect = requestedRect(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
        if (!current()) return;
        ShowWindow(hwnd, SW_RESTORE);
        if (!current()) return;
        SetWindowPos(hwnd, nullptr, rect.left, rect.top, rectWidth(rect), rectHeight(rect),
            SWP_NOZORDER | SWP_NOACTIVATE);
        if (!current()) return;
        if ((reference.present & FolderState::Maximized) && reference.maximized)
            ShowWindow(hwnd, SW_MAXIMIZE);
        if (!current()) return;
        if (reference.present & FolderState::View) checkHR(restoreOptions(view, reference.view));
    };
    apply();
    restoringState = false;
    if (!current()) return;
    synchronizeColumns(); // Reattached on the reference widths.
    if (current()) requestViewProfile(); // The fill choice takes part in the profile.
}

void FolderWindow::fitWindowToContents() {
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() { return isWindowOperational() && shellView == source && viewGeneration == generation; };
    if (!current() || !enumerationComplete || restoringState || viewMenuPreview) return;
    CComQIPtr<IFolderView2> view(source);
    if (!view || !current()) return;
    int count = 0;
    if (FAILED(view->ItemCount(SVGIO_ALLVIEW, &count)) || !current()) return;
    POINT spacing = {};
    if (FAILED(view->GetSpacing(&spacing)) || !current()) return;
    spacing.x = (std::max<LONG>)(spacing.x, scaleDPI(32));
    spacing.y = (std::max<LONG>)(spacing.y, scaleDPI(20));
    SIZE content = {scaleDPI(96), scaleDPI(64)};
    for (int i = 0; i < count; ++i) {
        CComHeapPtr<ITEMID_CHILD> id;
        if (FAILED(view->Item(i, &id)) || !current()) return;
        POINT position = {};
        if (FAILED(view->GetItemPosition(id, &position)) || !current()) return;
        content.cx = (std::max<LONG>)(content.cx, position.x + spacing.x);
        content.cy = (std::max<LONG>)(content.cy, position.y + spacing.y);
    }
    UINT mode = 0;
    if (FAILED(view->GetCurrentViewMode(&mode)) || !current()) return;
    if (mode == FVM_DETAILS) {
        CComQIPtr<IColumnManager> columns(source);
        UINT length = 0;
        if (!columns || FAILED(columns->GetColumnCount(CM_ENUM_VISIBLE, &length)) || !current()) return;
        std::vector<PROPERTYKEY> keys(length);
        if (length && (FAILED(columns->GetColumns(CM_ENUM_VISIBLE, keys.data(), length)) || !current())) return;
        LONG width = 0;
        for (const auto &key : keys) {
            CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
            if (FAILED(columns->GetColumnInfo(key, &info)) || !current()) return;
            if (info.uWidth > 32768 || width > 32768) return;
            width += static_cast<LONG>(info.uWidth);
        }
        content.cx = (std::max<LONG>)(scaleDPI(96), width);
    }
    HWND body = nullptr;
    if (FAILED(source->GetWindow(&body)) || !current()) return;
    RECT viewRect = {}, windowRect = {};
    if (!GetClientRect(body, &viewRect) || !GetWindowRect(hwnd, &windowRect)) return;
    MONITORINFO monitor = {sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) return;
    const UINT dpi = GetDpiForWindow(hwnd);
    // Columns that fill the window follow its width: their sum says nothing
    // about the content, and fitting to it would widen the window each time.
    const bool keepWidth = mode == FVM_DETAILS && columnFillActive();
    const LONG width = (std::min<LONG>)(rectWidth(monitor.rcWork), keepWidth ? rectWidth(windowRect)
        : (std::max<LONG>)(GetSystemMetricsForDpi(SM_CXMINTRACK, dpi),
        content.cx + rectWidth(windowRect) - rectWidth(viewRect) + scaleDPI(12)));
    const LONG height = (std::min<LONG>)(rectHeight(monitor.rcWork),
        (std::max<LONG>)(GetSystemMetricsForDpi(SM_CYMINTRACK, dpi),
        content.cy + rectHeight(windowRect) - rectHeight(viewRect) + scaleDPI(12)));
    const LONG x = (std::max<LONG>)(monitor.rcWork.left, (std::min<LONG>)(windowRect.left, monitor.rcWork.right - width));
    const LONG y = (std::max<LONG>)(monitor.rcWork.top, (std::min<LONG>)(windowRect.top, monitor.rcWork.bottom - height));
    ShowWindow(hwnd, SW_RESTORE);
    if (current()) SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

bool FolderWindow::columnFillActive() const {
    return settings::getDetailsColumnsEnabled() && fillColumns;
}

// The proportions belong to the fill: without it, the widths alone describe
// the columns, and a later fill starts from them.
void FolderWindow::captureColumnWeights(FolderViewSettings &view) const {
    if (columnLayout) {
        columnLayout->describe(view);
    } else if (!columnFillActive()) {
        view.present &= ~static_cast<uint32_t>(FolderViewSettings::ColumnWeights);
        view.columnWeights.clear();
    }
}

void FolderWindow::loadColumnFill() {
    if (columnFillLoaded || !loadFolderState()) return;
    const auto &state = getSavedFolderState().view;
    columnFillSet = (state.present & FolderViewSettings::ColumnFill) != 0;
    fillColumns = !columnFillSet || state.fillColumns;
    columnFillLoaded = true;
}

void FolderWindow::toggleFillColumns() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!settings::getDetailsColumnsEnabled() || !isWindowOperational()
            || provisionalBaseline || provisionalInputBlocked || provisionalCaptureBusy) return;
    loadColumnFill();
    if (!columnFillLoaded || !isWindowOperational()) return;
    fillColumns = !fillColumns;
    columnFillSet = true;
    viewStateDirty(FolderState::View);
    if (columnLayout) columnLayout->stop();
    columnLayout.reset();
    synchronizeColumns();
    requestViewProfile();
    if (isWindowOperational()) {
        scheduleStateSave();
        checkLE(PostMessageW(hwnd, MSG_COLUMNS_CHANGED, viewGeneration, 0));
    }
}

void FolderWindow::synchronizeColumns() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational()) return;
    loadColumnFill();
    if (!columnFillActive()) {
        if (columnLayout) columnLayout->stop();
        columnLayout.reset();
        return;
    }
    if (!initialStateApplied || !enumerationComplete || restoringState
            || viewMenuPreview || viewMenuRestore || provisionalBaseline) return;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    CComQIPtr<IFolderView2> view(source);
    UINT mode = 0;
    if (!view || FAILED(view->GetCurrentViewMode(&mode)) || !isWindowOperational()
            || shellView != source || viewGeneration != generation) return;
    if (mode != FVM_DETAILS) {
        if (columnLayout) columnLayout->stop();
        columnLayout.reset();
        return;
    }
    if (!columnLayout) {
        const FolderViewSettings &saved = getSavedFolderState().view;
        auto attached = ShellViewColumns::attach(source, hwnd, MSG_COLUMNS_CHANGED, generation,
            (saved.present & FolderViewSettings::ColumnWeights) ? saved.columnWeights
                : std::vector<FolderViewSettings::Column>());
        if (!isWindowOperational() || shellView != source || viewGeneration != generation) {
            if (attached) attached->stop();
            return;
        }
        columnLayout = std::move(attached);
    }
    auto layout = columnLayout;
    if (layout) {
        layout->setDeferred(settings::getDetailsColumnsDeferred());
        // A resize gesture keeps the value it started with.
        layout->suspend(ShellViewColumns::Hold::Resize, resizingColumns && resizeDeferred);
        layout->synchronize();
    }
}

void FolderWindow::finishColumnResize() {
    resizingColumns = false;
    if (!isWindowOperational()) return;
    if (columnBoundsPending && !IsIconic(hwnd)) onSize(clientSize(hwnd));
    synchronizeColumns();
}

} // namespace filespacer
