#include "FolderWindow.h"
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "ShellSelectionRuntime.h"
#include "MixedFolder.h"
#include "ShellViewIcons.h"
#include "ShellViewFilter.h"
#include "ShellViewLabels.h"
#include "ShellViewColumns.h"
#include "CreateItemWindow.h"
#include "ClosedWindows.h"
#include "ShellMenuIcons.h"
#include "GeomUtils.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "Settings.h"
#include "Everything.h"
#include "Peek.h"
#include "DPI.h"
#include "UIStrings.h"
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <propkey.h>
#include <propsys.h>
#include <Propvarutil.h>
#include <VersionHelpers.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <memory>
#include <atomic>
#include <type_traits>
#include <utility>
#include <oleacc.h>

#pragma comment(lib, "Propsys.lib")
#pragma comment(lib, "Oleacc.lib")

// Example of how to host an IExplorerBrowser:
// https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/winui/shell/appplatform/ExplorerBrowserCustomContents

namespace filespacer {

FolderWindow::~FolderWindow() {
    if (columnLayout) columnLayout->stop();
    watchProvisionalInput(false); // The hook must never compare a destroyed window.
}

static constexpr int BOTTOM_STATUS_HEIGHT_DIP = 23;
// Coalesce selection notifications before updating the selected item and status.
static constexpr UINT SELECTION_UPDATE_DELAY_MS = 100;

static void toggleGlobalThumbnails() {
    SHELLSTATE state = {};
    SHGetSetSettings(&state, SSF_ICONSONLY, FALSE);
    state.fIconsOnly = !state.fIconsOnly;
    // The Shell publishes its normal ShellState setting-change notification.
    SHGetSetSettings(&state, SSF_ICONSONLY, TRUE);
}

// on the Desktop only
const wchar_t * const HIDDEN_ITEM_PARSE_NAMES[] = {
    L"::{26EE0668-A00A-44D7-9371-BEB064C98683}", // Control Panel
    L"::{018D5C66-4533-4307-9B53-224DE2ED1FE6}", // OneDrive (may fail if not installed)
    L"::{031E4825-7B94-4DC3-B131-E946B44C8DD5}", // Libraries
    // added in build 22621.675  >:(
    L"::{F874310E-B6B7-47DC-BC84-B9E6B38F5903}", // Home
    L"::{B4BFCC3A-DB2C-424C-B029-7FE99A87C641}", // Desktop (why???)
    L"::{A8CDFF1C-4878-43BE-B5FD-F8091C1C60D0}", // Documents
    L"::{374DE290-123F-4565-9164-39C4925E467B}", // Downloads
    L"::{1CF1260C-4DD0-4EBB-811F-33C572699FDE}", // Music
    L"::{3ADD1653-EB32-4CB0-BBD7-DFA0ABB5ACCA}", // Pictures
    L"::{A0953C92-50DC-43BF-BE83-3742FED03C9C}", // Videos
    // added in build 22621.2715
    L"::{E88865EA-0E1C-4E20-9AA6-EDCD0212C87C}", // Gallery
    // special items that are NOT hidden: user folder, This PC, Network, Recycle Bin, Linux
};

const UINT WM_GETISHELLBROWSER_COMPAT = WM_USER + 7;

static CComHeapPtr<ITEMID_CHILD> hiddenItemIDs[_countof(HIDDEN_ITEM_PARSE_NAMES)];

bool FolderWindow::spatialView(IFolderView *const folderView) const {
    if (hybridViewEligible && viewProfile == FVO_DEFAULT) return false;
    UINT viewMode;
    return folderView->GetAutoArrange() == S_FALSE
        && checkHR(folderView->GetCurrentViewMode(&viewMode))
        && viewMode != FVM_DETAILS && viewMode != FVM_LIST;
}

void FolderWindow::init() {
    SearchBox::init();
    for (int i = 0; i < _countof(HIDDEN_ITEM_PARSE_NAMES); i++) {
        CComPtr<IShellItem> item;
        if (SUCCEEDED(SHCreateItemFromParsingName(HIDDEN_ITEM_PARSE_NAMES[i], nullptr,
                IID_PPV_ARGS(&item)))) {
            CComQIPtr<IParentAndItem> parentAndItem(item);
            if (parentAndItem)
                checkHR(parentAndItem->GetParentAndItem(nullptr, nullptr, &hiddenItemIDs[i]));
        }
    }
}

FolderWindow::FolderWindow(IShellItem *const item, const std::wstring &identity)
        : ItemWindow(item, identity) {}

// The menu owns this session while tracking. Once the menu closes, the window
// retains it only while a refill observed by apply() is still running, to replay
// the same restoration on that refill's EnumDone; saves are never held back.
struct FolderWindow::ViewMenuPreview {
    struct Entry { UINT command; };
    static thread_local ViewMenuPreview *active;
    FolderWindow &owner;
    HMENU menu;
    HMENU displayedMenu = nullptr;
    CComPtr<IContextMenu> context;
    CComPtr<IShellView> source;
    CComPtr<IFolderView2> view;
    const ULONG_PTR generation;
    const ULONG_PTR serial;
    ShellViewState initial;
    std::vector<Entry> entries;
    UINT initialCommand = 0, currentCommand = 0, wantedCommand = 0;
    UINT pendingCommand = 0; // Entry outside the modes, run after the preview is cancelled.
    UINT chosenCommand = 0; // Mode retained when the menu closed: 0 for the initial one.
    bool tracking = true, posted = false, busy = false;
    // The final apply() failed without any refill: no EnumDone replays it. The
    // session still withholds saving the preview, but its initial selection
    // no longer describes the view.
    bool restoreFailed = false;
    const ULONG_PTR enumerationBaseline; // Serial when the menu opened; earlier EnumDone are stale.
    std::vector<std::vector<uint8_t>> positionChunks;
    size_t nextPositionChunk = 0;
    POINT lastPoint = {}; // Last cursor position seen; moves without displacement are not hovers.

    HRESULT preparePositionChunks() {
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

    void advancePositions() {
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

    void finishPositions() {
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

    ViewMenuPreview(FolderWindow &window, HMENU popup, IContextMenu *handler,
            IFolderView2 *folderView) : owner(window), menu(popup), context(handler),
        source(window.shellView), view(folderView), generation(window.viewGeneration),
        serial(++window.viewMenuPreviewSerial),
        enumerationBaseline(window.viewHost.enumerationSerial()) {
        GetCursorPos(&lastPoint);
    }

    bool current() const {
        return owner.isWindowOperational() && owner.shellView == source
            && owner.viewGeneration == generation;
    }

    bool capture() {
        initial.view = captureOptions(view, {});
        if (!current() || !(initial.view.present & FolderViewSettings::Mode)) return false;
        auto valid = [&]() { return current(); };
        HRESULT result = captureReopenSelection(source, valid, initial.selection);
        if (FAILED(result) || !current()) return false;
        captureViewScroll(source, valid, initial.scroll);
        if (!current()) return false;
        if (owner.spatialView(view) && current()) {
            result = captureIconPositions(view, valid, owner.enumerationComplete,
                owner.getSavedFolderState().icons, initial.icons, !owner.searchText.empty());
            if (FAILED(result) || !current()) return false;
        }
        for (int i = 0, count = GetMenuItemCount(menu); i < count; ++i) {
            MENUITEMINFO info = {sizeof(info)};
            info.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
            if (!GetMenuItemInfo(menu, i, TRUE, &info) || info.hSubMenu
                    || (info.fType & MFT_SEPARATOR) || (info.fState & (MFS_DISABLED | MFS_GRAYED))
                    || info.wID < IDM_SHELL_FIRST || info.wID > IDM_SHELL_LAST) continue;
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

    static LRESULT CALLBACK filter(int code, WPARAM wParam, LPARAM lParam) {
        if (code == MSGF_MENU && active) {
            const MSG &message = *reinterpret_cast<const MSG *>(lParam);
            if (message.message == WM_MOUSEMOVE || message.message == WM_NCMOUSEMOVE) {
                POINT point = {};
                if (GetCursorPos(&point)) active->pointAt(point);
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    void pointAt(POINT point) {
        if (!tracking || !current() || !displayedMenu) return;
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

    void highlight(UINT command, HMENU popup) {
        if (!tracking || !current()) return;
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

    void restoreDetails() {
        if (!current() || (tracking && currentCommand)) return;
        auto valid = [&]() { return current(); };
        ++owner.navigationDepth;
        if (!currentCommand && !initial.icons.empty() && owner.spatialView(view) && current()) {
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

    void apply(UINT command) {
        if (busy || !current() || command == currentCommand) return;
        positionChunks.clear();
        nextPositionChunk = 0;
        busy = true;
        int before = -1, after = -1;
        const HRESULT counted = view->ItemCount(SVGIO_ALLVIEW, &before);
        if (!current()) { busy = false; return; }
        const ULONG_PTR baseline = owner.viewHost.enumerationSerial();
        HRESULT result;
        if (command) {
            auto info = owner.makeInvokeInfo(static_cast<int>(command - IDM_SHELL_FIRST), POINT{});
            info.hwnd = owner.hwnd;
            result = owner.invokeViewCommand(context, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info));
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

    void update() {
        posted = false;
        if (tracking) apply(wantedCommand);
    }

    bool isEntry(UINT command) const {
        for (const auto &entry : entries)
            if (entry.command == command) return true;
        return false;
    }

    // Runs the entry chosen outside the modes once the initial mode is restored.
    void invokePending() {
        const UINT command = pendingCommand;
        pendingCommand = 0;
        if (!command || !current()) return;
        if (command == IDM_TOGGLE_THUMBNAILS) {
            toggleGlobalThumbnails();
            return;
        }
        if (command == IDM_FILL_COLUMNS) {
            owner.toggleFillColumns();
            return;
        }
        auto info = owner.makeInvokeInfo(static_cast<int>(command - IDM_SHELL_FIRST), POINT{});
        info.hwnd = owner.hwnd;
        checkHR(owner.invokeViewCommand(context, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info)));
    }

    // While tracking, a refill completed by this EnumDone rebuilt the items
    // the restoration concerned: restore them again (idempotent).
    void enumerated(ULONG_PTR enumeration) {
        if (busy || enumeration <= enumerationBaseline || !current()) return;
        restoreDetails();
    }

    // The menu closed while a refill observed by apply() was running: its
    // EnumDone replays the same restoration, or retries a failed cancellation,
    // then lets the completed view be saved.
    void replay(ULONG_PTR enumeration) {
        FolderWindow &window = owner;
        if (busy || enumeration <= enumerationBaseline || !window.enumerationComplete || !current()) return;
        if (chosenCommand != currentCommand) apply(chosenCommand);
        else restoreDetails();
        if (!current()) return;
        // Releasing the record may destroy this session: only the window is used after it.
        if (chosenCommand != currentCommand) {
            window.viewMenuRestore.reset();
            window.replayProviderSync();
            return;
        }
        if (window.enumerationComplete) window.viewMenuRestore.reset(); // Else another refill was observed.
        window.scheduleStateSave();
        if (!window.viewMenuRestore) window.replayProviderSync();
    }

    void finish(UINT command) {
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
        invokePending();
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
        window.viewMenuPreview.reset();
        if (settled) window.scheduleStateSave();
        // A preference changed while the menu was open: synchronized now that
        // it is closed, unless a refill still running replays it on completion.
        if (!window.viewMenuRestore) window.replayProviderSync();
    }
};

thread_local FolderWindow::ViewMenuPreview *FolderWindow::ViewMenuPreview::active = nullptr;
thread_local HHOOK FolderWindow::provisionalHook = nullptr;
thread_local std::vector<FolderWindow *> FolderWindow::provisionalWindows;

bool FolderWindow::watchProvisionalInput(bool watch) {
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
        for (auto candidate : provisionalWindows)
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
    if (!isClosing && currentView()) replayProviderSync(); // Outside this restoration, not from within it.
    return currentView();
}

// A provider synchronization deferred by synchronizeViewProvider() runs once
// the step that deferred it is over, from the message queue rather than from
// within that step's callback.
void FolderWindow::replayProviderSync() {
    if (!providerSyncPending || !isWindowOperational()) return;
    providerSyncPending = false;
    checkLE(PostMessage(hwnd, MSG_SYNCHRONIZE_VIEW_PROVIDER, 0, 0));
}

bool FolderWindow::hasStatusText() {
    return bottomStatusBar && (GetWindowLongPtr(bottomStatusBar, GWL_STYLE) & WS_VISIBLE);
}

void FolderWindow::setStatusText(const wchar_t *text) {
    if (bottomStatusBar) {
        SendMessage(
            bottomStatusBar,
            SB_SETTEXTW,
            SBT_NOBORDERS,
            (LPARAM)(text ? text : L""));
    }
}

SIZE FolderWindow::defaultSize() const {
    return scaleDPI(settings::getFolderWindowSize());
}

bool FolderWindow::isFolder() const {
    return true;
}

bool FolderWindow::handleTopLevelMessage(MSG *msg) {
    CComPtr<ItemWindow> messageKeepAlive(this);
    if (!isWindowOperational())
        return false;
    if (handleAltMouseMessage(msg))
        return true;
    if (!isWindowOperational())
        return false;
    if (activationClickPending && msg->message >= WM_MOUSEFIRST && msg->message <= WM_MOUSELAST
            && msg->message != WM_MOUSEMOVE) {
        // Only the button-down that caused the activation is a candidate.
        activationClickPending = false;
        if (msg->message == WM_LBUTTONDOWN && isBackgroundActivationClick(msg))
            return true;
        if (!isWindowOperational())
            return false;
    }
    if (msg->message == WM_KEYDOWN && GetKeyState(VK_MENU) >= 0) {
        const bool control = GetKeyState(VK_CONTROL) < 0;
        const bool shift = GetKeyState(VK_SHIFT) < 0;
        if (!shift && ((!control && msg->wParam == VK_F5)
                || (control && msg->wParam == L'R'))) {
            onCommand(IDM_REFRESH);
            return true;
        }
        if (control && !shift && (msg->wParam == L'N' || msg->wParam == L'W')) {
            onCommand(msg->wParam == L'N' ? IDM_NEW_WINDOW : IDM_CLOSE_WINDOW);
            return true;
        }
        if (control && shift && msg->wParam >= L'1' && msg->wParam <= L'8') {
            changeViewMode(static_cast<int>(msg->wParam - L'1'));
            return true;
        }
    }
    if ((msg->message == WM_KEYUP && msg->wParam == VK_CONTROL)
            || (msg->message == WM_MOUSEWHEEL && !(GET_KEYSTATE_WPARAM(msg->wParam) & MK_CONTROL)))
        zoomWheelDelta = 0;
    if (msg->message == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(msg->wParam) & MK_CONTROL)
            && !(GET_KEYSTATE_WPARAM(msg->wParam) & MK_SHIFT) && GetKeyState(VK_MENU) >= 0) {
        CComPtr<IShellView> view = shellView;
        HWND root = nullptr;
        const HWND target = WindowFromPoint({GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)});
        if (view && SUCCEEDED(view->GetWindow(&root)) && isWindowOperational() && shellView == view
                && root && (msg->hwnd == root || IsChild(root, msg->hwnd)
                    || target == root || IsChild(root, target))) {
            zoomWheelDelta += GET_WHEEL_DELTA_WPARAM(msg->wParam);
            const int steps = zoomWheelDelta / WHEEL_DELTA;
            zoomWheelDelta %= WHEEL_DELTA;
            if (steps) changeViewMode(-1, steps);
            return true;
        }
    }
    if (msg->message == WM_KEYDOWN && msg->wParam == L'T'
            && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0
            && GetKeyState(VK_MENU) >= 0) {
        // This window command also works while editing the address or filter.
        onCommand(IDM_REOPEN_WINDOW);
        return true;
    }
    if (msg->message == WM_KEYDOWN && GetKeyState(VK_MENU) >= 0
            && (((msg->wParam == L'F' || msg->wParam == L'E') && GetKeyState(VK_CONTROL) < 0)
                || (msg->wParam == VK_F3 && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_SHIFT) >= 0)))
        return searchBox.focus();
    if (searchBox.hasFocus()) {
        if (pathBar.handleTopLevelMessage(msg))
            return true;
        if (!isWindowOperational())
            return false;
        // Keep native edit shortcuts away from the Shell's file commands.
        return searchBox.handleTopLevelMessage(msg);
    }
    if (ItemWindow::handleTopLevelMessage(msg))
        return true;
    if (!isWindowOperational() || pathBar.hasFocus())
        return false; // The Shell view must not consume Ctrl+C/Ctrl+V from the address.
    if (provisionalInputBlocked) return false;
    if (activateOnShiftRelease && msg->message == WM_KEYUP && msg->wParam == VK_SHIFT) {
        CComPtr<IShellView> view = shellView;
        if (view)
            checkHR(view->UIActivate(SVUIA_ACTIVATE_FOCUS));
        activateOnShiftRelease = false;
        // don't return true
    }
    if (!isWindowOperational())
        return false;
    CComPtr<IShellView> view = shellView;
    if (!view)
        return false;
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_BACK
            && GetKeyState(VK_MENU) >= 0 && GetKeyState(VK_SHIFT) >= 0) {
        const bool closeSource = GetKeyState(VK_CONTROL) >= 0 && !settings::getKeepSourceWindowOpen();
        const ULONG_PTR generation = viewGeneration;
        const HWND focus = GetFocus();
        HWND root = nullptr;
        if (SUCCEEDED(view->GetWindow(&root)) && isWindowOperational() && shellView == view
                && viewGeneration == generation && root && focus == msg->hwnd
                && (focus == root || IsChild(root, focus))) {
            const LRESULT code = SendMessageW(focus, WM_GETDLGCODE, VK_BACK, reinterpret_cast<LPARAM>(msg));
            if (!isWindowOperational() || shellView != view || viewGeneration != generation || GetFocus() != focus)
                return true;
            if (!(code & (DLGC_HASSETSEL | DLGC_WANTALLKEYS))) {
                CComPtr<IShellItem> parent;
                if (SUCCEEDED(item->GetParent(&parent)) && isWindowOperational()
                        && shellView == view && viewGeneration == generation)
                    openChild(parent, closeSource);
                return true;
            }
        }
    }
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_SPACE && settings::getPeekEnabled()
            && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_SHIFT) >= 0 && GetKeyState(VK_MENU) >= 0) {
        const ULONG_PTR generation = viewGeneration;
        const auto current = [&] { return isWindowOperational() && shellView == view && viewGeneration == generation; };
        HWND root = nullptr;
        const HWND focus = GetFocus();
        const HRESULT foundRoot = view->GetWindow(&root);
        if (!current()) return true;
        if (SUCCEEDED(foundRoot) && root && focus == msg->hwnd
                && (focus == root || IsChild(root, focus))) {
            const LRESULT code = SendMessageW(focus, WM_GETDLGCODE, VK_SPACE, reinterpret_cast<LPARAM>(msg));
            if (!current() || GetFocus() != focus) return true;
            // Native rename editors and controls that own Space keep the key.
            if (!(code & (DLGC_HASSETSEL | DLGC_WANTALLKEYS | DLGC_BUTTON))) {
                // The native selection first: PowerToys configuration files are
                // only read once there is something to preview, and never for repeats.
                CComQIPtr<IFolderView2> folderView(view);
                if (!current()) return true;
                CComPtr<IShellItemArray> selection;
                if (folderView && SUCCEEDED(nativeViewSelection(folderView, FALSE, &selection, current)) && selection) {
                    const auto paths = peekPaths(selection);
                    if (!current() || GetFocus() != focus)
                        return true;
                    if (!paths.empty()) {
                        // Repeats of a press that started a preview are consumed
                        // without re-reading the configuration or starting another.
                        if (msg->lParam & (1L << 30)) return peekPressActive;
                        peekPressActive = false;
                        const PeekInstallation installation = powerToysPeekEnabled() ? findPeek() : PeekInstallation{};
                        if (!current() || GetFocus() != focus) return true;
                        if (!installation.executable.empty()) {
                            peekPressActive = true;
                            const DWORD error = launchPeek(installation, paths);
                            if (error && current()) {
                                const auto detail = getErrorMessage(error);
                                const auto message = formatString(IDS_PEEK_ERROR,
                                    installation.executable.c_str(), error, detail.get());
                                MessageBoxW(hwnd, message.get(), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
                            }
                            return true;
                        }
                    }
                }
            }
        }
        if (!isWindowOperational() || shellView != view || viewGeneration != generation)
            return true;
    }
    if (msg->message == WM_LBUTTONUP || msg->message == WM_KEYUP
            || msg->message == WM_MOUSEWHEEL || msg->message == WM_MOUSEHWHEEL) {
        HWND root = nullptr;
        if (SUCCEEDED(view->GetWindow(&root)) && isWindowOperational() && shellView == view
                && root && (msg->hwnd == root || IsChild(root, msg->hwnd)))
            scheduleStateSave(msg->message == WM_LBUTTONUP);
    }
    if (!isWindowOperational() || shellView != view) return true;
    const bool escape = msg->message == WM_CHAR && msg->wParam == VK_ESCAPE;
    HWND escapeFocus = escape ? GetFocus() : nullptr;
    // Let the Shell handle its own accelerators, including rename cancellation.
    ++navigationDepth;
    const HRESULT translated = view->TranslateAccelerator(msg);
    resumeDeferredClose();
    if (translated == S_OK)
        return true;
    if (!isWindowOperational())
        return true;
    if (escapeFocus && escapeFocus == msg->hwnd && escapeFocus == GetFocus()
            && shellView.p == view.p) {
        HWND viewWindow = nullptr;
        HRESULT hr = view->GetWindow(&viewWindow);
        if (!isWindowOperational())
            return true;
        if (SUCCEEDED(hr) && viewWindow
                && (escapeFocus == viewWindow || IsChild(viewWindow, escapeFocus))) {
            // Preserve text selection and controls that request this key themselves.
            LRESULT dialogCode = SendMessage(escapeFocus, WM_GETDLGCODE,
                VK_ESCAPE, reinterpret_cast<LPARAM>(msg));
            if (!isWindowOperational())
                return true;
            if (!(dialogCode & (DLGC_HASSETSEL | DLGC_WANTALLKEYS))
                    && escapeFocus == GetFocus() && shellView.p == view.p)
                clearSelection();
        }
    }
    // Keep normal dispatch so the Shell can also cancel a pending cut operation.
    return !isWindowOperational();
}

struct ViewGestureTarget {
    CComPtr<IAccessible> accessible;
    CComVariant child;
    LONG role = 0;
};

// Both Shell engines expose logical items through MSAA. Modern Details can
// hit a read-only text cell inside that item rather than the item itself.
static void viewGestureTargetAt(POINT point, ViewGestureTarget &target) {
    if (FAILED(AccessibleObjectFromPoint(point, &target.accessible, &target.child))) return;
    for (unsigned depth = 0; depth < 8 && target.accessible; ++depth) {
        CComVariant role, state;
        target.role = 0;
        if (FAILED(target.accessible->get_accRole(target.child, &role)) || role.vt != VT_I4) break;
        target.role = role.lVal;
        if (target.role != ROLE_SYSTEM_TEXT || FAILED(target.accessible->get_accState(target.child, &state))
                || state.vt != VT_I4
                || (state.lVal != 0 && !(static_cast<DWORD>(state.lVal) & STATE_SYSTEM_READONLY))) break;
        // Simple children belong to this IAccessible; full objects expose a
        // separate parent. List labels can have no state flags; editable text
        // is excluded by the focused control's dialog code before hit testing.
        if (target.child.vt == VT_I4 && target.child.lVal != CHILDID_SELF) {
            target.child = CComVariant(CHILDID_SELF);
        } else {
            CComPtr<IDispatch> parent;
            if (FAILED(target.accessible->get_accParent(&parent)) || !parent) break;
            CComQIPtr<IAccessible> accessible(parent);
            if (!accessible) break;
            target.accessible = accessible;
            target.child = CComVariant(CHILDID_SELF);
        }
    }
}

static LONG gestureRoleAt(POINT point) {
    ViewGestureTarget target;
    viewGestureTargetAt(point, target);
    return target.role;
}

bool FolderWindow::handleResizeMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const bool handled = resizeGesture.handleMessage(message, wParam, lParam);
    if (resizeGesture.takeCompleted()) {
        deferredGestureClick = {};
        finishColumnResize();
        if (isWindowOperational()) persistViewState();
    }
    return handled;
}

void FolderWindow::cancelAltGesture() {
    resizeGesture.cancel();
    if (resizeGesture.takeCompleted() && isWindowOperational()) {
        finishColumnResize();
        persistViewState();
    }
}

bool FolderWindow::handleAltMouseMessage(MSG *msg) {
    if (handleResizeMessage(msg->message, msg->wParam, msg->lParam)) return true;
    // Auto-repeat (bit 30 of lParam: key already down) is not a new Alt press.
    if (msg->message == WM_SYSKEYDOWN && msg->wParam == VK_MENU && !(msg->lParam & (1 << 30)))
        suppressAltUp = false;
    if (msg->message == WM_SYSKEYUP && msg->wParam == VK_MENU && suppressAltUp) {
        suppressAltUp = false;
        return true;
    }
    if ((msg->message != WM_LBUTTONDOWN && msg->message != WM_LBUTTONDBLCLK
            && msg->message != WM_RBUTTONDOWN
            && msg->message != WM_RBUTTONDBLCLK
            && msg->message != WM_MBUTTONDOWN && msg->message != WM_MBUTTONDBLCLK)
            || GetCapture() || !shellView)
        return false;
    // The modern renderer recognizes double clicks without CS_DBLCLKS. Its
    // first press was consumed by the window gesture, so recognize the second
    // using Windows' time/position settings and invoke the public Shell verb.
    const bool rendererDoubleClick = msg->message == WM_LBUTTONDOWN && deferredGestureClick.hwnd == msg->hwnd
            && deferredGestureClick.hwnd && deferredGestureGeneration == viewGeneration
            && msg->time - deferredGestureClick.time <= GetDoubleClickTime()
            && std::abs(msg->pt.x - deferredGestureClick.pt.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2
            && std::abs(msg->pt.y - deferredGestureClick.pt.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2
            && (currentGestureModifiers() & GestureAlt)
            && !(GetClassLongPtrW(msg->hwnd, GCL_STYLE) & CS_DBLCLKS);
    // A window gesture consumes its first click. Prepare that click's selection
    // through MSAA before the native double-click. Replaying a button-down while
    // the second press is physically held can start an unintended Shell drag.
    if (msg->message == WM_LBUTTONDBLCLK || rendererDoubleClick) {
        const MSG click = deferredGestureClick;
        deferredGestureClick = {};
        const CComPtr<IShellView> view = shellView;
        HWND root = nullptr;
        const ULONG_PTR generation = viewGeneration;
        if (click.hwnd == msg->hwnd && click.hwnd && deferredGestureGeneration == generation
                && msg->time - click.time <= GetDoubleClickTime()
                && SUCCEEDED(view->GetWindow(&root)) && root
                && (click.hwnd == root || IsChild(root, click.hwnd))) {
            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
            ViewGestureTarget target;
            viewGestureTargetAt(click.pt, target);
            const auto &accessible = target.accessible;
            const auto &child = target.child;
            CComVariant state;
            if (accessible && target.role == ROLE_SYSTEM_LISTITEM) {
                LONG flags = SELFLAG_NONE;
                if (rendererDoubleClick && (click.wParam & MK_SHIFT)) {
                    flags = SELFLAG_ADDSELECTION;
                } else if (click.wParam & MK_CONTROL) {
                    if (FAILED(accessible->get_accState(child, &state)) || state.vt != VT_I4) return false;
                    flags |= state.lVal & STATE_SYSTEM_SELECTED ? SELFLAG_REMOVESELECTION : SELFLAG_ADDSELECTION;
                } else {
                    flags |= SELFLAG_TAKESELECTION;
                }
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                CComQIPtr<IFolderView2> folderView(view);
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                if (!folderView) return false;
                const bool extendSelection = rendererDoubleClick && (click.wParam & MK_SHIFT);
                int rangeAnchor = -1;
                if (extendSelection) {
                    if (folderView->GetSelectionMarkedItem(&rangeAnchor) != S_OK || rangeAnchor < 0)
                        folderView->GetFocusedItem(&rangeAnchor);
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                // TAKEFOCUS in the MSAA proxy can synthesize Alt restoration
                // after the real key release. Resolve the pointed item through
                // a selection-only operation, then focus its public Shell index.
                std::vector<CComHeapPtr<ITEMID_CHILD>> previousSelection;
                const bool preserveSelection = (click.wParam & MK_CONTROL)
                    || (rendererDoubleClick && (click.wParam & MK_SHIFT));
                if (click.wParam & MK_CONTROL) {
                    int count = 0;
                    checkHR(folderView->ItemCount(SVGIO_SELECTION, &count));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    if (count < 0) return false;
                    previousSelection.resize(static_cast<size_t>(count));
                    CComPtr<IEnumIDList> selectedItems;
                    checkHR(folderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selectedItems)));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    for (auto &id : previousSelection) {
                        if (!selectedItems) return false;
                        const HRESULT hr = selectedItems->Next(1, &id, nullptr);
                        if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                        if (hr != S_OK) { checkHR(hr); return false; }
                    }
                }
                checkHR(accessible->accSelect(SELFLAG_TAKESELECTION, child));
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                int index = -1;
                const HRESULT found = folderView->GetSelectedItem(-1, &index);
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                CComHeapPtr<ITEMID_CHILD> pointed;
                if (found == S_OK && index >= 0) {
                    checkHR(folderView->Item(index, &pointed));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                if (preserveSelection) {
                    checkHR(selectionClear.clear(view, [&]() {
                        return isWindowOperational() && shellView == view && viewGeneration == generation;
                    }));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    for (const auto &id : previousSelection) {
                        checkHR(view->SelectItem(id, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                        if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    }
                    if (extendSelection && index >= 0) {
                        // Modern MSAA rejects EXTENDSELECTION. Apply the same
                        // view-order range through the public folder interface.
                        const int anchor = rangeAnchor >= 0 ? rangeAnchor : index;
                        for (int rangeIndex = (std::min)(anchor, index); rangeIndex <= (std::max)(anchor, index); ++rangeIndex) {
                            checkHR(folderView->SelectItem(rangeIndex, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                        }
                    } else checkHR(accessible->accSelect(flags, child));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                if (pointed) {
                    DWORD selection = 0;
                    checkHR(folderView->GetSelectionState(pointed, &selection));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    const UINT mark = rendererDoubleClick ? 0 : SVSI_SELECTIONMARK;
                    checkHR(view->SelectItem(pointed, (selection & SVSI_SELECT) | SVSI_FOCUSED | mark | SVSI_NOTAKEFOCUS));
                }
                if (rendererDoubleClick && isWindowOperational() && shellView == view && viewGeneration == generation) {
                    ++navigationDepth;
                    const HRESULT invoked = folderView->InvokeVerbOnSelection("properties");
                    resumeDeferredClose();
                    // A native Ctrl-double-click on a selected item invokes
                    // its first-click selection, then restores that item.
                    if (flags == SELFLAG_REMOVESELECTION && pointed && isWindowOperational()
                            && shellView == view && viewGeneration == generation)
                        checkHR(view->SelectItem(pointed, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                    checkHR(invoked);
                    return true;
                }
            }
            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
        }
        return false;
    }
    deferredGestureClick = {};
    const GestureButton button = msg->message == WM_LBUTTONDOWN ? GestureButton::Left
        : msg->message == WM_MBUTTONDOWN || msg->message == WM_MBUTTONDBLCLK ? GestureButton::Middle : GestureButton::Right;
    const unsigned modifiers = currentGestureModifiers();
    const WindowShortcuts bindings = settings::getWindowShortcuts();
    const bool moving = settings::windowShortcutApplies(bindings.move, button, modifiers, hwnd);
    if (!moving && !settings::windowShortcutApplies(bindings.resize, button, modifiers, hwnd)) return false;
    const HWND owner = hwnd;
    const CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && hwnd == owner && shellView == view
            && viewGeneration == generation;
    };
    HWND root = nullptr;
    if (FAILED(view->GetWindow(&root)) || !current() || !root
            || (msg->hwnd != root && !IsChild(root, msg->hwnd)))
        return false;
    const HWND focus = GetFocus();
    if (focus && (focus == root || IsChild(root, focus))) {
        const LRESULT code = SendMessage(focus, WM_GETDLGCODE, 0, 0);
        if (!current()) return true;
        if (code & DLGC_HASSETSEL) return false; // Native rename/text editing.
    }
    const POINT point = msg->pt;
    const HWND hitWindow = WindowFromPoint(point);
    if (hitWindow != root && !IsChild(root, hitWindow)) return false;
    const LONG role = gestureRoleAt(point);
    debugPrintf(L"ALT HWND=%p message=0x%04X role=%ld\n", owner, msg->message, role);
    if (!current()) return true;
    // Unknown roles, headers, scrollbars, rename edits and embedded buttons keep
    // their native input. Only list contents participate in FileSpacer gestures.
    if (role != ROLE_SYSTEM_LIST && role != ROLE_SYSTEM_LISTITEM) return false;
    SetForegroundWindow(owner);
    if (!current()) return true;
    if (button == GestureButton::Left) {
        deferredGestureClick = *msg;
        deferredGestureGeneration = generation;
    }
    if (!moving || button != GestureButton::Left) {
        const bool begun = resizeGesture.begin(owner, owner, point, button, moving);
        if (begun && !moving) {
            resizingColumns = true;
            if (columnLayout) columnLayout->suspend(settings::getDetailsColumnsDeferred());
        }
        return begun;
    }
    suppressAltUp = (modifiers & GestureAlt) != 0;
    if (DragDetect(owner, point) && current()) {
        deferredGestureClick = {};
        if (GetCapture() == owner) ReleaseCapture();
        SendMessage(owner, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    }
    return true;
}

void FolderWindow::onCreate() {
    ItemWindow::onCreate();
    const DWORD historyLimit = settings::getSearchHistoryLimit();
    searchBox.setDropdownEnabled(settings::getEverythingEnabled() || historyLimit != 0, settings::getEverythingPath().get());
    loadSearchHistory();
    quickAccessIconThread.Attach(new QuickAccessIconThread(hwnd, GetSystemMetrics(SM_CXSMICON)));
    quickAccessIconThread->start();
    placeSearchBox();
    CComHeapPtr<wchar_t> folderName;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &folderName)))
        searchBox.setFolderName(folderName);

    // Keep the status bar available even when initially hidden.
    bottomStatusBar = checkLE(CreateWindowEx(
        0,
        STATUSCLASSNAME,
        nullptr,
        WS_CHILD | (settings::getStatusTextEnabled() ? WS_VISIBLE : 0)
            | CCS_NOPARENTALIGN | CCS_NORESIZE,
        0, 0, 0, 0,
        hwnd,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr));

    int bottomStatusHeight = 0;

    if (hasStatusText()) {
        bottomStatusHeight = scaleDPI(BOTTOM_STATUS_HEIGHT_DIP);

        RECT client = clientRect(hwnd);

        SetWindowPos(
            bottomStatusBar,
            nullptr,
            0,
            client.bottom - bottomStatusHeight,
            rectWidth(client),
            bottomStatusHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);

        // One borderless text part for now.
        // Later we can reserve a right-hand part for the view buttons.
        int parts[] = {-1};
        SendMessage(bottomStatusBar, SB_SETPARTS, 1, (LPARAM)parts);
        SendMessage(bottomStatusBar, SB_SETTEXTW, SBT_NOBORDERS, (LPARAM)L"");
    }

    RECT browserRect = windowBody();
    browserRect.bottom -= bottomStatusHeight;
    browserRect.bottom += CAPTION_HEIGHT; // initial rect is wrong

    FOLDERSETTINGS initial = folderSettings();
    SFGAOF attributes = SFGAO_FILESYSTEM | SFGAO_STREAM;
    hybridViewEligible = SUCCEEDED(item->GetAttributes(attributes, &attributes))
        && (attributes & SFGAO_FILESYSTEM) && !(attributes & SFGAO_STREAM);
    if (!isWindowOperational()) return;
    if (hybridViewEligible) {
        CComHeapPtr<ITEMIDLIST> id;
        hybridViewEligible = SUCCEEDED(SHGetIDListFromObject(item, &id)) && id && !ILIsEmpty(id);
    }
    if (!isWindowOperational()) return;
    loadColumnFill();
    if (hybridViewEligible && loadFolderState()
            && (getSavedFolderState().view.present & FolderViewSettings::Mode)) {
        const auto mode = getSavedFolderState().view.mode;
        if (mode == FVM_AUTO || (mode >= FVM_FIRST && mode <= FVM_LAST))
            initial.ViewMode = static_cast<UINT>(mode);
    }
    if (!isWindowOperational()) return;
    viewProfile = profileForMode(initial.ViewMode);
    if (!checkHR(viewHost.initialize(hwnd, browserRect, initial,
            static_cast<IServiceProvider *>(this), this, viewProfile)))
        return;
    CComPtr<IFolderFilter> filter;
    auto concreteFilter = new FolderNameFilter(L"", hiddenItemIDs, _countof(hiddenItemIDs));
    filter.Attach(concreteFilter);
    HRESULT filterResult = concreteFilter->initialize();
    if (SUCCEEDED(filterResult))
        filterResult = viewHost.setFilter(filter);
    if (checkHR(filterResult))
        nameFilter = filter;
    // Start the first navigation only after the window has been shown.
    initialNavigationPending = true;
    searchBox.setLiveSearch(settings::getLiveNameSearch());
}

FolderWindow::QuickAccessIconThread::QuickAccessIconThread(HWND ownerWindow, int imageSize)
        : owner(ownerWindow), iconSize(imageSize) {}

FolderWindow::QuickAccessIconThread::~QuickAccessIconThread() {
    if (bitmap)
        DeleteObject(bitmap);
}

FolderWindow::StorageWaitThread::StorageWaitThread(HWND ownerWindow, HANDLE mutexHandle, ULONG_PTR waitSerial)
        : owner(ownerWindow), mutex(mutexHandle), serial(waitSerial) {}

FolderWindow::StorageWaitThread::~StorageWaitThread() {
    checkLE(CloseHandle(mutex));
}

void FolderWindow::StorageWaitThread::run() {
    // The stop event comes first: when both are signaled the wait reports the
    // lowest index (WaitForMultipleObjects, Learn), so a stopped wait never
    // takes the mutex. A satisfied (or abandoned) mutex wait grants ownership
    // to this thread: give it back at once, this thread only signals.
    HANDLE handles[] = {stopEvent, mutex};
    const DWORD waited = WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE);
    if (waited != WAIT_OBJECT_0 + 1 && waited != WAIT_ABANDONED_0 + 1) return;
    checkLE(ReleaseMutex(mutex));
    AcquireSRWLockShared(&stopLock);
    if (!isStopped()) PostMessage(owner, MSG_STORAGE_AVAILABLE, serial, 0);
    ReleaseSRWLockShared(&stopLock);
}

// Storage was unavailable for a provisional window. The named mutex another
// process holds is waited on (its release is the public event); only
// SQLITE_BUSY, which has no Windows event, is retried on a growing, bounded
// delay. Any other failure waits for the next save request, as before.
void FolderWindow::scheduleStorageRetry() {
    if (storageWait) return; // The pending wait already covers this window.
    if (void *held = folderStateBlockingMutex()) {
        HANDLE duplicate = nullptr;
        if (checkLE(DuplicateHandle(GetCurrentProcess(), held, GetCurrentProcess(), &duplicate,
                0, FALSE, DUPLICATE_SAME_ACCESS))) {
            storageWait.Attach(new StorageWaitThread(hwnd, duplicate, ++storageWaitSerial));
            storageWait->start();
            storageRetryDelay = 0;
            return;
        }
        // A failed duplication falls back to the bounded delay below.
    } else if (!folderStateBusy()) {
        storageRetryDelay = 0;
        // Storage may have been reopened meanwhile (another window succeeded
        // while the error was reported): continue from the queue, not from here.
        if (folderStateStore()) checkLE(PostMessage(hwnd, MSG_STORAGE_AVAILABLE, ++storageWaitSerial, 0));
        return;
    }
    storageRetryDelay = storageRetryDelay ? (std::min)(storageRetryDelay * 2, 16000u) : 1000u;
    SetTimer(hwnd, TIMER_SAVE_STATE, storageRetryDelay, nullptr);
}

void FolderWindow::cancelStorageWait() {
    if (storageWait) {
        storageWait->stop();
        storageWait.Release();
    }
    storageRetryDelay = 0;
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
    CComPtr<IShellItemImageFactory> factory;
    HBITMAP image = nullptr;
    if (FAILED(SHCreateItemFromParsingName(
            L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}", nullptr,
            IID_PPV_ARGS(&factory)))
            || FAILED(factory->GetImage({iconSize, iconSize}, SIIGBF_ICONONLY, &image)))
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

void FolderWindow::addToolbarButtons(HWND tb) {
    TBBUTTON buttons[] = {
        makeToolbarButton(MDL2_REFRESH, IDM_REFRESH, 0),
        {scaleDPI(188), IDC_NAME_SEARCH, TBSTATE_ENABLED, BTNS_SEP, {}, 0, 0},
        makeToolbarButton(ICON_VIEW_MODE, IDM_VIEW_MENU, BTNS_DROPDOWN),
    };
    SendMessage(tb, TB_ADDBUTTONS, _countof(buttons), (LPARAM)buttons);
    ItemWindow::addToolbarButtons(tb);
    searchBox.create(tb, hwnd);
}

int FolderWindow::getToolbarTooltip(WORD command) {
    switch (command) {
        case IDM_REFRESH:
            return IDS_REFRESH_COMMAND;
        case IDM_QUICK_ACCESS:
            return IDS_QUICK_ACCESS;
        case IDM_VIEW_MENU:
            return IDS_VIEW_COMMAND;
    }
    return ItemWindow::getToolbarTooltip(command);
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

void FolderWindow::scheduleStateSave(bool icons) {
    if (restoringState || !viewReady || viewMenuPreview) return;
    if (initialStateApplied) {
        CComPtr<ItemWindow> keepAlive(this);
        CComPtr<IFolderView> view;
        const ULONG_PTR generation = viewGeneration;
        if (FAILED(viewHost.currentView(IID_PPV_ARGS(&view))) || !isWindowOperational()
                || generation != viewGeneration) return;
        const bool spatial = spatialView(view);
        if (!isWindowOperational() || generation != viewGeneration) return;
        if (spatial != spatialLayout) {
            spatialLayout = spatial;
            // Changing mode can discard the Shell's free positions even when
            // the provider stays the same. Restore once, before any capture.
            if (spatial) {
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
    return hybridViewEligible && (mode == FVM_LIST || (mode == FVM_DETAILS && !columnFillActive()))
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
            || viewMenuDepth || viewMenuPreview || viewMenuRestore || provisionalBaseline) return;
    GUITHREADINFO gui = {sizeof(gui)};
    if (!GetGUIThreadInfo(GetCurrentThreadId(), &gui)
            || (gui.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) return;
    CComPtr<ItemWindow> keepAlive(this);
    viewProfilePending = false;
    ++navigationDepth;
    struct Navigation {
        FolderWindow &owner;
        ~Navigation() { owner.resumeDeferredClose(); }
    } navigation{*this};
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() { return isWindowOperational() && shellView == source && viewGeneration == generation; };
    CComPtr<IFolderView2> view;
    if (FAILED(viewHost.currentView(IID_PPV_ARGS(&view))) || !current()) return;
    auto state = std::make_unique<ShellViewState>();
    state->view = captureOptions(view, getSavedFolderState().view);
    if (columnFillSet) {
        state->view.present |= FolderViewSettings::ColumnFill;
        state->view.fillColumns = fillColumns;
    }
    if (!current() || !(state->view.present & FolderViewSettings::Mode)) return;
    const auto desired = viewMenuRequested
        ? static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING)
        : profileForMode(static_cast<UINT>(state->view.mode));
    if (desired == viewProfile) return;
    // Reuse the complete capture path, including protection of positions that
    // could not yet be restored. A profile change must not trust a partial grid.
    viewStateDirty(FolderState::Icons);
    persistViewState();
    if (!current() || deferredCloseMessage) return;
    state->icons = getSavedFolderState().icons;
    if (FAILED(captureReopenSelection(source, current, state->selection)) || !current() || deferredCloseMessage) return;
    HWND body = nullptr;
    source->GetWindow(&body);
    if (!current()) return;
    const HWND focus = GetFocus();
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
    shellView.Release();
    checkHR(viewHost.destroy());
    if (!isWindowOperational() || deferredCloseMessage) return;
    viewProfile = desired;
    RECT bounds = windowBody();
    if (hasStatusText()) bounds.bottom -= scaleDPI(BOTTOM_STATUS_HEIGHT_DIP);
    FOLDERSETTINGS initial = {static_cast<UINT>(storedViewState->view.mode), storedViewState->view.flags};
    HRESULT result = viewHost.initialize(hwnd, bounds, initial, static_cast<IServiceProvider *>(this), this, viewProfile);
    if (SUCCEEDED(result) && isWindowOperational()) result = viewHost.setFilter(nameFilter);
    if (SUCCEEDED(result) && isWindowOperational()) result = browseToItem();
    if (!isWindowOperational()) return;
    if (SUCCEEDED(result)) result = viewHost.lockNavigation(true);
    if (!checkHR(result)) {
        viewProfileChanging = false;
        restoringState = false;
        viewMenuRequested = false;
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
    const auto &preview = viewMenuPreview ? viewMenuPreview : viewMenuRestore;
    if (!isWindowOperational() && preview && !preview->restoreFailed) closingScroll = preview->initial.scroll;
    if (provisionalBaseline) {
        if (!recoverProvisionalView()) return captured;
        // Closing deliberately merges without replaying the native view. Do
        // not immediately recapture its fallback over the merged reference.
        return captured | FolderState::View | FolderState::Icons;
    }
    if (!viewHost || !viewReady || !initialStateApplied || restoringState || viewMenuPreview || viewProfileChanging
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
    }
    FolderViewSettings options = captureOptions(view, getSavedFolderState().view);
    if (columnFillSet) {
        options.present |= FolderViewSettings::ColumnFill;
        options.fillColumns = fillColumns;
    }
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
    if (session && !session->restoreFailed) return session->initial.selection;
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

void FolderWindow::onDestroy() {
    if (columnLayout) columnLayout->stop();
    columnLayout.reset();
    viewMenuRequested = false;
    viewProfilePosted = false;
    watchProvisionalInput(false);
    if (viewMenuPreview) restoringState = true;
    viewMenuPreview.reset();
    viewMenuRestore.reset();
    resizeGesture.stop();
    checkHR(viewHost.stopObserving());
    KillTimer(hwnd, TIMER_SAVE_STATE);
    cancelStorageWait();
    providerSyncPending = false;
    viewStateDirty(FolderState::Icons);
    persistViewState();
    saveSearchHistory();
    if (!closingScroll.empty() && !getFolderStateKey().empty()
            && !getSavedFolderState().firstFailedAt) {
        auto store = folderStateStore();
        if (!store || !store->saveViewScroll(getFolderStateKey(), closingScroll)) stateStorageError();
    }
    searchBox.destroy();
    if (quickAccessIconThread)
        quickAccessIconThread->stop();
    if (quickAccessImages) {
        SendMessage(getQuickAccessToolbar(), TB_SETIMAGELIST, 0, 0);
        ImageList_Destroy(quickAccessImages);
        quickAccessImages = nullptr;
    }
    // Release cached Shell interfaces even if an external COM client keeps
    // the host object alive after its native window has closed.
    selectionClear.reset();
    shellView.Release();
    selected.Release();
    viewReady = false;
    ItemWindow::onDestroy();
    viewHost.destroy();
    nameFilter.Release();
}

bool FolderWindow::onCommand(WORD command) {
    if (command == IDM_FILL_COLUMNS) { toggleFillColumns(); return true; }
    if (provisionalBaseline && (command == IDM_RESTORE_LAYOUT || command == IDM_REPLACE_LAYOUT
            || command == IDM_TOGGLE_STATE_LOCK)) {
        if (provisionalInputBlocked || provisionalCaptureBusy) return true;
        // Lock capture temporarily clears the cached view before invoking our
        // virtual capture. Reconcile first, while the SQL reference is intact.
        persistViewState();
        if (!isWindowOperational() || provisionalBaseline) return true;
    }
    switch (command) {
        case IDM_NEW_WINDOW: {
            CComPtr<ItemWindow> keepAlive(this);
            auto path = settings::getStartingFolder();
            CComPtr<IShellItem> target = itemFromPath(path.get());
            if (target && isWindowOperational()) openChild(target, false);
            return true;
        }
        case IDM_RESTORE_LAYOUT:
            restoreReferenceLayout();
            return true;
        case IDM_REPLACE_LAYOUT: {
            CComPtr<ItemWindow> keepAlive(this);
            if (MessageBoxW(hwnd, getString(IDS_TITLE_REPLACE_CONFIRM), getString(IDS_APP_NAME),
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES && isWindowOperational())
                toggleStateLock(true);
            return true;
        }
        case IDM_FIT_CONTENT:
            fitWindowToContents();
            return true;
        case IDM_QUICK_ACCESS:
            openQuickAccessMenu(clientToScreen(hwnd, {0, 0}));
            return true;
        case IDM_NEW_FOLDER:
            newItem(CMDSTR_NEWFOLDERA);
            return true;
    }
    return ItemWindow::onCommand(command);
}

void FolderWindow::changeViewMode(int index, int steps) {
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
        {FVM_ICON, scaleDPI(256)}, {FVM_ICON, scaleDPI(96)}, {FVM_ICON, scaleDPI(48)},
        {FVM_SMALLICON, scaleDPI(16)}, {FVM_LIST, -1}, {FVM_DETAILS, -1}, {FVM_TILE, -1}, {FVM_CONTENT, -1}
    };
    FOLDERVIEWMODE mode;
    int size;
    if (!checkHR(view->GetViewModeAndIconSize(&mode, &size)) || !current()) return;
    if (index < 0) {
        index = 5; // Unknown provider modes start from Details.
        for (int i = 0; i < static_cast<int>(_countof(modes)); ++i) {
            if (mode != modes[i].mode) continue;
            index = i;
            if (mode != FVM_ICON || size >= modes[i].size) break;
        }
        // Nonstandard icon sizes move to the next preset in the wheel direction.
        if (mode == FVM_ICON && steps > 0 && size < modes[index].size) ++index;
        if (mode == FVM_ICON && steps < 0 && size > modes[index].size) --index;
        index = (std::max)(0, (std::min)(static_cast<int>(_countof(modes)) - 1, index - steps));
    }
    if (index < 0 || index >= static_cast<int>(_countof(modes))) return;
    const Mode target = modes[index];
    if (mode == target.mode && (target.size < 0 || size == target.size)) return;
    viewStateDirty(FolderState::Icons);
    persistViewState();
    if (!current()) return;
    ++navigationDepth;
    const HRESULT result = view->SetViewModeAndIconSize(target.mode, target.size);
    resumeDeferredClose();
    if (current() && checkHR(result)) {
        // Some native mode changes discard free positions without changing
        // the spatial flag. Replay the saved positions before another capture.
        restoreIconsPending = searchText.empty();
        restoreIconsUnpruned = restoreIconsPending;
        finishViewInitialization();
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
    const LONG width = (std::min<LONG>)(rectWidth(monitor.rcWork),
        (std::max<LONG>)(GetSystemMetricsForDpi(SM_CXMINTRACK, dpi),
        content.cx + rectWidth(windowRect) - rectWidth(viewRect) + scaleDPI(12)));
    const LONG height = (std::min<LONG>)(rectHeight(monitor.rcWork),
        (std::max<LONG>)(GetSystemMetricsForDpi(SM_CYMINTRACK, dpi),
        content.cy + rectHeight(windowRect) - rectHeight(viewRect) + scaleDPI(12)));
    const LONG x = (std::max<LONG>)(monitor.rcWork.left, (std::min<LONG>)(windowRect.left, monitor.rcWork.right - width));
    const LONG y = (std::max<LONG>)(monitor.rcWork.top, (std::min<LONG>)(windowRect.top, monitor.rcWork.bottom - height));
    ShowWindow(hwnd, SW_RESTORE);
    if (current()) SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void FolderWindow::placeSearchBox() {
    HWND toolbar = getCommandToolbar();
    const bool pathEnabled = settings::getPathBarEnabled();
    RECT rect = {};
    const LRESULT index = SendMessage(toolbar, TB_COMMANDTOINDEX, IDC_NAME_SEARCH, 0);
    if (index < 0 || !SendMessage(toolbar, TB_GETITEMRECT, index, (LPARAM)&rect))
        return;
    SIZE ideal = {};
    if (SendMessage(toolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal)) {
        const int otherButtonsWidth = ideal.cx - rectWidth(rect);
        const int available = (std::max)(0,
            (int)clientSize(GetParent(toolbar)).cx - otherButtonsWidth
                - getNavigationToolbarWidth() - scaleDPI(pathEnabled ? 16 : 8));
        const int minimum = (std::min)(scaleDPI(80), available);
        const int maximum = (std::max)(minimum, available - scaleDPI(64));
        const int width = pathEnabled ? (std::max)(minimum,
            (std::min)(maximum, available * searchShare / 1000)) : available;
        if (rectWidth(rect) != width) {
            TBBUTTONINFO info = {sizeof(info), TBIF_SIZE};
            info.cx = static_cast<WORD>(width);
            SendMessage(toolbar, TB_SETBUTTONINFO, IDC_NAME_SEARCH, (LPARAM)&info);
            SendMessage(toolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);
            SetWindowPos(toolbar, nullptr, 0, 0, ideal.cx, clientSize(toolbar).cy,
                SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
            ItemWindow::onSize(clientSize(hwnd));
            SendMessage(toolbar, TB_GETITEMRECT, index, (LPARAM)&rect);
        }
    }
    searchBox.place(rect, pathEnabled);
}

void FolderWindow::applyNameFilter(const wchar_t *text) {
    if (provisionalInputBlocked || provisionalCaptureBusy) return;
    if (searchText == text) {
        return;
    }
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IFolderFilter> filter = nameFilter;
    CComPtr<IShellView> view = shellView;
    if (!isWindowOperational())
        return;
    if (provisionalBaseline && provisionalReady && searchText.empty()
            && !captureProvisionalCurrent(true)) return;
    HRESULT hr = filter ? S_OK : E_NOINTERFACE;
    if (SUCCEEDED(hr)) {
        if (searchText.empty() && *text)
            persistViewState(); // Save the complete view before hiding any items.
        if (!isWindowOperational()) return;
        const std::wstring previousText = searchText;
        auto activeFilter = static_cast<FolderNameFilter *>(filter.p);
        activeFilter->setQuery(text);
        searchText = text;
        // Keep the interface already retained by DefView and refresh natively.
        restoreIconsPending = searchText.empty();
        hr = view ? refreshEnumeration() : S_OK;
        if (FAILED(hr) && isWindowOperational()) {
            activeFilter->setQuery(previousText.c_str());
            searchText = previousText;
            if (view)
                refreshEnumeration();
        }
    }
    if (FAILED(hr) && isWindowOperational()) {
        auto error = getErrorMessage(hr);
        auto message = formatString(IDS_SEARCH_ERROR, error.get());
        MessageBox(hwnd, message.get(), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
    }
}

LRESULT FolderWindow::onNotify(NMHDR *notification) {
    if (notification->code == SearchBox::DROPDOWN) {
        openSearchMenu();
        return 0;
    }
    if (notification->code == SearchBox::SELECT) {
        auto search = reinterpret_cast<SearchBox::Notification *>(notification);
        selectSearchSuggestion(search->command, search->text);
        return 0;
    }
    if (notification->code == SearchBox::RESIZE) {
        if (!settings::getPathBarEnabled())
            return 0;
        const HWND toolbar = getCommandToolbar();
        const LRESULT index = SendMessage(toolbar, TB_COMMANDTOINDEX, IDC_NAME_SEARCH, 0);
        RECT rect = {};
        SIZE ideal = {};
        if (index >= 0 && SendMessage(toolbar, TB_GETITEMRECT, index, (LPARAM)&rect)
                && SendMessage(toolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal)) {
            const int available = (std::max)(0, (int)clientSize(GetParent(toolbar)).cx
                - (int)ideal.cx + rectWidth(rect) - getNavigationToolbarWidth() - scaleDPI(16));
            if (available) {
                const int minimum = (std::min)(scaleDPI(80), available);
                const int maximum = (std::max)(minimum, available - scaleDPI(64));
                const int width = (std::max)(minimum, (std::min)(maximum,
                    reinterpret_cast<SearchBox::Notification *>(notification)->width));
                searchShare = width * 1000 / available;
                placeSearchBox();
            }
        }
        return 0;
    }
    if (notification->code == SearchBox::FOCUS_VIEW) {
        if (shellView)
            shellView->UIActivate(SVUIA_ACTIVATE_FOCUS);
        return 0;
    }
    if (notification->code == SearchBox::APPLY) {
        CComPtr<ItemWindow> keepAlive(this);
        auto search = reinterpret_cast<SearchBox::Notification *>(notification);
        const std::wstring query = search->text;
        const bool explicitSubmit = search->explicitSubmit;
        applyNameFilter(search->text);
        if (isWindowOperational() && explicitSubmit && searchText == query)
            rememberSearch(query);
        return 0;
    }
    return ItemWindow::onNotify(notification);
}

void FolderWindow::rememberSearch(const std::wstring &query) {
    const DWORD limit = settings::getSearchHistoryLimit();
    if (!limit || query.find_first_not_of(L" \t\r\n") == std::wstring::npos) return;
    searchHistory.erase(std::remove(searchHistory.begin(), searchHistory.end(), query), searchHistory.end());
    searchHistory.insert(searchHistory.begin(), query);
    if (searchHistory.size() > limit) searchHistory.resize(limit);
    pendingSearches.erase(std::remove(pendingSearches.begin(), pendingSearches.end(), query), pendingSearches.end());
    pendingSearches.insert(pendingSearches.begin(), query);
    if (pendingSearches.size() > limit) pendingSearches.resize(limit);
    searchHistoryDirty = true;
    scheduleStateSave();
}

bool FolderWindow::loadSearchHistory() {
    const DWORD limit = settings::getSearchHistoryLimit();
    if (searchHistoryLoaded || !limit || getFolderStateKey().empty()) return true;
    auto store = folderStateStore();
    std::vector<std::string> queries;
    if (!store || !store->readSearchHistory(getFolderStateKey(), limit, &queries)) {
        stateStorageError();
        return false;
    }
    auto history = pendingSearches;
    for (const auto &query : queries) {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, query.c_str(), -1, nullptr, 0);
        if (length <= 1) continue;
        std::wstring value(static_cast<size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, query.c_str(), -1, &value[0], length)) {
            value.pop_back();
            if (std::find(history.begin(), history.end(), value) == history.end())
                history.push_back(std::move(value));
        }
    }
    if (history.size() > limit) history.resize(limit);
    searchHistory.swap(history);
    searchHistoryLoaded = true;
    return true;
}

void FolderWindow::saveSearchHistory() {
    if (!searchHistoryDirty || getFolderStateKey().empty()) return;
    auto store = folderStateStore();
    if (!store) { stateStorageError(); return; }
    std::vector<std::string> queries;
    for (const auto &query : pendingSearches) queries.push_back(toUtf8(query.c_str()));
    bool stateMissing = false;
    if (store->saveSearchHistory(getFolderStateKey(), queries, settings::getSearchHistoryLimit(), true,
            &stateMissing)) {
        // No folder state row yet (its access record is still pending): the
        // queries stay pending for the save that follows that record.
        if (stateMissing) return;
        pendingSearches.clear();
        searchHistoryDirty = false;
        searchHistoryLoaded = false;
        loadSearchHistory();
    } else stateStorageError();
}

void FolderWindow::openSearchMenu() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational()) return;
    loadSearchHistory();
    if (!isWindowOperational()) return;
    const DWORD limit = settings::getSearchHistoryLimit();
    if (searchHistory.size() > limit) searchHistory.resize(limit);
    HMENU menu = createSearchMenu(settings::getEverythingEnabled(), searchHistory);
    if (!menu) return;
    searchBox.showDropdown(menu);
    DestroyMenu(menu);
}

void FolderWindow::selectSearchSuggestion(UINT command, const wchar_t *text) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational()) return;
    const std::wstring query = text;
    if (command >= 2 && settings::getSearchHistoryLimit()) {
        searchBox.setText(query.c_str());
        applyNameFilter(query.c_str());
        if (isWindowOperational() && searchText == query) rememberSearch(query);
    } else if (command == 1 && settings::getEverythingEnabled()) {
        auto manualPath = settings::getEverythingPath();
        const std::wstring executable = manualPath[0] ? std::wstring(manualPath.get()) : findEverything();
        std::wstring folder;
        DWORD error = ERROR_SUCCESS;
        if (settings::getEverythingScope() == settings::EverythingScope::Folder) {
            CComHeapPtr<wchar_t> path;
            const HRESULT result = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (!isWindowOperational()) return;
            if (FAILED(result)) error = ERROR_PATH_NOT_FOUND;
            else folder = path;
        }
        if (!error) error = launchEverything(executable, everythingArguments(query, folder));
        if (error) {
            auto detail = getErrorMessage(error);
            auto message = formatString(IDS_EVERYTHING_ERROR, executable.c_str(), error, detail.get());
            MessageBoxW(hwnd, message.get(), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
        } else if (isWindowOperational()) {
            rememberSearch(query);
            searchBox.setText(L"");
            applyNameFilter(L"");
        }
    }
}

LRESULT FolderWindow::onDropdown(int command, POINT pos) {
    switch (command) {
        case IDM_QUICK_ACCESS:
            openQuickAccessMenu(pos);
            return TBDDRET_DEFAULT;
        case IDM_VIEW_MENU:
            openViewMenu(pos);
            return TBDDRET_DEFAULT;
    }
    return ItemWindow::onDropdown(command, pos);
}

void FolderWindow::onActivate(WORD state, HWND prevWindow) {
    CComPtr<ItemWindow> keepAlive(this); // Shell calls can reenter dispatch
    // WA_CLICKACTIVE also reports a caption click, whose non-client button
    // message never reaches the message loop after this notification: only a
    // click inside the view can be the activation click absorbed later.
    activationClickPending = state == WA_CLICKACTIVE && cursorOverShellView();
    if (state == WA_INACTIVE) {
        searchBox.hideDropdown();
        cancelAltGesture();
        viewStateDirty(FolderState::Icons);
    }
    ItemWindow::onActivate(state, prevWindow);
    if (state != WA_INACTIVE) {
        // override behavior causing sort columns to be focused when shift is held
        activateOnShiftRelease = GetKeyState(VK_SHIFT) < 0;
        if (shellView)
            checkHR(shellView->UIActivate(SVUIA_ACTIVATE_FOCUS));
        if (updateSelectionOnActivate) {
            updateSelection(); // no delay
            updateSelectionOnActivate = false;
        }
    }
}

void FolderWindow::onSize(SIZE size) {
    if (viewHost && columnLayout && resizingColumns && settings::getDetailsColumnsDeferred()) {
        // Keep the whole client layout stable; Shell-only deferral still redraws the toolbars.
        columnBoundsPending = true;
        return;
    }
    columnBoundsPending = false;
    ItemWindow::onSize(size);
    placeSearchBox();

    int bottomStatusHeight = 0;

    if (hasStatusText()) {
        bottomStatusHeight = scaleDPI(BOTTOM_STATUS_HEIGHT_DIP);

        SetWindowPos(
            bottomStatusBar,
            nullptr,
            0,
            size.cy - bottomStatusHeight,
            size.cx,
            bottomStatusHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);

        // Keep the single part stretched to the right edge.
        int parts[] = {-1};
        SendMessage(bottomStatusBar, SB_SETPARTS, 1, (LPARAM)parts);
    }

    if (viewHost) {
        RECT browserRect = windowBody();
        browserRect.bottom -= bottomStatusHeight;

        checkHR(viewHost.setBounds(browserRect));

        CComQIPtr<IFolderView2> folderView(shellView);
        if (folderView)
            folderView->SetRedraw(true); // Windows 11 has so many cool bugs
    }
    if (columnLayout && (!resizingColumns || !settings::getDetailsColumnsDeferred())) columnLayout->resize();
}

bool FolderWindow::columnFillActive() const {
    return settings::getDetailsColumnsEnabled() && fillColumns;
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
    if (!fillColumns && !settings::getDetailsColumnsDeferred()) {
        CComPtr<IShellView> source = shellView;
        const ULONG_PTR generation = viewGeneration;
        auto current = [&]() {
            return isWindowOperational() && shellView == source && viewGeneration == generation
                && settings::getDetailsColumnsEnabled() && !fillColumns;
        };
        if (current()) {
            TASKDIALOGCONFIG config = {sizeof(config)};
            config.hwndParent = hwnd;
            config.hInstance = GetModuleHandleW(nullptr);
            config.pszWindowTitle = MAKEINTRESOURCEW(IDS_APP_NAME);
            config.pszMainInstruction = getString(IDS_FILL_COLUMNS);
            config.pszContent = getString(IDS_FILL_COLUMNS_WARNING);
            config.pszMainIcon = TD_WARNING_ICON;
            config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
            config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
            config.nDefaultButton = IDNO;
            int button = IDNO;
            const HRESULT result = TaskDialogIndirect(&config, &button, nullptr, nullptr);
            if (!current() || !checkHR(result) || button != IDYES) return;
        } else return;
    }
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
        auto attached = ShellViewColumns::attach(source, hwnd, MSG_COLUMNS_CHANGED, generation);
        if (!isWindowOperational() || shellView != source || viewGeneration != generation) {
            if (attached) attached->stop();
            return;
        }
        columnLayout = std::move(attached);
    }
    auto layout = columnLayout;
    if (layout) {
        layout->setDeferred(settings::getDetailsColumnsDeferred());
        layout->suspend(resizingColumns && settings::getDetailsColumnsDeferred());
        layout->synchronize();
    }
}

void FolderWindow::finishColumnResize() {
    resizingColumns = false;
    if (!isWindowOperational()) return;
    if (columnBoundsPending && !IsIconic(hwnd)) onSize(clientSize(hwnd));
    synchronizeColumns();
}

void FolderWindow::onSettingsChanged() {
    CComPtr<ItemWindow> keepAlive(this);
    if (bottomStatusBar)
        checkLE(SetWindowPos(bottomStatusBar, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                | (settings::getStatusTextEnabled() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
    ItemWindow::onSettingsChanged();
    searchBox.setLiveSearch(settings::getLiveNameSearch());
    if (isWindowOperational())
        searchBox.setDropdownEnabled(settings::getEverythingEnabled() || settings::getSearchHistoryLimit() != 0, settings::getEverythingPath().get());
    const DWORD limit = settings::getSearchHistoryLimit();
    if (searchHistory.size() > limit) searchHistory.resize(limit);
    if (pendingSearches.size() > limit) pendingSearches.resize(limit);
    if (!limit) {
        searchHistoryDirty = false; // The preference already cleared persisted histories.
        searchHistoryLoaded = false;
    }
    if (isWindowOperational()) {
        synchronizeColumns();
        requestViewProfile();
    }
    if (isWindowOperational())
        setSingleSelectionLabels(shellView, settings::getFullNamesOnSelection());
    if (isWindowOperational() && viewHost)
        synchronizeViewProvider();
    if (viewHost && hasStatusText())
        updateStatus();
}

void FolderWindow::selectionChanged() {
    updateSelectionOnActivate = false;
    if (GetActiveWindow() != hwnd) { // in background
        // this could happen when dragging a file. don't try to create any windows yet
        // sometimes items also become deselected and then reselected
        // eg. when a file is deleted from a folder
        // Note: this also happens when certain operations create a progress window
        updateSelectionOnActivate = true;
    } else {
        scheduleUpdateSelection();
    }
}

void FolderWindow::scheduleUpdateSelection() {
    checkLE(SetTimer(hwnd, TIMER_UPDATE_SELECTION, SELECTION_UPDATE_DELAY_MS, nullptr));
}

// BrowseToObject and IShellView::Refresh dispatch window messages before they
// return. A close handled inside them destroys the view the Shell is still
// using; keep the request until the call returns, then post it.
HRESULT FolderWindow::browseToItem() {
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellItem> currentItem = item;
    if (!isWindowOperational() || !viewHost) return E_ABORT;
    ++navigationDepth;
    debugPrintf(L"NATIVE HWND=%p BrowseToObject begin\n", hwnd);
    bool isolated = false;
#ifdef FILESPACER_DEBUG
    isolated = settings::testMode;
#endif
    const HRESULT result = viewHost.navigate(currentItem, settings::getMixedFolderSorting(), isolated);
    debugPrintf(L"NATIVE HWND=%p BrowseToObject returned: HRESULT=0x%08lX\n", hwnd, static_cast<unsigned long>(result));
    resumeDeferredClose();
    return result;
}

HRESULT FolderWindow::refreshView() {
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> view = shellView;
    if (!isWindowOperational() || !view) return E_ABORT;
    ++navigationDepth;
    const HRESULT result = view->Refresh();
    resumeDeferredClose();
    return result;
}

// Restart the enumeration. The baseline is read before the call so that only
// an EnumDone of the new enumeration marks it complete; a stale event still
// queued from the previous one can no longer let positions be captured early.
HRESULT FolderWindow::refreshEnumeration() {
    viewMenuRestore.reset(); // An explicit restart supersedes a preview's pending replay.
    enumerationComplete = false;
    viewOptionsEnumerationBaseline = viewHost.enumerationSerial();
    return refreshView();
}

void FolderWindow::resumeDeferredClose() {
    --navigationDepth;
    if (navigationDepth || !isWindowOperational()) return;
    const UINT closeMessage = deferredCloseMessage;
    deferredCloseMessage = 0;
    if (closeMessage)
        checkLE(PostMessageW(hwnd, closeMessage, 0, 0));
    else {
        if (selectionBindingPending) {
            selectionBindingPending = false;
            checkLE(PostMessageW(hwnd, selectionDetail::readyMessage(), 0, 0));
        }
        if (viewProfilePending) requestViewProfile();
    }
}

// Verbs of the view's own menus (Refresh, New) reload the view and dispatch
// messages before InvokeCommand returns.
HRESULT FolderWindow::invokeViewCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info) {
    ++navigationDepth;
    const HRESULT result = invokeShellCommand(menu, info);
    resumeDeferredClose();
    return result;
}

// A click that activates this window (WM_ACTIVATE with WA_CLICKACTIVE) is
// followed by its button-down message, posted to the clicked window only after
// the activation. When that first message lands on the empty background of the
// view, the application's own message loop drops it so the native view keeps
// its selection. Item clicks, other buttons and Alt gestures are not affected.
bool FolderWindow::cursorOverShellView() const {
    POINT cursor = {};
    HWND root = nullptr;
    if (!shellView || !GetCursorPos(&cursor) || FAILED(shellView->GetWindow(&root)) || !root)
        return false;
    const HWND target = WindowFromPoint(cursor);
    return target && (target == root || IsChild(root, target));
}

bool FolderWindow::isBackgroundActivationClick(const MSG *msg) const {
    if (!shellView || GetKeyState(VK_MENU) < 0 || GetAsyncKeyState(VK_MENU) < 0
            || !settings::getKeepSelectionOnActivate())
        return false;
    HWND root = nullptr;
    if (FAILED(shellView->GetWindow(&root)) || !root)
        return false;
    if (msg->hwnd != root && !IsChild(root, msg->hwnd))
        return false;
    return gestureRoleAt(msg->pt) == ROLE_SYSTEM_LIST;
}

LRESULT FolderWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    CComPtr<ItemWindow> messageKeepAlive(this);
    if (message == WM_ENTERSIZEMOVE) {
        resizingColumns = true;
        if (columnLayout) columnLayout->suspend(settings::getDetailsColumnsDeferred());
    } else if (message == WM_EXITSIZEMOVE) {
        finishColumnResize();
    } else if (message == MSG_COLUMNS_CHANGED) {
        if (wParam == viewGeneration) synchronizeColumns();
        return 0;
    }
    if (message >= 0xC000 && message == selectionDetail::readyMessage()) {
        if (!isWindowOperational()) return 0;
        if (navigationDepth) { selectionBindingPending = true; return 0; }
        if (viewReady && initialStateApplied && shellView) {
            CComPtr<IShellView> view = shellView;
            const ULONG_PTR generation = viewGeneration;
            auto current = [&] { return isWindowOperational() && shellView == view && viewGeneration == generation; };
            ++navigationDepth;
            selectionClear.bind(view, current);
            resumeDeferredClose();
        }
        return 0;
    }
    if (message == MSG_EXPIRE_FOLDER_STATE && provisionalBaseline) {
        const LRESULT result = ItemWindow::handleMessage(message, wParam, lParam); // Invalidates the key first.
        provisionalBaseline.reset(); provisionalCurrent.reset(); provisionalRestore.reset();
        provisionalColumnWidths.clear();
        cancelStorageWait(); // Nothing provisional is left to recover.
        watchProvisionalInput(false);
        blockProvisionalInput(false);
        return result;
    }
    LPARAM testNamespace = 0;
#ifdef FILESPACER_DEBUG
    testNamespace = settings::testMode ? 1 : 0;
#endif
    if (settingsChangedMessage && message == settingsChangedMessage && lParam == testNamespace
            && wParam >= 1 && wParam <= settings::MAX_SEARCH_HISTORY_LIMIT + 1) {
        const size_t limit = static_cast<size_t>(wParam - 1);
        if (searchHistory.size() > limit) searchHistory.resize(limit);
        if (pendingSearches.size() > limit) pendingSearches.resize(limit);
        if (!limit) {
            searchHistoryDirty = false;
            // This delivered clear snapshot remains authoritative even when
            // the registry was re-enabled before the message arrived.
            searchHistoryLoaded = true;
        }
    }
    // Capture and cancellation notifications are sent synchronously, outside
    // the application's GetMessage/pretranslation loop.
    if (handleResizeMessage(message, wParam, lParam)) return 0;
    if (message == WM_CANCELMODE) {
        searchBox.hideDropdown();
        cancelAltGesture();
    }
    if (message == WM_MOVE) searchBox.hideDropdown();
    // The synchronous Shell call can dispatch another window message. Closing
    // the host also destroys its child view, so defer the entire close request.
    if (navigationDepth && (message == WM_CLOSE || message == MSG_CLOSE_AUTOMATIC)) {
        if (!deferredCloseMessage || message == WM_CLOSE)
            deferredCloseMessage = message;
        return 0;
    }
    if (message == WM_GETISHELLBROWSER_COMPAT) {
        return reinterpret_cast<LRESULT>(
            static_cast<IShellBrowser *>(this));
    }

    switch (message) {
        case WM_ENTERIDLE: {
            auto preview = viewMenuPreview;
            if (wParam == MSGF_MENU && preview) {
                preview->update();
                preview->advancePositions();
                return 0;
            }
            break;
        }
        case MSG_ENUM_READY:
        case MSG_ENUM_DONE:
            if (isWindowOperational() && viewHost.isViewNotification(wParam)) {
                debugPrintf(L"NATIVE HWND=%p enumeration: message=%u generation=%llu serial=%llu\n",
                    hwnd, message, static_cast<unsigned long long>(wParam), static_cast<unsigned long long>(lParam));
                if (message == MSG_ENUM_READY) initialEnumerationReady = true;
                else if (static_cast<ULONG_PTR>(lParam) > viewOptionsEnumerationBaseline)
                    enumerationComplete = true;
                finishViewInitialization();
                auto preview = viewMenuPreview;
                if (message == MSG_ENUM_DONE && preview)
                    preview->enumerated(static_cast<ULONG_PTR>(lParam));
                auto restore = viewMenuRestore;
                if (message == MSG_ENUM_DONE && restore && isWindowOperational())
                    restore->replay(static_cast<ULONG_PTR>(lParam));
                if (isWindowOperational()) synchronizeColumns();
            }
            return 0;
        case MSG_VIEW_CHANGED:
            if (isWindowOperational() && viewHost.isViewNotification(wParam)) {
                scheduleStateSave();
                requestViewProfile();
            }
            return 0;
        case MSG_SYNCHRONIZE_VIEW_PROFILE:
            synchronizeViewProfile();
            return 0;
        case MSG_OPEN_PENDING_VIEW_MENU:
            if (viewMenuRequested && viewGeneration == wParam && isWindowOperational()
                    && initialStateApplied && !viewProfileChanging) {
                viewMenuRequested = false;
                if (GetForegroundWindow() == hwnd) openViewMenu(pendingViewMenuPoint);
                else requestViewProfile();
            }
            return 0;
        case MSG_FINISH_VIEW_INITIALIZATION:
            if (isWindowOperational() && viewHost.isViewNotification(wParam))
                finishViewInitialization();
            return 0;
        case MSG_STORAGE_AVAILABLE:
            if (wParam == storageWaitSerial && isWindowOperational()) {
                cancelStorageWait();
                if (!provisionalBaseline) return 0;
                if (!loadFolderState()) {
                    // Taken again meanwhile (or still busy): wait again. Any
                    // other failure of an open store is not retried from here,
                    // which would repost this message without end: the next
                    // save request reads again, as before.
                    if (folderStateBlockingMutex() || folderStateBusy()) scheduleStorageRetry();
                } else {
                    persistViewState();
                    saveSearchHistory();
                }
            }
            return 0;
        case MSG_SYNCHRONIZE_VIEW_PROVIDER:
            if (isWindowOperational()) synchronizeViewProvider();
            return 0;
        case MSG_VIEW_MENU_PREVIEW: {
            auto preview = viewMenuPreview;
            if (preview && preview->serial == wParam && isWindowOperational()) preview->update();
            return 0;
        }
        case WM_MENUSELECT: {
            auto preview = viewMenuPreview;
            if (preview && HIWORD(wParam) != 0xFFFF && !(HIWORD(wParam) & MF_POPUP))
                preview->highlight(LOWORD(wParam), reinterpret_cast<HMENU>(lParam));
            break;
        }
        case MSG_QUICK_ACCESS_ICON: {
            HBITMAP bitmap = quickAccessIconThread ? quickAccessIconThread->takeBitmap() : nullptr;
            if (!bitmap)
                return 0;
            const int iconSize = GetSystemMetrics(SM_CXSMICON);
            HIMAGELIST images = ImageList_Create(iconSize, iconSize, ILC_COLOR32, 1, 0);
            if (images) {
                if (ImageList_Add(images, bitmap, nullptr) >= 0) {
                    quickAccessImages = images;
                    const HWND toolbar = getQuickAccessToolbar();
                    const LRESULT buttonSize = SendMessage(toolbar, TB_GETBUTTONSIZE, 0, 0);
                    SendMessage(toolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(iconSize, iconSize));
                    SendMessage(toolbar, TB_SETIMAGELIST, 0, (LPARAM)images);
                    TBBUTTONINFO buttonInfo = {sizeof(buttonInfo)};
                    buttonInfo.dwMask = TBIF_IMAGE | TBIF_TEXT | TBIF_STYLE;
                    buttonInfo.iImage = 0;
                    buttonInfo.fsStyle = BTNS_DROPDOWN;
                    buttonInfo.pszText = const_cast<wchar_t *>(L"");
                    SendMessage(toolbar, TB_SETBUTTONINFO, IDM_QUICK_ACCESS,
                        (LPARAM)&buttonInfo);
                    SendMessage(toolbar, TB_SETBUTTONSIZE, 0, buttonSize);
                    placeSearchBox();
                } else {
                    ImageList_Destroy(images);
                }
            }
            DeleteObject(bitmap);
            return 0;
        }
        case WM_SETTINGCHANGE:
            // ShlObj_core.h documents "ShellState" for Shell option changes.
            if (shellView && lParam
                    && lstrcmpi(reinterpret_cast<LPCWSTR>(lParam), L"ShellState") == 0)
                checkHR(refreshEnumeration());
            break;
        case WM_WINDOWPOSCHANGED: {
            CComPtr<ItemWindow> keepAlive(this);
            // Preserve the normal layout and WM_SIZE/WM_MOVE processing.
            LRESULT result = ItemWindow::handleMessage(message, wParam, lParam);
            if (isWindowOperational() && initialNavigationPending && IsWindowVisible(hwnd)) {
                initialNavigationPending = false;
                checkLE(PostMessage(hwnd, MSG_INITIAL_NAVIGATION, 0, 0));
            }
            return result;
        }
        case MSG_INITIAL_NAVIGATION: {
            if (!isWindowOperational() || !viewHost) return 0;
            // Paint the frame and its controls before entering the synchronous navigation.
            checkLE(RedrawWindow(hwnd, nullptr, nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN));
            if (!isWindowOperational()) return 0;
            // will call IExplorerBrowserEvents callbacks
            HRESULT hr;
            if (!checkHR(hr = browseToItem()) && isWindowOperational()) {
                recordFolderAccess(hr);
                if (hasStatusText())
                    setStatusText(getErrorMessage(hr).get());
            }
            if (!isWindowOperational()) return 0;
            const HRESULT optionsResult = viewHost.lockNavigation(true);
            checkHR(optionsResult);
            return 0;
        }
        case WM_EXITMENULOOP:
            scheduleStateSave();
            requestViewProfile();
            break;
        case WM_TIMER:
            if (wParam == TIMER_SAVE_STATE) {
                KillTimer(hwnd, TIMER_SAVE_STATE);
                if (!isWindowOperational()) return 0;
                if (provisionalBaseline && provisionalReady && !provisionalRestore && provisionalInputBlocked) {
                    const bool observed = captureProvisionalCurrent(true, provisionalInputBlocked);
                    if (observed && provisionalInputBlocked && !provisionalCaptureBusy)
                        blockProvisionalInput(false);
                }
                if (provisionalBaseline && !loadFolderState()) {
                    // Unavailable storage: wait for its release event (or back
                    // off on SQLITE_BUSY) without repeatedly capturing the
                    // provisional layout. Shell readiness still comes from its events.
                    scheduleStorageRetry();
                    return 0;
                }
                storageRetryDelay = 0;
                persistViewState();
                saveSearchHistory();
                return 0;
            }
            if (wParam == TIMER_UPDATE_SELECTION) {
                KillTimer(hwnd, TIMER_UPDATE_SELECTION);
                updateSelection();
                return 0;
            }
            break;
    }

    return ItemWindow::handleMessage(message, wParam, lParam);
}

void FolderWindow::updateSelection() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational() || !viewHost)
        return;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    const auto current = [&] { return isWindowOperational() && shellView == source && viewGeneration == generation; };

    CComPtr<IFolderView2> folderView;
    if (!isWindowOperational()
            || FAILED(viewHost.currentView(IID_PPV_ARGS(&folderView)))
            || !isWindowOperational())
        return;

    int numSelected;
    if (!checkHR(folderView->ItemCount(SVGIO_SELECTION, &numSelected)) || !isWindowOperational())
        return;

    if (numSelected == 1) {
        CComPtr<IShellItemArray> selection;
        if (nativeViewSelection(folderView, FALSE, &selection, current) == S_OK && current()) {
            CComPtr<IShellItem> newSelected;
            if (checkHR(selection->GetItemAt(0, &newSelected)) && isWindowOperational())
                selected = newSelected;
        }
    } else {
        selected = nullptr;
    }

    if (isWindowOperational() && hasStatusText())
        updateStatus();
}

void FolderWindow::updateStatus() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational() || !viewHost)
        return;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    const auto current = [&] { return isWindowOperational() && shellView == source && viewGeneration == generation; };
    CComPtr<IFolderView2> folderView;
    if (!isWindowOperational()
            || FAILED(viewHost.currentView(IID_PPV_ARGS(&folderView)))
            || !isWindowOperational())
        return;

    int numItems = 0;
    int numSelected = 0;

    checkHR(folderView->ItemCount(SVGIO_ALLVIEW, &numItems));
    if (!isWindowOperational())
        return;
    checkHR(folderView->ItemCount(SVGIO_SELECTION, &numSelected));
    if (!isWindowOperational())
        return;

    local_wstr_ptr status;

    if (numSelected == 0) {
        status = formatString(IDS_FOLDER_STATUS, numItems);
    } else {
        ULONGLONG totalSize = 0;
        bool hasSize = false;

        CComPtr<IShellItemArray> selection;

        if (nativeViewSelection(folderView, FALSE, &selection, current) == S_OK && current()) {
            DWORD count = 0;
            checkHR(selection->GetCount(&count));

            for (DWORD i = 0; i < count && isWindowOperational(); i++) {
                CComPtr<IShellItem> selectedItem;

                if (!checkHR(selection->GetItemAt(i, &selectedItem)) || !isWindowOperational())
                    continue;

                CComQIPtr<IShellItem2> selectedItem2(selectedItem);
                if (!selectedItem2 || !isWindowOperational())
                    continue;

                ULONGLONG size = 0;

                if (SUCCEEDED(selectedItem2->GetUInt64(PKEY_Size, &size))) {
                    totalSize += size;
                    hasSize = true;
                }
            }
        }

        if (hasSize) {
            wchar_t sizeText[64] = {};

            StrFormatByteSizeW(
                (LONGLONG)totalSize,
                sizeText,
                _countof(sizeText));

            status = formatString(
                IDS_FOLDER_STATUS_SEL_SIZE,
                numItems,
                numSelected,
                sizeText);
        } else {
            status = formatString(
                IDS_FOLDER_STATUS_SEL,
                numItems,
                numSelected);
        }
    }

    if (isWindowOperational())
        setStatusText(status.get());
}

void FolderWindow::clearSelection() {
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == view && viewGeneration == generation;
    };
    if (!current()) return;
    if (view) {
        ++navigationDepth;
        checkHR(selectionClear.clear(view, current)); // keep logical and keyboard focus
        resumeDeferredClose();
    }
    if (current()) selected = nullptr; // in case this happened in the background
}

IDispatch * FolderWindow::getShellViewDispatch() {
    return this;
}

void FolderWindow::onItemChanged() {
    if (iconRestoreFailed) captureIconChange();
    if (provisionalBaseline && provisionalReady && viewReady)
        captureProvisionalCurrent(true);
    ItemWindow::onItemChanged();
    CComHeapPtr<wchar_t> folderName;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &folderName)))
        searchBox.setFolderName(folderName);
    if (viewHost) {
        CComPtr<IShellView> source = shellView;
        const ULONG_PTR generation = viewGeneration;
        auto current = [&]() {
            return isWindowOperational() && shellView == source && viewGeneration == generation;
        };
        CComPtr<IFolderView2> folderView;
        if (checkHR(viewHost.currentView(IID_PPV_ARGS(&folderView)))) {
            auto state = std::make_unique<ShellViewState>();
            state->view = captureOptions(folderView, {});
            if (!current()) return;
            // The transient snapshot may hold a filtered subset: it is applied once
            // to the recreated view and never persisted as the folder's placement.
            if (iconRestoreFailed) state->icons = getSavedFolderState().icons;
            else if (spatialView(folderView))
                checkHR(captureIconPositions(folderView, current, enumerationComplete, {}, state->icons));
            if (!current()) return;
            checkHR(captureReopenSelection(source, current, state->selection));
            if (!current()) return;
            storedViewState = std::move(state);
        }

        checkHR(viewHost.stopObserving());
        selectionClear.reset();
        shellView = nullptr;
        checkHR(viewHost.lockNavigation(false)); // temporarily enable navigation
        HRESULT hr = browseToItem();
        if (!isWindowOperational()) return;
        if (!checkHR(hr)) recordFolderAccess(hr);
        checkHR(viewHost.lockNavigation(true));
    }
}

void FolderWindow::refresh() {
    viewStateDirty(FolderState::Icons);
    persistViewState();
    ItemWindow::refresh();
    if (shellView) {
        restoreIconsPending = searchText.empty();
        HRESULT hr = refreshEnumeration();
        checkHR(hr);
        recordFolderAccess(hr);
    } // TODO: invoke context menu verb instead?
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
    ShellMenuIcons icons;
    if (SUCCEEDED(result) && isWindowOperational()) {
        std::vector<std::wstring> paths;
        for (const Entry &entry : entries)
            paths.push_back(entry.path);
        icons.start(hwnd, menu, iconSize, std::move(paths));
        if (isWindowOperational())
            command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                point.x, point.y, hwnd, nullptr);
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
        openChild(target, closeSource);
    }
}

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
    if (hybridViewEligible && viewProfile == FVO_DEFAULT) {
        viewMenuRequested = true;
        pendingViewMenuPoint = point;
        requestViewProfile();
        return;
    }
    ++viewMenuDepth;
    struct MenuScope {
        FolderWindow &owner;
        ~MenuScope() { --owner.viewMenuDepth; owner.requestViewProfile(); }
    } menuScope{*this};
    if (provisionalBaseline && !captureProvisionalCurrent(true, true)) return;
    if (!isWindowOperational()) return;
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
    if (columnLayout) columnLayout->suspend(true);
    struct ColumnsResume {
        std::shared_ptr<ShellViewColumns> layout;
        ~ColumnsResume() { if (layout) layout->suspend(false); }
    } columnsResume{columnLayout};
    viewMenuRestore.reset(); // A new menu supersedes a previous preview's pending replay.
    const bool previewEnabled = settings::getViewMenuPreviewEnabled();
    if (previewEnabled) {
        // Native icon moves may not have scheduled a save yet. Preserve the
        // complete original placement before a preview can be closed.
        viewStateDirty(FolderState::Icons);
        persistViewState();
    }
    if (!isWindowOperational()) return;
    CComPtr<IFolderView2> folderView;
    auto detachSite = [](IContextMenu *handler) { checkHR(IUnknown_SetSite(handler, nullptr)); };
    std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
    if (checkHR(viewHost.currentView(IID_PPV_ARGS(&folderView))) && isWindowOperational()
            && checkHR(IUnknown_SetSite(contextMenu, folderView)))
        site.reset(contextMenu);
    if (!isWindowOperational()) return;
    const int menuCount = GetMenuItemCount(subMenu);
    if (menuCount < 0) return;
    int thumbnailPosition = menuCount;
    for (int i = 0; i < menuCount; ++i) {
        const UINT command = GetMenuItemID(subMenu, i);
        if (command < IDM_SHELL_FIRST || command > IDM_SHELL_LAST) continue;
        wchar_t verb[64] = {};
        const HRESULT result = contextMenu->GetCommandString(command - IDM_SHELL_FIRST,
            GCS_VERBW, nullptr, reinterpret_cast<char *>(verb), _countof(verb));
        if (!isWindowOperational()) return;
        // Canonical Shell verb; the translated label is never used as an identifier.
        if (SUCCEEDED(result) && lstrcmpiW(verb, L"arrangeautogrid") == 0) {
            thumbnailPosition = i + 1;
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
    if (previewEnabled && folderView && viewReady && initialStateApplied && enumerationComplete && !restoringState) {
        preview = std::make_shared<ViewMenuPreview>(*this, subMenu, contextMenu, folderView);
        if (preview->capture()) {
            hook = SetWindowsHookEx(WH_MSGFILTER, ViewMenuPreview::filter, nullptr, GetCurrentThreadId());
            checkLE(hook);
            if (hook) {
                ViewMenuPreview::active = preview.get();
                viewMenuPreview = preview;
                KillTimer(hwnd, TIMER_SAVE_STATE);
            }
        }
    }
    const int cmd = isWindowOperational() ? TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x, point.y, hwnd, nullptr) : 0;
    if (hook) {
        UnhookWindowsHookEx(hook);
        ViewMenuPreview::active = previous;
        preview->finish((cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) || cmd == IDM_TOGGLE_THUMBNAILS || cmd == IDM_FILL_COLUMNS
            ? static_cast<UINT>(cmd) : 0);
    } else if (isWindowOperational() && cmd == IDM_FILL_COLUMNS) {
        toggleFillColumns();
    } else if (isWindowOperational() && cmd == IDM_TOGGLE_THUMBNAILS) {
        toggleGlobalThumbnails();
    } else if (isWindowOperational() && viewHost && cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) {
        auto info = makeInvokeInfo(cmd - IDM_SHELL_FIRST, point);
        info.hwnd = hwnd;
        checkHR(invokeViewCommand(contextMenu, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info)));
    }
}

/* IUnknown */

STDMETHODIMP FolderWindow::QueryInterface(REFIID id, void **obj) {
    static const QITAB interfaces[] = {
		QITABENT(FolderWindow, IServiceProvider),
		QITABENT(FolderWindow, ICommDlgBrowser),
		QITABENT(FolderWindow, ICommDlgBrowser2),
		QITABENT(FolderWindow, IExplorerBrowserEvents),
		QITABENT(FolderWindow, IDispatch),
		QITABENT(FolderWindow, IWebBrowser),
		QITABENT(FolderWindow, IWebBrowserApp),
		QITABENT(FolderWindow, IShellBrowser),
		{},
	};
    HRESULT hr = QISearch(this, interfaces, id, obj);
    if (SUCCEEDED(hr))
        return hr;
    return ItemWindow::QueryInterface(id, obj);
}

STDMETHODIMP_(ULONG) FolderWindow::AddRef() {
    return ItemWindow::AddRef();
}

STDMETHODIMP_(ULONG) FolderWindow::Release() {
    return ItemWindow::Release();
}

/* IOleWindow / IShellBrowser */

STDMETHODIMP FolderWindow::GetWindow(HWND *phwnd) {
    if (!phwnd)
        return E_POINTER;

    *phwnd = hwnd;
    return S_OK;
}

STDMETHODIMP FolderWindow::ContextSensitiveHelp(BOOL) {
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::InsertMenusSB(
        HMENU,
        LPOLEMENUGROUPWIDTHS) {
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::SetMenuSB(
        HMENU,
        HOLEMENU,
        HWND) {
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::RemoveMenusSB(HMENU) {
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::SetStatusTextSB(LPCWSTR text) {
    setStatusText(text ? text : L"");
    return S_OK;
}

STDMETHODIMP FolderWindow::EnableModelessSB(BOOL) {
    return S_OK;
}

STDMETHODIMP FolderWindow::TranslateAcceleratorSB(MSG *, WORD) {
    return S_FALSE;
}

STDMETHODIMP FolderWindow::BrowseObject(
        PCUIDLIST_RELATIVE,
        UINT) {
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::GetViewStateStream(
        DWORD,
        IStream **stream) {

    if (!stream)
        return E_POINTER;

    *stream = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::GetControlWindow(
        UINT,
        HWND *control) {

    if (!control)
        return E_POINTER;

    *control = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::SendControlMsg(
        UINT,
        UINT,
        WPARAM,
        LPARAM,
        LRESULT *result) {

    if (result)
        *result = 0;

    return E_NOTIMPL;
}

STDMETHODIMP FolderWindow::QueryActiveShellView(IShellView **view) {
    if (!view)
        return E_POINTER;

    *view = nullptr;

    if (!shellView)
        return E_FAIL;

    *view = shellView;
    (*view)->AddRef();

    return S_OK;
}

STDMETHODIMP FolderWindow::OnViewWindowActive(IShellView *) {
    return S_OK;
}

STDMETHODIMP FolderWindow::SetToolbarItems(
        LPTBBUTTONSB,
        UINT,
        UINT) {
    return E_NOTIMPL;
}

/* IServiceProvider */

STDMETHODIMP FolderWindow::QueryService(
        REFGUID guidService,
        REFIID riid,
        void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (guidService == SID_SExplorerBrowserFrame) {
        return QueryInterface(riid, ppv);

    } else if (guidService == SID_SFolderView) {
        if (shellView)
            return shellView->QueryInterface(riid, ppv);

    } else if (guidService == SID_STopLevelBrowser) {
        return QueryInterface(riid, ppv);
    }

    return E_NOINTERFACE;
}

/* ICommDlgBrowser */

// Called when double-clicking an item or pressing Enter.
STDMETHODIMP FolderWindow::OnDefaultCommand(IShellView *const view) {
    CComPtr<ItemWindow> keepAlive(this);
    // Preserve the Shell's native handling of Alt-modified activation.
    if (GetKeyState(VK_MENU) < 0)
        return S_FALSE;

    const bool closeSource = settings::getKeepSourceWindowOpen()
        == (GetKeyState(VK_CONTROL) < 0);
    const ULONG_PTR generation = viewGeneration;
    const auto current = [&] { return isWindowOperational() && shellView == view && viewGeneration == generation; };
    CComQIPtr<IFolderView2> folderView(view);

    int numSelected = 0;
    if (folderView
            && checkHR(folderView->ItemCount(SVGIO_SELECTION, &numSelected))
            && numSelected == 1) {

        CComPtr<IShellItemArray> selection;
        if (nativeViewSelection(folderView, FALSE, &selection, current) == S_OK && current()) {
            CComPtr<IShellItem> selectedItem;

            if (checkHR(selection->GetItemAt(0, &selectedItem))) {
                // Resolve shortcuts before deciding whether this is a folder.
                CComPtr<IShellItem> resolved = resolveLink(selectedItem);

                SFGAOF attributes = 0;
                HRESULT access = resolved ? resolved->GetAttributes(SFGAO_FOLDER, &attributes) : E_FAIL;
                if (resolved && FAILED(access)) recordFolderItemFailure(resolved, access);
                if (resolved && checkHR(access) && (attributes & SFGAO_FOLDER)) {

                    // Folder activation is handled by our spatial window logic.
                    selected = selectedItem;
                    openChild(selectedItem, closeSource);
                    return S_OK;
                }
            }
        }
    }

    // Keep FileSpacer's original behavior for non-folder items.
    if (!invokingDefaultVerb
            && settings::getDeselectOnOpen()
            && folderView
            && numSelected == 1) {

        invokingDefaultVerb = true;
        folderView->InvokeVerbOnSelection(nullptr);
        invokingDefaultVerb = false;

        clearSelection();
        return S_OK;
    }

    return S_FALSE;
}

STDMETHODIMP FolderWindow::OnStateChange(IShellView *source, ULONG change) {
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> current = shellView;
    CComPtr<IUnknown> sourceIdentity, currentIdentity;
    if (source) source->QueryInterface(IID_PPV_ARGS(&sourceIdentity));
    if (current) current->QueryInterface(IID_PPV_ARGS(&currentIdentity));
    if (isWindowOperational() && current == shellView && sourceIdentity
            && sourceIdentity == currentIdentity && change == CDBOSC_SELCHANGE) {
        selectionChanged();
        scheduleStateSave();
    }
    return S_OK;
}

STDMETHODIMP FolderWindow::IncludeObject(IShellView *, PCUITEMID_CHILD childID) {
    // SetFilter handles filtering; retain the existing exclusions if this is called.
    for (int i = 0; i < _countof(hiddenItemIDs); i++) {
        if (hiddenItemIDs[i] && ILIsEqual(childID, hiddenItemIDs[i]))
            return S_FALSE;
    }
    return S_OK;
}

/* ICommDlgBrowser2 */

STDMETHODIMP FolderWindow::GetDefaultMenuText(IShellView *, wchar_t *, int) {
    return S_FALSE;
}

STDMETHODIMP FolderWindow::GetViewFlags(DWORD *flags) {
    // ExplorerBrowser needs its IncludeObject path to apply IFolderFilter.
    *flags = CDB2GVF_NOSELECTVERB;
    return S_OK;
}

STDMETHODIMP FolderWindow::Notify(IShellView *, DWORD) {
    return S_OK;
}

/* IExplorerBrowserEvents */

// order: OnNavigationPending, OnViewCreated, OnNavigationComplete
// OR: OnNavigationPending, OnNavigationFailed
STDMETHODIMP FolderWindow::OnNavigationPending(PCIDLIST_ABSOLUTE) {
    CComPtr<ItemWindow> keepAlive(this);
    viewMenuPreview.reset();
    viewMenuRestore.reset();
    cancelAltGesture();
    cancelStorageWait();
    providerSyncPending = false;
    debugPrintf(L"NATIVE HWND=%p OnNavigationPending\n", hwnd);
    checkHR(viewHost.stopObserving());
    viewGeneration = 0;
    selectionClear.reset();
    provisionalViewWindow = nullptr; // The previous view's HWND may be reused.
    viewReady = false;
    initialEnumerationReady = false;
    viewOptionsApplied = false;
    viewOptionsEnumerationBaseline = 0;
    initialStateApplied = false;
    restoreIconsPending = true;
    restoreIconsUnpruned = false;
    enumerationComplete = false;
    provisionalRestoreOptionsApplied = false;
    return S_OK;
}

STDMETHODIMP FolderWindow::OnNavigationComplete(PCIDLIST_ABSOLUTE) {
    CComPtr<ItemWindow> keepAlive(this);
    debugPrintf(L"NATIVE HWND=%p OnNavigationComplete\n", hwnd);
    recordFolderAccess(S_OK);
    if (shellView && !viewProfileChanging && GetActiveWindow() == hwnd) {
        CComPtr<IShellView> view = shellView;
        checkHR(view->UIActivate(SVUIA_ACTIVATE_FOCUS));
    }
    if (!isWindowOperational()) return S_OK;
    viewReady = true; // Navigation completion alone does not mean enumeration completed.
    finishViewInitialization();
    return S_OK;
}

STDMETHODIMP FolderWindow::OnNavigationFailed(PCIDLIST_ABSOLUTE) {
    debugPrintf(L"NATIVE HWND=%p OnNavigationFailed\n", hwnd);
    // This callback also reports cancellation and has no HRESULT: do not count a failure.
    checkHR(viewHost.stopObserving());
    viewReady = false;
    setStatusText(getString(IDS_FOLDER_ERROR));
    if (reopeningWindow.sequence) {
        auto store = folderStateStore();
        if (!store || !store->returnClosedWindow(reopeningWindow)) reportFolderStateError(hwnd);
        reopeningWindow = {};
        refreshReopenJumpLists();
    }
    return S_OK;
}

STDMETHODIMP FolderWindow::OnViewCreated(IShellView *const view) {
    CComPtr<ItemWindow> keepAlive(this);
    if (columnLayout) columnLayout->stop();
    columnLayout.reset();
    debugPrintf(L"NATIVE HWND=%p OnViewCreated\n", hwnd);
    selectionClear.reset();
    shellView = view;
    iconChangeBaseline.reset();
    iconRestoreFailed = false;
    iconPositionErrorShown = false;
    watchProvisionalInput(provisionalBaseline != nullptr);
    initialEnumerationReady = false;
    viewOptionsApplied = false;
    viewOptionsEnumerationBaseline = 0;
    initialStateApplied = false;
    restoreIconsPending = true;
    restoreIconsUnpruned = false;
    restoringState = true;
    viewReady = false;
    enumerationComplete = false;
    const HRESULT observed = viewHost.observeView(view, hwnd, MSG_ENUM_READY,
        MSG_ENUM_DONE, viewGeneration, MSG_VIEW_CHANGED);
    debugPrintf(L"NATIVE HWND=%p observeView: HRESULT=0x%08lX generation=%llu\n",
        hwnd, static_cast<unsigned long>(observed), static_cast<unsigned long long>(viewGeneration));
    checkHR(observed);
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView.p == view && viewGeneration == generation;
    };
    if (!current()) return S_OK;
    if (FAILED(observed)) setStatusText(getErrorMessage(observed).get());

    if (provisionalBaseline) provisionalInputBlocked = true;
    HWND body = nullptr;
    provisionalViewWindow = SUCCEEDED(view->GetWindow(&body)) ? body : nullptr;
    if (!current()) return S_OK;

    if (!loadFolderState() && !provisionalBaseline)
        provisionalBaseline.reset(new ShellViewState);
    if (!current()) return S_OK;
    if (!provisionalBaseline && (getSavedFolderState().present & FolderState::Icons)
            && !getSavedFolderState().icons.empty()) {
        // Known positions remain protected until actually applied, including
        // a non-spatial presentation that may later become spatial.
        iconRestoreFailed = true;
        watchProvisionalInput(true);
    }
    if (provisionalBaseline) blockProvisionalInput(true);

    if (!current()) return S_OK;
    setSingleSelectionLabels(view, settings::getFullNamesOnSelection());
    if (!current()) return S_OK;
    restoringState = false;
    finishViewInitialization();

    return S_OK;
}

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

// Navigation and enumeration callbacks can arrive in either order. Complete
// initial enumeration before applying stored presentation changes. Those changes
// may rebuild the native contents, requiring a later EnumDone before selection,
// icon positions or persistence can use the view.
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
    if (storedViewState && !initialStateApplied && searchText.empty()) {
        // A newly created view can publish an empty completion before its real
        // contents. Do not consume the rename snapshot if the native folder
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
                if ((!provisionalBaseline || provisionalReady) && provisionalInputBlocked)
                    blockProvisionalInput(false);
                return;
            }
        }
        restoringState = false;
    }
    const bool initial = !initialStateApplied;
    initialStateApplied = true;
    if (initial) {
        selectionClear.bind(view, current);
        if (!current()) return;
        CComQIPtr<IFolderView> folderView(view);
        if (!folderView || !current()) return;
        spatialLayout = spatialView(folderView);
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
                if (!store || !store->readViewScroll(getFolderStateKey(), &bytes)) stateStorageError();
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
    if (current()) {
        if (viewMenuRequested && viewProfile != FVO_DEFAULT)
            checkLE(PostMessageW(hwnd, MSG_OPEN_PENDING_VIEW_MENU, viewGeneration, 0));
        else requestViewProfile();
    }
}

/* IDispatch */

STDMETHODIMP FolderWindow::GetTypeInfoCount(UINT *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::GetTypeInfo(UINT, LCID, ITypeInfo **) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::GetIDsOfNames(
    REFIID, LPOLESTR *, UINT, LCID, DISPID *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::Invoke(
    DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) { return E_NOTIMPL; }

/* IWebBrowser */

STDMETHODIMP FolderWindow::get_Document(IDispatch **dispatch) {
    return QueryInterface(__uuidof(IDispatch), (void **)dispatch); // for SHOpenFolderAndSelectItems
}

STDMETHODIMP FolderWindow::GoBack() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::GoForward() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::GoHome() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::GoSearch() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::Navigate(
    BSTR, VARIANT *, VARIANT *, VARIANT *, VARIANT *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::Refresh() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::Refresh2(VARIANT *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::Stop() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Application(IDispatch **) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Parent(IDispatch **) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Container(IDispatch **) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_TopLevelContainer(VARIANT_BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Type(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Left(long *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_Left(long) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Top(long *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_Top(long) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Width(long *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_Width(long) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Height(long *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_Height(long) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_LocationName(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_LocationURL(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Busy(VARIANT_BOOL *) { return E_NOTIMPL; }

/* IWebBrowserApp */

STDMETHODIMP FolderWindow::get_HWND(SHANDLE_PTR *pHWND) {
    // this window is brought to the foreground when a Shell Window is activated
    *pHWND = (SHANDLE_PTR)hwnd;
    return S_OK;
}

STDMETHODIMP FolderWindow::Quit() { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::ClientToWindow(int *, int *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::PutProperty(BSTR, VARIANT) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::GetProperty(BSTR, VARIANT *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Name(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_FullName(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Path(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_Visible(VARIANT_BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_Visible(VARIANT_BOOL) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_StatusBar(VARIANT_BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_StatusBar(VARIANT_BOOL) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_StatusText(BSTR *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_StatusText(BSTR) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_ToolBar(int *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_ToolBar(int) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_MenuBar(VARIANT_BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_MenuBar(VARIANT_BOOL) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::get_FullScreen(VARIANT_BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FolderWindow::put_FullScreen(VARIANT_BOOL) { return E_NOTIMPL; }

} // namespace
