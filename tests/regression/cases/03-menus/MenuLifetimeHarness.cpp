// API doubles for deterministic reentrancy tests. Production methods are included
// verbatim by Prepare-Sources.ps1; this does not validate a real Shell extension.
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using HRESULT = int;
using UINT = unsigned;
using WORD = unsigned short;
using HWND = void *;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using HHOOK = void *;
using HOOKPROC = LRESULT (*)(int, WPARAM, LPARAM);
struct POINT { int x, y; };
struct CMINVOKECOMMANDINFO { UINT cbSize; HWND hwnd = nullptr; const char *lpVerb = nullptr; };
struct CMINVOKECOMMANDINFOEX {
    UINT cbSize; HWND hwnd; const char *lpDirectory; const wchar_t *lpDirectoryW;
};
constexpr HRESULT S_OK = 0, E_FAIL = -1;
constexpr unsigned FVO_DEFAULT = 0;
namespace selectionDetail { UINT readyMessage() { return 0xc001; } }
constexpr UINT CMF_CANRENAME = 1, CMF_EXTENDEDVERBS = 2, CMF_ITEMMENU = 4,
    CMF_NODEFAULT = 8, CMF_OPTIMIZEFORINVOKE = 16;
constexpr UINT TPM_RIGHTBUTTON = 2, TPM_RETURNCMD = 256, MM_ADDSEPARATOR = 1;
constexpr int SVGIO_SELECTION = 1, SVGIO_BACKGROUND = 0, VK_SHIFT = 16,
    CP_ACP = 0, SIGDN_DESKTOPABSOLUTEPARSING = 1;
constexpr WORD IDM_SETTINGS = 20, IDM_TOGGLE_THUMBNAILS = 21, IDM_FILL_COLUMNS = 22;
constexpr int IDS_GLOBAL_THUMBNAILS = 1, IDS_FILL_COLUMNS = 2;
constexpr int IDM_SHELL_FIRST = 100, IDM_SHELL_LAST = 200, IDR_ITEM_MENU = 1;
#define SUCCEEDED(hr) ((hr) >= 0)
inline bool checkResult(HRESULT hr) { return SUCCEEDED(hr); }
#define checkHR(hr) checkResult(hr)
#define checkLE(expr) (expr)
#define IID_PPV_ARGS(pp) pp
#define MAKEINTRESOURCE(id) (id)
#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
#define debugPrintf(...) ((void)0)

// This deterministic harness exercises the non-preview menu lifetime path.
// Real preview capture/hover/restore is exercised by group 28 on the Shell.
namespace settings {
    bool getViewMenuPreviewEnabled() { return false; }
    bool getDetailsColumnsEnabled() { return true; }
}
struct FolderState { enum { Icons = 8 }; };
constexpr int WH_MSGFILTER = -1;
HHOOK SetWindowsHookEx(int, HOOKPROC, void *, unsigned long) { assert(false); return nullptr; }
bool UnhookWindowsHookEx(HHOOK) { assert(false); return false; }
unsigned long GetCurrentThreadId() { assert(false); return 0; }
bool KillTimer(HWND, std::uintptr_t) { assert(false); return false; }

struct ItemWindow;
enum class Stage { None, Popup, Settings, ItemObject, QueryMenu, Window,
    Path, View, Site, Invoke, Unsite, MenuRelease, ViewRelease, BrowserRelease };
struct Observation {
    ItemWindow *source = nullptr;
    Stage closeAt = Stage::None;
    bool fired = false, closed = false, destroyed = false, cleared = false;
    int menus = 0, objects = 0, invoked = 0, saves = 0, sites = 0, detaches = 0;
    int profileRequests = 0, selectionReadyPosts = 0;
    int columnSuspends = 0, columnResumes = 0, thumbnailToggles = 0, columnToggles = 0;
    int command = IDM_SHELL_FIRST;
    bool failSelection = false, failQuery = false, failView = false, failSite = false;
} observation;
void boundary(Stage);

struct Ref {
    int refs = 1;
    Stage releaseStage;
    explicit Ref(Stage stage = Stage::None) : releaseStage(stage) { ++observation.objects; }
    virtual ~Ref() { --observation.objects; }
    void AddRef() { assert(refs > 0); ++refs; }
    void Release() { boundary(releaseStage); assert(refs > 0); if (--refs == 0) delete this; }
};

template<class T> struct CComPtr {
    T *p = nullptr;
    CComPtr() = default;
    CComPtr(T *value) : p(value) { if (p) p->AddRef(); }
    CComPtr(const CComPtr &value) : CComPtr(value.p) {}
    CComPtr(CComPtr &&value) noexcept : p(std::exchange(value.p, nullptr)) {}
    ~CComPtr() { Release(); }
    CComPtr &operator=(T *value) { CComPtr temp(value); std::swap(p, temp.p); return *this; }
    CComPtr &operator=(const CComPtr &value) { return *this = value.p; }
    operator T *() const { return p; }
    T *operator->() const { assert(p); return p; }
    T **operator&() { assert(!p); return &p; }
    void Attach(T *value) { Release(); p = value; }
    void Release() { T *old = std::exchange(p, nullptr); if (old) old->Release(); }
};
template<class T> struct CComHeapPtr {
    T *p = nullptr;
    ~CComHeapPtr() { delete[] p; }
    operator T *() const { return p; }
    T **operator&() { assert(!p); return &p; }
};

using COLORREF = unsigned;
using HTHEME = void *;
struct Bitmap {};
using HBITMAP = Bitmap *;
struct MENUITEMINFO {
    unsigned cbSize, fMask = 0;
    HBITMAP hbmpItem = nullptr;
    unsigned wID = 0, fState = 0;
    wchar_t *dwTypeData = nullptr;
};
using MENUITEMINFOW = MENUITEMINFO;
constexpr int COLOR_MENUTEXT=7, MENU_POPUPITEM=14, MPI_NORMAL=1, TMT_TEXTCOLOR=3803, SM_CXSMICON=49;
constexpr unsigned MIIM_BITMAP=128;
constexpr unsigned MIIM_ID=2, MIIM_STRING=64, MIIM_STATE=1, MFS_UNCHECKED=0, MFS_CHECKED=8, SSF_ICONSONLY=1;
constexpr unsigned GCS_VERBW=4;
constexpr bool TRUE=true;
constexpr bool FALSE=false;
struct SHELLSTATE { bool fIconsOnly = false; };
void SHGetSetSettings(SHELLSTATE *, unsigned flags, bool set) { assert(flags == SSF_ICONSONLY && !set); }
const wchar_t *getString(int) { return L"fixture label"; }
int lstrcmpiW(const wchar_t *first, const wchar_t *second) { return std::wcscmp(first, second); }
constexpr wchar_t MDL2_SETTINGS=0xe713;
COLORREF GetSysColor(int) { return 0; }
HTHEME OpenThemeData(HWND, const wchar_t *) { return nullptr; }
HRESULT GetThemeColor(HTHEME, int, int, int, COLORREF *) { return S_OK; }
void CloseThemeData(HTHEME) {}
int GetSystemMetrics(int) { return 16; }
HBITMAP captionCommandBitmap(wchar_t, int, COLORREF) { return new Bitmap; }
bool DeleteObject(HBITMAP bitmap) { delete bitmap; return true; }
struct Menu {};
using HMENU = Menu *;
int GetMenuItemCount(HMENU menu) { assert(menu); return 0; }
UINT GetMenuItemID(HMENU menu, int) { assert(menu); return static_cast<UINT>(-1); }
bool InsertMenuItemW(HMENU menu, UINT, bool, const MENUITEMINFO *info) {
    assert(menu && (info->wID == IDM_TOGGLE_THUMBNAILS || info->wID == IDM_FILL_COLUMNS));
    return true;
}
bool SetMenuItemInfo(HMENU, UINT, bool, MENUITEMINFO *) { return true; }
HMENU CreatePopupMenu() { ++observation.menus; return new Menu; }
HMENU LoadMenu(void *, int) { return CreatePopupMenu(); }
bool DestroyMenu(HMENU m) { assert(m); --observation.menus; delete m; return true; }
void *GetModuleHandle(void *) { return nullptr; }
void Shell_MergeMenus(HMENU m, HMENU common, UINT, UINT, UINT, UINT) { assert(m && common); }
short GetKeyState(int) { return 0; }
int GetClassName(HWND h, wchar_t *name, std::size_t size) { assert(h && size > 1); name[0] = 0; return 0; }
int WideCharToMultiByte(int, int, const wchar_t *p, int, char *out, int count, void *, void *) {
    if (!p) return 0;
    if (out && count) out[0] = 0;
    return 1;
}
int TrackPopupMenuEx(HMENU m, UINT, int, int, HWND owner, void *) {
    assert(m && owner && !observation.closed);
    boundary(Stage::Popup);
    return observation.command;
}

struct IShellItem : Ref {
    HRESULT GetDisplayName(int, wchar_t **out) {
        boundary(Stage::Path);
        *out = new wchar_t[2]{L'x', 0};
        return S_OK;
    }
};
struct IFolderView2 : Ref {};
struct IContextMenu : Ref {
    IFolderView2 *site = nullptr;
    IContextMenu() : Ref(Stage::MenuRelease) {}
    ~IContextMenu() override { assert(!site); }
    HRESULT QueryContextMenu(HMENU m, UINT, UINT, UINT, UINT) {
        assert(m && !observation.closed);
        boundary(Stage::QueryMenu);
        return observation.failQuery ? E_FAIL : S_OK;
    }
    HRESULT InvokeCommand(CMINVOKECOMMANDINFO *) {
        assert(!observation.closed);
        ++observation.invoked;
        boundary(Stage::Invoke);
        return S_OK;
    }
    HRESULT GetCommandString(UINT, UINT, void *, char *text, std::size_t) {
        constexpr wchar_t verb[] = L"view";
        std::memcpy(text, verb, sizeof(verb));
        return S_OK;
    }
};
using IContextMenu2 = IContextMenu;
using IContextMenu3 = IContextMenu;
// The real scoped registration is covered by the native Shell/STA suite.
// Here keep the existing destruction/reentrancy boundaries at InvokeCommand.
HRESULT invokeShellCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info) {
    return menu->InvokeCommand(info);
}
struct IShellView : Ref {
    IShellView() : Ref(Stage::ViewRelease) {}
    HRESULT GetItemObject(int mode, IContextMenu **out) {
        assert(!observation.closed);
        assert(mode == SVGIO_BACKGROUND);
        boundary(Stage::ItemObject);
        if (mode == SVGIO_SELECTION && observation.failSelection) return E_FAIL;
        *out = new IContextMenu;
        return S_OK;
    }
    HRESULT GetWindow(HWND *out) {
        assert(!observation.closed);
        boundary(Stage::Window);
        *out = reinterpret_cast<HWND>(2);
        return S_OK;
    }
};
struct IExplorerBrowser : Ref {
    IExplorerBrowser() : Ref(Stage::BrowserRelease) {}
    HRESULT GetCurrentView(IFolderView2 **out) {
        assert(!observation.closed);
        boundary(Stage::View);
        if (observation.failView) return E_FAIL;
        *out = new IFolderView2;
        return S_OK;
    }
};
HRESULT IUnknown_SetSite(IContextMenu *menu, IFolderView2 *site) {
    if (site) {
        assert(!observation.closed);
        boundary(Stage::Site);
        if (observation.failSite) return E_FAIL;
        site->AddRef();
        menu->site = site;
        ++observation.sites;
    } else {
        boundary(Stage::Unsite);
        if (auto previous = std::exchange(menu->site, nullptr)) previous->Release();
        ++observation.detaches;
    }
    return S_OK;
}

struct ItemWindow : Ref {
    HWND hwnd = reinterpret_cast<HWND>(1);
    bool closing = false;
    CComPtr<IShellItem> item;
    CComPtr<IContextMenu2> contextMenu2;
    CComPtr<IContextMenu3> contextMenu3;
    ItemWindow() { item.Attach(new IShellItem); }
    ~ItemWindow() override { observation.destroyed = true; }
    HWND getToolbarMenuOwner() const { return nullptr; }
    bool isWindowOperational() const { return hwnd != nullptr && !closing; }
    virtual void closeNow() { closing = true; hwnd = nullptr; observation.closed = true; Release(); }
    virtual void trackContextMenu(POINT);
    int trackContextMenu(POINT, HMENU, HWND = nullptr);
    bool onCommand(WORD cmd) {
        if (cmd == IDM_SETTINGS) {
            // The existing inner Settings guard intentionally ends before its caller.
            CComPtr<ItemWindow> keepAlive(this);
            boundary(Stage::Settings);
            observation.cleared = observation.closed;
        }
        return true;
    }
    CMINVOKECOMMANDINFOEX makeInvokeInfo(int, POINT) { return {sizeof(CMINVOKECOMMANDINFOEX), hwnd, nullptr, nullptr}; }
};
// Stand-in for the new backend boundary. Keep the browser reference across
// reentrant GetCurrentView just as ShellViewHost::currentView does.
struct ShellViewHost {
    CComPtr<IExplorerBrowser> browser;
    ShellViewHost() { browser.Attach(new IExplorerBrowser); }
    explicit operator bool() const { return static_cast<bool>(browser); }
    HRESULT currentView(IFolderView2 **out) {
        CComPtr<IExplorerBrowser> current = browser;
        return current ? current->GetCurrentView(out) : E_FAIL;
    }
    void destroy() { browser.Release(); }
};
// Column geometry is outside this lifetime harness; retain its suspend/resume
// boundary so closure during a menu must still balance the scoped ownership.
struct ShellViewColumns {
    void suspend(bool value) { if (value) ++observation.columnSuspends; else ++observation.columnResumes; }
};
struct FolderWindow : ItemWindow {
    struct ViewMenuPreview {
        static ViewMenuPreview *active;
        ViewMenuPreview(FolderWindow &, HMENU, IContextMenu *, IFolderView2 *) { assert(false); }
        bool capture() { assert(false); return false; }
        void finish(UINT) { assert(false); }
        static LRESULT filter(int, WPARAM, LPARAM) { assert(false); return 0; }
    };
    std::shared_ptr<ViewMenuPreview> viewMenuPreview, viewMenuRestore;
    std::shared_ptr<ShellViewColumns> columnLayout = std::make_shared<ShellViewColumns>();
    bool fillColumns = true;
    void toggleGlobalThumbnails() { assert(isWindowOperational()); ++observation.thumbnailToggles; }
    void toggleFillColumns() { assert(isWindowOperational()); ++observation.columnToggles; }
    bool viewReady = true, initialStateApplied = true, enumerationComplete = true, restoringState = false;
    bool provisionalInputBlocked = false, provisionalCaptureBusy = false, provisionalBaseline = false;
    bool captureProvisionalCurrent(bool, bool) { assert(false); return false; }
    enum { TIMER_SAVE_STATE = 2 };
    void viewStateDirty(unsigned) { assert(false); }
    void persistViewState() { assert(false); }
    CComPtr<IShellView> shellView;
    ShellViewHost viewHost;
    FolderWindow() { shellView.Attach(new IShellView); }
    void closeNow() override {
        if (observation.closed) return;
        closing = true;
        hwnd = nullptr;
        observation.closed = true;
        shellView.Release();
        viewHost.destroy();
        Release();
    }
    void scheduleStateSave() { assert(!observation.closed && !observation.cleared); ++observation.saves; }
    // Close deferral around view verbs; a direct closeNow() here is not a queued WM_CLOSE.
    unsigned navigationDepth = 0;
    UINT deferredCloseMessage = 0;
    bool hybridViewEligible = false, viewMenuRequested = false;
    bool selectionBindingPending = false, viewProfilePending = false;
    unsigned viewProfile = FVO_DEFAULT, viewMenuDepth = 0;
    POINT pendingViewMenuPoint = {};
    void requestViewProfile() { if (isWindowOperational()) ++observation.profileRequests; }
    HRESULT invokeViewCommand(IContextMenu *menu, CMINVOKECOMMANDINFO *info);
    void resumeDeferredClose();
    CComPtr<IContextMenu> queryBackgroundMenu(HMENU *);
    void newItem(const char *);
    void trackContextMenu(POINT) override;
    void openViewMenu(POINT);
    void openBackgroundSubMenu(IContextMenu *, HMENU, POINT);
};
FolderWindow::ViewMenuPreview *FolderWindow::ViewMenuPreview::active = nullptr;
bool PostMessageW(HWND window, UINT message, WPARAM, LPARAM) {
    assert(window && !observation.closed && message == selectionDetail::readyMessage());
    ++observation.selectionReadyPosts;
    return true;
}
HMENU findViewMenu(IContextMenu *, HMENU menu) { return menu; } // Borrowed submenu stand-in.
void boundary(Stage stage) {
    if (stage != Stage::None && stage == observation.closeAt && !observation.fired) {
        observation.fired = true;
        observation.source->closeNow();
    }
}

#ifndef FILESPACER_TEST_METHODS
#define FILESPACER_TEST_METHODS "ProductionMethods.inc"
#endif
#include FILESPACER_TEST_METHODS

enum class Operation { Context, NewItem, View };
void run(Operation op, Stage stage, int command = IDM_SHELL_FIRST,
         bool failSelection = false, bool failQuery = false, bool failView = false, bool failSite = false) {
    observation = {};
    observation.closeAt = stage;
    observation.command = command;
    observation.failSelection = failSelection;
    observation.failQuery = failQuery;
    observation.failView = failView;
    observation.failSite = failSite;
    auto source = new FolderWindow;
    observation.source = source;
    if (op == Operation::Context) source->trackContextMenu({0, 0});
    else if (op == Operation::NewItem) source->newItem("NewFolder");
    else source->openViewMenu({0, 0});
    if (stage != Stage::None) {
        assert(observation.fired && observation.closed);
        assert(observation.destroyed && observation.saves == 0);
    } else {
        assert(!observation.destroyed);
        source->closeNow();
    }
    assert(observation.objects == 0 && observation.menus == 0);
    assert(observation.columnSuspends == observation.columnResumes);
    assert(observation.sites == observation.detaches);
    if (stage != Stage::None && stage != Stage::Invoke && stage != Stage::Unsite
            && stage != Stage::BrowserRelease && stage != Stage::ViewRelease)
        assert(observation.invoked == 0);
}

int main() {
    run(Operation::View, Stage::None, IDM_TOGGLE_THUMBNAILS);
    assert(observation.thumbnailToggles == 1 && observation.invoked == 0);
    run(Operation::View, Stage::None, IDM_FILL_COLUMNS);
    assert(observation.columnToggles == 1 && observation.invoked == 0);
    run(Operation::Context, Stage::None);
    run(Operation::Context, Stage::None, 0);
    run(Operation::Context, Stage::None, IDM_SETTINGS);
    run(Operation::Context, Stage::Settings, IDM_SETTINGS);
    run(Operation::Context, Stage::None, IDM_SHELL_FIRST, false, true);
    run(Operation::Context, Stage::None, IDM_SHELL_FIRST, false, false, true);
    run(Operation::Context, Stage::None, IDM_SHELL_FIRST, false, false, false, true);
    for (Stage stage : {Stage::ItemObject, Stage::QueryMenu, Stage::Window,
            Stage::Popup, Stage::Path, Stage::View, Stage::Site, Stage::Invoke,
            Stage::Unsite, Stage::MenuRelease, Stage::ViewRelease, Stage::BrowserRelease})
        run(Operation::Context, stage);
    for (Operation operation : {Operation::NewItem, Operation::View}) {
        run(operation, Stage::None);
        run(operation, Stage::None, IDM_SHELL_FIRST, false, true);
        run(operation, Stage::None, IDM_SHELL_FIRST, false, false, true);
        run(operation, Stage::None, IDM_SHELL_FIRST, false, false, false, true);
        for (Stage stage : {Stage::ItemObject, Stage::QueryMenu, Stage::View,
                Stage::Site, Stage::Invoke, Stage::Unsite})
            run(operation, stage);
    }
    run(Operation::View, Stage::Popup);
    observation = {};
    auto source = new FolderWindow;
    observation.source = source;
    source->hybridViewEligible = true;
    source->openViewMenu({12, 34});
    assert(source->viewMenuRequested && source->pendingViewMenuPoint.x == 12 && source->pendingViewMenuPoint.y == 34);
    assert(observation.profileRequests == 1 && observation.menus == 0 && observation.invoked == 0);
    source->selectionBindingPending = true;
    source->navigationDepth = 1;
    source->resumeDeferredClose();
    assert(!source->selectionBindingPending && observation.selectionReadyPosts == 1);
    source->closeNow();
    assert(observation.objects == 0 && observation.menus == 0);
    std::cout << "PASS: menu closure boundaries, deferred modern-profile menu and selection capability wakeup; references released.\n";
}
