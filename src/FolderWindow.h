#pragma once
#include <common.h>

#include "ItemWindow.h"
#include "SearchBox.h"
#include "ShellViewHost.h"
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
        MSG_ENUM_DONE = WM_APP + 47
    };
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;

    static bool spatialView(IFolderView *folderView);

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
    HRESULT showGestureProperties(POINT point);
    WindowResizeGesture resizeGesture;
    bool suppressAltButtonUp = false;
    bool suppressAltUp = false;

    ClosedWindow reopeningWindow;
    bool initialEnumerationReady = false;
    bool viewOptionsApplied = false;
    bool initialStateApplied = false;
    bool restoreIconsPending = false;
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

    struct ShellViewState {
        FolderViewSettings view;
        std::vector<uint8_t> selection;

        int numItems = 0;
        std::unique_ptr<CComHeapPtr<ITEMID_CHILD>[]> itemIds;
        std::unique_ptr<POINT[]> itemPositions;
    };

    static void getViewState(IFolderView2 *folderView, ShellViewState *state);
    static void setItemPositions(IFolderView2 *folderView, const ShellViewState &state);

    void loadIconPositions();
    void scheduleStateSave(bool icons = false);
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
    bool initialNavigationPending = false;
    unsigned navigationDepth = 0;
    UINT deferredCloseMessage = 0;
    HRESULT browseToItem();
    HRESULT refreshView();
    void resumeDeferredClose();

    CComPtr<IShellItem> selected; // retains the selected shortcut rather than its resolved target

    // jank flags
    bool updateSelectionOnActivate = false;
    bool activateOnShiftRelease = false;
    bool invokingDefaultVerb = false;

    std::unique_ptr<ShellViewState> storedViewState;
};

} // namespace
