#pragma once
#include <common.h>
#include "FolderStateStore.h"

#include "COMUtils.h"
#include "ProxyIcon.h"
#include "PathBar.h"
#include "WinUtils.h"
#include <cstdint>
#include <string>
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>

namespace filespacer {

enum class FolderOpenResult;

class ItemWindow : public WindowImpl, public UnknownImpl {
    friend FolderOpenResult openFolderWindow(IShellItem *, HMONITOR, int);
    friend ProxyIcon;
protected:
    static HACCEL accelTable;
    static int CAPTION_HEIGHT;
    static int cascadeSize();

public:
    static CComPtr<ItemWindow> activeWindow;

    static void init();
    static void uninit();

    static void flashWindow(HWND hwnd);
    static void expireFolderState(HWND hwnd);
    static UINT settingsChangedMessage;

    ItemWindow(IShellItem *item, const std::wstring &identity);

    virtual SIZE requestedSize();
    virtual RECT requestedRect(HMONITOR preferMonitor); // called for root windows

    void resetViewState(); // call immediately after constructing to reset all view state properties

    void close();
    void setForeground();

    // attempt to relocate item if it has been renamed, moved, or deleted
    // return true if item has not changed
    bool resolveItem();

    virtual bool handleTopLevelMessage(MSG *msg);

    CComPtr<IShellItem> item;

protected:
    enum ViewStateIndex {
        STATE_POS, // 0x1
        STATE_SIZE, // 0x2
        STATE_LAST
    };
    enum UserMessage {
        // WPARAM: 0, LPARAM: 0
        MSG_UPDATE_ICONS = WM_USER,
        // see SHChangeNotification_Lock
        MSG_SHELL_NOTIFY = WM_USER + 2, // Keep existing message IDs.
        // WPARAM: 0, LPARAM: 0
        MSG_FLASH_WINDOW,
        MSG_EXPIRE_FOLDER_STATE,
        MSG_LAST
    };
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;

    virtual SIZE defaultSize() const = 0;
    virtual const wchar_t * appUserModelID() const;
    virtual bool isFolder() const;
    virtual DWORD windowStyle() const;
    virtual DWORD windowExStyle() const;
    bool useCustomFrame() const override;

#if 0 // Deferred until FileSpacer has its own public services.
    virtual const wchar_t * helpURL() const;
#endif


    virtual void updateWindowPropStore(IPropertyStore *propStore);
    static void propStoreWriteString(IPropertyStore *propStore,
        const PROPERTYKEY &key, const wchar_t *value);

    // A COM reference keeps this object alive, not its native window or view.
    // External COM/Win32 calls may reenter and change this state before returning.
    bool isWindowOperational() const { return hwnd != nullptr && !closing; }

    bool loadFolderState();
    FolderState &getSavedFolderState() { return savedState; }
    void resetViewState(uint32_t mask);
    void persistViewState();
    virtual uint32_t captureViewState(uint32_t mask);
    void stateStorageError();
    void recordFolderAccess(HRESULT result);
    void viewStateDirty(uint32_t mask);
    void viewStateClean(uint32_t mask);


    // general window commands
    virtual RECT windowBody();

    // message callbacks
    virtual void onCreate();
    virtual bool onCloseRequest(); // return false to block close (probably a bad idea)
    virtual void onDestroy();
    virtual bool onCommand(WORD command);
    virtual LRESULT onDropdown(int command, POINT pos);
    virtual bool onControlCommand(HWND controlHwnd, WORD notif);
    virtual LRESULT onNotify(NMHDR *nmHdr);
    virtual void onActivate(WORD state, HWND prevWindow);
    virtual void onSize(SIZE size);
    virtual void onSettingsChanged();
    virtual void onPaint(PAINTSTRUCT paint);

    PathBar pathBar;

    static TBBUTTON makeToolbarButton(const wchar_t *text, WORD command, BYTE style,
        BYTE state = TBSTATE_ENABLED);
    virtual void addToolbarButtons(HWND tb);
    HWND getCommandToolbar() const { return cmdToolbar; }
    HWND getQuickAccessToolbar() const { return quickAccessToolbar; }
    int getNavigationToolbarWidth() const;
    virtual int getToolbarTooltip(WORD command);

    virtual void trackContextMenu(POINT pos);
	int trackContextMenu(POINT pos, HMENU menu, HWND owner = nullptr);

    void openChild(IShellItem *childItem, bool closeSource);

    virtual IDispatch * getShellViewDispatch();
    void onViewReady();
    virtual void onItemChanged();
    virtual void refresh();

    void deleteProxy();
    CMINVOKECOMMANDINFOEX makeInvokeInfo(int cmd, POINT point);

    CComHeapPtr<wchar_t> title;


    // for handling delayed context menu messages while open (eg. for Open With menu)
    CComQIPtr<IContextMenu2> contextMenu2;
    CComQIPtr<IContextMenu3> contextMenu3;

private:
    bool create(RECT rect, int showCommand);
    virtual const wchar_t * className() const;

    bool centeredProxy() const; // requires useCustomFrame() == true

    void fakeDragMove();
    bool normalWindowRect(RECT *rect) const;
    void recordGeometry();
    void autoSizeProxy(LONG width);
    void layoutToolbarRow(int width);
    LRESULT hitTestNCA(POINT cursor);

    void openParent(bool closeSource);

    void updateTaskbar(); // Properties belong to the visible folder window.

    // folder windows are registered with the Shell
    void registerShellWindow();
    void unregisterShellWindow();

    void registerShellNotify();
    void unregisterShellNotify();

    void itemMoved(IShellItem *newItem);

    void openParentMenu();

    void openCaptionMenu(POINT pos);
    void openProxyProperties();
    void openProxyContextMenu();
    void proxyRename(const wchar_t *name);

    CComPtr<IShellLink> link;
    std::string stateKey;
    FolderState savedState;
    bool stateLoaded = false;
    bool storageErrorShown = false;
    uint32_t dirtyViewState = 0; // FolderState::Part flags

    std::wstring identityProperty;
    RECT lastNormalRect = {};
    bool geometryKnown = false;
    bool lastMaximized = false;
    bool windowLifetime = false;

    ProxyIcon proxyIcon;
    HWND toolbarRebar = nullptr, toolbarHost = nullptr;
    HWND parentToolbar = nullptr, cmdToolbar = nullptr;
    HWND quickAccessToolbar = nullptr;
    long shellWindowCookie = 0;
    ULONG shellNotifyID = 0;
    CComHeapPtr<ITEMIDLIST> notifyItemID;

    bool firstActivate = false, closing = false;
    bool taskbarGrouped = false; // Fixed for this HWND; changes apply to new windows.
    bool taskbarPinningSet = false;

    SRWLOCK iconLock = SRWLOCK_INIT;
    HICON iconLarge = nullptr, iconSmall = nullptr;
    std::wstring taskbarAppID; // UI thread; also used by SHAddToRecentDocs
    std::wstring taskbarIconResource; // protected by iconLock, like the icons

    class IconThread : public StoppableThread {
    public:
        IconThread(IShellItem *item, ItemWindow *callbackWindow);
    protected:
        void run() override;
    private:
        CComHeapPtr<ITEMIDLIST> itemIDList;
        ItemWindow *callbackWindow;
    };
    CComPtr<IconThread> iconThread;

};

} // namespace

