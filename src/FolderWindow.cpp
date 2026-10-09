#include "FolderWindow.h"
#include "CreateItemWindow.h"
#include "GeomUtils.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "Settings.h"
#include "CrashRecovery.h"
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
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#pragma comment(lib, "Propsys.lib")

// Example of how to host an IExplorerBrowser:
// https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/winui/shell/appplatform/ExplorerBrowserCustomContents

namespace filespacer {

static constexpr int BOTTOM_STATUS_HEIGHT_DIP = 23;
static constexpr UINT WM_APP_REBUILD_STANDARD_VIEW = WM_APP + 43;
// Coalesce selection notifications before updating the selected item and status.
static constexpr UINT SELECTION_UPDATE_DELAY_MS = 100;

const EXPLORER_BROWSER_OPTIONS BROWSER_OPTIONS =
    EBO_NOBORDER | EBO_NOTRAVELLOG | EBO_NOPERSISTVIEWSTATE;

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

// One immutable filter per submitted query. The Shell may call it off the UI thread;
// it owns no window, browser, or apartment-bound Shell object.
class FolderNameFilter final : public IFolderFilter {
public:
    explicit FolderNameFilter(const wchar_t *text) : query(text),
        wildcard(query.find_first_of(L"*?") != std::wstring::npos) {}

    HRESULT initialize() {
        return CoCreateFreeThreadedMarshaler(this, &marshaler);
    }

    STDMETHODIMP QueryInterface(REFIID id, void **object) override {
        if (!object)
            return E_POINTER;
        *object = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IFolderFilter)) {
            *object = static_cast<IFolderFilter *>(this);
            AddRef();
            return S_OK;
        }
        if (id == IID_IMarshal && marshaler)
            return marshaler->QueryInterface(id, object);
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&references));
    }

    STDMETHODIMP_(ULONG) Release() override {
        const LONG count = InterlockedDecrement(&references);
        if (!count)
            delete this;
        return static_cast<ULONG>(count);
    }

    STDMETHODIMP GetEnumFlags(IShellFolder *, PCIDLIST_ABSOLUTE, HWND *,
            DWORD *flags) override {
        if (!flags)
            return E_POINTER;
        SHELLSTATE state = {};
        SHGetSetSettings(&state, SSF_SHOWALLOBJECTS | SSF_SHOWSUPERHIDDEN, FALSE);
        *flags = SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_ENABLE_ASYNC;
        if (state.fShowAllObjects) {
            *flags |= SHCONTF_INCLUDEHIDDEN;
            if (state.fShowSuperHidden)
                *flags |= SHCONTF_INCLUDESUPERHIDDEN;
        }
        return S_OK;
    }

    STDMETHODIMP ShouldShow(IShellFolder *folder, PCIDLIST_ABSOLUTE folderID,
            PCUITEMID_CHILD childID) override {
        if (!folder || !childID)
            return E_INVALIDARG;
        // SetFilter replaces IncludeObject; retain its Desktop exclusions.
        if (folderID && ILIsEmpty(folderID)) {
            for (const auto &hidden : hiddenItemIDs) {
                if (hidden && ILIsEqual(childID, hidden))
                    return S_FALSE;
            }
        }
        SFGAOF attributes = SFGAO_FILESYSTEM;
        const bool filesystem = SUCCEEDED(folder->GetAttributesOf(1, &childID, &attributes))
            && (attributes & SFGAO_FILESYSTEM);
        STRRET result = {};
        const SHGDNF flags = filesystem
            ? static_cast<SHGDNF>(SHGDN_INFOLDER | SHGDN_FORPARSING) : SHGDN_INFOLDER;
        HRESULT hr = folder->GetDisplayNameOf(childID, flags, &result);
        if (FAILED(hr))
            return hr;
        CComHeapPtr<wchar_t> name;
        hr = StrRetToStr(&result, childID, &name);
        if (FAILED(hr))
            return hr;
        // Filesystem names include extensions even when the view hides them.
        const wchar_t *fullName = name;
        const wchar_t *filename = filesystem ? PathFindFileName(fullName) : fullName;
        if (wildcard)
            return PathMatchSpecEx(filename, query.c_str(),
                PMSF_NORMAL | PMSF_DONT_STRIP_SPACES);
        return StrStrIW(filename, query.c_str()) ? S_OK : S_FALSE;
    }

private:
    LONG references = 1;
    const std::wstring query;
    const bool wildcard;
    CComPtr<IUnknown> marshaler;
};

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
    if (!isWindowOperational())
        return false;
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

void FolderWindow::onCreate() {
    ItemWindow::onCreate();
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

    if (!checkHR(browser.CoCreateInstance(__uuidof(ExplorerBrowser))))
        return;
    checkHR(browser->SetOptions(BROWSER_OPTIONS));
    const bool requestedFullNames = recovery::canUseFullNames() && settings::getFullNamesOnSelection();
    CComQIPtr<IFolderViewOptions> options(browser);
    if (requestedFullNames && !options) {
        fullNamesFailure = IDS_LABEL_NO_OPTIONS;
        recovery::blockFullNames();
        settings::disableFullNamesOnSelection();
    } else if (options) {
        const auto customFlags = FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING;
        const HRESULT hr = options->SetFolderViewOptions(customFlags,
            requestedFullNames ? customFlags : FVO_DEFAULT);
        fullNamesOnSelection = SUCCEEDED(hr) && requestedFullNames;
        if (requestedFullNames && FAILED(hr)) {
            fullNamesFailure = IDS_LABEL_OPTIONS_FAILED;
            recovery::blockFullNames();
            settings::disableFullNamesOnSelection();
            // A failed option change can be partial. Do not reuse that browser.
            browser.Release();
            if (!checkHR(browser.CoCreateInstance(__uuidof(ExplorerBrowser)))) return;
            checkHR(browser->SetOptions(BROWSER_OPTIONS));
        }
    }
    if (!checkHR(browser->Initialize(hwnd, &browserRect, tempPtr(folderSettings())))) {
        browser = nullptr;
        return;
    }

    checkHR(IUnknown_SetSite(browser, (IServiceProvider *)this));
    checkHR(browser->Advise(this, &eventsCookie));
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
    settings.ViewMode = FVM_DETAILS; // also set in initDefaultView
    settings.fFlags = FWF_NOWEBVIEW | FWF_NOHEADERINALLVIEWS;
    return settings;
}

void FolderWindow::initDefaultView(IFolderView2 *const folderView) {
    // FVM_SMALLICON only seems to work if it's also specified with an icon size
    // https://docs.microsoft.com/en-us/windows/win32/menurc/about-icons
    checkHR(folderView->SetCurrentViewMode(FVM_DETAILS));
    checkHR(folderView->SetCurrentFolderFlags(FWF_AUTOARRANGE, FWF_AUTOARRANGE));
}

void FolderWindow::detachListView() {
    if (listView)
        RemoveWindowSubclass(listView, listViewSubclassProc, 0);
    if (listViewOwner)
        RemoveWindowSubclass(listViewOwner, listViewOwnerProc, 0);
    listView = nullptr;
    listViewOwner = nullptr;
}

void FolderWindow::listViewCreated() {
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> view = shellView;
    if (!view || !isWindowOperational())
        return;
    detachListView();
    firstODDispInfo = false;
    HWND viewWindow = nullptr;
    if (!checkHR(view->GetWindow(&viewWindow)) || !isWindowOperational() || !viewWindow
            || GetWindowThreadProcessId(viewWindow, nullptr) != GetCurrentThreadId()) {
        if (fullNamesOnSelection) disableFullNames(IDS_LABEL_NO_CONTROL);
        return;
    }

    // Compatibility workaround: the Shell does not expose its internal ListView.
    // Start from IShellView's public HWND, without assuming private class names or depth.
    // Accept only one standard ListView on our UI thread; never guess between candidates.
    struct Candidates { HWND window = nullptr; bool ambiguous = false; } candidates;
    EnumChildWindows(viewWindow, [](HWND child, LPARAM data) -> BOOL {
        wchar_t className[32];
        if (GetClassName(child, className, _countof(className))
                && lstrcmp(className, WC_LISTVIEW) == 0
                && GetWindowThreadProcessId(child, nullptr) == GetCurrentThreadId()) {
            auto &found = *reinterpret_cast<Candidates *>(data);
            if (found.window) {
                found.ambiguous = true;
                return FALSE;
            }
            found.window = child;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&candidates));
    HWND control = candidates.ambiguous ? nullptr : candidates.window;
    if (fullNamesOnSelection && !control) {
        disableFullNames(candidates.ambiguous ? IDS_LABEL_AMBIGUOUS_CONTROL : IDS_LABEL_NO_CONTROL);
        return;
    }
    HWND owner = control ? GetParent(control) : viewWindow;
    if (!owner || GetWindowThreadProcessId(owner, nullptr) != GetCurrentThreadId()) {
        if (fullNamesOnSelection) disableFullNames(IDS_LABEL_NO_CONTROL);
        return;
    }
    if (!checkLE(SetWindowSubclass(owner, listViewOwnerProc, 0, reinterpret_cast<DWORD_PTR>(this)))) {
        if (fullNamesOnSelection) disableFullNames(IDS_LABEL_INSTALL_FAILED);
        return;
    }
    listViewOwner = owner;
    if (control && checkLE(SetWindowSubclass(control, listViewSubclassProc, 0,
            reinterpret_cast<DWORD_PTR>(this))))
        listView = control;
    if (fullNamesOnSelection && !listView) disableFullNames(IDS_LABEL_INSTALL_FAILED);
}

void FolderWindow::disableFullNames(UINT reason) {
    fullNamesOnSelection = false;
    fullNamesFailure = reason;
    recovery::blockFullNames();
    // Session fallback still works if the preference cannot be persisted.
    const LSTATUS status = settings::disableFullNamesOnSelection();
    if (status != ERROR_SUCCESS) fullNamesFailure = IDS_RECOVERY_DISABLE_FAILED;
    rebuildStandardView = true;
    PostMessageW(hwnd, WM_APP_REBUILD_STANDARD_VIEW, 0, 0);
}

LRESULT CALLBACK FolderWindow::listViewSubclassProc(HWND hwnd, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    CComPtr<ItemWindow> keepAlive(reinterpret_cast<FolderWindow *>(refData));
    if (message == WM_NCDESTROY) {
        auto window = reinterpret_cast<FolderWindow *>(refData);
        RemoveWindowSubclass(hwnd, listViewSubclassProc, 0);
        if (window->listView == hwnd) {
            window->listView = nullptr;
            window->checkActivationDrag = false;
        }
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    if (message == WM_DESTROY
            || !reinterpret_cast<FolderWindow *>(refData)->isWindowOperational())
        return DefSubclassProc(hwnd, message, wParam, lParam);
    if (message == WM_MOUSEACTIVATE || message == WM_CANCELMODE
            || message == WM_KILLFOCUS || message == WM_LBUTTONUP
            || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN)
        reinterpret_cast<FolderWindow *>(refData)->checkActivationDrag = false;
    if (message == WM_MOUSEACTIVATE && LOWORD(lParam) == HTCLIENT
            && HIWORD(lParam) == WM_LBUTTONDOWN) {
        FolderWindow *window = (FolderWindow *)refData;
        POINT point = {};
        // Activate immediately, then distinguish a background click from a
        // selection drag when the original button-down message arrives.
        if (GetForegroundWindow() != window->hwnd && settings::getKeepSelectionOnActivate()
                && GetCursorPos(&point) && WindowFromPoint(point) == hwnd
                && ScreenToClient(hwnd, &point) && ListView_GetSelectedCount(hwnd) > 0) {
            LVHITTESTINFO hitTest = {};
            hitTest.pt = point;
            const UINT controls = (LVHT_EX_GROUP & ~LVHT_EX_GROUP_BACKGROUND) | LVHT_EX_FOOTER;
            if (ListView_HitTestEx(hwnd, &hitTest) == -1
                    && (hitTest.flags & LVHT_NOWHERE) && !(hitTest.flags & controls)
                    && window->isWindowOperational() && window->listView == hwnd) {
                window->checkActivationDrag = true;
                return MA_ACTIVATE;
            }
        }
    } else if (message == WM_LBUTTONDOWN) {
        auto window = reinterpret_cast<FolderWindow *>(refData);
        const bool checkDrag = window->checkActivationDrag;
        window->checkActivationDrag = false;
        if (checkDrag && !(wParam & (MK_CONTROL | MK_SHIFT))) {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (ClientToScreen(hwnd, &point)) {
                // DragDetect uses the Windows drag threshold and consumes a
                // simple click. A drag forwards this original button-down to
                // the Shell; no synthetic mouse input or selection restore.
                const BOOL dragging = DragDetect(hwnd, point);
                if (!window->isWindowOperational() || window->listView != hwnd || !dragging)
                    return 0;
            }
        }
    } else if (message == WM_RBUTTONDOWN) {
        FolderWindow *window = (FolderWindow *)refData;
        LVHITTESTINFO hitTest = {};
        hitTest.pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int item = ListView_HitTestEx(hwnd, &hitTest);
        // Defer selection updates only for a context click on the existing
        // selection. Empty space and unselected items follow the Shell normally.
        if (item >= 0 && (ListView_GetItemState(hwnd, item, LVIS_SELECTED) & LVIS_SELECTED)
                && window->isWindowOperational() && window->listView == hwnd) {
            window->handlingRButtonDown = true;
            LRESULT res = DefSubclassProc(hwnd, message, wParam, lParam);
            window->handlingRButtonDown = false;
            if (!window->isWindowOperational())
                return res;
            if (window->selectedWhileHandlingRButtonDown) {
                window->scheduleUpdateSelection();
                window->selectedWhileHandlingRButtonDown = false;
            }
            return res;
        }
    } else if (message == WM_NOTIFY) {
        NMHDR *nmHdr = (NMHDR *)lParam;
        if (nmHdr->code == HDN_ITEMCHANGED)
            PostMessage(hwnd, WM_SETREDRAW, 1, 0); // Windows 11 likes to turn this off, why???
    }
    if (!reinterpret_cast<FolderWindow *>(refData)->isWindowOperational())
        return 0;
    if (message == WM_LBUTTONUP || message == WM_KEYUP || message == WM_MOUSEWHEEL
            || message == LVM_SETVIEW || message == LVM_SETCOLUMNWIDTH
            || message == LVM_SETCOLUMNORDERARRAY || message == LVM_SETIMAGELIST
            || message == LVM_ENABLEGROUPVIEW || message == LVM_SORTITEMS
            || message == LVM_SORTITEMSEX || message == LVM_SETEXTENDEDLISTVIEWSTYLE) {
        auto window = reinterpret_cast<FolderWindow *>(refData);
        window->scheduleStateSave(message == WM_LBUTTONUP);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK FolderWindow::listViewOwnerProc(HWND hwnd, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
			
    FolderWindow *window = (FolderWindow *)refData;
    CComPtr<ItemWindow> keepAlive(window);
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, listViewOwnerProc, 0);
        if (window->listViewOwner == hwnd)
            window->listViewOwner = nullptr;
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    if (message == WM_CLOSE || message == WM_DESTROY
            || !window->isWindowOperational())
        return DefSubclassProc(hwnd, message, wParam, lParam);

    {
        CComPtr<IContextMenu3> menu3 = window->contextMenu3;
        CComPtr<IContextMenu2> menu2 = window->contextMenu2;
        if (menu3 && window->isWindowOperational()) {
            LRESULT result = 0;
            if (SUCCEEDED(menu3->HandleMenuMsg2(message, wParam, lParam, &result)))
                return result;
        } else if (menu2 && window->isWindowOperational()) {
            if (SUCCEEDED(menu2->HandleMenuMsg(message, wParam, lParam)))
                return 0;
        }
    }
    if (!window->isWindowOperational())
        return 0;
    if (message == WM_NOTIFY) {
        NMHDR *nmHdr = (NMHDR *)lParam;
        if (nmHdr->code == HDN_ENDTRACKW || nmHdr->code == HDN_ENDTRACKA
                || nmHdr->code == HDN_ENDDRAG || nmHdr->code == LVN_COLUMNCLICK)
            window->scheduleStateSave();
        if (window->fullNamesOnSelection && window->listView
                && nmHdr->hwndFrom == window->listView && nmHdr->code == NM_CUSTOMDRAW) {
            auto customDraw = reinterpret_cast<NMLVCUSTOMDRAW *>(lParam);
            const HWND control = nmHdr->hwndFrom;
            const DWORD stage = recovery::readLabelDraw(customDraw).stage;
            if (stage == CDDS_PREPAINT || stage == CDDS_ITEMPREPAINT) {
                // Preserve the Shell's draw handler and all its requested draw stages.
                const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
                if (!window->isWindowOperational() || window->listView != control
                        || window->listViewOwner != hwnd || (result & CDRF_SKIPDEFAULT))
                    return result;
                if (stage == CDDS_PREPAINT)
                    return result | CDRF_NOTIFYITEMDRAW;

                // Keep the tested opaque-label workaround. The label geometry and text
                // remain native; only the background is filled before native drawing.
                // Its integration into the Shell's internal control is NOT a public contract.
                const auto draw = recovery::readLabelDraw(customDraw);
                if (draw.itemType != LVCDI_ITEM
                        || !(draw.state & CDIS_SELECTED)
                        || GetFocus() != control || recovery::labelView(control) != LV_VIEW_ICON)
                    return result;
                if (!window->isWindowOperational() || window->listView != control
                        || draw.item > INT_MAX)
                    return result;
                RECT label = {}, client = {}, visible = {};
                if (!recovery::labelRect(control, static_cast<int>(draw.item), &label) || !window->isWindowOperational()
                        || window->listView != control || !GetClientRect(control, &client)
                        || !IntersectRect(&visible, &label, &client))
                    return result;
                COLORREF background = recovery::labelBackground(control);
                if (!window->isWindowOperational() || window->listView != control)
                    return result;
                if (background == CLR_NONE)
                    background = GetSysColor(COLOR_WINDOW);
                recovery::fillLabel(draw.dc, &visible, background);
                return result;
            }
        }
        if (nmHdr->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *nmLV = (NMLISTVIEW *)nmHdr;
            if ((nmLV->uChanged & LVIF_STATE)) {
                if ((nmLV->uOldState & LVIS_SELECTED) != (nmLV->uNewState & LVIS_SELECTED)) {
                    window->selectionChanged();
                } else if (((nmLV->uOldState ^ nmLV->uNewState) & LVIS_FOCUSED)
                        && nmLV->iItem == -1) {
                    // seems to happen when window contents are modified while in the background
                    // TODO: this only works if window has been active once before
                    if (window->hasStatusText())
                        window->updateStatus();
                }
            }
        } else if (nmHdr->code == LVN_ODSTATECHANGED) {
            NMLVODSTATECHANGE *nmOD = (NMLVODSTATECHANGE *)nmHdr;
            if ((nmOD->uOldState & LVIS_SELECTED) != (nmOD->uNewState & LVIS_SELECTED))
                window->selectionChanged();
        } else if (nmHdr->code == LVN_GETDISPINFO && !window->firstODDispInfo) {
            window->firstODDispInfo = true;
            LRESULT res = DefSubclassProc(hwnd, message, wParam, lParam);
            // initial item count should be known at this point
            if (window->isWindowOperational() && window->hasStatusText())
                window->updateStatus();
            return res;
        }
    }
    if (!window->isWindowOperational())
        return 0;
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

namespace {
FolderViewSettings::Key stateKey(const PROPERTYKEY &key) {
    FolderViewSettings::Key result = {};
    std::memcpy(result.data(), &key.fmtid, 16);
    std::memcpy(result.data() + 16, &key.pid, 4);
    return result;
}
PROPERTYKEY propertyKey(const FolderViewSettings::Key &key) {
    PROPERTYKEY result = {};
    std::memcpy(&result.fmtid, key.data(), 16);
    std::memcpy(&result.pid, key.data() + 16, 4);
    return result;
}
bool sameView(const FolderViewSettings &a, const FolderViewSettings &b) {
    if (a.present != b.present || a.mode != b.mode || a.iconSize != b.iconSize
            || a.flags != b.flags || a.groupBy != b.groupBy
            || a.groupAscending != b.groupAscending
            || a.columns.size() != b.columns.size() || a.sort.size() != b.sort.size()) return false;
    for (size_t i = 0; i < a.columns.size(); ++i)
        if (a.columns[i].key != b.columns[i].key || a.columns[i].width != b.columns[i].width) return false;
    for (size_t i = 0; i < a.sort.size(); ++i)
        if (a.sort[i].key != b.sort[i].key || a.sort[i].direction != b.sort[i].direction) return false;
    return true;
}

FolderViewSettings captureOptions(IFolderView2 *view, const FolderViewSettings &previous) {
    FolderViewSettings state = previous; // Keep unsupported parts instead of writing empty values.
    FOLDERVIEWMODE mode;
    int iconSize;
    if (SUCCEEDED(view->GetViewModeAndIconSize(&mode, &iconSize))) {
        state.mode = mode; state.iconSize = iconSize;
        state.present |= FolderViewSettings::Mode;
    }
    DWORD flags;
    if (SUCCEEDED(view->GetCurrentFolderFlags(&flags))) {
        state.flags = flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID);
        state.present |= FolderViewSettings::Flags;
    }
    CComQIPtr<IColumnManager> manager(view);
    UINT count;
    if (manager && SUCCEEDED(manager->GetColumnCount(CM_ENUM_VISIBLE, &count)) && count) {
        std::vector<PROPERTYKEY> keys(count);
        std::vector<FolderViewSettings::Column> columns;
        bool complete = SUCCEEDED(manager->GetColumns(CM_ENUM_VISIBLE, keys.data(), count));
        if (complete) for (const auto &key : keys) {
            CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
            if (FAILED(manager->GetColumnInfo(key, &info))) { complete = false; break; }
            FolderViewSettings::Column column;
            column.key = stateKey(key);
            column.width = static_cast<uint32_t>(invScaleDPI(static_cast<int>(info.uWidth)));
            columns.push_back(column);
        }
        if (complete) { state.columns = std::move(columns); state.present |= FolderViewSettings::Columns; }
    }
    int sortCount;
    if (SUCCEEDED(view->GetSortColumnCount(&sortCount)) && sortCount > 0) {
        std::vector<SORTCOLUMN> columns(static_cast<size_t>(sortCount));
        if (SUCCEEDED(view->GetSortColumns(columns.data(), sortCount))) {
            state.sort.clear();
            for (const auto &column : columns) {
                FolderViewSettings::SortColumn sort;
                sort.key = stateKey(column.propkey); sort.direction = column.direction;
                state.sort.push_back(sort);
            }
            state.present |= FolderViewSettings::Sort;
        }
    }
    PROPERTYKEY group = {};
    BOOL ascending = FALSE;
    HRESULT groupResult = view->GetGroupBy(&group, &ascending);
    if (SUCCEEDED(groupResult)) {
        state.groupBy = groupResult == S_FALSE ? FolderViewSettings::Key{} : stateKey(group);
        state.groupAscending = groupResult == S_OK && ascending != FALSE;
        state.present |= FolderViewSettings::Group;
    }
    return state;
}

void restoreOptions(IFolderView2 *view, const FolderViewSettings &state) {
    if (state.present & FolderViewSettings::Mode)
        checkHR(view->SetViewModeAndIconSize(static_cast<FOLDERVIEWMODE>(state.mode), state.iconSize));
    if (state.present & FolderViewSettings::Flags)
        checkHR(view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID, state.flags));
    CComQIPtr<IColumnManager> manager(view);
    if (manager && (state.present & FolderViewSettings::Columns) && !state.columns.empty()) {
        std::vector<PROPERTYKEY> keys;
        for (const auto &column : state.columns) keys.push_back(propertyKey(column.key));
        if (checkHR(manager->SetColumns(keys.data(), static_cast<UINT>(keys.size())))) {
            for (const auto &column : state.columns) {
                CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
                info.uWidth = static_cast<UINT>(scaleDPI(static_cast<int>(column.width)));
                checkHR(manager->SetColumnInfo(propertyKey(column.key), &info));
            }
        }
    }
    if ((state.present & FolderViewSettings::Sort) && !state.sort.empty()) {
        std::vector<SORTCOLUMN> columns;
        for (const auto &column : state.sort)
            columns.push_back({propertyKey(column.key), static_cast<SORTDIRECTION>(column.direction)});
        checkHR(view->SetSortColumns(columns.data(), static_cast<int>(columns.size())));
    }
    if (state.present & FolderViewSettings::Group)
        checkHR(view->SetGroupBy(propertyKey(state.groupBy), state.groupAscending));
}
}

void FolderWindow::getViewState(IFolderView2 *const folderView, ShellViewState *state) {
    // A fresh snapshot records only settings successfully read from this view.
    state->view = captureOptions(folderView, {});

    int count = 0;
    if (!checkHR(folderView->ItemCount(SVGIO_ALLVIEW, &count)) || count <= 0)
        return;
    state->itemIds = std::unique_ptr<CComHeapPtr<ITEMID_CHILD>[]>(
        new CComHeapPtr<ITEMID_CHILD>[count]);
    state->itemPositions = std::unique_ptr<POINT[]>(new POINT[count]);
    for (int i = 0; i < count; i++) {
        CComHeapPtr<ITEMID_CHILD> item;
        POINT position = {};
        if (checkHR(folderView->Item(i, &item)) && item
                && checkHR(folderView->GetItemPosition(item, &position))) {
            state->itemIds[state->numItems].Attach(item.Detach());
            state->itemPositions[state->numItems] = position;
            state->numItems++;
        }
    }
}

void FolderWindow::setViewState(IFolderView2 *const folderView, const ShellViewState &state) {
    restoreOptions(folderView, state.view);
    if (state.numItems > 0) {
        std::vector<PCUITEMID_CHILD> items;
        items.reserve(static_cast<size_t>(state.numItems));
        for (int i = 0; i < state.numItems; i++)
            items.push_back(state.itemIds[i]);
        checkHR(folderView->SelectAndPositionItems(static_cast<UINT>(items.size()),
            items.data(), state.itemPositions.get(), SVSI_NOSTATECHANGE));
    }
}

void FolderWindow::scheduleStateSave(bool icons) {
    if (restoringState || !viewReady) return;
    if (icons) viewStateDirty(FolderState::Icons);
    // Coalesce related native notifications; no polling and no database write per mouse move.
    SetTimer(hwnd, TIMER_SAVE_STATE, 250, nullptr);
}

uint32_t FolderWindow::captureViewState(uint32_t mask) {
    uint32_t captured = ItemWindow::captureViewState(mask);
    if (!browser || !viewReady || restoringState || getSavedFolderState().firstFailedAt) return captured;
    CComPtr<IFolderView2> view;
    if (FAILED(browser->GetCurrentView(IID_PPV_ARGS(&view)))) return captured;
    FolderViewSettings options = captureOptions(view, getSavedFolderState().view);
    if (options.present && (!(getSavedFolderState().present & FolderState::View)
            || !sameView(options, getSavedFolderState().view) || (mask & FolderState::View))) {
        getSavedFolderState().view = std::move(options);
        viewStateDirty(FolderState::View);
        captured |= FolderState::View;
    }
    // A filtered subset must never replace the complete folder's icon positions.
    if (!nameFilter && (mask & FolderState::Icons) && spatialView(view)) {
        CComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(nullptr, 0));
        if (stream && writeIconPositions(view, stream)) {
            STATSTG stat = {};
            if (SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && !stat.cbSize.HighPart) {
                std::vector<uint8_t> bytes(stat.cbSize.LowPart);
                LARGE_INTEGER zero = {};
                if (SUCCEEDED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))
                        && (bytes.empty() || SUCCEEDED(IStream_Read(stream, bytes.data(), static_cast<ULONG>(bytes.size()))))) {
                    if (!(getSavedFolderState().present & FolderState::Icons) || bytes != getSavedFolderState().icons) {
                        getSavedFolderState().icons = std::move(bytes);
                        captured |= FolderState::Icons;
                    } else {
                        viewStateClean(FolderState::Icons);
                    }
                }
            }
        }
    }
    return captured;
}

bool FolderWindow::writeIconPositions(IFolderView *const folderView, IStream *const stream) {
    CComPtr<IEnumIDList> enumeration;
    if (!checkHR(folderView->Items(SVGIO_ALLVIEW, IID_PPV_ARGS(&enumeration)))) return false;
    std::unordered_set<std::string> visible;
    CComHeapPtr<ITEMID_CHILD> id;
    HRESULT result;
    while ((result = enumeration->Next(1, &id, nullptr)) == S_OK) {
        POINT position;
        if (!checkHR(folderView->GetItemPosition(id, &position))) return false;
        position = invScaleDPI(position);
        if (!checkHR(IStream_WritePidl(stream, id))
                || !checkHR(IStream_Write(stream, &position, sizeof(position)))) return false;
        visible.emplace(reinterpret_cast<const char *>(id.m_pData), ILGetSize(id));
        id.Free();
    }
    if (result != S_FALSE) return false;

    // Navigation can finish before background enumeration. Preserve unseen items
    // until the documented SFVM_BACKGROUNDENUMDONE notification confirms completeness.
    // PIDLs are serialized/deserialized by Shell APIs, not decoded by FileSpacer.
    if (!enumerationComplete && !getSavedFolderState().icons.empty()) {
        CComPtr<IStream> previous;
        previous.Attach(SHCreateMemStream(getSavedFolderState().icons.data(), static_cast<UINT>(getSavedFolderState().icons.size())));
        if (!previous) return false;
        ULARGE_INTEGER offset = {};
        LARGE_INTEGER zero = {};
        while (SUCCEEDED(previous->Seek(zero, STREAM_SEEK_CUR, &offset))
                && offset.QuadPart < getSavedFolderState().icons.size()) {
            POINT position;
            if (!checkHR(IStream_ReadPidl(previous, &id))
                    || !checkHR(IStream_Read(previous, &position, sizeof(position)))) return false;
            const std::string key(reinterpret_cast<const char *>(id.m_pData), ILGetSize(id));
            if (!visible.count(key)) {
                if (!checkHR(IStream_WritePidl(stream, id))
                        || !checkHR(IStream_Write(stream, &position, sizeof(position)))) return false;
                visible.insert(key);
            }
            id.Free();
        }
    }
    return true;
}

void FolderWindow::loadIconPositions() {
    if (!browser || !loadFolderState() || !(getSavedFolderState().present & FolderState::Icons)) return;
    CComPtr<IFolderView> view;
    if (SUCCEEDED(browser->GetCurrentView(IID_PPV_ARGS(&view))) && spatialView(view)
            && !getSavedFolderState().icons.empty()) {
        CComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(getSavedFolderState().icons.data(), static_cast<UINT>(getSavedFolderState().icons.size())));
        if (stream) {
            restoringState = true;
            readIconPositions(view, stream);
            restoringState = false;
        }
    }
}

bool FolderWindow::readIconPositions(IFolderView *const folderView, IStream *const stream) {
    // https://devblogs.microsoft.com/oldnewthing/20130318-00/?p=4933
    CComHeapPtr<ITEMID_CHILD> idList;
    POINT pos;
    while (SUCCEEDED(IStream_ReadPidl(stream, &idList))
            && SUCCEEDED(IStream_Read(stream, &pos, sizeof(pos)))) {
        pos = scaleDPI(pos);
        checkHR(folderView->SelectAndPositionItems(1, (PCITEMID_CHILD *)&idList.m_pData,
            &pos, SVSI_NOSTATECHANGE));
        idList.Free();
    }
    // TODO: auto-position the remaining items to avoid overlap
    return true;
}

void FolderWindow::onDestroy() {
    detachListView();
    KillTimer(hwnd, TIMER_SAVE_STATE);
    viewStateDirty(FolderState::Icons);
    persistViewState();
    searchBox.destroy();
    if (quickAccessIconThread)
        quickAccessIconThread->stop();
    if (quickAccessImages) {
        SendMessage(getQuickAccessToolbar(), TB_SETIMAGELIST, 0, 0);
        ImageList_Destroy(quickAccessImages);
        quickAccessImages = nullptr;
    }
    if (shellView) {
        CComQIPtr<IShellFolderView> sfv(shellView);
        if (sfv) {
            CComPtr<IShellFolderViewCB> oldCB;
            checkHR(sfv->SetCallback(prevCB, &oldCB));
            prevCB = nullptr;
        }
    }
    viewReady = false;
    ItemWindow::onDestroy();
    if (browser) {
        checkHR(browser->Unadvise(eventsCookie));
        checkHR(IUnknown_SetSite(browser, nullptr));
        checkHR(browser->Destroy());
        browser = nullptr;
    }
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
    if (searchText == text)
        return;
    CComQIPtr<IFolderFilterSite> site(browser);
    HRESULT hr = site ? S_OK : E_NOINTERFACE;
    CComPtr<IFolderFilter> next;
    if (SUCCEEDED(hr) && *text) {
        auto filter = new FolderNameFilter(text);
        next.Attach(filter);
        hr = filter->initialize();
    }
    if (SUCCEEDED(hr)) {
        if (!nameFilter && next)
            persistViewState(); // Save the complete view before hiding any items.
        hr = site->SetFilter(next);
    }
    if (SUCCEEDED(hr)) {
        CComPtr<IFolderFilter> previous = nameFilter;
        const std::wstring previousText = searchText;
        nameFilter = next;
        searchText = text;
        // Leave Shell enumeration, sorting, selection and change notifications native.
        hr = shellView ? shellView->Refresh() : S_OK;
        if (FAILED(hr)) {
            site->SetFilter(previous);
            nameFilter = previous;
            searchText = previousText;
            if (shellView)
                shellView->Refresh();
        }
    }
    if (FAILED(hr)) {
        auto error = getErrorMessage(hr);
        auto message = formatString(IDS_SEARCH_ERROR, error.get());
        MessageBox(hwnd, message.get(), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
    }
}

LRESULT FolderWindow::onNotify(NMHDR *notification) {
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
        auto search = reinterpret_cast<SearchBox::Notification *>(notification);
        applyNameFilter(search->text);
        return 0;
    }
    return ItemWindow::onNotify(notification);
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
    if (state == WA_INACTIVE)
        viewStateDirty(FolderState::Icons);
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

    if (browser) {
        RECT browserRect = windowBody();
        browserRect.bottom -= bottomStatusHeight;

        checkHR(browser->SetRect(nullptr, browserRect));

        CComQIPtr<IFolderView2> folderView(shellView);
        if (folderView)
            folderView->SetRedraw(true); // Windows 11 has so many cool bugs
    }
}

void FolderWindow::onSettingsChanged() {
    if (bottomStatusBar)
        checkLE(SetWindowPos(bottomStatusBar, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                | (settings::getStatusTextEnabled() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
    ItemWindow::onSettingsChanged();
    searchBox.setLiveSearch(settings::getLiveNameSearch());
    if (browser && hasStatusText())
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

LRESULT FolderWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_GETISHELLBROWSER_COMPAT) {
        debugPrintf(L"NILESOFT: WM_GETISHELLBROWSER received, hwnd=%p\n", hwnd);

        return reinterpret_cast<LRESULT>(
            static_cast<IShellBrowser *>(this));
    }

    switch (message) {
        case WM_APP_REBUILD_STANDARD_VIEW: {
            CComPtr<ItemWindow> keepAlive(this);
            if (!isWindowOperational()) return 0;
            if (wParam == 1 && fullNamesFailure) {
                auto text = formatString(IDS_LABEL_INCOMPATIBLE, getString(fullNamesFailure));
                fullNamesFailure = 0;
                MessageBoxW(hwnd, text.get(), getString(IDS_APP_NAME), MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (!rebuildStandardView) return 0;
            rebuildStandardView = false;
            persistViewState();
            KillTimer(hwnd, TIMER_SAVE_STATE);
            viewReady = false;
            detachListView();
            if (shellView) {
                CComQIPtr<IShellFolderView> sfv(shellView);
                if (sfv) {
                    CComPtr<IShellFolderViewCB> old;
                    sfv->SetCallback(prevCB, &old);
                }
            }
            prevCB.Release();
            shellView.Release();
            if (browser) {
                browser->Unadvise(eventsCookie);
                IUnknown_SetSite(browser, nullptr);
                browser->Destroy();
                browser.Release();
            }
            // Keep the active name filter across the replacement browser.
            // Recreate the Shell view: changing creation-time options on the existing
            // browser would not establish that the standard renderer is now in use.
            RECT rect = windowBody();
            if (hasStatusText()) rect.bottom -= scaleDPI(BOTTOM_STATUS_HEIGHT_DIP);
            HRESULT hr = browser.CoCreateInstance(__uuidof(ExplorerBrowser));
            if (SUCCEEDED(hr)) hr = browser->SetOptions(BROWSER_OPTIONS);
            if (SUCCEEDED(hr)) hr = browser->Initialize(hwnd, &rect, tempPtr(folderSettings()));
            if (SUCCEEDED(hr)) hr = IUnknown_SetSite(browser, static_cast<IServiceProvider *>(this));
            if (SUCCEEDED(hr)) hr = browser->Advise(this, &eventsCookie);
            if (SUCCEEDED(hr) && nameFilter) {
                CComQIPtr<IFolderFilterSite> site(browser);
                hr = site ? site->SetFilter(nameFilter) : E_NOINTERFACE;
            }
            if (SUCCEEDED(hr)) hr = browser->BrowseToObject(item, SBSP_ABSOLUTE);
            if (SUCCEEDED(hr)) browser->SetOptions(BROWSER_OPTIONS | EBO_NAVIGATEONCE);
            if (!isWindowOperational()) return 0;
            const UINT reason = SUCCEEDED(hr) ? fullNamesFailure : IDS_LABEL_REBUILD_FAILED;
            fullNamesFailure = 0;
            auto text = formatString(IDS_LABEL_INCOMPATIBLE, getString(reason));
            MessageBoxW(hwnd, text.get(), getString(IDS_APP_NAME), MB_OK | MB_ICONWARNING);
            return 0;
        }
        case MSG_QUICK_ACCESS_ICON: {
            HBITMAP bitmap = quickAccessIconThread ? quickAccessIconThread->takeBitmap() : nullptr;
            if (!bitmap)
                return 0;
            const int iconSize = GetSystemMetrics(SM_CXSMICON);
            int imageWidth = iconSize;
            // Bias this icon to the left without cropping or changing its Shell pixels.
            DIBSECTION source = {};
            if (GetObject(bitmap, static_cast<int>(sizeof(source)), &source)
                    == static_cast<int>(sizeof(source))
                    && source.dsBm.bmBits && source.dsBm.bmBitsPixel == 32
                    && source.dsBm.bmWidth > 0 && source.dsBm.bmWidth <= iconSize
                    && source.dsBm.bmHeight > 0 && source.dsBm.bmHeight <= iconSize) {
                const int paddedWidth = iconSize + 2 * scaleDPI(2);
                BITMAPINFO bitmapInfo = {{sizeof(BITMAPINFOHEADER), paddedWidth,
                    -iconSize, 1, 32, BI_RGB}};
                void *pixels = nullptr;
                HBITMAP padded = CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS,
                    &pixels, nullptr, 0);
                if (padded) {
                    ZeroMemory(pixels, static_cast<size_t>(paddedWidth)
                        * static_cast<size_t>(iconSize) * sizeof(DWORD));
                    const int top = (iconSize - source.dsBm.bmHeight) / 2;
                    for (int y = 0; y < source.dsBm.bmHeight; ++y) {
                        const int sourceY = source.dsBmih.biHeight < 0
                            ? y : source.dsBm.bmHeight - 1 - y;
                        CopyMemory(static_cast<DWORD *>(pixels)
                                + static_cast<size_t>(y + top) * static_cast<size_t>(paddedWidth),
                            static_cast<const BYTE *>(source.dsBm.bmBits)
                                + static_cast<size_t>(sourceY)
                                    * static_cast<size_t>(source.dsBm.bmWidthBytes),
                            static_cast<size_t>(source.dsBm.bmWidth) * sizeof(DWORD));
                    }
                    DeleteObject(bitmap);
                    bitmap = padded;
                    imageWidth = paddedWidth;
                }
            }
            HIMAGELIST images = ImageList_Create(imageWidth, iconSize, ILC_COLOR32, 1, 0);
            if (images) {
                if (ImageList_Add(images, bitmap, nullptr) >= 0) {
                    quickAccessImages = images;
                    const HWND toolbar = getQuickAccessToolbar();
                    const LRESULT buttonSize = SendMessage(toolbar, TB_GETBUTTONSIZE, 0, 0);
                    SendMessage(toolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(imageWidth, iconSize));
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
                checkHR(shellView->Refresh());
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
            // Paint the frame and its controls before entering the synchronous navigation.
            checkLE(RedrawWindow(hwnd, nullptr, nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN));
            // will call IExplorerBrowserEvents callbacks
            HRESULT hr;
            if (!checkHR(hr = browser->BrowseToObject(item, SBSP_ABSOLUTE))) {
                recordFolderAccess(hr);
                if (hasStatusText())
                    setStatusText(getErrorMessage(hr).get());
            }
            checkHR(browser->SetOptions(BROWSER_OPTIONS | EBO_NAVIGATEONCE));
            return 0;
        }
        case WM_EXITMENULOOP:
            scheduleStateSave();
            break;
        case WM_TIMER:
            if (wParam == TIMER_SAVE_STATE) {
                KillTimer(hwnd, TIMER_SAVE_STATE);
                persistViewState();
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
    if (!isWindowOperational() || !browser)
        return;
    if (handlingRButtonDown) {
        selectedWhileHandlingRButtonDown = true;
        return;
    }

    CComPtr<IExplorerBrowser> currentBrowser = browser;
    CComPtr<IFolderView2> folderView;
    if (!isWindowOperational()
            || FAILED(currentBrowser->GetCurrentView(IID_PPV_ARGS(&folderView)))
            || !isWindowOperational())
        return;

    int numSelected;
    if (!checkHR(folderView->ItemCount(SVGIO_SELECTION, &numSelected)) || !isWindowOperational())
        return;

    if (numSelected == 1) {
        CComPtr<IShellItemArray> selection;
        if (folderView->GetSelection(FALSE, &selection) == S_OK && isWindowOperational()) {
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
    if (!isWindowOperational() || !browser)
        return;
    CComPtr<IExplorerBrowser> currentBrowser = browser;
    CComPtr<IFolderView2> folderView;
    if (!isWindowOperational()
            || FAILED(currentBrowser->GetCurrentView(IID_PPV_ARGS(&folderView)))
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

        if (folderView->GetSelection(FALSE, &selection) == S_OK && isWindowOperational()) {
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
    if (browser) {
        CComPtr<IFolderView2> folderView;
        if (checkHR(browser->GetCurrentView(IID_PPV_ARGS(&folderView)))) {
            storedViewState = std::make_unique<ShellViewState>();
            getViewState(folderView, storedViewState.get());
        }

        CComQIPtr<IShellFolderView> sfv(shellView);
        if (sfv) {
            CComPtr<IShellFolderViewCB> oldCB;
            checkHR(sfv->SetCallback(prevCB, &oldCB));
            prevCB = nullptr;
        }
        shellView = nullptr;
        checkHR(browser->SetOptions(BROWSER_OPTIONS)); // temporarily enable navigation
        HRESULT hr = browser->BrowseToObject(item, SBSP_ABSOLUTE);
        if (!checkHR(hr)) recordFolderAccess(hr);
        checkHR(browser->SetOptions(BROWSER_OPTIONS | EBO_NAVIGATEONCE));
    }
}

void FolderWindow::refresh() {
    viewStateDirty(FolderState::Icons);
    persistViewState();
    ItemWindow::refresh();
    if (shellView) {
        HRESULT hr = shellView->Refresh();
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
    if (!contextMenu || !isWindowOperational() || !browser)
        return;
    CComPtr<IExplorerBrowser> currentBrowser = browser;
    CComPtr<IFolderView2> folderView;
    auto detachSite = [](IContextMenu *handler) { checkHR(IUnknown_SetSite(handler, nullptr)); };
    std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
    // Own the installed site, not an additional reference to the menu handler.
    if (isWindowOperational() && checkHR(currentBrowser->GetCurrentView(IID_PPV_ARGS(&folderView)))
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
    checkHR(contextMenu->InvokeCommand(&info));
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
        HWND menuOwner = hwnd;
        HWND shellViewHwnd = nullptr;
        if (isWindowOperational() && SUCCEEDED(view->GetWindow(&shellViewHwnd)) && shellViewHwnd)
            menuOwner = shellViewHwnd;
        int cmd = 0;
        if (isWindowOperational()) {
            wchar_t ownerClass[128] = {};
            GetClassName(menuOwner, ownerClass, _countof(ownerClass));
            debugPrintf(L"NILESOFT: context menu owner=%p class=%s\n", menuOwner, ownerClass);
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
                if (!isWindowOperational() || !browser)
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
                CComPtr<IExplorerBrowser> currentBrowser = browser;
                CComPtr<IFolderView2> folderView;
                auto detachSite = [](IContextMenu *handler) {
                    checkHR(IUnknown_SetSite(handler, nullptr));
                };
                std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
                if (isWindowOperational()
                        && checkHR(currentBrowser->GetCurrentView(IID_PPV_ARGS(&folderView)))
                        && isWindowOperational()) {
                    if (checkHR(IUnknown_SetSite(contextMenu, folderView)))
                        site.reset(contextMenu);
                }
                if (isWindowOperational()) {
                    info.hwnd = hwnd;
                    contextMenu->InvokeCommand((CMINVOKECOMMANDINFO *)&info);
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

// Owns the icon work for one popup only. The worker never accesses a menu or UI COM object.
class QuickAccessMenuIcons {
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
                work->pending.push_back({static_cast<UINT>(index + 1), bitmap});
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
    void start(HWND owner, HMENU menu, int iconSize, std::vector<std::wstring> paths) {
        if (paths.empty())
            return;
        state->owner = owner;
        state->menu = menu;
        state->iconSize = iconSize;
        state->paths = std::move(paths);
        state->message = RegisterWindowMessage(L"FileSpacer.QuickAccessMenuIcons");
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
    ~QuickAccessMenuIcons() { stop(); }
};

void FolderWindow::openQuickAccessMenu(POINT point) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    // Only these two names depend on Explorer implementation details.
    // Resolve the property key at runtime and fail visibly if it is unavailable.
    auto unavailable = [this](UINT stage, HRESULT error) {
        if (!isWindowOperational())
            return;
        MessageBox(hwnd, formatString(IDS_QUICK_ACCESS_UNAVAILABLE,
            getString(stage), (DWORD)error).get(), getString(IDS_QUICK_ACCESS),
            MB_OK | MB_ICONWARNING);
    };
    CComPtr<IShellItem> quickAccess;
    HRESULT result = SHCreateItemFromParsingName(
        L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}", nullptr,
        IID_PPV_ARGS(&quickAccess));
    if (FAILED(result)) {
        unavailable(IDS_QUICK_ACCESS_OPEN, result);
        return;
    }
    if (!isWindowOperational())
        return;
    PROPERTYKEY pinnedKey = {};
    result = PSGetPropertyKeyFromName(L"System.Home.IsPinned", &pinnedKey);
    if (FAILED(result)) {
        unavailable(IDS_QUICK_ACCESS_KEY, result);
        return;
    }
    if (!isWindowOperational())
        return;
    CComPtr<IEnumShellItems> items;
    result = quickAccess->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items));
    if (FAILED(result)) {
        unavailable(IDS_QUICK_ACCESS_ENUM, result);
        return;
    }
    struct Entry {
        std::wstring name;
        std::wstring path;
    };
    std::vector<Entry> entries;
    while (isWindowOperational()) {
        CComPtr<IShellItem> entry;
        result = items->Next(1, &entry, nullptr);
        if (result == S_FALSE)
            break;
        if (FAILED(result)) {
            unavailable(IDS_QUICK_ACCESS_ENUM, result);
            return;
        }
        if (!isWindowOperational())
            return;
        SFGAOF attributes = 0;
        result = entry->GetAttributes(SFGAO_FOLDER, &attributes);
        if (FAILED(result)) {
            unavailable(IDS_QUICK_ACCESS_READ, result);
            return;
        }
        if (!isWindowOperational())
            return;
        if (!(attributes & SFGAO_FOLDER))
            continue;
        CComPtr<IShellItem2> properties;
        result = entry->QueryInterface(IID_PPV_ARGS(&properties));
        BOOL pinned = FALSE;
        if (SUCCEEDED(result) && isWindowOperational())
            result = properties->GetBool(pinnedKey, &pinned);
        if (FAILED(result)) {
            unavailable(IDS_QUICK_ACCESS_READ, result);
            return;
        }
        if (!isWindowOperational())
            return;
        if (!pinned)
            continue;
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
    QuickAccessMenuIcons icons;
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
    int cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x, point.y, hwnd, nullptr);
    if (isWindowOperational() && browser && cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) {
        auto info = makeInvokeInfo(cmd - IDM_SHELL_FIRST, point);
        CComPtr<IExplorerBrowser> currentBrowser = browser;
        CComPtr<IFolderView2> folderView;
        auto detachSite = [](IContextMenu *handler) { checkHR(IUnknown_SetSite(handler, nullptr)); };
        std::unique_ptr<IContextMenu, decltype(detachSite)> site(nullptr, detachSite);
        if (isWindowOperational() && checkHR(currentBrowser->GetCurrentView(IID_PPV_ARGS(&folderView)))
                && isWindowOperational()) {
            if (checkHR(IUnknown_SetSite(contextMenu, folderView)))
                site.reset(contextMenu);
        }
        if (isWindowOperational()) {
            info.hwnd = hwnd;
            contextMenu->InvokeCommand((CMINVOKECOMMANDINFO *)&info);
        }
    }
}

/* IUnknown */

STDMETHODIMP FolderWindow::QueryInterface(REFIID id, void **obj) {
    static const QITAB interfaces[] = {
		QITABENT(FolderWindow, IServiceProvider),
		QITABENT(FolderWindow, ICommDlgBrowser),
		QITABENT(FolderWindow, ICommDlgBrowser2),
		QITABENT(FolderWindow, IExplorerBrowserEvents),
		QITABENT(FolderWindow, IShellFolderViewCB),
		QITABENT(FolderWindow, IDispatch),
		QITABENT(FolderWindow, IWebBrowser),
		QITABENT(FolderWindow, IWebBrowserApp),
		QITABENT(FolderWindow, IShellBrowser),
		{},
	};
    HRESULT hr = QISearch(this, interfaces, id, obj);
    if (SUCCEEDED(hr))
        return hr;
    if (id == __uuidof(IFolderFilter) && prevCB)
        return prevCB->QueryInterface(id, obj);
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
    debugPrintf(
        L"NILESOFT: QueryActiveShellView called, shellView=%p\n",
        shellView.p);

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

// called when double-clicking a file
STDMETHODIMP FolderWindow::OnDefaultCommand(IShellView *const view) {
    const bool closeSource = settings::getKeepSourceWindowOpen()
        == (GetKeyState(VK_CONTROL) < 0);
    CComQIPtr<IFolderView2> folderView(view);

    int numSelected = 0;
    if (folderView
            && checkHR(folderView->ItemCount(SVGIO_SELECTION, &numSelected))
            && numSelected == 1) {

        CComPtr<IShellItemArray> selection;
        if (folderView->GetSelection(FALSE, &selection) == S_OK) {
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
            && GetKeyState(VK_MENU) >= 0
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

STDMETHODIMP FolderWindow::OnStateChange(IShellView *, ULONG change) {
    CComPtr<ItemWindow> keepAlive(this);
    // Keep ListView notifications when available; the default Shell view may use
    // another control and then selection updates must use the public callback.
    if (!listView && viewReady && isWindowOperational() && change == CDBOSC_SELCHANGE)
        selectionChanged();
    return S_OK;
}

STDMETHODIMP FolderWindow::IncludeObject(IShellView *, PCUITEMID_CHILD childID) {
    // will only be called on Desktop, thanks to CDB2GVF_NOINCLUDEITEM
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
    *flags = CDB2GVF_NOSELECTVERB | CDB2GVF_NOINCLUDEITEM;
    return S_OK;
}

STDMETHODIMP FolderWindow::Notify(IShellView *, DWORD) {
    return S_OK;
}

/* IExplorerBrowserEvents */

// order: OnNavigationPending, OnViewCreated, OnNavigationComplete
// OR: OnNavigationPending, OnNavigationFailed
STDMETHODIMP FolderWindow::OnNavigationPending(PCIDLIST_ABSOLUTE) {
    return S_OK;
}

STDMETHODIMP FolderWindow::OnNavigationComplete(PCIDLIST_ABSOLUTE) {
    recordFolderAccess(S_OK);
    listViewCreated();

    if (shellView && GetActiveWindow() == hwnd)
        checkHR(shellView->UIActivate(SVUIA_ACTIVATE_FOCUS));

    // item count will often be incorrect at this point; see listViewOwnerProc
    if (hasStatusText())
        updateStatus();

    viewReady = true;
    loadIconPositions();
    scheduleStateSave();
    onViewReady();
    if (fullNamesFailure && !rebuildStandardView) {
        // Report outside the Shell's synchronous navigation callback.
        PostMessageW(hwnd, WM_APP_REBUILD_STANDARD_VIEW, 1, 0);
    }
    return S_OK;
}

STDMETHODIMP FolderWindow::OnNavigationFailed(PCIDLIST_ABSOLUTE) {
    // This callback also reports cancellation and has no HRESULT: do not count a failure.
    viewReady = false;
    setStatusText(getString(IDS_FOLDER_ERROR));
    return S_OK;
}

STDMETHODIMP FolderWindow::OnViewCreated(IShellView *const view) {
    detachListView();
    shellView = view;

    CComQIPtr<IShellFolderView> sfv(view);
    if (sfv) {
        prevCB = nullptr;
        checkHR(sfv->SetCallback(this, &prevCB));
    }

    restoringState = true;
    viewReady = false;
    enumerationComplete = false;
    CComQIPtr<IFolderView2> folderView(view);
    if (folderView) {
        initDefaultView(folderView);
        if (storedViewState) {
            setViewState(folderView, *storedViewState);
            storedViewState = nullptr;
        } else if (loadFolderState() && (getSavedFolderState().present & FolderState::View)) {
            restoreOptions(folderView, getSavedFolderState().view);
        }
    }
    restoringState = false;

    return S_OK;
}

/* IShellFolderViewCB */

STDMETHODIMP FolderWindow::MessageSFVCB(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == 17) { // refresh
        if (wParam) { // pre refresh
            enumerationComplete = false;
            viewStateDirty(FolderState::Icons);
            persistViewState();
        } else { // post refresh (existing Shell callback)
            loadIconPositions();
            scheduleStateSave();
        }
    } else if (msg == SFVM_BACKGROUNDENUMDONE) {
        enumerationComplete = true;
        loadIconPositions();
        scheduleStateSave();
    } else if (msg == SFVM_DIDDRAGDROP) { // sent to source, not target!
        scheduleStateSave(true);
    } else if (msg == SFVM_FSNOTIFY) {
        if (lParam & (SHCNE_CREATE | SHCNE_MKDIR | SHCNE_DELETE | SHCNE_RMDIR
                | SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER))
            scheduleStateSave(true);
    }
    if (prevCB)
        return prevCB->MessageSFVCB(msg, wParam, lParam);
    return E_NOTIMPL;
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
