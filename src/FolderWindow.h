#pragma once
#include <common.h>

#include "ItemWindow.h"
#include "SearchBox.h"
#include "ShellViewHost.h"
#include "ShellViewSelection.h"
#include "WindowResizeGesture.h"
#include <memory>
#include <vector>
#include <ExDisp.h>
#include <shlobj_core.h>

namespace filespacer {

class FolderWindow : public ItemWindow, public IServiceProvider, public ICommDlgBrowser2,
        public IExplorerBrowserEvents, public IWebBrowserApp,
        public IShellBrowser {
public:
    static void init();

    FolderWindow(IShellItem *item, const std::wstring &identity);
    ~FolderWindow() override;
    void setReopenState(const ClosedWindow &window) { reopeningWindow = window; }

    bool handleTopLevelMessage(MSG *msg) override;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID id, void **obj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
	// IOleWindow / IShellBrowser
    STDMETHODIMP GetWindow(HWND *phwnd) override;
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) override;

    STDMETHODIMP InsertMenusSB(
        HMENU hmenuShared,
        LPOLEMENUGROUPWIDTHS lpMenuWidths) override;

    STDMETHODIMP SetMenuSB(
        HMENU hmenuShared,
        HOLEMENU holemenuRes,
        HWND hwndActiveObject) override;

    STDMETHODIMP RemoveMenusSB(HMENU hmenuShared) override;
    STDMETHODIMP SetStatusTextSB(LPCWSTR pszStatusText) override;
    STDMETHODIMP EnableModelessSB(BOOL fEnable) override;
    STDMETHODIMP TranslateAcceleratorSB(MSG *pmsg, WORD wID) override;

    STDMETHODIMP BrowseObject(
        PCUIDLIST_RELATIVE pidl,
        UINT wFlags) override;

    STDMETHODIMP GetViewStateStream(
        DWORD grfMode,
        IStream **ppStrm) override;

    STDMETHODIMP GetControlWindow(
        UINT id,
        HWND *phwnd) override;

    STDMETHODIMP SendControlMsg(
        UINT id,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT *pret) override;

    STDMETHODIMP QueryActiveShellView(
        IShellView **ppshv) override;

    STDMETHODIMP OnViewWindowActive(
        IShellView *pshv) override;

    STDMETHODIMP SetToolbarItems(
        LPTBBUTTONSB lpButtons,
        UINT nButtons,
        UINT uFlags) override;
    // IServiceProvider
    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void **ppv) override;
    // ICommDlgBrowser
    STDMETHODIMP OnDefaultCommand(IShellView *view) override;
    STDMETHODIMP OnStateChange(IShellView *view, ULONG change) override;
    STDMETHODIMP IncludeObject(IShellView *view, PCUITEMID_CHILD pidl) override;
    // ICommDlgBrowser2
    STDMETHODIMP GetDefaultMenuText(IShellView *view, wchar_t *text, int maxChars) override;
    STDMETHODIMP GetViewFlags(DWORD *flags) override;
    STDMETHODIMP Notify(IShellView *view, DWORD notifyType) override;
    // IExplorerBrowserEvents
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE folder) override;
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE folder) override;
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE folder) override;
    STDMETHODIMP OnViewCreated(IShellView *shellView) override;
    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT *) override;
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo **) override;
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR *, UINT, LCID, DISPID *) override;
    STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *)
        override;
    // IWebBrowser
    STDMETHODIMP GoBack() override;
    STDMETHODIMP GoForward() override;
    STDMETHODIMP GoHome() override;
    STDMETHODIMP GoSearch() override;
    STDMETHODIMP Navigate(BSTR, VARIANT *, VARIANT *, VARIANT *, VARIANT *) override;
    STDMETHODIMP Refresh() override;
    STDMETHODIMP Refresh2(VARIANT *) override;
    STDMETHODIMP Stop() override;
    STDMETHODIMP get_Application(IDispatch **) override;
    STDMETHODIMP get_Parent(IDispatch **) override;
    STDMETHODIMP get_Container(IDispatch **) override;
    STDMETHODIMP get_Document(IDispatch **) override;
    STDMETHODIMP get_TopLevelContainer(VARIANT_BOOL *) override;
    STDMETHODIMP get_Type(BSTR *) override;
    STDMETHODIMP get_Left(long *) override;
    STDMETHODIMP put_Left(long) override;
    STDMETHODIMP get_Top(long *) override;
    STDMETHODIMP put_Top(long) override;
    STDMETHODIMP get_Width(long *) override;
    STDMETHODIMP put_Width(long) override;
    STDMETHODIMP get_Height(long *) override;
    STDMETHODIMP put_Height(long) override;
    STDMETHODIMP get_LocationName(BSTR *) override;
    STDMETHODIMP get_LocationURL(BSTR *) override;
    STDMETHODIMP get_Busy(VARIANT_BOOL *) override;
    // IWebBrowserApp
    STDMETHODIMP Quit() override;
    STDMETHODIMP ClientToWindow(int *, int *) override;
    STDMETHODIMP PutProperty(BSTR, VARIANT) override;
    STDMETHODIMP GetProperty(BSTR, VARIANT *) override;
    STDMETHODIMP get_Name(BSTR *) override;
    STDMETHODIMP get_HWND(SHANDLE_PTR *) override;
    STDMETHODIMP get_FullName(BSTR *) override;
    STDMETHODIMP get_Path(BSTR *) override;
    STDMETHODIMP get_Visible(VARIANT_BOOL *) override;
    STDMETHODIMP put_Visible(VARIANT_BOOL) override;
    STDMETHODIMP get_StatusBar(VARIANT_BOOL *) override;
    STDMETHODIMP put_StatusBar(VARIANT_BOOL) override;
    STDMETHODIMP get_StatusText(BSTR *) override;
    STDMETHODIMP put_StatusText(BSTR) override;
    STDMETHODIMP get_ToolBar(int *) override;
    STDMETHODIMP put_ToolBar(int) override;
    STDMETHODIMP get_MenuBar(VARIANT_BOOL *) override;
    STDMETHODIMP put_MenuBar(VARIANT_BOOL) override;
    STDMETHODIMP get_FullScreen(VARIANT_BOOL *) override;
    STDMETHODIMP put_FullScreen(VARIANT_BOOL) override;

protected:
    enum TimerID {
        TIMER_UPDATE_SELECTION = 1,
        TIMER_SAVE_STATE,
        TIMER_LAST
    };
    enum UserMessage {
        MSG_INITIAL_NAVIGATION = ItemWindow::MSG_LAST,
        // WM_USER + 7 is the Shell compatibility query for IShellBrowser.
        MSG_QUICK_ACCESS_ICON = WM_APP + 45,
        MSG_ENUM_READY = WM_APP + 46,
        MSG_ENUM_DONE = WM_APP + 47,
        MSG_VIEW_MENU_PREVIEW = WM_APP + 48,
        MSG_VIEW_CHANGED = WM_APP + 50,
        MSG_FINISH_VIEW_INITIALIZATION = WM_APP + 51,
        // WPARAM: storageWaitSerial. The folder-state mutex another process held was released.
        MSG_STORAGE_AVAILABLE = WM_APP + 52,
        // Deferred synchronizeViewProvider(), posted when a guard that refused it clears.
        MSG_SYNCHRONIZE_VIEW_PROVIDER = WM_APP + 53,
        MSG_SYNCHRONIZE_VIEW_PROFILE = WM_APP + 54,
        MSG_OPEN_PENDING_VIEW_MENU = WM_APP + 55
    };
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;

    bool spatialView(IFolderView *folderView) const;

	bool hasStatusText();
	void setStatusText(const wchar_t *text);

	SIZE defaultSize() const override;
	bool isFolder() const override;

    uint32_t captureViewState(uint32_t mask) override;
    std::vector<uint8_t> captureReopenViewState() override;

    virtual FOLDERSETTINGS folderSettings() const;

    void onCreate() override;
    void onDestroy() override;
    bool onCommand(WORD command) override;
    void restoreReferenceLayout();
    void fitWindowToContents();
    LRESULT onDropdown(int command, POINT pos) override;
    LRESULT onNotify(NMHDR *notification) override;
    void onActivate(WORD state, HWND prevWindow) override;
    void onSize(SIZE size) override;
    void onSettingsChanged() override;

    void addToolbarButtons(HWND tb) override;
    int getToolbarTooltip(WORD command) override;

    void trackContextMenu(POINT pos) override;


    IDispatch * getShellViewDispatch() override;
    void onItemChanged() override;
    void refresh() override;

    CComPtr<IShellView> shellView;

private:
    bool handleAltMouseMessage(MSG *msg);
    bool handleResizeMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void cancelAltGesture();
    WindowResizeGesture resizeGesture;
    MSG deferredGestureClick = {};
    ULONG_PTR deferredGestureGeneration = 0;
    bool suppressAltUp = false;
    bool peekPressActive = false; // The current Space press started a preview: its repeats are consumed.
    int zoomWheelDelta = 0;
    void changeViewMode(int index, int steps = 0);

    ClosedWindow reopeningWindow;
    std::vector<uint8_t> closingScroll;
    bool restoreScrollPending = true; // Once for this window, never on refresh/profile changes.
    bool initialEnumerationReady = false;
    bool viewOptionsApplied = false;
    bool initialStateApplied = false;
    bool restoreIconsPending = false;
    bool restoreIconsUnpruned = false; // Restore after a Shell mode change: no EnumDone proves completeness.
    bool spatialLayout = false;
    ULONG_PTR viewGeneration = 0;
    ULONG_PTR viewOptionsEnumerationBaseline = 0;
    void finishViewInitialization();
    void restoreReopenViewState();
    void placeSearchBox();
    void applyNameFilter(const wchar_t *text);
    int searchShare = 350; // thousandths of the space shared by address and search
    SearchBox searchBox;
    CComPtr<IFolderFilter> nameFilter;
    std::wstring searchText;
    std::vector<std::wstring> searchHistory;
    std::vector<std::wstring> pendingSearches;
    bool searchHistoryLoaded = false;
    bool searchHistoryDirty = false;
    bool loadSearchHistory();
    void rememberSearch(const std::wstring &query);
    void saveSearchHistory();
    void openSearchMenu();
    void selectSearchSuggestion(UINT command, const wchar_t *text);

    struct ShellViewState {
        FolderViewSettings view;
        std::vector<uint8_t> selection;
        std::vector<uint8_t> scroll; // View-menu cancellation and closing keep its initial viewport.
        std::vector<uint8_t> icons; // Existing icon-position blob, applied once to the new view.
    };
    // The native fallback is observable before SQLite can be read. It is not
    // a saved preference: only later differences may be merged into storage.
    std::unique_ptr<ShellViewState> provisionalBaseline;
    std::unique_ptr<ShellViewState> provisionalCurrent;
    std::shared_ptr<ShellViewState> provisionalRestore;
    std::vector<FolderViewSettings::Column> provisionalColumnWidths;
    bool provisionalReady = false;
    bool provisionalIconsComplete = false, provisionalCaptureBusy = false;
    bool provisionalRestoreOptionsApplied = false;
    bool provisionalInputBlocked = false;
    HWND disabledProvisionalView = nullptr, disabledProvisionalSearch = nullptr;
    HWND disabledProvisionalToolbar = nullptr;
    HWND disabledProvisionalFocus = nullptr; // Control whose keyboard focus EnableWindow(FALSE) cleared.
    HWND provisionalViewWindow = nullptr;
    ULONG_PTR disabledProvisionalGeneration = 0;
    void blockProvisionalInput(bool blocked);
    bool captureProvisionalBaseline();
    bool captureProvisionalCurrent(bool icons, bool beforeInput = false);
    bool recoverProvisionalView();
    bool watchProvisionalInput(bool watch);
    static LRESULT CALLBACK provisionalMessageHook(int code, WPARAM removed, LPARAM data);
    static thread_local HHOOK provisionalHook;
    static thread_local std::vector<FolderWindow *> provisionalWindows;

    struct ViewMenuPreview;
    std::shared_ptr<ViewMenuPreview> viewMenuPreview;
    // Finished session kept while a refill it observed is still running; its
    // EnumDone replays the restoration. Dropped by navigation, closing, an
    // enumeration restart and any new View menu.
    std::shared_ptr<ViewMenuPreview> viewMenuRestore;
    ULONG_PTR viewMenuPreviewSerial = 0;

    // Consumption of a restore attempt is not permission to export its fallback.
    bool iconRestoreFailed = false, iconPositionErrorShown = false;
    std::unique_ptr<std::vector<uint8_t>> iconChangeBaseline;
    ULONG_PTR iconChangeEnumeration = 0;
    void iconPositionError();
    void iconRestoreResult(HRESULT result);
    bool observeIconChange();
    bool captureIconChange();
    HRESULT loadIconPositions(bool pruneMissing = true);
    void scheduleStateSave(bool icons = false);
    void synchronizeViewProvider();
    void replayProviderSync();
    bool providerSyncPending = false; // A refused synchronizeViewProvider() to replay once its guard clears.

    // Unavailable storage of a provisional window: wait for the named mutex
    // another process holds (a signalable object) instead of polling it.
    // Learn: RegisterWaitForSingleObject does not support a mutex, so a thread
    // of our own waits on it, releases it at once and posts MSG_STORAGE_AVAILABLE.
    class StorageWaitThread : public StoppableThread {
    public:
        StorageWaitThread(HWND ownerWindow, HANDLE mutexHandle, ULONG_PTR waitSerial);
        ~StorageWaitThread() override;
    private:
        void run() override;
        HWND owner;
        HANDLE mutex; // Duplicated handle owned by this object; the store's own may close.
        ULONG_PTR serial;
    };
    CComPtr<StorageWaitThread> storageWait;
    ULONG_PTR storageWaitSerial = 0;
    unsigned storageRetryDelay = 0; // Growing, bounded delay for SQLITE_BUSY alone (no Windows event).
    void scheduleStorageRetry();
    void cancelStorageWait();
    bool viewReady = false;
    bool enumerationComplete = false;
    bool restoringState = false;

    void selectionChanged();
    void scheduleUpdateSelection();
    void updateSelection();
    void clearSelection();
    void updateStatus();

    CComPtr<IContextMenu> queryBackgroundMenu(HMENU *popupMenu);
    void newItem(const char *verb);
    void openQuickAccessMenu(POINT point);
    void openViewMenu(POINT point);
    void openBackgroundSubMenu(IContextMenu *contextMenu, HMENU subMenu, POINT point);

	HWND bottomStatusBar = nullptr;
	
    class QuickAccessIconThread : public StoppableThread {
    public:
        QuickAccessIconThread(HWND ownerWindow, int imageSize);
        ~QuickAccessIconThread() override;
        HBITMAP takeBitmap();
    private:
        void run() override;
        HWND owner;
        int iconSize;
        HBITMAP bitmap = nullptr;
    };
    CComPtr<QuickAccessIconThread> quickAccessIconThread;
    HIMAGELIST quickAccessImages = nullptr;

    ShellViewHost viewHost;
    ShellSelectionClear selectionClear;
    bool selectionBindingPending = false;
    bool hybridViewEligible = false;
    FOLDERVIEWOPTIONS viewProfile = FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING;
    bool viewProfilePending = false, viewProfilePosted = false, viewProfileChanging = false;
    bool viewMenuRequested = false;
    unsigned viewMenuDepth = 0;
    POINT pendingViewMenuPoint = {};
    HWND pendingKeyboardControl = nullptr;
    bool pendingKeyboardView = false;
    FOLDERVIEWOPTIONS profileForMode(UINT mode) const;
    void requestViewProfile();
    void synchronizeViewProfile();
    bool initialNavigationPending = false;
    unsigned navigationDepth = 0;
    UINT deferredCloseMessage = 0;
    HRESULT browseToItem();
    HRESULT refreshView();
    HRESULT refreshEnumeration();
    HRESULT invokeViewCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info);
    void resumeDeferredClose();
    bool isBackgroundActivationClick(const MSG *msg) const;
    bool cursorOverShellView() const;
    bool activationClickPending = false;

    CComPtr<IShellItem> selected; // retains the selected shortcut rather than its resolved target

    // jank flags
    bool updateSelectionOnActivate = false;
    bool activateOnShiftRelease = false;
    bool invokingDefaultVerb = false;

    std::unique_ptr<ShellViewState> storedViewState;
};

} // namespace
