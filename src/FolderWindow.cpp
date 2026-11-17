#include "FolderWindow.h"
#include "FolderWindowInternal.h"
#include "FolderWindowViewMenuPreview.h"
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "ShellViewOperations.h"
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
#include "TitleMenu.h"
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
#include <set>
#include <map>
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
    iconSnap.reset();
    if (columnLayout) columnLayout->stop();
    watchProvisionalInput(false); // The hook must never compare a destroyed window.
}

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






void FolderWindow::onCreateContent() {
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
    // Content created after WM_CREATE, for an opening effect that started or
    // not, already has its final frame and client geometry.
    if (!frameApplied) browserRect.bottom += CAPTION_HEIGHT;

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
    const bool storedStateAvailable = !hybridViewEligible || loadFolderState();
    if (hybridViewEligible && storedStateAvailable) {
        const auto &saved = getSavedFolderState().view;
        if (saved.present & FolderViewSettings::Mode) {
            const auto mode = saved.mode;
            if (mode == FVM_AUTO || (mode >= FVM_FIRST && mode <= FVM_LAST))
                initial.ViewMode = static_cast<UINT>(mode);
        }
        if (saved.present & FolderViewSettings::Flags) {
            const DWORD placementFlags = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
            initial.fFlags = (initial.fFlags & ~placementFlags) | (saved.flags & placementFlags);
        }
    }
    if (!isWindowOperational()) return;
    // A provisional view must support the complete native View menu and free
    // placement before SQLite becomes readable. Profile replacement is deferred
    // during that recovery; its normal synchronization resumes afterwards.
    viewProfile = !storedStateAvailable
        ? static_cast<FOLDERVIEWOPTIONS>(FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING)
        : profileForMode(initial.ViewMode);
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
    // The animated path was shown before the browser was initialized.
    if (isWindowOperational() && IsWindowVisible(hwnd)) {
        initialNavigationPending = false;
        checkLE(PostMessageW(hwnd, MSG_INITIAL_NAVIGATION, 0, 0));
    }
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
















void FolderWindow::onDestroy() {
    iconSnap.reset();
    iconRealignPending = false;
    if (columnLayout) columnLayout->stop();
    columnLayout.reset();
    viewProfilePosted = false;
    watchProvisionalInput(false);
    if (viewMenuPreview && viewMenuPreview->prepared) restoringState = true;
    viewMenuPreview.reset();
    viewMenuRestore.reset();
    resizeGesture.stop();
    checkHR(viewHost.stopObserving());
    KillTimer(hwnd, TIMER_SAVE_STATE);
    endLoading(false);
    cancelStorageWait();
    providerSyncPending = false;
    viewStateDirty(FolderState::Icons);
    persistViewState();
    saveSearchHistory();
    // The viewport is a convenience: failing to keep it never deserves the
    // storage dialog. An empty one erases the stored position.
    if ((!closingScroll.empty() || closingScrollStale) && !getFolderStateKey().empty()
            && !getSavedFolderState().firstFailedAt) {
        if (auto store = folderStateStore()) store->saveViewScroll(getFolderStateKey(), closingScroll);
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
    if (command == IDM_REALIGN_ICONS) { realignIcons(); return true; }
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
        deactivateIconDrag();
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
    if (viewHost && columnLayout && resizingColumns && resizeDeferred) {
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
    if (columnLayout && (!resizingColumns || !resizeDeferred)) columnLayout->resize();
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
// The view fills in the background and reports the end of its enumeration by an
// event. Meanwhile, once half a second has passed, the address bar and the
// taskbar button show a bar that fills as in Explorer. No real proportion is
// known: it advances with time and completes on the event. The timer only
// animates the bar; nothing is decided from it.
void FolderWindow::beginLoading() {
    if (!hwnd) return;
    if (loadingFinishing) clearLoading();
    if (loadingSince) return;
    loadingSince = GetTickCount64();
    loadingShownAt = 0;
    SetTimer(hwnd, TIMER_LOADING, 500, nullptr);
}

void FolderWindow::clearLoading() {
    if (hwnd) KillTimer(hwnd, TIMER_LOADING);
    if (loadingShownAt) { debugPrintf(L"LOADING hidden\n"); }
    loadingSince = loadingShownAt = 0;
    loadingFinishing = false;
    pathBar.setProgress(-1);
    if (loadingTaskbar && hwnd) loadingTaskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
    loadingTaskbar.Release();
}

void FolderWindow::endLoading(bool completed) {
    if (!loadingSince) return;
    if (!loadingShownAt || !completed || !isWindowOperational()) {
        clearLoading();
        return;
    }
    if (loadingFinishing) return;
    // Shown complete for a moment, then removed by the next tick.
    loadingFinishing = true;
    pathBar.setProgress(1000);
    if (loadingTaskbar) loadingTaskbar->SetProgressValue(hwnd, 1000, 1000);
    SetTimer(hwnd, TIMER_LOADING, 200, nullptr);
}

void FolderWindow::loadingTick() {
    if (loadingFinishing || !loadingSince || !isWindowOperational()) {
        clearLoading();
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (!loadingShownAt) {
        loadingShownAt = now;
        SetTimer(hwnd, TIMER_LOADING, 50, nullptr);
        if (SUCCEEDED(loadingTaskbar.CoCreateInstance(CLSID_TaskbarList)) && FAILED(loadingTaskbar->HrInit()))
            loadingTaskbar.Release();
        debugPrintf(L"LOADING shown after %llu ms\n", now - loadingSince);
    }
    // Quick at first, then ever slower: never complete before the event.
    const ULONGLONG elapsed = now - loadingShownAt;
    const int value = static_cast<int>(50 + 850 * elapsed / (elapsed + 3000));
    pathBar.setProgress(value);
    if (loadingTaskbar) loadingTaskbar->SetProgressValue(hwnd, static_cast<ULONGLONG>(value), 1000);
}

HRESULT FolderWindow::refreshEnumeration() {
    viewMenuRestore.reset(); // An explicit restart supersedes a preview's pending replay.
    enumerationComplete = false;
    beginLoading();
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
            checkLE(PostMessageW(hwnd, shellViewOperationsReadyMessage(), 0, 0));
        }
        if (viewProfilePending) requestViewProfile();
        replayProviderSync(); // Refused while this navigation was in progress.
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



LRESULT FolderWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    CComPtr<ItemWindow> messageKeepAlive(this);
    if (message == MSG_ICON_SNAP) return handleIconSnap(wParam, lParam);
    if (message == WM_CANCELMODE && !hasPendingIconPlacement()) iconSnap.reset();
    if (message == WM_ENTERSIZEMOVE) {
        resizingColumns = true;
        resizeDeferred = settings::getDetailsColumnsDeferred();
        if (columnLayout) columnLayout->suspend(ShellViewColumns::Hold::Resize, resizeDeferred);
    } else if (message == WM_EXITSIZEMOVE) {
        finishColumnResize();
    } else if (message == MSG_COLUMNS_CHANGED) {
        // Received, whatever follows: a refused pass must not mute later requests.
        if (columnLayout) columnLayout->acknowledge();
        if (wParam == viewGeneration) synchronizeColumns();
        return 0;
    }
    if (message >= 0xC000 && message == shellViewOperationsReadyMessage()) {
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
                if (enumerationComplete) endLoading(true);
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
                // A mode or column-set change needs no EnumDone: attach, detach
                // or refit the Details columns from the event itself.
                synchronizeColumns();
                if (isWindowOperational()) requestViewProfile();
            }
            return 0;
        case MSG_SYNCHRONIZE_VIEW_PROFILE:
            synchronizeViewProfile();
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
                    && lstrcmpi(reinterpret_cast<LPCWSTR>(lParam), L"ShellState") == 0) {
                // Same sequence as an explicit refresh: the free positions are
                // persisted before the refill and restored after it.
                CComPtr<ItemWindow> keepAlive(this);
                viewStateDirty(FolderState::Icons);
                persistViewState();
                if (isWindowOperational() && shellView) {
                    restoreIconsPending = searchText.empty();
                    checkHR(refreshEnumeration());
                }
            }
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
            if (wParam == TIMER_LOADING) {
                loadingTick();
                return 0;
            }
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
        // A profile change in progress already holds the snapshot to apply: the
        // recreated view, not restored yet, must not replace it.
        if (!(viewProfileChanging && storedViewState)
                && checkHR(viewHost.currentView(IID_PPV_ARGS(&folderView)))) {
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

// The focused, selected item's public accessibility bounds are already scrolled
// screen coordinates. Never discover a private Shell child control for this effect.
static bool openingIconRect(IShellView *view, IFolderView2 *folder,
        const std::function<bool()> &current, RECT &rect) {
    HWND body = nullptr;
    FOLDERVIEWMODE mode = FVM_AUTO;
    int size = 0;
    if (FAILED(view->GetWindow(&body)) || !current() || !body
            || FAILED(folder->GetViewModeAndIconSize(&mode, &size)) || !current()
            || size <= 0) return false;
    HWND focus = GetFocus();
    if (!focus || (focus != body && !IsChild(body, focus))) return false;
    CComPtr<IAccessible> accessible;
    if (FAILED(AccessibleObjectFromWindow(focus, static_cast<DWORD>(OBJID_CLIENT), IID_PPV_ARGS(&accessible)))
            || !current() || !accessible) return false;
    CComVariant child(CHILDID_SELF);
    for (unsigned depth = 0; depth < 8; ++depth) {
        CComVariant role, state;
        if (FAILED(accessible->get_accRole(child, &role)) || !current()) return false;
        if (role.vt == VT_I4 && (role.lVal == ROLE_SYSTEM_LISTITEM || role.lVal == ROLE_SYSTEM_OUTLINEITEM)) {
            if (FAILED(accessible->get_accState(child, &state)) || !current() || state.vt != VT_I4
                    || !(state.lVal & STATE_SYSTEM_SELECTED)
                    || (state.lVal & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN))) return false;
            long x = 0, y = 0, width = 0, height = 0;
            if (FAILED(accessible->accLocation(&x, &y, &width, &height, child)) || !current()
                    || width <= 0 || height <= 0) return false;
            // Asked from inside the process, the accessible object of the view
            // answers in the coordinates of this system-DPI-aware process, already
            // scaled when the display has another DPI: they are not converted again.
            const POINT topLeft = {x, y}, bottomRight = {x + width, y + height};
            const bool large = mode == FVM_ICON || mode == FVM_THUMBNAIL || mode == FVM_THUMBSTRIP;
            // Icon layouts put the image above the label; rows put it to its left.
            const LONG left = large ? topLeft.x + (bottomRight.x - topLeft.x - size) / 2 : topLeft.x;
            const LONG top = large ? topLeft.y : topLeft.y + (bottomRight.y - topLeft.y - size) / 2;
            rect = {left, top, left + size, top + size};
            RECT client = {}, visible = {};
            if (!GetClientRect(body, &client)) return false;
            MapWindowPoints(body, nullptr, reinterpret_cast<POINT *>(&client), 2);
            return IntersectRect(&visible, &rect, &client) && EqualRect(&visible, &rect);
        }
        CComVariant focused;
        if (accessible->get_accFocus(&focused) != S_OK || !current()) return false;
        if (focused.vt == VT_DISPATCH && focused.pdispVal) {
            CComQIPtr<IAccessible> next(focused.pdispVal);
            if (!current() || !next) return false;
            accessible = next;
            child = CComVariant(CHILDID_SELF);
        } else if (focused.vt == VT_I4 && focused.lVal != CHILDID_SELF) {
            child = focused;
        } else return false;
    }
    return false;
}

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
                    RECT origin = {};
                    const bool animate = FolderOpeningAnimation::isEnabled()
                        && openingIconRect(view, folderView, current, origin);
                    if (!current()) return S_OK;
                    selected = selectedItem;
                    openChild(selectedItem, closeSource, animate ? &origin : nullptr);
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
    iconSnap.reset();
    iconRealignPending = false;
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
    beginLoading();
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
    endLoading(false);
    viewReady = false;
    setStatusText(getString(IDS_FOLDER_ERROR));
    if (reopeningWindow.sequence) {
        auto store = folderStateStore();
        if (!store || !store->returnClosedWindow(reopeningWindow)) stateStorageError();
        reopeningWindow = {};
        refreshReopenJumpLists();
    }
    return S_OK;
}

STDMETHODIMP FolderWindow::OnViewCreated(IShellView *const view) {
    CComPtr<ItemWindow> keepAlive(this);
    iconSnap.reset();
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
    beginLoading();
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
    if (!provisionalBaseline) {
        // OnViewCreated is the documented point for setting modes and flags
        // before the native view is shown. Doing this after enumeration can
        // queue a second refill that discards positions restored in between.
        const auto &saved = storedViewState ? storedViewState->view : getSavedFolderState().view;
        FolderViewSettings initial;
        initial.present = saved.present & (FolderViewSettings::Mode | FolderViewSettings::Flags);
        initial.mode = saved.mode;
        initial.iconSize = saved.iconSize;
        initial.flags = saved.flags;
        if (initial.present) {
            CComQIPtr<IFolderView2> folderView(view);
            if (!current()) return S_OK;
            // The ordinary post-enumeration restoration retries any part an
            // individual provider cannot apply at this earlier point.
            if (folderView) (void)restoreOptions(folderView, initial);
            if (!current()) return S_OK;
        }
    }
    setSingleSelectionLabels(view, settings::getFullNamesOnSelection());
    if (!current()) return S_OK;
    restoringState = false;
    finishViewInitialization();

    return S_OK;
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
