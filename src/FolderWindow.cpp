#include "FolderWindow.h"
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "MixedFolder.h"
#include "ShellViewIcons.h"
#include "ShellViewFilter.h"
#include "ShellViewLabels.h"
#include "CreateItemWindow.h"
#include "ClosedWindows.h"
#include "ShellMenuIcons.h"
#include "GeomUtils.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "Settings.h"
#include "Everything.h"
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

// This job owns only native PIDLs and a cancellable notification target. It can
// finish after the window closes without retaining its view or blocking its STA.
class FolderWindow::StartupIconsThread : public UnknownImpl {
public:
    struct Item {
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> id;
        Item() = default;
        Item(Item &&other) noexcept : id(other.id.Detach()) {}
    };
    std::vector<Item> items;
    StartupIconsThread(HWND owner, ULONG_PTR serial) : target(owner), generation(serial) { lockProcess(); }
    ~StartupIconsThread() override = default;
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = UnknownImpl::Release();
        // The job and its PIDLs must be freed before the last process lock ends.
        if (!remaining) unlockProcess();
        return remaining;
    }
    bool start() {
        AddRef();
        if (SHCreateThread(run, this, static_cast<SHCT_FLAGS>(0), nullptr)) return true;
        Release(); return false;
    }
    void stop() {
        AcquireSRWLockExclusive(&notificationLock); target.store(nullptr); ReleaseSRWLockExclusive(&notificationLock);
    }
private:
    std::atomic<HWND> target;
    ULONG_PTR generation;
    SRWLOCK notificationLock = SRWLOCK_INIT;
    static DWORD WINAPI run(void *parameter) {
        auto *self = static_cast<StartupIconsThread *>(parameter);
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(initialized)) for (const auto &item : self->items) {
            if (!self->target.load()) break;
            SHFILEINFOW info = {};
            SHGetFileInfoW(reinterpret_cast<LPCWSTR>(item.id.m_pData), 0, &info, sizeof(info), SHGFI_PIDL | SHGFI_SYSICONINDEX);
        }
        AcquireSRWLockShared(&self->notificationLock);
        const HWND owner = self->target.load();
        if (owner) PostMessageW(owner, MSG_STARTUP_ICONS, self->generation, 0);
        ReleaseSRWLockShared(&self->notificationLock);
        // A close may cancel the target while COM releases its apartment objects.
        // The worker reference keeps this process alive through that teardown.
        if (SUCCEEDED(initialized)) CoUninitialize();
        self->Release(); return 0;
    }
};


FolderWindow::~FolderWindow() = default;

static constexpr int BOTTOM_STATUS_HEIGHT_DIP = 23;
// Coalesce selection notifications before updating the selected item and status.
static constexpr UINT SELECTION_UPDATE_DELAY_MS = 100;

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


bool FolderWindow::spatialView(IFolderView *const folderView) {
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
            checkHR(CComQIPtr<IParentAndItem>(item)
                ->GetParentAndItem(nullptr, nullptr, &hiddenItemIDs[i]));
        }
    }
}

FolderWindow::FolderWindow(IShellItem *const item, const std::wstring &identity)
        : ItemWindow(item, identity) {}

// The menu owns this session while tracking. A pending EnumDone can retain it
// afterward so an incomplete presentation never reaches the state store.
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
    bool tracking = true, posted = false, busy = false, changed = false, waiting = false;
    ULONG_PTR enumerationBaseline = 0;
    std::vector<std::vector<uint8_t>> positionChunks;
    size_t nextPositionChunk = 0;

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
            if (++count == 32 || offset == initial.icons.size()) {
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
        while (!busy && current() && !currentCommand && nextPositionChunk < positionChunks.size()) advancePositions();
    }

    ViewMenuPreview(FolderWindow &window, HMENU popup, IContextMenu *handler,
            IFolderView2 *folderView) : owner(window), menu(popup), context(handler),
        source(window.shellView), view(folderView), generation(window.viewGeneration),
        serial(++window.viewMenuPreviewSerial) {}

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
        if (!current() || waiting || (tracking && currentCommand)) return;
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
            checkHR(restoreReopenSelection(source, selection, valid));
        }
        owner.resumeDeferredClose();
    }

    void apply(UINT command) {
        if (busy || !current() || command == currentCommand) return;
        positionChunks.clear();
        nextPositionChunk = 0;
        busy = true;
        int before = -1, after = -1;
        HRESULT counted = view->ItemCount(SVGIO_ALLVIEW, &before);
        if (!current()) { busy = false; return; }
        enumerationBaseline = owner.viewHost.enumerationSerial();
        ++owner.navigationDepth;
        HRESULT result;
        if (command) {
            auto info = owner.makeInvokeInfo(static_cast<int>(command - IDM_SHELL_FIRST), POINT{});
            info.hwnd = owner.hwnd;
            result = invokeShellCommand(context, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info));
        } else {
            FolderViewSettings presentation = initial.view;
            presentation.present &= FolderViewSettings::Mode | FolderViewSettings::Flags;
            result = restoreOptions(view, presentation);
        }
        owner.resumeDeferredClose();
        if (SUCCEEDED(result)) { currentCommand = command; changed = true; }
        if (current()) {
            HRESULT recounted = view->ItemCount(SVGIO_ALLVIEW, &after);
            if (current()) {
                waiting = (FAILED(counted) || FAILED(recounted) || before != after)
                    && owner.viewHost.enumerationSerial() <= enumerationBaseline;
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
        auto info = owner.makeInvokeInfo(static_cast<int>(command - IDM_SHELL_FIRST), POINT{});
        info.hwnd = owner.hwnd;
        checkHR(owner.invokeViewCommand(context, reinterpret_cast<CMINVOKECOMMANDINFO *>(&info)));
    }

    void enumerated(ULONG_PTR enumeration) {
        if (!busy && waiting && enumeration > enumerationBaseline && current()) {
            waiting = false;
            restoreDetails();
            if (!tracking && current()) {
                invokePending();
                if (!current()) return;
                owner.viewMenuPreview.reset();
                owner.scheduleStateSave();
            }
        }
    }

    void finish(UINT command) {
        tracking = false;
        // An outside click / Escape retains the original mode. A chosen entry
        // already previewed is committed without another round trip through it.
        // An entry outside the modes (sorting, auto arrange...) is not a mode:
        // the preview is cancelled first, then the entry runs the ordinary way.
        const bool mode = !command || isEntry(command);
        const UINT chosen = mode && command != initialCommand ? command : 0;
        if (!chosen) finishPositions();
        const bool applying = chosen != currentCommand;
        apply(chosen);
        if (!current()) return;
        if (!chosen && currentCommand) return; // Never persist a preview after failed cancellation.
        if (!mode) pendingCommand = command;
        // Returning to the initial mode already restored its details in apply().
        // Only a committed preview still needs its selection/focus restored here.
        if (!applying && chosen && changed && !waiting) restoreDetails();
        if (!waiting) {
            invokePending();
            if (!current()) return;
            owner.viewMenuPreview.reset();
            owner.scheduleStateSave();
        }
    }
};

thread_local FolderWindow::ViewMenuPreview *FolderWindow::ViewMenuPreview::active = nullptr;

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
    if (msg->message == WM_KEYDOWN && msg->wParam == L'T'
            && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0
            && GetKeyState(VK_MENU) >= 0) {
        // This window command also works while editing the address or filter.
        onCommand(IDM_REOPEN_WINDOW);
        return true;
    }
    if (msg->message == WM_KEYDOWN && msg->wParam == L'F'
            && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0)
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
    if (view->TranslateAccelerator(msg) == S_OK)
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

// MSAA supplies semantic roles without discovering or subclassing Shell controls.
static LONG gestureRoleAt(POINT point) {
    CComPtr<IAccessible> accessible;
    CComVariant child, role;
    if (FAILED(AccessibleObjectFromPoint(point, &accessible, &child)) || !accessible
            || FAILED(accessible->get_accRole(child, &role)) || role.vt != VT_I4)
        return 0;
    return role.lVal;
}

bool FolderWindow::handleResizeMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const bool handled = resizeGesture.handleMessage(message, wParam, lParam);
    if (resizeGesture.takeCompleted()) {
        deferredGestureClick = {};
        if (isWindowOperational()) persistViewState();
    }
    return handled;
}

void FolderWindow::cancelAltGesture() {
    resizeGesture.cancel();
    if (resizeGesture.takeCompleted() && isWindowOperational()) persistViewState();
}

bool FolderWindow::handleAltMouseMessage(MSG *msg) {
    if (handleResizeMessage(msg->message, msg->wParam, msg->lParam)) return true;
    if (msg->message == WM_SYSKEYDOWN && msg->wParam == VK_MENU) suppressAltUp = false;
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
    // A window gesture consumes its first click. Prepare that click's selection
    // through MSAA before the native double-click. Replaying a button-down while
    // the second press is physically held can start an unintended Shell drag.
    if (msg->message == WM_LBUTTONDBLCLK) {
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
            CComPtr<IAccessible> accessible;
            CComVariant child, role, state;
            if (SUCCEEDED(AccessibleObjectFromPoint(click.pt, &accessible, &child)) && accessible
                    && SUCCEEDED(accessible->get_accRole(child, &role)) && role.vt == VT_I4
                    && role.lVal == ROLE_SYSTEM_LISTITEM) {
                LONG flags = SELFLAG_NONE;
                if (click.wParam & MK_CONTROL) {
                    if (FAILED(accessible->get_accState(child, &state)) || state.vt != VT_I4) return false;
                    flags |= state.lVal & STATE_SYSTEM_SELECTED ? SELFLAG_REMOVESELECTION : SELFLAG_ADDSELECTION;
                } else {
                    flags |= SELFLAG_TAKESELECTION;
                }
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                CComQIPtr<IFolderView2> folderView(view);
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                if (!folderView) return false;
                // TAKEFOCUS in the MSAA proxy can synthesize Alt restoration
                // after the real key release. Resolve the pointed item through
                // a selection-only operation, then focus its public Shell index.
                std::vector<CComHeapPtr<ITEMID_CHILD>> previousSelection;
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
                if (click.wParam & MK_CONTROL) {
                    checkHR(view->SelectItem(nullptr, SVSI_DESELECTOTHERS));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    for (const auto &id : previousSelection) {
                        checkHR(view->SelectItem(id, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                        if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    }
                    checkHR(accessible->accSelect(flags, child));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                if (pointed) {
                    DWORD selection = 0;
                    checkHR(folderView->GetSelectionState(pointed, &selection));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    checkHR(view->SelectItem(pointed, (selection & SVSI_SELECT) | SVSI_FOCUSED | SVSI_SELECTIONMARK | SVSI_NOTAKEFOCUS));
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
    const bool moving = matchesShortcut(bindings.move, button, modifiers);
    if (!moving && !matchesShortcut(bindings.resize, button, modifiers)) return false;
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
    if (!moving || button != GestureButton::Left)
        return resizeGesture.begin(owner, owner, point, button, moving);
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
    if (historyLimit && !getFolderStateKey().empty()) {
        auto store = folderStateStore();
        std::vector<std::string> queries;
        if (!store || !store->readSearchHistory(getFolderStateKey(), historyLimit, &queries))
            stateStorageError();
        else for (const auto &query : queries) {
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                query.c_str(), -1, nullptr, 0);
            if (length <= 1) continue;
            std::wstring value(static_cast<size_t>(length), L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, query.c_str(), -1,
                    &value[0], length)) {
                value.pop_back();
                searchHistory.push_back(std::move(value));
            }
        }
    }
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

    if (!checkHR(viewHost.initialize(hwnd, browserRect, folderSettings(),
            static_cast<IServiceProvider *>(this), this)))
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


void FolderWindow::stopStartupIcons() {
    KillTimer(hwnd, TIMER_STARTUP_ICONS);
    startupIconsDeadline = 0;
    if (startupIconsThread) { startupIconsThread->stop(); startupIconsThread.Release(); }
}

void FolderWindow::preloadVisibleIcons(IFolderView2 *view) {
    if (!viewHost.contentMasked() || startupIconsThread) return;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() { return isWindowOperational() && generation == viewGeneration; };
    HWND viewWindow = nullptr;
    RECT viewport = {};
    POINT spacing = {};
    CComQIPtr<IShellView> currentView(view);
    HRESULT geometry = currentView ? currentView->GetWindow(&viewWindow) : E_NOINTERFACE;
    if (SUCCEEDED(geometry) && !GetClientRect(viewWindow, &viewport)) geometry = E_FAIL;
    if (SUCCEEDED(geometry)) geometry = view->GetSpacing(&spacing);
    UINT mode = FVM_AUTO;
    if (SUCCEEDED(geometry)) geometry = view->GetCurrentViewMode(&mode);
    const bool ordered = mode == FVM_DETAILS || mode == FVM_LIST || view->GetAutoArrange() == S_OK;
    if (!current()) return;
    if (FAILED(geometry)) { viewHost.revealContent(); return; }
    spacing.x = std::max<LONG>(spacing.x, 1);
    spacing.y = std::max<LONG>(spacing.y, 1);
    CComPtr<StartupIconsThread> job;
    job.Attach(new (std::nothrow) StartupIconsThread(hwnd, generation));
    if (job) {
        try {
            int index = -1;
            for (;;) {
                int next = -1;
                const HRESULT found = view->GetVisibleItem(index, FALSE, &next);
                if (!current()) return;
                if (found == S_FALSE) break;
                if (FAILED(found) || next <= index) { job.Release(); break; }
                index = next;
                CComHeapPtr<ITEMID_CHILD> child;
                POINT position = {};
                HRESULT hr = view->Item(index, &child);
                if (SUCCEEDED(hr)) hr = view->GetItemPosition(child, &position);
                if (!current()) return;
                if (FAILED(hr)) { job.Release(); break; }
                // Ordered rows/columns beyond the viewport cannot become visible
                // later in the same enumeration. Free icon positions still need a scan.
                if (ordered && (mode == FVM_LIST ? position.x >= viewport.right : position.y >= viewport.bottom)) break;
                RECT bounds = {position.x, position.y, position.x + spacing.x, position.y + spacing.y};
                RECT visible = {};
                if (!IntersectRect(&visible, &viewport, &bounds)) continue;
                CComPtr<IShellItem> shown, native;
                hr = view->GetItem(index, IID_PPV_ARGS(&shown));
                if (SUCCEEDED(hr)) hr = nativeShellItem(shown, &native);
                if (!current()) return;
                if (FAILED(hr)) { job.Release(); break; }
                job->items.emplace_back();
                hr = SHGetIDListFromObject(native, &job->items.back().id);
                if (!current()) return;
                if (FAILED(hr)) { job.Release(); break; }
            }
        } catch (const std::bad_alloc &) { job.Release(); }
    }
    if (!current()) return;
    if (job && !job->items.empty() && SetTimer(hwnd, TIMER_STARTUP_ICONS, 500, nullptr)) {
        startupIconsDeadline = GetTickCount64() + 500;
        startupIconsThread = job;
        if (job->start()) return;
        stopStartupIcons();
    }
    viewHost.revealContent();
}

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
    if (icons) viewStateDirty(FolderState::Icons);
    // Coalesce related native notifications; no polling and no database write per mouse move.
    SetTimer(hwnd, TIMER_SAVE_STATE, 250, nullptr);
}

void FolderWindow::synchronizeViewProvider() {
    if (!isWindowOperational() || !viewReady || !initialStateApplied || restoringState
            || viewMenuPreview || navigationDepth) return;
    if (!settings::getMixedFolderSorting() && !viewHost.mixedSortingPreference()) return;
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation;
    };
    CComQIPtr<IFolderView2> view(source);
    if (!view || !current()) return;
    const bool free = spatialView(view);
    if (!current()) return;
    const bool mixed = settings::getMixedFolderSorting() && !free;
    if (mixed == viewHost.mixedSortingPreference()) return;
    // A view menu can still hold the old provider while previewing its options.
    GUITHREADINFO gui = {sizeof(gui)};
    if (GetGUIThreadInfo(GetCurrentThreadId(), &gui)
            && (gui.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) {
        scheduleStateSave();
        return;
    }
    // Switching back from an arranged view must not save its temporary grid
    // over the folder's last free placement.
    if (free) loadIconPositions();
    if (!current()) return;
    viewStateDirty(FolderState::Icons);
    persistViewState();
    if (current()) onItemChanged(); // Existing canonical positions/selection transfer.
}

uint32_t FolderWindow::captureViewState(uint32_t mask) {
    uint32_t captured = ItemWindow::captureViewState(mask);
    if (!viewHost || !viewReady || !initialStateApplied || restoringState || viewMenuPreview
            || getSavedFolderState().firstFailedAt) return captured;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    const HWND owner = hwnd;
    auto current = [&]() { return hwnd == owner && shellView == source && viewGeneration == generation; };
    CComPtr<IFolderView2> view;
    if (FAILED(viewHost.currentView(IID_PPV_ARGS(&view))) || !current()) return captured;
    FolderViewSettings options = captureOptions(view, getSavedFolderState().view);
    if (!current()) return captured;
    if (options.present && (!(getSavedFolderState().present & FolderState::View)
            || !sameView(options, getSavedFolderState().view) || (mask & FolderState::View))) {
        getSavedFolderState().view = std::move(options);
        viewStateDirty(FolderState::View);
        captured |= FolderState::View;
    }
    // A filtered subset must never replace the complete folder's icon positions.
    if ((mask & FolderState::Icons) && spatialView(view)) {
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

void FolderWindow::loadIconPositions() {
    if (!viewHost || !loadFolderState() || !(getSavedFolderState().present & FolderState::Icons)) return;
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation;
    };
    CComPtr<IFolderView> view;
    if (SUCCEEDED(viewHost.currentView(IID_PPV_ARGS(&view))) && current() && spatialView(view)
            && current() && !getSavedFolderState().icons.empty()) {
        restoringState = true;
        checkHR(restoreIconPositions(view, getSavedFolderState().icons, current));
        if (current()) restoringState = false;
    }
}

std::vector<uint8_t> FolderWindow::captureReopenViewState() {
    if (viewMenuPreview) return viewMenuPreview->initial.selection;
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
    CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto valid = [&]() { return isWindowOperational() && shellView == view && viewGeneration == generation; };
    checkHR(restoreReopenSelection(view, bytes, valid));
    if (valid()) updateSelection();
}

void FolderWindow::onDestroy() {
    stopStartupIcons();
    if (viewMenuPreview) restoringState = true;
    viewMenuPreview.reset();
    resizeGesture.stop();
    checkHR(viewHost.stopObserving());
    KillTimer(hwnd, TIMER_SAVE_STATE);
    viewStateDirty(FolderState::Icons);
    persistViewState();
    saveSearchHistory();
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
    shellView.Release();
    selected.Release();
    viewReady = false;
    ItemWindow::onDestroy();
    viewHost.destroy();
    nameFilter.Release();
}

bool FolderWindow::onCommand(WORD command) {
    switch (command) {
        case IDM_QUICK_ACCESS:
            openQuickAccessMenu(clientToScreen(hwnd, {0, 0}));
            return true;
        case IDM_NEW_FOLDER:
            newItem(CMDSTR_NEWFOLDERA);
            return true;
        case IDM_NEW_TEXT_FILE:
            newItem(".txt");
            return true;
    }
    return ItemWindow::onCommand(command);
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
    if (searchText == text) {
        return;
    }
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IFolderFilter> filter = nameFilter;
    CComPtr<IShellView> view = shellView;
    if (!isWindowOperational())
        return;
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
        enumerationComplete = false;
        restoreIconsPending = searchText.empty();
        hr = view ? refreshView() : S_OK;
        if (FAILED(hr) && isWindowOperational()) {
            activeFilter->setQuery(previousText.c_str());
            searchText = previousText;
            if (view)
                refreshView();
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
    searchHistoryDirty = true;
    scheduleStateSave();
}

void FolderWindow::saveSearchHistory() {
    if (!searchHistoryDirty || getFolderStateKey().empty()) return;
    std::vector<std::string> queries;
    for (const auto &query : searchHistory) queries.push_back(toUtf8(query.c_str()));
    auto store = folderStateStore();
    if (store && store->saveSearchHistory(getFolderStateKey(), queries, settings::getSearchHistoryLimit()))
        searchHistoryDirty = false;
    else stateStorageError();
}

void FolderWindow::openSearchMenu() {
    CComPtr<ItemWindow> keepAlive(this);
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
    activationClickPending = state == WA_CLICKACTIVE;
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
    if (!limit) searchHistoryDirty = false; // The preference already cleared persisted histories.
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
    FolderViewSettings presentation;
    if (storedViewState) presentation = storedViewState->view;
    else if (loadFolderState() && (getSavedFolderState().present & FolderState::View))
        presentation = getSavedFolderState().view;
    if (!isWindowOperational()) { resumeDeferredClose(); return E_ABORT; }
    const UINT mode = (presentation.present & FolderViewSettings::Mode)
        ? static_cast<UINT>(presentation.mode) : FVM_DETAILS;
    const DWORD flags = (presentation.present & FolderViewSettings::Flags)
        ? presentation.flags : FWF_AUTOARRANGE;
    const bool free = mode != FVM_DETAILS && mode != FVM_LIST && !(flags & FWF_AUTOARRANGE);
    const HRESULT result = viewHost.navigate(currentItem, settings::getMixedFolderSorting() && !free, isolated);
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

void FolderWindow::resumeDeferredClose() {
    --navigationDepth;
    if (navigationDepth || !isWindowOperational()) return;
    const UINT closeMessage = deferredCloseMessage;
    deferredCloseMessage = 0;
    if (closeMessage)
        checkLE(PostMessageW(hwnd, closeMessage, 0, 0));
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
    LPARAM testNamespace = 0;
#ifdef FILESPACER_DEBUG
    testNamespace = settings::testMode ? 1 : 0;
#endif
    if (settingsChangedMessage && message == settingsChangedMessage && lParam == testNamespace
            && wParam >= 1 && wParam <= settings::MAX_SEARCH_HISTORY_LIMIT + 1) {
        const size_t limit = static_cast<size_t>(wParam - 1);
        if (searchHistory.size() > limit) searchHistory.resize(limit);
        if (!limit) searchHistoryDirty = false;
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
            }
            return 0;
        case MSG_STARTUP_ICONS:
            if (isWindowOperational() && viewHost.isViewNotification(wParam)) {
                stopStartupIcons(); viewHost.revealContent();
            }
            return 0;
        case MSG_VIEW_CHANGED:
            if (isWindowOperational() && viewHost.isViewNotification(wParam))
                scheduleStateSave();
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
                checkHR(refreshView());
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
            break;
        case WM_TIMER:
            if (wParam == TIMER_STARTUP_ICONS) {
                if (startupIconsThread && GetTickCount64() >= startupIconsDeadline) {
                    stopStartupIcons(); viewHost.revealContent();
                }
                return 0;
            }
            if (wParam == TIMER_SAVE_STATE) {
                KillTimer(hwnd, TIMER_SAVE_STATE);
                synchronizeViewProvider();
                if (!isWindowOperational()) return 0;
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
        if (nativeViewSelection(folderView, FALSE, &selection) == S_OK && isWindowOperational()) {
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

        if (nativeViewSelection(folderView, FALSE, &selection) == S_OK && isWindowOperational()) {
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
    if (shellView)
        checkHR(shellView->SelectItem(nullptr, SVSI_DESELECTOTHERS)); // keep focus
    selected = nullptr; // in case this happened in the background
}


IDispatch * FolderWindow::getShellViewDispatch() {
    return this;
}

void FolderWindow::onItemChanged() {
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
            if (spatialView(folderView))
                checkHR(captureIconPositions(folderView, current, enumerationComplete, {}, state->icons));
            if (!current()) return;
            checkHR(captureReopenSelection(source, current, state->selection));
            if (!current()) return;
            storedViewState = std::move(state);
        }

        checkHR(viewHost.stopObserving());
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
        enumerationComplete = false;
        restoreIconsPending = searchText.empty();
        HRESULT hr = refreshView();
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
    if (!isWindowOperational())
        return;
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
        preview->finish(cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST ? static_cast<UINT>(cmd) : 0);
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
    // Preserve the Shell's native handling of Alt-modified activation.
    if (GetKeyState(VK_MENU) < 0)
        return S_FALSE;

    const bool closeSource = settings::getKeepSourceWindowOpen()
        == (GetKeyState(VK_CONTROL) < 0);
    CComQIPtr<IFolderView2> folderView(view);

    int numSelected = 0;
    if (folderView
            && checkHR(folderView->ItemCount(SVGIO_SELECTION, &numSelected))
            && numSelected == 1) {

        CComPtr<IShellItemArray> selection;
        if (nativeViewSelection(folderView, FALSE, &selection) == S_OK) {
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
    stopStartupIcons();
    CComPtr<ItemWindow> keepAlive(this);
    viewMenuPreview.reset();
    cancelAltGesture();
    debugPrintf(L"NATIVE HWND=%p OnNavigationPending\n", hwnd);
    checkHR(viewHost.stopObserving());
    viewGeneration = 0;
    viewReady = false;
    initialEnumerationReady = false;
    viewOptionsApplied = false;
    viewOptionsEnumerationBaseline = 0;
    initialStateApplied = false;
    restoreIconsPending = true;
    enumerationComplete = false;
    return S_OK;
}

STDMETHODIMP FolderWindow::OnNavigationComplete(PCIDLIST_ABSOLUTE) {
    CComPtr<ItemWindow> keepAlive(this);
    debugPrintf(L"NATIVE HWND=%p OnNavigationComplete\n", hwnd);
    recordFolderAccess(S_OK);
    if (shellView && GetActiveWindow() == hwnd) {
        CComPtr<IShellView> view = shellView;
        checkHR(view->UIActivate(SVUIA_ACTIVATE_FOCUS));
    }
    if (!isWindowOperational()) return S_OK;
    viewReady = true; // Navigation completion alone does not mean enumeration completed.
    finishViewInitialization();
    return S_OK;
}

STDMETHODIMP FolderWindow::OnNavigationFailed(PCIDLIST_ABSOLUTE) {
    stopStartupIcons(); viewHost.revealContent();
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
    debugPrintf(L"NATIVE HWND=%p OnViewCreated\n", hwnd);
    shellView = view;
    initialEnumerationReady = false;
    viewOptionsApplied = false;
    viewOptionsEnumerationBaseline = 0;
    initialStateApplied = false;
    restoreIconsPending = true;
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

    if (!current()) return S_OK;
    setSingleSelectionLabels(view, settings::getFullNamesOnSelection());
    if (!current()) return S_OK;
    restoringState = false;
    finishViewInitialization();

    return S_OK;
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
            checkHR(restoreOptions(folderView, options));
            if (!current()) return;
            viewOptionsEnumerationBaseline = viewHost.enumerationSerial();
            const HRESULT afterResult = folderView->ItemCount(SVGIO_ALLVIEW, &after);
            if (!current()) return;
            restoringState = false;
            // Counts invalidate an earlier completion; they never establish
            // initial readiness (an enumerated empty folder is valid).
            if (FAILED(beforeResult) || FAILED(afterResult) || before != after) {
                enumerationComplete = false;
                return;
            }
        }
        restoringState = false;
    }
    const bool initial = !initialStateApplied;
    initialStateApplied = true;
    if (restoreIconsPending) {
        restoreIconsPending = false;
        if (initial && storedViewState) {
            auto state = std::move(storedViewState);
            CComQIPtr<IFolderView2> folderView(view);
            if (folderView) {
                restoringState = true;
                if (!state->icons.empty() && spatialView(folderView))
                    checkHR(restoreIconPositions(folderView, state->icons, current));
                if (!current()) return;
                checkHR(restoreReopenSelection(view, state->selection, current));
                if (!current()) return;
                restoringState = false;
            }
        } else if (searchText.empty()) {
            loadIconPositions();
        }
    }
    if (!current()) return;
    if (initial) {
        restoreReopenViewState();
        if (!current()) return;
        onViewReady();
    }
    if (!current()) return;
    scheduleStateSave();
    updateSelection();
    if (current() && hasStatusText()) updateStatus();
    if (current() && viewHost.contentMasked()) {
        CComQIPtr<IFolderView2> folderView(view);
        if (folderView) preloadVisibleIcons(folderView);
        else viewHost.revealContent();
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
