#define GDIPVER 0x0110 // Enable GDI+ 1.1 declarations.
#include "ItemWindow.h"
#include "CreateItemWindow.h"
#include "FolderIdentity.h"
#include <ctime>
#include "main.h"
#include "GeomUtils.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "ShellUtils.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "DPI.h"
#include "UIStrings.h"
#include <windowsx.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <vssym32.h>
#include <shellapi.h>
#include <propkey.h>
#include <Propvarutil.h>
#include <VersionHelpers.h>
#include <string>
#include <utility>
#include <memory>
#include <type_traits>
#include <vector>
#include <cstring>
#include <wincodec.h>
#include <shlwapi.h>
#include <bcrypt.h>
#include <olectl.h>
#pragma warning(push)
#pragma warning(disable: 4458) // Shadowed members in Windows SDK GDI+ headers.
#include <gdiplus.h>
#pragma warning(pop)
#include <cmath>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Windowscodecs.lib")

namespace filespacer {

const wchar_t ITEM_WINDOW_CLASS[] = L"FileSpacer Item Window";
const wchar_t TESTPOS_CLASS[] = L"FileSpacer Test Window";
const wchar_t TOOLBAR_HOST_CLASS[] = L"FileSpacer Toolbar Host";
const wchar_t WINDOW_THEME[] = L"CompositedWindow::Window";
const UINT SC_DRAGMOVE = SC_MOVE | 2; // https://stackoverflow.com/a/35880547/11525734

static HBITMAP captionCommandBitmap(const wchar_t *glyph, int size, COLORREF color);

static LRESULT CALLBACK toolbarHostProc(
        HWND host, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lParam);
        SetWindowLongPtr(host, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
    }

    HWND owner = (HWND)GetWindowLongPtr(host, GWLP_USERDATA);
    if (owner) {
        switch (message) {
            case WM_NOTIFY:
            case WM_COMMAND:
            case WM_CTLCOLORSTATIC:
                return SendMessage(owner, message, wParam, lParam);
            case WM_LBUTTONDOWN: {
                POINT cursor;
                GetCursorPos(&cursor);
                if (DragDetect(host, cursor))
                    SendMessage(owner, WM_SYSCOMMAND, SC_DRAGMOVE, 0);
                return 0;
            }
        }
    }

    return DefWindowProc(host, message, wParam, lParam);
}


// dimensions
static int PARENT_BUTTON_WIDTH = 34; // caption only, matches close button width in windows 10
static int COMP_CAPTION_VMARGIN = 1;
static int TOOLBAR_HEIGHT = 24;
static int TOOLBAR_VERTICAL_PADDING = 3;
static int STATUS_TEXT_MARGIN = 4;

int ItemWindow::CAPTION_HEIGHT = 0; // calculated in init()

static LOGFONT SYMBOL_LOGFONT = {14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
    ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets"};

// these are Windows metrics/colors that are not exposed through the API >:(
static int WIN10_CXSIZEFRAME = 8; // TODO not correct at higher DPIs

static HFONT statusFont = nullptr;
static HFONT symbolFont = nullptr;
static ULONG_PTR navigationGraphicsToken = 0;
static Gdiplus::GraphicsPath *navigationIconPaths[2] = {};
static Gdiplus::GraphicsPath *refreshIconHead = nullptr;
static Gdiplus::RectF navigationIconBounds[2];
static const float NAVIGATION_STROKE_WIDTHS[] = {3.0f, 3.0f};
static float navigationIconScale = 0.0f;
static int refreshButtonWidth = 0;

static BOOL compositionEnabled = FALSE;

HACCEL ItemWindow::accelTable;
UINT ItemWindow::settingsChangedMessage = 0;

CComPtr<ItemWindow> ItemWindow::activeWindow;

// Lucide arrow-up / rotate-cw (stroke 3, refresh rotated -90 degrees).
// Draw the refresh head separately with square caps and a miter join.
// ISC / MIT (Feather arrow-up): see licenses/Lucide-LICENSE.txt.
static void initNavigationIcons() {
    navigationIconScale = (float)scaleDPI(16) / 24.0f;
    Gdiplus::Pen pen(Gdiplus::Color(255, 0, 0, 0), NAVIGATION_STROKE_WIDTHS[0]);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetMiterLimit(1.0f);
    for (int i = 0; i < 2; ++i) {
        Gdiplus::GraphicsPath *path = navigationIconPaths[i] = new Gdiplus::GraphicsPath;
        if (i == 0) {
            const Gdiplus::PointF head[] = {{5.0f, 12.0f}, {12.0f, 5.0f}, {19.0f, 12.0f}};
            path->AddLines(head, (INT)ARRAYSIZE(head));
            path->StartFigure();
            path->AddLine(12.0f, 19.0f, 12.0f, 5.0f);
        } else {
            // After the -90-degree rotation, the free end starts at 2 o'clock.
            path->AddArc(3.0f, 3.0f, 18.0f, 18.0f, 60.0f, 210.0f);
            path->AddBezier(12.0f, 3.0f, 14.52f, 3.0f, 16.93f, 4.0f, 18.74f, 5.74f);
            path->AddLine(18.74f, 5.74f, 21.0f, 8.0f);
            refreshIconHead = new Gdiplus::GraphicsPath;
            const Gdiplus::PointF head[] = {{21.0f, 3.0f}, {21.0f, 8.0f}, {16.0f, 8.0f}};
            refreshIconHead->AddLines(head, (INT)ARRAYSIZE(head));
            Gdiplus::Matrix rotation(0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 24.0f);
            path->Transform(&rotation);
            refreshIconHead->Transform(&rotation);
            // The stroked arc's bounds also contain the square head.
        }
        pen.SetWidth(NAVIGATION_STROKE_WIDTHS[i]);
        path->GetBounds(&navigationIconBounds[i], nullptr, &pen);
    }
    const int iconWidth = (int)std::ceil(navigationIconBounds[1].Width * navigationIconScale);
    refreshButtonWidth = (std::max)(TOOLBAR_HEIGHT, iconWidth + scaleDPI(8));
}

static void setRefreshButtonWidth(HWND toolbar) {
    if (!navigationGraphicsToken)
        return;
    TBBUTTONINFO button = {sizeof(button), TBIF_SIZE};
    button.cx = (WORD)refreshButtonWidth;
    SendMessage(toolbar, TB_SETBUTTONINFO, IDM_REFRESH, (LPARAM)&button);
}

static void drawNavigationIcon(NMTBCUSTOMDRAW *customDraw) {
    const NMCUSTOMDRAW &draw = customDraw->nmcd;
    const bool disabled = (draw.uItemState & CDIS_DISABLED) != 0;
    const int state = disabled ? TS_DISABLED
        : (draw.uItemState & CDIS_SELECTED) ? TS_PRESSED
        : (draw.uItemState & CDIS_HOT) ? TS_HOT : TS_NORMAL;
    HTHEME theme = OpenThemeData(draw.hdr.hwndFrom, L"Toolbar");
    const COLORREF background = GetSysColor(
        IsWindows10OrGreater() ? COLOR_WINDOW : COLOR_3DFACE);
    const COLORREF color = getNavigationIconColor(disabled, background);
    const int iconIndex = draw.dwItemSpec == IDM_PREV_WINDOW ? 0 : 1;
    const Gdiplus::RectF &bounds = navigationIconBounds[iconIndex];
    // TB_GETRECT is the full button rectangle, independent of its empty text.
    RECT rect = draw.rc;
    SendMessage(draw.hdr.hwndFrom, TB_GETRECT, draw.dwItemSpec, (LPARAM)&rect);
    const float scale = navigationIconScale;
    float x = (float)rect.left + ((float)rectWidth(rect) - bounds.Width * scale) / 2.0f
        - bounds.X * scale;
    float y = (float)rect.top + ((float)rectHeight(rect) - bounds.Height * scale) / 2.0f
        - bounds.Y * scale;
    if (!theme && state == TS_PRESSED) {
        x += 1.0f;
        y += 1.0f;
    }
    Gdiplus::Graphics graphics(draw.hdc);
    graphics.SetSmoothingMode(iconIndex == 1
        ? Gdiplus::SmoothingModeAntiAlias8x8
        : Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Matrix placement(scale, 0.0f, 0.0f, scale, x, y);
    graphics.SetTransform(&placement);
    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)),
        NAVIGATION_STROKE_WIDTHS[iconIndex]);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    graphics.DrawPath(&pen, navigationIconPaths[iconIndex]);
    if (iconIndex == 1) {
        pen.SetLineCap(Gdiplus::LineCapSquare, Gdiplus::LineCapSquare, Gdiplus::DashCapFlat);
        pen.SetLineJoin(Gdiplus::LineJoinMiter);
        graphics.DrawPath(&pen, refreshIconHead);
    }
    if (theme)
        CloseThemeData(theme);
}

static bool highContrastEnabled() {
    HIGHCONTRAST highContrast = {sizeof(highContrast)};
    checkLE(SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &highContrast, 0));
    return highContrast.dwFlags & HCF_HIGHCONTRASTON;
}

static int windowResizeMargin() {
    return IsThemeActive() ? WIN10_CXSIZEFRAME : GetSystemMetrics(SM_CXSIZEFRAME);
}

static int captionTopMargin() {
    return compositionEnabled ? COMP_CAPTION_VMARGIN : 0;
}

static int toolbarEdgeHeight() {
    return GetSystemMetrics(SM_CYBORDER);
}

static bool invisibleBorders() {
    return compositionEnabled && IsWindows10OrGreater() && !highContrastEnabled();
}

int ItemWindow::cascadeSize() {
    return CAPTION_HEIGHT + (invisibleBorders() ? 0 : windowResizeMargin());
}

void ItemWindow::init() {
    settingsChangedMessage = checkLE(RegisterWindowMessage(L"FileSpacer.SettingsChanged"));
    TaskbarOwnerWindow::init();
    ProxyIcon::init();
    PathBar::init();

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASS itemClass = {};
    itemClass.lpfnWndProc = windowProc;
    itemClass.hInstance = hInstance;
    itemClass.lpszClassName = ITEM_WINDOW_CLASS;
    if (!compositionEnabled)
        itemClass.style = CS_HREDRAW; // redraw caption when resizing
    // change toolbar color
    if (IsWindows10OrGreater())
        itemClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&itemClass);

    WNDCLASS testPosClass = {};
    testPosClass.lpszClassName = TESTPOS_CLASS;
    testPosClass.lpfnWndProc = DefWindowProc;
    testPosClass.hInstance = hInstance;
    RegisterClass(&testPosClass);

    WNDCLASS toolbarHostClass = {};
    toolbarHostClass.lpszClassName = TOOLBAR_HOST_CLASS;
    toolbarHostClass.lpfnWndProc = toolbarHostProc;
    toolbarHostClass.hInstance = hInstance;
    toolbarHostClass.hbrBackground =
        GetSysColorBrush(IsWindows10OrGreater() ? COLOR_WINDOW : COLOR_3DFACE);
    RegisterClass(&toolbarHostClass);

    checkHR(DwmIsCompositionEnabled(&compositionEnabled));

    if (compositionEnabled) {
        RECT adjustedRect = {};
        AdjustWindowRectEx(&adjustedRect, WS_OVERLAPPEDWINDOW, FALSE, 0);
        CAPTION_HEIGHT = -adjustedRect.top; // = 31
    } else {
        CAPTION_HEIGHT = GetSystemMetrics(SM_CYCAPTION);
    }

    PARENT_BUTTON_WIDTH = scaleDPI(PARENT_BUTTON_WIDTH);
    COMP_CAPTION_VMARGIN = scaleDPI(COMP_CAPTION_VMARGIN);
    TOOLBAR_HEIGHT = scaleDPI(TOOLBAR_HEIGHT);
    TOOLBAR_VERTICAL_PADDING = scaleDPI(TOOLBAR_VERTICAL_PADDING);
    STATUS_TEXT_MARGIN = scaleDPI(STATUS_TEXT_MARGIN);
    WIN10_CXSIZEFRAME = scaleDPI(WIN10_CXSIZEFRAME);
    SYMBOL_LOGFONT.lfHeight = scaleDPI(SYMBOL_LOGFONT.lfHeight);

    if (HTHEME theme = OpenThemeData(nullptr, WINDOW_THEME)) {
        LOGFONT logFont;
        if (checkHR(GetThemeSysFont(theme, TMT_STATUSFONT, &logFont)))
            statusFont = CreateFontIndirect(&logFont);
        ProxyIcon::initTheme(theme);
        checkHR(CloseThemeData(theme));
    } else {
        NONCLIENTMETRICS metrics = {sizeof(metrics)};
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        statusFont = CreateFontIndirect(&metrics.lfStatusFont);
        ProxyIcon::initMetrics(metrics);
    }

    symbolFont = checkLE(CreateFontIndirect(&SYMBOL_LOGFONT));
    Gdiplus::GdiplusStartupInput graphicsInput;
    ULONG_PTR graphicsToken = 0;
    if (checkHR(Gdiplus::GdiplusStartup(&graphicsToken, &graphicsInput, nullptr)
            == Gdiplus::Ok ? S_OK : E_FAIL)) {
        navigationGraphicsToken = graphicsToken;
        initNavigationIcons();
    }


    accelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ITEM_ACCEL));
}

void ItemWindow::uninit() {
    ProxyIcon::uninit();
    if (symbolFont)
        DeleteFont(symbolFont);
    if (navigationGraphicsToken) {
        for (auto &path : navigationIconPaths) {
            delete path;
            path = nullptr;
        }
        delete refreshIconHead;
        refreshIconHead = nullptr;
        Gdiplus::GdiplusShutdown(navigationGraphicsToken);
        navigationGraphicsToken = 0;
    }
}

void ItemWindow::flashWindow(HWND hwnd) {
    PostMessage(hwnd, MSG_FLASH_WINDOW, 0, 0);
}

void ItemWindow::expireFolderState(HWND hwnd) {
    SendMessage(hwnd, MSG_EXPIRE_FOLDER_STATE, 0, 0);
}

ItemWindow::ItemWindow(IShellItem *const item, const std::wstring &identity)
        : item(item),
          identityProperty(folderWindowProperty(identity)),
          proxyIcon(this) {
    // The window property is an ASCII prefix followed by a hexadecimal digest.
    for (wchar_t character : identityProperty)
        stateKey.push_back(static_cast<char>(character));
}

const wchar_t * ItemWindow::className() const {
    return ITEM_WINDOW_CLASS;
}

const wchar_t * ItemWindow::appUserModelID() const {
    return taskbarAppID.empty() ? APP_ID : taskbarAppID.c_str();
}

bool ItemWindow::isFolder() const {
    return false;
}

DWORD ItemWindow::windowStyle() const {
    return WS_OVERLAPPEDWINDOW & ~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX;
}

DWORD ItemWindow::windowExStyle() const {
    return 0;
}

bool ItemWindow::useCustomFrame() const {
    return true;
}

bool ItemWindow::centeredProxy() const {
    return compositionEnabled;
}

#if 0 // Deferred until FileSpacer has its own public services.
const wchar_t * ItemWindow::helpURL() const {
    return L"https://github.com/vanjac/chromafiler/wiki";
}
#endif


static std::wstring quoteCommandArgument(const wchar_t *argument) {
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (const wchar_t *next = argument; *next; next++) {
        if (*next == L'\\') {
            backslashes++;
        } else {
            quoted.append(backslashes * (*next == L'"' ? 2 : 1), L'\\');
            if (*next == L'"')
                quoted += L'\\';
            quoted += *next;
            backslashes = 0;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    return quoted + L'"';
}

static std::wstring folderTaskbarID(const wchar_t *parsingName) {
    // A stable SHA-256 of the Shell parsing name fits the 128-character limit.
    // Unlike HWNDs, this identity also matches a later launch of a pinned folder.
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!checkHR(HRESULT_FROM_NT(BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))))
        return {};
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32] = {};
    bool success = checkHR(HRESULT_FROM_NT(BCryptCreateHash(
        algorithm, &hash, nullptr, 0, nullptr, 0, 0)));
    if (success) {
        success = checkHR(HRESULT_FROM_NT(BCryptHashData(hash,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(parsingName)),
            static_cast<ULONG>(lstrlenW(parsingName) * sizeof(wchar_t)), 0)))
            && checkHR(HRESULT_FROM_NT(BCryptFinishHash(hash, digest, sizeof(digest), 0)));
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!success)
        return {};
    const wchar_t hex[] = L"0123456789ABCDEF";
    std::wstring id = std::wstring(APP_ID) + L".Folder";
    for (unsigned char byte : digest) {
        id += hex[byte >> 4];
        id += hex[byte & 15];
    }
    return id;
}

static std::wstring folderRelaunchIcon(PCIDLIST_ABSOLUTE pidl, HICON icon) {
    // Ask the folder's native icon handler, including desktop.ini customizations.
    // This runs on the existing COM icon thread, never on the UI thread.
    CComPtr<IShellFolder> folder;
    PCUITEMID_CHILD childID = nullptr;
    CComPtr<IExtractIconW> extractor;
    if (SUCCEEDED(SHBindToParent(pidl, IID_PPV_ARGS(&folder), &childID))
            && SUCCEEDED(folder->GetUIObjectOf(nullptr, 1, &childID,
                __uuidof(IExtractIconW), nullptr, reinterpret_cast<void **>(&extractor)))) {
        wchar_t file[32768];
        int index = 0;
        UINT flags = 0;
        if (extractor->GetIconLocation(GIL_FORSHORTCUT, file, _countof(file),
                &index, &flags) == S_OK) {
            if (!(flags & GIL_NOTFILENAME))
                return std::wstring(file) + L"," + std::to_wstring(index);
            if (flags & GIL_DONTCACHE)
                return {}; // respect a handler that forbids caching its image
        }
    }

    // Some Shell extensions generate their icons instead of naming a resource.
    // OLE saves the already Shell-selected HICON as ICO; no manual icon encoder.
    if (!icon)
        return {};
    CComHeapPtr<wchar_t> parsingName, localAppData;
    if (!checkHR(SHGetNameFromIDList(pidl, SIGDN_DESKTOPABSOLUTEPARSING, &parsingName))
            || !checkHR(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
        return {};
    std::wstring id = folderTaskbarID(parsingName);
    if (id.empty())
        return {};
    std::wstring directory = std::wstring(localAppData) + L"\\FileSpacer\\FolderIcons";
    int result = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS)
        return {};
    std::wstring file = directory + L"\\" + id + L".ico";
    std::wstring temporary = file + L"." + std::to_wstring(GetCurrentProcessId())
        + L"." + std::to_wstring(GetCurrentThreadId()) + L".tmp";
    PICTDESC description = {};
    description.cbSizeofstruct = sizeof(description);
    description.picType = PICTYPE_ICON;
    description.icon.hicon = icon;
    CComPtr<IPicture> picture;
    CComPtr<IStream> stream;
    if (!checkHR(OleCreatePictureIndirect(&description, __uuidof(IPicture), FALSE,
            reinterpret_cast<void **>(&picture)))
            || !checkHR(SHCreateStreamOnFileEx(temporary.c_str(),
                STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE,
                FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &stream)))
        return {};
    bool saved = checkHR(picture->SaveAsFile(stream, TRUE, nullptr));
    stream.Release();
    if (saved)
        saved = !!checkLE(MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING));
    if (!saved) {
        DeleteFileW(temporary.c_str());
        if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES)
            return {};
    }
    // Keep this file after closing the window: Windows' pinned shortcut needs it.
    return file + L",0";
}

void ItemWindow::updateWindowPropStore(IPropertyStore *const propStore) {
    CComHeapPtr<wchar_t> parsingName;
    wchar_t executable[32768];
    std::wstring id;
    bool grouped = settings::getGroupFolderWindows();
    AcquireSRWLockShared(&iconLock);
    std::wstring resource = taskbarIconResource;
    ReleaseSRWLockShared(&iconLock);
    bool relaunch = false;
    if (!grouped && checkHR(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsingName))) {
        id = folderTaskbarID(parsingName);
        DWORD length = checkLE(GetModuleFileNameW(nullptr, executable, _countof(executable)));
        relaunch = !resource.empty() && !id.empty()
            && length > 0 && length < _countof(executable);
    }

    // PreventPinning is fixed before the first explicit ID on this owner window.
    // A common application group cannot represent separately pinned folders.
    PROPVARIANT preventPinning = {VT_BOOL};
    preventPinning.boolVal = relaunch ? VARIANT_FALSE : VARIANT_TRUE;
    checkHR(propStore->SetValue(PKEY_AppUserModel_PreventPinning, preventPinning));
    PROPVARIANT empty = {VT_EMPTY};
    if (relaunch) {
        std::wstring command = quoteCommandArgument(executable) + L" "
            + quoteCommandArgument(parsingName);
#ifdef FILESPACER_DEBUG
        if (settings::testMode)
            command += L" /test";
#endif
        propStoreWriteString(propStore, PKEY_AppUserModel_RelaunchCommand, command.c_str());
        // The Shell supplies this dynamic folder name; it is not a UI literal.
        propStoreWriteString(propStore, PKEY_AppUserModel_RelaunchDisplayNameResource, title);
        propStoreWriteString(propStore, PKEY_AppUserModel_RelaunchIconResource, resource.c_str());
    } else {
        checkHR(propStore->SetValue(PKEY_AppUserModel_RelaunchCommand, empty));
        checkHR(propStore->SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource, empty));
        checkHR(propStore->SetValue(PKEY_AppUserModel_RelaunchIconResource, empty));
        if (grouped)
            id = APP_ID;
        else if (id.empty())
            id = std::wstring(APP_ID) + L".Window"
                + std::to_wstring(reinterpret_cast<UINT_PTR>(taskbarOwner->getWnd()));
    }
    taskbarAppID = id;
    // Setting ID last tells the taskbar to refresh all relaunch information.
    propStoreWriteString(propStore, PKEY_AppUserModel_ID, id.c_str());
}

void ItemWindow::propStoreWriteString(IPropertyStore *const propStore,
        const PROPERTYKEY &key, const wchar_t *value) {
    PROPVARIANT propVar;
    if (checkHR(InitPropVariantFromString(value, &propVar))) {
        checkHR(propStore->SetValue(key, propVar));
        checkHR(PropVariantClear(&propVar));
    }
}

bool ItemWindow::loadFolderState() {
    if (stateLoaded) return true;
    auto store = folderStateStore();
    if (!store || !store->read(stateKey, &savedState)) {
        stateStorageError();
        return false;
    }
    stateLoaded = true;
    storageErrorShown = false;
    return true;
}

void ItemWindow::stateStorageError() {
    if (!storageErrorShown) {
        storageErrorShown = true;
        reportFolderStateError(hwnd);
    }
}

void ItemWindow::recordFolderAccess(HRESULT result) {
    const bool success = SUCCEEDED(result);
    if (!success && !isFolderAccessFailure(result)) return;
    // A successful access, unlike a delayed save, may recreate an age-expired state.
    if (success && stateKey.empty()) {
        for (wchar_t character : identityProperty)
            stateKey.push_back(static_cast<char>(character));
    }
    if (stateKey.empty() || !loadFolderState()) return;
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    bool expired = false;
    auto store = folderStateStore();
    if (!store || !store->recordAccess(stateKey, success, now, &expired)) {
        if (!success && !savedState.firstFailedAt) savedState.firstFailedAt = now;
        stateStorageError();
        return;
    }
    if (success) {
        savedState.lastSuccessAt = now;
        savedState.firstFailedAt = 0;
    } else if (!savedState.firstFailedAt) {
        savedState.firstFailedAt = now;
    }
    if (expired) stateKey.clear();
    storageErrorShown = false;
}

void ItemWindow::resetViewState(uint32_t mask) {
    auto store = folderStateStore();
    if (!store || !store->clear(stateKey, mask)) {
        stateStorageError();
        return;
    }
    savedState.present &= ~mask;
    if (mask & FolderState::View) savedState.view = {};
    if (mask & FolderState::Icons) savedState.icons.clear();
    viewStateClean(mask);
}

void ItemWindow::resetViewState() {
    resetViewState(FolderState::All);
}

void ItemWindow::persistViewState() {
    if (stateKey.empty()) return; // No identity, or state expired after qualified access failures.
    if (!loadFolderState()) return; // Do not overwrite state that could not be read.
    const uint32_t captured = captureViewState(dirtyViewState);
    if (!captured) return;
    auto store = folderStateStore();
    if (store && store->save(stateKey, savedState, captured)) {
        savedState.present |= captured;
        viewStateClean(captured);
        storageErrorShown = false;
    } else {
        stateStorageError(); // Failed writes remain dirty for the next event or close.
    }
}

uint32_t ItemWindow::captureViewState(uint32_t mask) {
    RECT rect = {};
    if (!normalWindowRect(&rect)) return 0;
    const uint32_t captured = mask & (FolderState::Position | FolderState::Size);
    if (captured & FolderState::Position) {
        savedState.x = invScaleDPI(rect.left);
        savedState.y = invScaleDPI(rect.top);
    }
    if (captured & FolderState::Size) {
        SIZE size = rectSize(rect);
        savedState.width = invScaleDPI(size.cx);
        savedState.height = invScaleDPI(size.cy);
    }
    return captured;
}

void ItemWindow::viewStateDirty(uint32_t mask) {
    dirtyViewState |= mask;
}

void ItemWindow::viewStateClean(uint32_t mask) {
    dirtyViewState &= ~mask;
}

bool ItemWindow::create(RECT rect, int showCommand) {
    if (identityProperty.empty() || !checkHR(item->GetDisplayName(SIGDN_NORMALDISPLAY, &title)))
        return false;
    debugPrintf(L"Open %s\n", &*title);
    taskbarOwner.Attach(new TaskbarOwnerWindow(this, showCommand));
    if (!taskbarOwner->getWnd()) {
        taskbarOwner = nullptr;
        return false;
    }
    HWND createHwnd = checkLE(CreateWindowEx(
        windowExStyle(), className(), title, windowStyle(),
        rect.left, rect.top, rectWidth(rect), rectHeight(rect),
        taskbarOwner->getWnd(), nullptr, GetModuleHandle(nullptr), (WindowImpl *)this));
    if (!createHwnd) {
        taskbarOwner = nullptr;
        return false;
    }
    ShowWindow(createHwnd, showCommand);
    return true;
}

void ItemWindow::close() {
    // TODO: remove this method and use Post/Send directly
    PostMessage(hwnd, WM_CLOSE, 0, 0);
}


void ItemWindow::setForeground() {
    SetForegroundWindow(hwnd);
}


RECT ItemWindow::windowBody() {
    RECT rect = clientRect(hwnd);
    if (useCustomFrame())
        rect.top += CAPTION_HEIGHT;
    if (toolbarRebar && (GetWindowLongPtr(toolbarRebar, GWL_STYLE) & WS_VISIBLE))
        rect.top += rectHeight(windowRect(toolbarRebar));
    return rect;
}

void ItemWindow::fakeDragMove() {
    // https://stackoverflow.com/a/35880547/11525734
    SendMessage(hwnd, WM_SYSCOMMAND, SC_DRAGMOVE, 0);
}

LRESULT ItemWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY)
        closing = true; // Publish closure before any external callback.
    const bool lifecycle = message == WM_CLOSE || message == WM_DESTROY
        || message == WM_NCDESTROY;
    CComPtr<ItemWindow> menuKeepAlive;
    if (!lifecycle && (contextMenu2 || contextMenu3))
        menuKeepAlive = this;
    if (message == WM_EXITSIZEMOVE)
        persistViewState();
    if (settingsChangedMessage && message == settingsChangedMessage) {
        onSettingsChanged();
        return 0;
    }
    if (message == WM_DRAWITEM && proxyIcon.drawTitle(
            reinterpret_cast<DRAWITEMSTRUCT *>(lParam)))
        return TRUE;
    // Extensions must not consume the host's close/destruction messages.
    if (!lifecycle && isWindowOperational()) {
        CComPtr<IContextMenu3> menu3 = contextMenu3;
        CComPtr<IContextMenu2> menu2 = contextMenu2;
        if (menu3 && isWindowOperational()) {
            LRESULT result = 0;
            if (SUCCEEDED(menu3->HandleMenuMsg2(message, wParam, lParam, &result)))
                return result;
        } else if (menu2 && isWindowOperational()) {
            if (SUCCEEDED(menu2->HandleMenuMsg(message, wParam, lParam)))
                return 0;
        }
    }
    if (menuKeepAlive && !isWindowOperational())
        return 0;

    switch (message) {
        case WM_NCCREATE:
            // Publish identity before Shell/COM initialization can dispatch another open.
            if (!SetPropW(hwnd, identityProperty.c_str(), reinterpret_cast<HANDLE>(1)))
                return FALSE;
            AddRef(); // keep the object alive from the first native window message
            lockProcess();
            windowLifetime = true;
            break;
        case WM_CREATE:
            onCreate();
            // ensure WM_NCCALCSIZE gets called; necessary for custom frame
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        case WM_CLOSE:
            if (closing)
                return 0;
            closing = true;
            if (!onCloseRequest()) {
                closing = false;
                return 0; // don't close
            }
            break; // pass to DefWindowProc
        case WM_DESTROY:
            onDestroy();
            return 0;
        case WM_NCDESTROY:
            RemovePropW(hwnd, identityProperty.c_str());
            proxyIcon.destroy();
            // don't need icon lock since icon thread is stopped
            if (iconLarge) {
                checkLE(DestroyIcon(iconLarge));
                FILESPACER_MEMLEAK_FREE;
            }
            if (iconSmall) {
                checkLE(DestroyIcon(iconSmall));
                FILESPACER_MEMLEAK_FREE;
            }
            hwnd = nullptr;
            if (windowLifetime) {
                windowLifetime = false;
                unlockProcess();
                Release(); // allow window to be deleted
            }
            return 0;
        case WM_ACTIVATE:
            onActivate(LOWORD(wParam), (HWND)lParam);
            return 0;
        case WM_NCACTIVATE: {
            proxyIcon.setActive(!!wParam);
            LRESULT res = DefWindowProc(hwnd, message, wParam, lParam);
            if (!compositionEnabled)
                proxyIcon.redrawToolbar();
            return res;
        }
        case WM_NCPAINT: {
            LRESULT res = DefWindowProc(hwnd, message, wParam, lParam);
            if (!compositionEnabled)
                proxyIcon.redrawToolbar();
            return res;
        }
        case WM_NCCALCSIZE:
            if (wParam == TRUE && useCustomFrame()) {
                // allow resizing past the edge of the window by reducing client rect
                // TODO: revisit this
                NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                int resizeMargin = windowResizeMargin();
                params->rgrc[0].left = params->rgrc[0].left + resizeMargin;
                if (!compositionEnabled)
                    params->rgrc[0].top = params->rgrc[0].top + resizeMargin;
                params->rgrc[0].right = params->rgrc[0].right - resizeMargin;
                params->rgrc[0].bottom = params->rgrc[0].bottom - resizeMargin;
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            LRESULT defHitTest = DefWindowProc(hwnd, message, wParam, lParam);
            if (defHitTest != HTCLIENT && defHitTest != HTSYSMENU)
                return defHitTest;
            return hitTestNCA(pointFromLParam(lParam));
        }
        case WM_PAINT: {
            PAINTSTRUCT paint;
            BeginPaint(hwnd, &paint);
            onPaint(paint);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
            invalidateNavigationIconColors();
            if (parentToolbar) {
                if (message == WM_SYSCOLORCHANGE)
                    SendMessage(parentToolbar, message, wParam, lParam);
                InvalidateRect(parentToolbar, nullptr, FALSE);
            }
            if (cmdToolbar) {
                if (message == WM_SYSCOLORCHANGE)
                    SendMessage(cmdToolbar, message, wParam, lParam);
                InvalidateRect(cmdToolbar, nullptr, FALSE);
            }
            break;
        case WM_THEMECHANGED:
            invalidateNavigationIconColors();
            // TODO: duplicate code, must be kept in sync with onCreate()
            // reset fonts
            proxyIcon.onThemeChanged();
            pathBar.setFont(statusFont);
            if (cmdToolbar && symbolFont)
                PostMessage(cmdToolbar, WM_SETFONT, (WPARAM)symbolFont, TRUE);
            if (quickAccessToolbar && symbolFont)
                PostMessage(quickAccessToolbar, WM_SETFONT, (WPARAM)symbolFont, TRUE);
            if (parentToolbar && symbolFont)
                PostMessage(parentToolbar, WM_SETFONT, (WPARAM)symbolFont, TRUE);
            // reset toolbar button sizes
            if (cmdToolbar) {
                SendMessage(cmdToolbar, TB_SETBUTTONSIZE, 0,
                    MAKELPARAM(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT));
                setRefreshButtonWidth(cmdToolbar);
                SIZE ideal = {};
                if (SendMessage(cmdToolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal))
                    SetWindowPos(cmdToolbar, nullptr, 0, 0, ideal.cx, TOOLBAR_HEIGHT,
                        SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
                InvalidateRect(cmdToolbar, nullptr, FALSE);
            }
            if (quickAccessToolbar)
                PostMessage(quickAccessToolbar, TB_SETBUTTONSIZE, 0,
                    MAKELPARAM(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT));
            if (parentToolbar) {
                SIZE size = rectSize(windowRect(parentToolbar));
                PostMessage(parentToolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(size.cx, size.cy));
            }
            if (toolbarRebar)
                layoutToolbarRow(clientSize(hwnd).cx);
            break;
        case WM_CTLCOLORSTATIC: { // status text background color
            HDC hdc = (HDC)wParam;
            int colorI = IsWindows10OrGreater() ? COLOR_WINDOW : COLOR_3DFACE;
            SetBkColor(hdc, GetSysColor(colorI));
            return colorI + 1;
        }
        case WM_WINDOWPOSCHANGED: {
            WINDOWPOS *winPos = (WINDOWPOS *)lParam;
            const auto checkFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED;
            if ((winPos->flags & checkFlags) != checkFlags)
                recordGeometry();
            break; // pass to DefWindowProc
        }
        case WM_SIZE: {
            onSize(sizeFromLParam(lParam));
            return 0;
        }
        case WM_CONTEXTMENU: {
            POINT pos = pointFromLParam(lParam);
            if (pos.x == -1 && pos.y == -1) {
                RECT body = windowBody();
                trackContextMenu(clientToScreen(hwnd, {body.left, body.top}));
                return 0;
            }
            break;
        }
        case WM_NCRBUTTONUP: { // WM_CONTEXTMENU doesn't seem to work in the caption
            POINT cursor = pointFromLParam(lParam);
            POINT clientCursor = screenToClient(hwnd, cursor);
            if (wParam == HTCAPTION) {
                if (PtInRect(tempPtr(windowBody()), clientCursor)) {
                    trackContextMenu(cursor);
                } else {
                    PostMessage(hwnd, WM_SYSCOMMAND, SC_KEYMENU, ' '); // show system menu
                }
                return 0;
            }
            break; // pass to DefWindowProc
        }
        case WM_NOTIFY:
            return onNotify((NMHDR *)lParam);
        case WM_COMMAND:
            if (lParam) {
                if (HIWORD(wParam) == 0 && LOWORD(wParam) != 0) { // special case for buttons, etc
                    if (onCommand(LOWORD(wParam)))
                        return 0;
                }
                if (onControlCommand((HWND)lParam, HIWORD(wParam)))
                    return 0;
            } else {
                if (onCommand(LOWORD(wParam)))
                    return 0;
            }
            break;
        case MSG_EXPIRE_FOLDER_STATE:
            stateKey.clear();
            return 0;
        case MSG_SHELL_NOTIFY: {
            LONG event;
            ITEMIDLIST **idls;
            HANDLE lock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &idls, &event);
            if (lock) {
                CComPtr<IShellItem> item1, item2;
                if (idls[0])
                    checkHR(SHCreateItemFromIDList(idls[0], IID_PPV_ARGS(&item1)));
                if (idls[1])
                    checkHR(SHCreateItemFromIDList(idls[1], IID_PPV_ARGS(&item2)));
                SHChangeNotification_Unlock(lock);
                int compare;
                // TODO SICHINT_TEST_FILESYSPATH_IF_NOT_EQUAL?
                if (item1 && checkHR(item1->Compare(item, SICHINT_CANONICAL, &compare))
                        && compare == 0) {
                    if ((event & (SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER)) && item2) {
                        debugPrintf(L"Item renamed!\n");
                        itemMoved(item2);
                    } else {
                        debugPrintf(L"Resolving item due to shell event\n");
                        resolveItem();
                    }
                }
            }
            return 0;
        }
        case MSG_UPDATE_ICONS: {
            AcquireSRWLockExclusive(&iconLock);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)iconLarge);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)iconSmall);
            taskbarOwner->setIcon(iconSmall, iconLarge);
            proxyIcon.setIcon(iconSmall);
            ReleaseSRWLockExclusive(&iconLock);
            updateTaskbar();

            autoSizeProxy(clientSize(hwnd).cx);
            return 0;
        }
        case MSG_FLASH_WINDOW: {
            FLASHWINFO flash = {sizeof(flash), hwnd, FLASHW_ALL, 3, 100};
            FlashWindowEx(&flash);
            return 0;
        }
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

bool ItemWindow::handleTopLevelMessage(MSG *msg) {
    // The message loop owns a local reference throughout pretranslation.
    if (!isWindowOperational())
        return false;
    if (pathBar.handleTopLevelMessage(msg))
        return true;
    if (!isWindowOperational() || pathBar.hasFocus())
        return false; // Leave native edit shortcuts to the edit control.
    return !!TranslateAccelerator(hwnd, accelTable, msg);
}

void ItemWindow::onCreate() {
    iconThread.Attach(new IconThread(item, this));
    iconThread->start();

    CComHeapPtr<ITEMIDLIST> idList;
    if (checkHR(SHGetIDListFromObject(item, &idList))) {
        if (checkHR(link.CoCreateInstance(__uuidof(ShellLink)))) {
            checkHR(link->SetIDList(idList));
        }
    }
    registerShellNotify();

    setTaskbarPreview();

    SHAddToRecentDocs(SHARD_APPIDINFO, tempPtr(SHARDAPPIDINFO{item, appUserModelID()}));

    HMODULE instance = GetWindowInstance(hwnd);
    if (useCustomFrame()) {
        MARGINS margins;
        margins.cxLeftWidth = 0;
        margins.cxRightWidth = 0;
        margins.cyTopHeight = CAPTION_HEIGHT;
        margins.cyBottomHeight = 0;
        if (compositionEnabled)
            checkHR(DwmExtendFrameIntoClientArea(hwnd, &margins));

        proxyIcon.create(hwnd, title,
            captionTopMargin(), CAPTION_HEIGHT - captionTopMargin());
    }

    // Keep controls available so display settings can change while the window is open.
    if (useCustomFrame()) {
        int toolbarBandHeight = TOOLBAR_HEIGHT
            + 2 * (toolbarEdgeHeight() + TOOLBAR_VERTICAL_PADDING);

        toolbarRebar = checkLE(CreateWindowEx(
            WS_EX_CONTROLPARENT, REBARCLASSNAME, nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
                | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE
                | RBS_VARHEIGHT,
            0, useCustomFrame() ? CAPTION_HEIGHT : 0,
            clientSize(hwnd).cx, toolbarBandHeight,
            hwnd, nullptr, instance, nullptr));

        REBARINFO rebarInfo = {sizeof(rebarInfo)};
        SendMessage(toolbarRebar, RB_SETBARINFO, 0, (LPARAM)&rebarInfo);

        toolbarHost = checkLE(CreateWindow(
            TOOLBAR_HOST_CLASS, nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, clientSize(hwnd).cx, TOOLBAR_HEIGHT,
            toolbarRebar, nullptr, instance, hwnd));

        REBARBANDINFO band = {sizeof(band)};
        band.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
        band.fStyle = RBBS_NOGRIPPER;
        band.hwndChild = toolbarHost;
        band.cxMinChild = 0;
        band.cyMinChild = toolbarBandHeight;
        band.cyChild = toolbarBandHeight;
        band.cyMaxChild = toolbarBandHeight;
        band.cx = clientSize(hwnd).cx;
        SendMessage(toolbarRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&band);

        cmdToolbar = checkLE(CreateWindowEx(
            0, TOOLBARCLASSNAME, nullptr,
            TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER
                | WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN,
            0, 0, 0, TOOLBAR_HEIGHT,
            toolbarHost, nullptr, instance, nullptr));
        SendMessage(cmdToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessage(cmdToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
        SendMessage(cmdToolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT));
        SendMessage(cmdToolbar, TB_SETBITMAPSIZE, 0, 0);
        if (symbolFont)
            SendMessage(cmdToolbar, WM_SETFONT, (WPARAM)symbolFont, FALSE);
        addToolbarButtons(cmdToolbar);
        SendMessage(cmdToolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT));
        setRefreshButtonWidth(cmdToolbar);
        SIZE ideal;
        SendMessage(cmdToolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);
        SetWindowPos(cmdToolbar, nullptr, 0, 0, ideal.cx, TOOLBAR_HEIGHT,
            SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }

    if (useCustomFrame() && (centeredProxy() || cmdToolbar)) {
        CComPtr<IShellItem> parentItem;
        bool canOpenParent = SUCCEEDED(item->GetParent(&parentItem));
        // Folder navigation belongs beside the address bar, even with a centered title.
        bool parentInCaption = centeredProxy() && !isFolder();
        int top = parentInCaption ? captionTopMargin() : 0;
        int width = parentInCaption ? PARENT_BUTTON_WIDTH : TOOLBAR_HEIGHT;
        int height = parentInCaption ? (CAPTION_HEIGHT - captionTopMargin()) : TOOLBAR_HEIGHT;
        HWND parentHwnd = parentInCaption ? hwnd : toolbarHost;
        if (isFolder()) {
            // Keep the Shell image list separate from the parent's text-only glyph.
            quickAccessToolbar = checkLE(CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
                TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN
                    | CCS_NORESIZE | CCS_NODIVIDER | WS_VISIBLE | WS_CHILD,
                0, 0, width, height, parentHwnd, nullptr, instance, nullptr));
            SendMessage(quickAccessToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
            SendMessage(quickAccessToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
            SendMessage(quickAccessToolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM(width, width));
            SendMessage(quickAccessToolbar, TB_SETBITMAPSIZE, 0, 0);
            if (symbolFont)
                SendMessage(quickAccessToolbar, WM_SETFONT, (WPARAM)symbolFont, FALSE);
            TBBUTTON quickAccess = makeToolbarButton(ICON_QUICK_ACCESS,
                IDM_QUICK_ACCESS, BTNS_DROPDOWN);
            SendMessage(quickAccessToolbar, TB_ADDBUTTONS, 1, (LPARAM)&quickAccess);
            SendMessage(quickAccessToolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(width, height));
        }
        parentToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
            TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER
                | WS_VISIBLE | WS_CHILD,
            quickAccessToolbar ? width : 0, top, width, height, parentHwnd,
                nullptr, instance, nullptr);
        SendMessage(parentToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessage(parentToolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM(width, width));
        SendMessage(parentToolbar, TB_SETBITMAPSIZE, 0, 0);
        if (symbolFont)
            SendMessage(parentToolbar, WM_SETFONT, (WPARAM)symbolFont, FALSE);
        TBBUTTON parentButton = {I_IMAGENONE, IDM_PREV_WINDOW,
            (BYTE)(canOpenParent ? TBSTATE_ENABLED : 0),
            BTNS_SHOWTEXT, {}, 0, (INT_PTR)(navigationGraphicsToken ? L"" : ICON_UP_DIR)};
        SendMessage(parentToolbar, TB_ADDBUTTONS, 1, (LPARAM)&parentButton);
        SendMessage(parentToolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(width, height));
    }

    if (toolbarHost && isFolder())
        pathBar.create(toolbarHost, hwnd, item, statusFont, symbolFont);

    // The derived window's controls have not been created yet.
    ItemWindow::onSettingsChanged();

    if (!getShellViewDispatch())
        onViewReady();
}

TBBUTTON ItemWindow::makeToolbarButton(const wchar_t *text, WORD command, BYTE style, BYTE state) {
    if (command == IDM_REFRESH && navigationGraphicsToken)
        text = L""; // The icon is painted after the native button background.
    return {I_IMAGENONE, command, state, (BYTE)(BTNS_SHOWTEXT | style), {}, 0, (INT_PTR)text};
}

void ItemWindow::addToolbarButtons(HWND tb) {
    TBBUTTON buttons[] = {
        makeToolbarButton(MDL2_MORE, IDM_CONTEXT_MENU, BTNS_DROPDOWN),
    };
    SendMessage(tb, TB_ADDBUTTONS, _countof(buttons), (LPARAM)buttons);
}

int ItemWindow::getToolbarTooltip(WORD command) {
    switch (command) {
        case IDM_PREV_WINDOW:
            return IDS_OPEN_PARENT_COMMAND;
        case IDM_CONTEXT_MENU:
            return IDS_MENU_COMMAND;
    }
    return 0;
}

void ItemWindow::trackContextMenu(POINT pos) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        menu(CreatePopupMenu(), destroyMenu);
    if (menu)
        trackContextMenu(pos, menu.get(), hwnd);
}

int ItemWindow::trackContextMenu(POINT pos, HMENU menu, HWND menuOwner) {
    // Both callers own this object and the supplied menu until we return.
    if (!isWindowOperational())
        return 0;
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)> common(
        LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_ITEM_MENU)), destroyMenu);

    Shell_MergeMenus(
        menu,
        common.get(),
        (UINT)-1,
        0,
        0xFFFF,
        MM_ADDSEPARATOR);
    if (!isWindowOperational())
        return 0;
    COLORREF iconColor = GetSysColor(COLOR_MENUTEXT);
    if (HTHEME theme = OpenThemeData(hwnd, L"Menu")) {
        GetThemeColor(theme, MENU_POPUPITEM, MPI_NORMAL, TMT_TEXTCOLOR, &iconColor);
        CloseThemeData(theme);
    }
    // The caller still owns the menu when this local bitmap is released.
    auto releaseSettingsIcon = [menu](HBITMAP bitmap) {
        MENUITEMINFO image = {sizeof(image)};
        image.fMask = MIIM_BITMAP;
        checkLE(SetMenuItemInfo(menu, IDM_SETTINGS, FALSE, &image));
        checkLE(DeleteObject(bitmap));
    };
    std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype(releaseSettingsIcon)> settingsIcon(
        captionCommandBitmap(MDL2_SETTINGS, GetSystemMetrics(SM_CXSMICON), iconColor),
        releaseSettingsIcon);
    if (settingsIcon) {
        MENUITEMINFO image = {sizeof(image)};
        image.fMask = MIIM_BITMAP;
        image.hbmpItem = settingsIcon.get();
        checkLE(SetMenuItemInfo(menu, IDM_SETTINGS, FALSE, &image));
    }
    HWND ownerHwnd = menuOwner ? menuOwner : hwnd;
    int cmd = TrackPopupMenuEx(
        menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD,
        pos.x,
        pos.y,
        ownerHwnd,
        nullptr);
    if (isWindowOperational())
        onCommand((WORD)cmd);
    common.reset();
    return isWindowOperational() ? cmd : 0;
}

bool ItemWindow::onCloseRequest() {
    return true;
}

void ItemWindow::onDestroy() {
    debugPrintf(L"Close %s\n", &*title);
    persistViewState();

    if (activeWindow == this)
        activeWindow = nullptr;

    pathBar.destroy();
    unregisterShellNotify();
    unregisterShellWindow();
    RemovePropW(hwnd, identityProperty.c_str());

    SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0); // remove owner
    taskbarOwner = nullptr;

    if (iconThread)
        iconThread->stop();
}

bool ItemWindow::onCommand(WORD command) {
    switch (command) {
        case IDM_PREV_WINDOW:
            openParent(settings::getKeepSourceWindowOpen()
                == (GetKeyState(VK_CONTROL) < 0));
            return true;
        case IDM_CLOSE_WINDOW:
            close();
            return true;
        case IDM_REFRESH:
            if (resolveItem())
                refresh();
            return true;
        case IDM_PROXY_MENU: {
            CComPtr<ItemWindow> keepAlive(this);
            if (!isWindowOperational())
                return true;
            proxyIcon.setPressedState(true);
            if (isWindowOperational())
                openProxyContextMenu();
            if (isWindowOperational())
                proxyIcon.setPressedState(false);
            return true;
        }
        case IDM_RENAME_PROXY:
            proxyIcon.beginRename();
            return true;
        case IDM_DELETE_PROXY:
            deleteProxy();
            return true;
        case IDM_PARENT_MENU: {
            openParentMenu();
            return true;
        }
#if 0 // Deferred public help.
        case IDM_HELP:
            ShellExecute(nullptr, L"open", helpURL(), nullptr, nullptr, SW_SHOWNORMAL);
            return true;
#endif
        case IDM_SETTINGS: {
            // Maintenance can close this window during the modal dialog.
            CComPtr<ItemWindow> keepAlive(this);
            openSettingsDialog(hwnd);
            return true;
        }
        case IDM_DEBUG_NAMES:
            debugDisplayNames(hwnd, item);
            return true;
    }
    return false;
}

LRESULT ItemWindow::onDropdown(int command, POINT pos) {
    switch (command) {
        case IDM_PROXY_BUTTON:
            openCaptionMenu(pos);
            return TBDDRET_DEFAULT;
        case IDM_CONTEXT_MENU:
            trackContextMenu(pos);
            return TBDDRET_DEFAULT;
    }
    return TBDDRET_NODEFAULT;
}

bool ItemWindow::onControlCommand(HWND controlHwnd, WORD notif) {
    return proxyIcon.onControlCommand(controlHwnd, notif);
}

LRESULT ItemWindow::onNotify(NMHDR *nmHdr) {
    if (pathBar.isWindow(nmHdr->hwndFrom) && nmHdr->code == PathBar::OPEN_ITEM) {
        auto notification = reinterpret_cast<PathBar::OpenItemNotification *>(nmHdr);
        openChild(notification->item,
            settings::getKeepSourceWindowOpen() == notification->control);
        return 0;
    }
    if (toolbarRebar && nmHdr->hwndFrom == toolbarRebar
            && nmHdr->code == RBN_CHILDSIZE) {
        NMREBARCHILDSIZE *rebarChildSize = (NMREBARCHILDSIZE *)nmHdr;
        int margin = toolbarEdgeHeight() + TOOLBAR_VERTICAL_PADDING;
        rebarChildSize->rcChild.top += margin;
        rebarChildSize->rcChild.bottom -= margin;
        return 0;
    } else if (toolbarRebar && nmHdr->hwndFrom == toolbarRebar
            && nmHdr->code == NM_CUSTOMDRAW) {
        NMCUSTOMDRAW *customDraw = (NMCUSTOMDRAW *)nmHdr;

        if (customDraw->dwDrawStage == CDDS_PREPAINT)
            return CDRF_NOTIFYPOSTPAINT;

        if (customDraw->dwDrawStage == CDDS_POSTPAINT) {
            RECT rect = clientRect(toolbarRebar);
            int edgeHeight = toolbarEdgeHeight();

            COLORREF borderColor = GetSysColor(COLOR_3DLIGHT);
            if (HTHEME theme = GetWindowTheme(toolbarRebar))
                GetThemeColor(theme, RP_BAND, 0, TMT_BORDERCOLORHINT, &borderColor);

            HBRUSH borderBrush = CreateSolidBrush(borderColor);

            RECT topEdge = rect;
            topEdge.bottom = topEdge.top + edgeHeight;
            FillRect(customDraw->hdc, &topEdge, borderBrush);

            RECT bottomEdge = rect;
            bottomEdge.top = bottomEdge.bottom - edgeHeight;
            FillRect(customDraw->hdc, &bottomEdge, borderBrush);

            DeleteBrush(borderBrush);
            return CDRF_DODEFAULT;
        }
    } else if (nmHdr->code == TTN_GETDISPINFO) {
        NMTTDISPINFO *dispInfo = (NMTTDISPINFO *)nmHdr;
        if (!(dispInfo->uFlags & TTF_IDISHWND)) {
            int res = getToolbarTooltip((WORD)dispInfo->hdr.idFrom);
            if (res) {
                dispInfo->hinst = GetModuleHandle(nullptr);
                dispInfo->lpszText = MAKEINTRESOURCE(res);
                dispInfo->uFlags |= TTF_DI_SETITEM;
            }
        }
    } else if (nmHdr->code == TBN_DROPDOWN) {
        NMTOOLBAR *nmToolbar = (NMTOOLBAR *)nmHdr;
        RECT buttonRect = {};
        if (!SendMessage(nmHdr->hwndFrom, TB_GETRECT, nmToolbar->iItem, (LPARAM)&buttonRect))
            return TBDDRET_NODEFAULT;
        POINT menuPos = {buttonRect.left, buttonRect.bottom};
        return onDropdown(nmToolbar->iItem, clientToScreen(nmHdr->hwndFrom, menuPos));
    } else if ((proxyIcon.isToolbarWindow(nmHdr->hwndFrom)
            || nmHdr->hwndFrom == parentToolbar || nmHdr->hwndFrom == cmdToolbar)
            && nmHdr->code == NM_CUSTOMDRAW) {
        NMTBCUSTOMDRAW *customDraw = (NMTBCUSTOMDRAW *)nmHdr;
        const bool navigationToolbar = nmHdr->hwndFrom == parentToolbar
            || nmHdr->hwndFrom == cmdToolbar;
        const bool navigationIcon = navigationGraphicsToken
            && ((nmHdr->hwndFrom == parentToolbar && customDraw->nmcd.dwItemSpec == IDM_PREV_WINDOW)
                || (nmHdr->hwndFrom == cmdToolbar && customDraw->nmcd.dwItemSpec == IDM_REFRESH));
        if (customDraw->nmcd.dwDrawStage == CDDS_PREPAINT) {
            LRESULT result = navigationToolbar ? CDRF_NOTIFYITEMDRAW : 0;
            if (nmHdr->hwndFrom != cmdToolbar)
                result |= CDRF_NOTIFYPOSTPAINT;
            return result;
        } else if (customDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT && navigationIcon) {
            return CDRF_NOTIFYPOSTPAINT;
        } else if (customDraw->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT && navigationIcon) {
            drawNavigationIcon(customDraw);
        } else if (customDraw->nmcd.dwDrawStage == CDDS_POSTPAINT) {
            // fix title bar rendering (when not layered)
            makeBitmapOpaque(customDraw->nmcd.hdc, clientRect(nmHdr->hwndFrom));
        }
        return CDRF_DODEFAULT;
    } else if (nmHdr->hwndFrom == parentToolbar && nmHdr->code == NM_LDOWN) {
        RECT buttonRect = {};
        const POINT screenPos = pointFromLParam(GetMessagePos());
        const POINT toolbarPos = screenToClient(parentToolbar, screenPos);
        if (!SendMessage(parentToolbar, TB_GETRECT, IDM_PREV_WINDOW, (LPARAM)&buttonRect)
                || !PtInRect(&buttonRect, toolbarPos))
            return FALSE;
        if (!SendMessage(parentToolbar, TB_ISBUTTONENABLED, IDM_PREV_WINDOW, 0))
            return TRUE;
        const bool closeSource = settings::getKeepSourceWindowOpen()
            == (GetKeyState(VK_CONTROL) < 0);
        if (DragDetect(hwnd, screenPos)) {
            fakeDragMove();
        } else {
            openParent(closeSource);
        }
        return TRUE;
    } else if (nmHdr->hwndFrom == parentToolbar && nmHdr->code == NM_RCLICK) {
        if (((NMMOUSE *)nmHdr)->dwItemSpec != IDM_PREV_WINDOW)
            return FALSE;
        openParentMenu();
        return TRUE;
    }
    return 0;
}

void ItemWindow::onActivate(WORD state, HWND) {
    if (state == WA_INACTIVE)
        persistViewState();
    if (state != WA_INACTIVE) {
        activeWindow = this;
        if (firstActivate)
            resolveItem();
        firstActivate = true;
    }
}

void ItemWindow::onSize(SIZE size) {
    autoSizeProxy(size.cx);
    layoutToolbarRow(size.cx);
}

void ItemWindow::updateTaskbar() {
    CComPtr<IPropertyStore> propStore;
    if (checkHR(SHGetPropertyStoreForWindow(taskbarOwner->getWnd(), IID_PPV_ARGS(&propStore)))) {
        PROPVARIANT preventPinning = {};
        if (checkHR(propStore->GetValue(PKEY_AppUserModel_PreventPinning, &preventPinning))) {
            AcquireSRWLockShared(&iconLock);
            bool cannotPin = settings::getGroupFolderWindows() || taskbarIconResource.empty();
            ReleaseSRWLockShared(&iconLock);
            bool replaceOwner = preventPinning.vt == VT_BOOL
                && (!!preventPinning.boolVal != cannotPin);
            checkHR(PropVariantClear(&preventPinning));
            if (replaceOwner) {
                // Recreate only the taskbar owner: PreventPinning cannot be
                // changed after its first ID. Keep the folder view and selection.
                CComPtr<TaskbarOwnerWindow> previous = taskbarOwner;
                CComPtr<TaskbarOwnerWindow> replacement;
                replacement.Attach(new TaskbarOwnerWindow(this));
                if (replacement->getWnd()) {
                    previous->setPreview(nullptr);
                    checkLE(SetWindowLongPtr(hwnd, GWLP_HWNDPARENT,
                        reinterpret_cast<LONG_PTR>(replacement->getWnd())));
                    taskbarOwner = replacement;
                    setTaskbarPreview();
                    ShowWindow(taskbarOwner->getWnd(), IsIconic(previous->getWnd())
                        ? SW_SHOWMINNOACTIVE : SW_SHOWNOACTIVATE);
                }
            } else {
                updateWindowPropStore(propStore);
            }
        }
    }
}

void ItemWindow::onSettingsChanged() {
    updateTaskbar();
    bool toolbarEnabled = settings::getToolbarEnabled();
    auto showControl = [](HWND control, bool visible) {
        if (control)
            checkLE(SetWindowPos(control, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                    | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
    };
    pathBar.show(toolbarEnabled && settings::getPathBarEnabled());
    showControl(quickAccessToolbar, toolbarEnabled && settings::getQuickAccessEnabled());
    if (parentToolbar && GetParent(parentToolbar) == toolbarHost)
        showControl(parentToolbar, toolbarEnabled && settings::getUpButtonEnabled());
    showControl(cmdToolbar, toolbarEnabled);
    if (isFolder() && cmdToolbar) {
        SendMessage(cmdToolbar, TB_HIDEBUTTON, IDM_REFRESH,
            MAKELPARAM(!settings::getRefreshButtonEnabled(), 0));
        SendMessage(cmdToolbar, TB_HIDEBUTTON, IDM_VIEW_MENU,
            MAKELPARAM(!settings::getViewButtonEnabled(), 0));
        SIZE ideal = {};
        if (SendMessage(cmdToolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal))
            SetWindowPos(cmdToolbar, nullptr, 0, 0, ideal.cx, clientSize(cmdToolbar).cy,
                SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }
    showControl(toolbarRebar, toolbarEnabled);
    onSize(clientSize(hwnd));
    checkLE(RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN));
}

int ItemWindow::getNavigationToolbarWidth() const {
    int width = 0;
    const HWND controls[] = {quickAccessToolbar, parentToolbar};
    for (HWND control : controls) {
        if (control && GetParent(control) == toolbarHost
                && (GetWindowLongPtr(control, GWL_STYLE) & WS_VISIBLE))
            width += clientSize(control).cx;
    }
    return width;
}

void ItemWindow::layoutToolbarRow(int width) {
    if (!toolbarRebar)
        return;

    RECT rebarRect = windowRect(toolbarRebar);
    SetWindowPos(toolbarRebar, nullptr,
        0, useCustomFrame() ? CAPTION_HEIGHT : 0,
        width, rectHeight(rebarRect),
        SWP_NOZORDER | SWP_NOACTIVATE);

    REBARBANDINFO band = {sizeof(band)};
    band.fMask = RBBIM_SIZE;
    band.cx = clientSize(toolbarRebar).cx;
    SendMessage(toolbarRebar, RB_SETBANDINFO, 0, (LPARAM)&band);

    int rebarHeight = (int)SendMessage(toolbarRebar, RB_GETBARHEIGHT, 0, 0);
    SetWindowPos(toolbarRebar, nullptr,
        0, useCustomFrame() ? CAPTION_HEIGHT : 0,
        width, rebarHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    int toolbarLeft = clientSize(toolbarHost).cx;
    if (cmdToolbar && (GetWindowLongPtr(cmdToolbar, GWL_STYLE) & WS_VISIBLE)) {
        toolbarLeft -= clientSize(cmdToolbar).cx;
        SetWindowPos(cmdToolbar, nullptr, toolbarLeft, 0, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    const int navigationWidth = getNavigationToolbarWidth();
    if (parentToolbar && GetParent(parentToolbar) == toolbarHost
            && (GetWindowLongPtr(parentToolbar, GWL_STYLE) & WS_VISIBLE))
        SetWindowPos(parentToolbar, nullptr, navigationWidth - clientSize(parentToolbar).cx,
            0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    const int pathLeft = STATUS_TEXT_MARGIN + navigationWidth;
    pathBar.place(pathLeft, toolbarLeft - STATUS_TEXT_MARGIN, TOOLBAR_HEIGHT);
}

void ItemWindow::autoSizeProxy(LONG width) {
    TITLEBARINFOEX titleBar = {sizeof(titleBar)};
    SendMessage(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&titleBar);
    int closeButtonWidth = rectWidth(titleBar.rgrect[5]);

    int left = centeredProxy() && !isFolder() ? PARENT_BUTTON_WIDTH : 0;
    proxyIcon.autoSize(width, left, closeButtonWidth);
}

bool ItemWindow::normalWindowRect(RECT *rect) const {
    if (!IsIconic(hwnd) && !IsZoomed(hwnd))
        return checkLE(GetWindowRect(hwnd, rect)) != FALSE;
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!checkLE(GetWindowPlacement(hwnd, &placement)))
        return false;
    *rect = placement.rcNormalPosition;
    // WINDOWPLACEMENT uses workspace coordinates for non-tool windows.
    if (!(GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        if (!checkLE(GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)))
            return false;
        OffsetRect(rect, monitor.rcWork.left - monitor.rcMonitor.left,
            monitor.rcWork.top - monitor.rcMonitor.top);
    }
    return true;
}

void ItemWindow::recordGeometry() {
    RECT rect = {};
    if (!normalWindowRect(&rect))
        return;
    if (geometryKnown) {
        if (rect.left != lastNormalRect.left || rect.top != lastNormalRect.top)
            viewStateDirty(1 << STATE_POS);
        if (rectWidth(rect) != rectWidth(lastNormalRect) || rectHeight(rect) != rectHeight(lastNormalRect))
            viewStateDirty(1 << STATE_SIZE);
    }
    lastNormalRect = rect;
    geometryKnown = true;
}

LRESULT ItemWindow::hitTestNCA(POINT cursor) {
    // from https://docs.microsoft.com/en-us/windows/win32/dwm/customframe?redirectedfrom=MSDN#appendix-c-hittestnca-function
    // the default window proc handles the left, right, and bottom edges
    // so only need to check top edge and caption
    RECT screenRect = clientRect(hwnd);
    MapWindowRect(hwnd, nullptr, &screenRect);

    int resizeMargin = windowResizeMargin();
    int captionTop = compositionEnabled ? (screenRect.top + resizeMargin) : screenRect.top;
    if (cursor.y < captionTop && useCustomFrame()) {
        // TODO window corners are a bit more complex than this
        if (cursor.x < screenRect.left + resizeMargin)
            return HTTOPLEFT;
        else if (cursor.x >= screenRect.right - resizeMargin)
            return HTTOPRIGHT;
        else
            return HTTOP;
    } else if (useCustomFrame() && !IsThemeActive()
            && cursor.x >= screenRect.right - GetSystemMetrics(SM_CXSIZE)
            && cursor.y < captionTop + CAPTION_HEIGHT) {
        return HTCLOSE;
    } else {
        return HTCAPTION; // can drag anywhere else in window to move!
    }
}

void ItemWindow::onPaint(PAINTSTRUCT paint) {
    if (useCustomFrame() && compositionEnabled) {
        // clear alpha channel
        BITMAPINFO bitmapInfo = {{sizeof(BITMAPINFOHEADER), 1, 1, 1, 32, BI_RGB}};
        RGBQUAD bitmapBits = { 0x00, 0x00, 0x00, 0x00 };
        StretchDIBits(paint.hdc, 0, 0, clientSize(hwnd).cx, CAPTION_HEIGHT,
                    0, 0, 1, 1, &bitmapBits, &bitmapInfo,
                    DIB_RGB_COLORS, SRCCOPY);
    }
}

void ItemWindow::openChild(IShellItem *const childItem, bool closeSource) {
    CComPtr<ItemWindow> keepAlive(this); // a COM wait may dispatch window messages
    if (!isWindowOperational())
        return;
    CComPtr<IShellItem> resolved = resolveLink(childItem);
    SFGAOF attributes = 0;
    if (!isWindowOperational() || !resolved) return;
    HRESULT access = resolved->GetAttributes(SFGAO_FOLDER, &attributes);
    if (!isWindowOperational())
        return;
    if (!checkHR(access)) {
        recordFolderItemFailure(resolved, access);
        return;
    }
    if (!(attributes & SFGAO_FOLDER)) {
        invokeDefaultVerb(childItem, hwnd, SW_SHOWNORMAL);
        return;
    }
    // A shortcut can point back to the source folder itself.
    const std::wstring targetProperty = folderWindowProperty(getFolderIdentity(resolved));
    if (!isWindowOperational())
        return;
    if (targetProperty == identityProperty) {
        ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
        if (isWindowOperational())
            setForeground();
        return;
    }
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    FolderOpenResult result = openFolderWindow(resolved, monitor, SW_SHOWNORMAL);
    if (isWindowOperational() && closeSource
            && (result == FolderOpenResult::Created || result == FolderOpenResult::Activated))
        close();
}


void ItemWindow::openParent(bool closeSource) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    CComPtr<IShellItem> parentItem;
    if (SUCCEEDED(item->GetParent(&parentItem)) && isWindowOperational())
        openChild(parentItem, closeSource);
}


SIZE ItemWindow::requestedSize() {
    if (loadFolderState() && (savedState.present & FolderState::Size))
        return scaleDPI(SIZE{savedState.width, savedState.height});
    return defaultSize();
}

RECT ItemWindow::requestedRect(HMONITOR preferMonitor) {
    SIZE size = requestedSize();

    // First try the position stored for this item.
    // A stored spatial position takes precedence over the preferred monitor.
    if (loadFolderState() && (savedState.present & FolderState::Position)) {
        POINT pos = scaleDPI(POINT{savedState.x, savedState.y});

        // Validate the stored position against the monitor on which
        // the position actually lies, not against the monitor from
        // which the item is being opened.
        HMONITOR savedMonitor =
            MonitorFromPoint(pos, MONITOR_DEFAULTTONULL);

        if (savedMonitor) {
            MONITORINFO savedMonitorInfo = {sizeof(savedMonitorInfo)};

            if (GetMonitorInfo(savedMonitor, &savedMonitorInfo)) {
                int cascade = cascadeSize();

                if (PtInRect(&savedMonitorInfo.rcWork, pos)
                        && PtInRect(
                            &savedMonitorInfo.rcWork,
                            {pos.x + cascade, pos.y + cascade})) {

                    return RECT{
                        pos.x,
                        pos.y,
                        pos.x + size.cx,
                        pos.y + size.cy
                    };
                }
            }
        }
    }

    // No valid stored position: choose an initial position.
    RECT rect;

    if (!preferMonitor) {
        rect = RECT{
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT + size.cx,
            CW_USEDEFAULT + size.cy
        };
    } else {
        MONITORINFO monitorInfo = {sizeof(monitorInfo)};
        GetMonitorInfo(preferMonitor, &monitorInfo);

        // Find a good initial position on the preferred monitor.
        HINSTANCE inst = GetModuleHandle(nullptr);

        HWND owner = checkLE(CreateWindow(
            TESTPOS_CLASS,
            nullptr,
            WS_OVERLAPPED,
            monitorInfo.rcWork.left,
            monitorInfo.rcWork.top,
            1,
            1,
            nullptr,
            nullptr,
            inst,
            0));

        HWND defWnd = checkLE(CreateWindow(
            TESTPOS_CLASS,
            nullptr,
            WS_OVERLAPPED,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            size.cx,
            size.cy,
            owner,
            nullptr,
            inst,
            0));

        rect = windowRect(defWnd);

        checkLE(DestroyWindow(defWnd));
        checkLE(DestroyWindow(owner));
    }

    viewStateDirty(1 << STATE_POS);
    return rect;
}


void ItemWindow::enableTaskbarOwner(bool enabled) {
    taskbarOwner->setEnabled(enabled);
}

void ItemWindow::setTaskbarPreview() {
    // update app user model id
    CComPtr<IPropertyStore> propStore;
    if (checkHR(SHGetPropertyStoreForWindow(taskbarOwner->getWnd(), IID_PPV_ARGS(&propStore))))
        updateWindowPropStore(propStore);
    // update taskbar preview
    taskbarOwner->setPreview(hwnd);
    // update alt-tab
    taskbarOwner->setText(title);
    AcquireSRWLockExclusive(&iconLock);
    taskbarOwner->setIcon(iconSmall, iconLarge);
    ReleaseSRWLockExclusive(&iconLock);
}

IDispatch * ItemWindow::getShellViewDispatch() {
    return nullptr;
}

void ItemWindow::onViewReady() {
    if (shellWindowCookie) {
        // onItemChanged was called
        CComQIPtr<IPersistIDList> persistIDList(item);
        CComPtr<IShellWindows> shellWindows;
        if (persistIDList && checkHR(shellWindows.CoCreateInstance(CLSID_ShellWindows))) {
            CComVariant pidlVar(persistIDList);
            checkHR(shellWindows->OnNavigate(shellWindowCookie, &pidlVar));
        }
    } else {
        registerShellWindow();
    }
}

void ItemWindow::registerShellWindow() {
    // https://www.vbforums.com/showthread.php?894889-VB6-Using-IShellWindows-to-register-for-SHOpenFolderAndSelectItems
    // https://github.com/derceg/explorerplusplus/blob/55208360ccbad78f561f22bdb3572ed7b0780fa0/Explorer%2B%2B/Explorer%2B%2B/ShellBrowser/BrowsingHandler.cpp#L238
    if (shellWindowCookie)
        return;
    CComQIPtr<IPersistIDList> persistIDList(item);
    CComPtr<IShellWindows> shellWindows;
    if (persistIDList && checkHR(shellWindows.CoCreateInstance(CLSID_ShellWindows))) {
        CComVariant empty, pidlVar(persistIDList);
        checkHR(shellWindows->RegisterPending(GetCurrentThreadId(), &pidlVar, &empty,
            SWC_BROWSER, &shellWindowCookie));
        checkHR(shellWindows->Register(getShellViewDispatch(), HandleToLong(hwnd),
            SWC_BROWSER, &shellWindowCookie));
    }
}

void ItemWindow::unregisterShellWindow() {
    if (shellWindowCookie) {
        CComPtr<IShellWindows> shellWindows;
        if (checkHR(shellWindows.CoCreateInstance(CLSID_ShellWindows)))
            checkHR(shellWindows->Revoke(shellWindowCookie));
        shellWindowCookie = 0;
    }
}

void ItemWindow::registerShellNotify() {
    CComPtr<IShellItem> parentItem;
    CComHeapPtr<ITEMIDLIST> idList;
    if (SUCCEEDED(item->GetParent(&parentItem))
            && checkHR(SHGetIDListFromObject(parentItem, &idList))) {
        SHChangeNotifyEntry notifEntry = {idList, FALSE};
        shellNotifyID = SHChangeNotifyRegister(hwnd,
            SHCNRF_ShellLevel | SHCNRF_InterruptLevel | SHCNRF_NewDelivery,
            SHCNE_DELETE | SHCNE_RENAMEITEM | (isFolder() ? (SHCNE_RMDIR | SHCNE_RENAMEFOLDER) : 0),
            MSG_SHELL_NOTIFY, 1, &notifEntry);
    }
}

void ItemWindow::unregisterShellNotify() {
    if (shellNotifyID) {
        SHChangeNotifyDeregister(shellNotifyID);
        shellNotifyID = 0;
    }
}

bool ItemWindow::resolveItem() {
    if (closing) // can happen when closing save prompt (window is activated)
        return true;
    SFGAOF attr = 0;
    // A failed validation establishes an access failure, not a deletion.
    HRESULT validation = item->GetAttributes(SFGAO_VALIDATE, &attr);
    if (SUCCEEDED(validation)) {
        recordFolderAccess(validation);
        return true;
    }
    if (link) {
        checkHR(link->Resolve(nullptr, SLR_NO_UI));

        CComHeapPtr<ITEMIDLIST> currentIDList, newIDList;
        CComPtr<IShellFolder> desktopFolder;
        if (checkHR(SHGetIDListFromObject(item, &currentIDList))
                && checkHR(link->GetIDList(&newIDList))
                && checkHR(SHGetDesktopFolder(&desktopFolder))) {
            HRESULT compareHR;
            if (checkHR(compareHR = desktopFolder->CompareIDs(SHCIDS_CANONICALONLY,
                    currentIDList, newIDList))) {
                if ((short)HRESULT_CODE(compareHR) != 0) {
                    debugPrintf(L"Item has moved!\n");
                    CComPtr<IShellItem> newItem;
                    if (checkHR(SHCreateItemFromIDList(newIDList, IID_PPV_ARGS(&newItem)))) {
                        item = newItem; // itemMoved() is unnecessary since we can reuse link
                        onItemChanged();
                        return false;
                    }
                }
            }
        }
    }

    debugPrintf(L"Item could not be accessed!\n");
    recordFolderAccess(validation);
    close();
    return false;
}

void ItemWindow::itemMoved(IShellItem *const newItem) {
    persistViewState();
    item = newItem;
    link = nullptr;
    CComHeapPtr<ITEMIDLIST> idList;
    if (checkHR(SHGetIDListFromObject(item, &idList))) {
        if (checkHR(link.CoCreateInstance(__uuidof(ShellLink)))) {
            checkHR(link->SetIDList(idList));
        }
    }
    onItemChanged();
}

void ItemWindow::onItemChanged() {
    // Local NTFS identities survive rename/move; path-based identities follow the new path.
    std::wstring property = folderWindowProperty(getFolderIdentity(item));
    if (!property.empty() && property != identityProperty
            && SetPropW(hwnd, property.c_str(), reinterpret_cast<HANDLE>(1))) {
        std::string newKey;
        for (wchar_t character : property)
            newKey.push_back(static_cast<char>(character));
        auto store = folderStateStore();
        if (!store || !store->move(stateKey, newKey))
            stateStorageError();
        stateKey = newKey;
        RemovePropW(hwnd, identityProperty.c_str());
        identityProperty = std::move(property);
    }
    CComHeapPtr<wchar_t> newTitle;
    if (checkHR(item->GetDisplayName(SIGDN_NORMALDISPLAY, &newTitle))) {
        title = newTitle;
        SetWindowText(hwnd, title);
        proxyIcon.setTitle(title);
        autoSizeProxy(clientSize(hwnd).cx);
        taskbarOwner->setText(title);
    }
    pathBar.setItem(item);
    if (parentToolbar) {
        CComPtr<IShellItem> parentItem;
        SendMessage(parentToolbar, TB_ENABLEBUTTON, IDM_PREV_WINDOW,
            SUCCEEDED(item->GetParent(&parentItem)));
    }
    // NTFS retains its database key; path-based identities transfer their row.
    viewStateDirty(FolderState::All);
    persistViewState();

    unregisterShellNotify();
    registerShellNotify();
    if (!getShellViewDispatch())
        onViewReady();
}

void ItemWindow::refresh() {
    if (iconThread)
        iconThread->stop();
    iconThread.Attach(new IconThread(item, this));
    iconThread->start();
}

void ItemWindow::openParentMenu() {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    int iconSize = GetSystemMetrics(SM_CXSMICON);
    auto destroyMenu = [](HMENU handle) {
        for (int i = 0, count = GetMenuItemCount(handle); i < count; i++) {
            MENUITEMINFO info = {sizeof(info)};
            info.fMask = MIIM_BITMAP;
            if (checkLE(GetMenuItemInfo(handle, i, TRUE, &info)) && info.hbmpItem)
                DeleteBitmap(info.hbmpItem);
        }
        checkLE(DestroyMenu(handle));
    };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        ownedMenu(CreatePopupMenu(), destroyMenu);
    HMENU menu = ownedMenu.get();
    if (!menu)
        return;

    int id = 0;
    CComPtr<IShellItem> curItem = item, parentItem;

    while (isWindowOperational() && SUCCEEDED(curItem->GetParent(&parentItem))) {
        if (!isWindowOperational())
            return;
        curItem = parentItem;
        parentItem = nullptr;
        id++;

        CComHeapPtr<wchar_t> name;
        if (!isWindowOperational())
            return;
        if (!checkHR(curItem->GetDisplayName(SIGDN_NORMALDISPLAY, &name)))
            continue;

        if (!isWindowOperational())
            return;
        AppendMenu(menu, MF_STRING, id, name);

        SHFILEINFO fileInfo = {};
        CComHeapPtr<ITEMIDLIST> idList;

        if (checkHR(SHGetIDListFromObject(curItem, &idList)) && isWindowOperational()) {
            checkLE(SHGetFileInfo(
                (wchar_t *)(ITEMIDLIST *)idList,
                0,
                &fileInfo,
                sizeof(fileInfo),
                SHGFI_PIDL | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_SMALLICON));
        }

        if (fileInfo.hIcon) {
            MENUITEMINFO itemInfo = {sizeof(itemInfo)};
            itemInfo.fMask = MIIM_BITMAP;
            itemInfo.hbmpItem =
                iconToPARGB32Bitmap(fileInfo.hIcon, iconSize, iconSize);

            DestroyIcon(fileInfo.hIcon);
            SetMenuItemInfo(menu, id, FALSE, &itemInfo);
        }
    }

    if (!isWindowOperational() || id == 0)
        return;

    POINT point;
    LONG_PTR buttonState = 0;

    if (parentToolbar) {
        RECT buttonRect;
        SendMessage(
            parentToolbar,
            TB_GETRECT,
            IDM_PREV_WINDOW,
            (LPARAM)&buttonRect);

        if (!isWindowOperational())
            return;
        point = clientToScreen(
            parentToolbar,
            {buttonRect.left, buttonRect.bottom});

        buttonState =
            SendMessage(parentToolbar, TB_GETSTATE, IDM_PREV_WINDOW, 0);

        if (!isWindowOperational())
            return;
        SendMessage(
            parentToolbar,
            TB_SETSTATE,
            IDM_PREV_WINDOW,
            buttonState | TBSTATE_PRESSED);
    } else {
        point = clientToScreen(hwnd, {0, 0});
    }

    if (!isWindowOperational())
        return;
    int cmd = TrackPopupMenuEx(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x,
        point.y,
        hwnd,
        nullptr);

    const bool closeSource = settings::getKeepSourceWindowOpen()
        == (GetKeyState(VK_CONTROL) < 0);

    if (isWindowOperational() && parentToolbar) {
        SendMessage(
            parentToolbar,
            TB_SETSTATE,
            IDM_PREV_WINDOW,
            buttonState);
    }

    // Spatial mode: locate the selected ancestor directly instead of
    // following window links.
    if (isWindowOperational() && cmd > 0) {
        CComPtr<IShellItem> targetItem = item;
        CComPtr<IShellItem> nextItem;

        bool found = true;

        for (int i = 0; i < cmd; i++) {
            nextItem = nullptr;

            if (!isWindowOperational() || FAILED(targetItem->GetParent(&nextItem))) {
                found = false;
                break;
            }

            targetItem = nextItem;
        }

        if (isWindowOperational() && found && targetItem)
            openChild(targetItem, closeSource);
    }


}

// This menu uses existing windows and icons, not Shell folder enumeration.
// Keep the snapshot and its bitmaps local to a single popup.
static HBITMAP captionCommandBitmap(const wchar_t *glyph, int size, COLORREF color) {
    if (!navigationGraphicsToken)
        return nullptr;
    Gdiplus::Bitmap image(size, size, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics graphics(&image);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        Gdiplus::Font font(L"Segoe MDL2 Assets", (Gdiplus::REAL)(size - 2),
            Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(Gdiplus::Color(255,
            GetRValue(color), GetGValue(color), GetBValue(color)));
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        const Gdiplus::RectF rect(0, 0, (Gdiplus::REAL)size, (Gdiplus::REAL)size);
        if (graphics.DrawString(glyph, -1, &font, rect, &format, &brush) != Gdiplus::Ok)
            return nullptr;
    }
    BITMAPINFO info = {{sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB}};
    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap)
        return nullptr;
    Gdiplus::BitmapData data = {};
    const Gdiplus::Rect rect(0, 0, size, size);
    if (image.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB,
            &data) != Gdiplus::Ok) {
        DeleteObject(bitmap);
        return nullptr;
    }
    const size_t stride = (size_t)size * sizeof(DWORD);
    for (int row = 0; row < size; ++row)
        std::memcpy(static_cast<BYTE *>(pixels) + row * stride,
            static_cast<BYTE *>(data.Scan0) + row * data.Stride, stride);
    image.UnlockBits(&data);
    return bitmap;
}

static HBITMAP captionWindowBitmap(IWICImagingFactory *factory, HWND window, int size) {
    DWORD_PTR value = 0;
    // A different FileSpacer process can be busy; do not let it block this menu.
    SendMessageTimeout(window, WM_GETICON, ICON_SMALL2, systemDPI,
        SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 100, &value);
    HICON icon = value ? CopyIcon(reinterpret_cast<HICON>(value)) : nullptr;
    if (!icon) {
        SHSTOCKICONINFO stock = {sizeof(stock)};
        if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SMALLICON, &stock)))
            icon = stock.hIcon;
    }
    if (!icon)
        return nullptr;
    CComPtr<IWICBitmap> source;
    HRESULT result = factory->CreateBitmapFromHICON(icon, &source);
    DestroyIcon(icon);
    CComPtr<IWICBitmapScaler> scaled;
    if (SUCCEEDED(result))
        result = factory->CreateBitmapScaler(&scaled);
    if (SUCCEEDED(result))
        result = scaled->Initialize(source, (UINT)size, (UINT)size, WICBitmapInterpolationModeFant);
    CComPtr<IWICBitmapSource> converted;
    if (SUCCEEDED(result))
        result = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, scaled, &converted);
    if (FAILED(result))
        return nullptr;
    BITMAPINFO info = {{sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB}};
    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap)
        return nullptr;
    const UINT stride = (UINT)((size_t)size * sizeof(DWORD));
    if (FAILED(converted->CopyPixels(nullptr, stride, stride * (UINT)size,
            static_cast<BYTE *>(pixels)))) {
        DeleteObject(bitmap);
        return nullptr;
    }
    return bitmap;
}

void ItemWindow::openCaptionMenu(POINT pos) {
    CComPtr<ItemWindow> keepAlive(this);
    if (!isWindowOperational())
        return;
    struct Entry {
        HWND window;
        std::wstring title;
    };
    std::vector<Entry> entries;
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        if (!IsWindowVisible(window))
            return TRUE;
        wchar_t windowClass[64] = {};
        if (!GetClassName(window, windowClass, (int)ARRAYSIZE(windowClass))
                || lstrcmp(windowClass, ITEM_WINDOW_CLASS) != 0)
            return TRUE;
        const int length = GetWindowTextLength(window);
        if (!length)
            return TRUE;
        std::wstring name((size_t)length + 1, L'\0');
        const int copied = GetWindowText(window, &name[0], length + 1);
        if (!copied)
            return TRUE;
        name.resize((size_t)copied);
        reinterpret_cast<std::vector<Entry> *>(data)->push_back({window, std::move(name)});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&entries));

    if (!isWindowOperational())
        return;
    std::vector<HBITMAP> bitmaps;
    auto destroyMenu = [&bitmaps](HMENU handle) {
        DestroyMenu(handle);
        for (HBITMAP bitmap : bitmaps)
            DeleteObject(bitmap);
    };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        ownedMenu(CreatePopupMenu(), destroyMenu);
    HMENU menu = ownedMenu.get();
    if (!menu)
        return;
    MENUINFO menuInfo = {sizeof(menuInfo)};
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_CHECKORBMP;
    SetMenuInfo(menu, &menuInfo);
    const int iconSize = GetSystemMetrics(SM_CXSMICON);
    COLORREF iconColor = GetSysColor(COLOR_MENUTEXT);
    if (HTHEME theme = OpenThemeData(hwnd, L"Menu")) {
        GetThemeColor(theme, MENU_POPUPITEM, MPI_NORMAL, TMT_TEXTCOLOR, &iconColor);
        CloseThemeData(theme);
    }
    auto append = [&](UINT command, const wchar_t *name, HBITMAP bitmap, bool enabled) {
        MENUITEMINFO info = {sizeof(info)};
        info.fMask = MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_STATE;
        info.wID = command;
        info.dwTypeData = const_cast<wchar_t *>(name);
        info.hbmpItem = bitmap;
        info.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
        InsertMenuItem(menu, (UINT)-1, TRUE, &info);
        if (bitmap)
            bitmaps.push_back(bitmap);
    };
    CComHeapPtr<wchar_t> path;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && isWindowOperational())
        item->GetDisplayName(SIGDN_DESKTOPABSOLUTEEDITING, &path);
    if (!isWindowOperational())
        return;
    SFGAOF attributes = 0;
    item->GetAttributes(SFGAO_CANRENAME, &attributes);
    if (!isWindowOperational())
        return;
    append(1, getString(IDS_CAPTION_COPY_PATH),
        captionCommandBitmap(MDL2_COPY_PATH, iconSize, iconColor), path != nullptr);
    append(2, getString(IDS_CAPTION_RENAME),
        captionCommandBitmap(MDL2_RENAME, iconSize, iconColor), (attributes & SFGAO_CANRENAME) != 0);
    append(3, getString(IDS_CAPTION_PROPERTIES),
        captionCommandBitmap(MDL2_PROPERTIES, iconSize, iconColor), true);
    if (!entries.empty())
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    CComPtr<IWICImagingFactory> factory;
    factory.CoCreateInstance(CLSID_WICImagingFactory);
    if (!isWindowOperational())
        return;
    for (size_t index = 0; index < entries.size() && isWindowOperational(); ++index) {
        std::wstring label;
        for (wchar_t character : entries[index].title) {
            label += character;
            if (character == L'&')
                label += L'&';
        }
        HBITMAP bitmap = factory ? captionWindowBitmap(factory, entries[index].window, iconSize)
            : nullptr;
        if (!bitmap)
            bitmap = captionCommandBitmap(MDL2_FOLDER, iconSize, iconColor);
        append((UINT)index + 4, label.c_str(), bitmap, true);
    }
    if (!isWindowOperational())
        return;
    const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pos.x, pos.y, hwnd, nullptr);
    ownedMenu.reset();
    if (!isWindowOperational())
        return;

    if (command == 1 && path) {
        const size_t bytes = ((size_t)lstrlen(path) + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
            return;
        void *text = GlobalLock(memory);
        if (!text) {
            GlobalFree(memory);
            return;
        }
        std::memcpy(text, path, bytes);
        GlobalUnlock(memory);
        if (OpenClipboard(hwnd)) {
            if (isWindowOperational() && EmptyClipboard() && isWindowOperational()
                    && SetClipboardData(CF_UNICODETEXT, memory))
                memory = nullptr; // The clipboard now owns the allocation.
            CloseClipboard();
        }
        if (memory)
            GlobalFree(memory);
    } else if (command == 2) {
        proxyIcon.beginRename();
    } else if (command == 3) {
        openProxyProperties();
    } else if (command >= 4 && (size_t)(command - 4) < entries.size()) {
        const HWND target = entries[command - 4].window;
        wchar_t windowClass[64] = {};
        // The selected window may have closed while the popup was open.
        if (GetClassName(target, windowClass, (int)ARRAYSIZE(windowClass))
                && lstrcmp(windowClass, ITEM_WINDOW_CLASS) == 0) {
            // The taskbar owner may be minimized instead of the folder window.
            const HWND owner = GetAncestor(target, GA_ROOTOWNER);
            if (owner && IsIconic(owner))
                ShowWindowAsync(owner, SW_RESTORE);
            if (IsIconic(target))
                ShowWindowAsync(target, SW_RESTORE);
            SetForegroundWindow(target);
        }
        // Selecting an existing window never navigates or closes the source.
    }
}

void ItemWindow::openProxyProperties() {
    // openCaptionMenu owns this object through resolution and invocation.
    if (!isWindowOperational())
        return;
    CComHeapPtr<ITEMIDLIST> idList;
    if (checkHR(SHGetIDListFromObject(item, &idList)) && isWindowOperational()) {
        SHELLEXECUTEINFO info = {sizeof(info)};
        info.fMask = SEE_MASK_INVOKEIDLIST;
        info.lpVerb = L"properties";
        info.lpIDList = idList;
        info.hwnd = hwnd;
        checkLE(ShellExecuteEx(&info));
    }
}

void ItemWindow::deleteProxy() {
    CComHeapPtr<ITEMIDLIST> idList;
    if (checkHR(SHGetIDListFromObject(item, &idList))) {
        SHELLEXECUTEINFO info = {sizeof(info)};
        info.fMask = SEE_MASK_INVOKEIDLIST;
        info.lpVerb = L"delete";
        info.lpIDList = idList;
        info.hwnd = hwnd;
        checkLE(ShellExecuteEx(&info));
    }
}

void ItemWindow::openProxyContextMenu() {
    // IDM_PROXY_MENU owns this object through the final button restoration.
    if (!isWindowOperational())
        return;
    CComPtr<IContextMenu> contextMenu;
    if (!checkHR(item->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&contextMenu)))
            || !isWindowOperational())
        return;
    auto destroyMenu = [](HMENU handle) { checkLE(DestroyMenu(handle)); };
    std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(destroyMenu)>
        popupMenu(CreatePopupMenu(), destroyMenu);
    if (!popupMenu)
        return;
    UINT contextFlags = CMF_ITEMMENU | (useCustomFrame() ? CMF_CANRENAME : 0);
    if (GetKeyState(VK_SHIFT) < 0)
        contextFlags |= CMF_EXTENDEDVERBS;
    if (!checkHR(contextMenu->QueryContextMenu(popupMenu.get(), 0, IDM_SHELL_FIRST,
            IDM_SHELL_LAST, contextFlags)) || !isWindowOperational())
        return;
    contextMenu2 = contextMenu;
    if (isWindowOperational())
        contextMenu3 = contextMenu;
    int cmd = 0;
    POINT point = {};
    if (isWindowOperational()) {
        point = proxyIcon.getMenuPoint(hwnd);
        if (isWindowOperational())
            cmd = TrackPopupMenuEx(popupMenu.get(), TPM_RETURNCMD | TPM_RIGHTBUTTON,
                point.x, point.y, hwnd, nullptr);
    }
    contextMenu2 = nullptr;
    contextMenu3 = nullptr;
    if (isWindowOperational() && cmd >= IDM_SHELL_FIRST && cmd <= IDM_SHELL_LAST) {
        cmd -= IDM_SHELL_FIRST;
        wchar_t verb[64] = {};
        bool hasVerb = checkHR(contextMenu->GetCommandString(cmd, GCS_VERBW, nullptr,
            (char*)verb, _countof(verb)));
        if (!isWindowOperational())
            return;
        if (hasVerb && lstrcmpi(verb, L"rename") == 0) {
            proxyIcon.beginRename();
        } else {
            auto info = makeInvokeInfo(cmd, point);
            contextMenu->InvokeCommand((CMINVOKECOMMANDINFO *)&info);
        }
    }
}

CMINVOKECOMMANDINFOEX ItemWindow::makeInvokeInfo(int cmd, POINT point) {
    CMINVOKECOMMANDINFOEX info = {sizeof(info)};
    // Main installs the Shell thread reference before creating any windows.
    // https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-invokecommand
    info.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE
        | CMIC_MASK_ASYNCOK | CMIC_MASK_FLAG_LOG_USAGE;
    if (GetKeyState(VK_CONTROL) < 0)
        info.fMask |= CMIC_MASK_CONTROL_DOWN;
    if (GetKeyState(VK_SHIFT) < 0)
        info.fMask |= CMIC_MASK_SHIFT_DOWN;
    info.hwnd = hwnd;
    info.lpVerb = MAKEINTRESOURCEA(cmd);
    info.lpVerbW = MAKEINTRESOURCEW(cmd);
    info.nShow = SW_SHOWNORMAL;
    info.ptInvoke = point;
    return info;
}

void ItemWindow::proxyRename(const wchar_t *name) {
    CComPtr<IFileOperation> operation;
    if (!checkHR(operation.CoCreateInstance(__uuidof(FileOperation))))
        return;
    checkHR(operation->SetOperationFlags(
        IsWindows8OrGreater() ? FOFX_ADDUNDORECORD : FOF_ALLOWUNDO));
    NewItemSink eventSink;
    if (!checkHR(operation->RenameItem(item, name, &eventSink)))
        return;
    unregisterShellNotify();
    checkHR(operation->PerformOperations());
    if (eventSink.newItem) {
        itemMoved(eventSink.newItem); // will call registerShellNotify()
    } else {
        registerShellNotify();
    }
}

ItemWindow::IconThread::IconThread(IShellItem *const item, ItemWindow *const callbackWindow)
        : callbackWindow(callbackWindow) {
    checkHR(SHGetIDListFromObject(item, &itemIDList));
}

void ItemWindow::IconThread::run() {
    SHFILEINFO fileInfo = {};
    checkLE(SHGetFileInfo((wchar_t *)(ITEMIDLIST *)itemIDList, 0, &fileInfo, sizeof(fileInfo),
        SHGFI_PIDL | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_SMALLICON));
    if (fileInfo.hIcon == nullptr) {
        // bug (possibly windows 7 only?) where the first call sometimes fails
        checkLE(SHGetFileInfo((wchar_t *)(ITEMIDLIST *)itemIDList, 0, &fileInfo, sizeof(fileInfo),
            SHGFI_PIDL | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_SMALLICON));
    }
    HICON hIconSmall = fileInfo.hIcon;
    fileInfo.hIcon = nullptr;
    checkLE(SHGetFileInfo((wchar_t *)(ITEMIDLIST *)itemIDList, 0, &fileInfo, sizeof(fileInfo),
        SHGFI_PIDL | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_LARGEICON));

    std::wstring relaunchIcon = folderRelaunchIcon(itemIDList, fileInfo.hIcon);

    AcquireSRWLockExclusive(&stopLock);
    if (!isStopped()) {
        AcquireSRWLockExclusive(&callbackWindow->iconLock);
        callbackWindow->taskbarIconResource = std::move(relaunchIcon);
        if (callbackWindow->iconLarge) {
            checkLE(DestroyIcon(callbackWindow->iconLarge));
            FILESPACER_MEMLEAK_FREE;
        }
        callbackWindow->iconLarge = fileInfo.hIcon;
        if (fileInfo.hIcon) {
            FILESPACER_MEMLEAK_ALLOC;
        }
    
        if (callbackWindow->iconSmall) {
            checkLE(DestroyIcon(callbackWindow->iconSmall));
            FILESPACER_MEMLEAK_FREE;
        }
        callbackWindow->iconSmall = hIconSmall;
        if (hIconSmall) {
            FILESPACER_MEMLEAK_ALLOC;
        }
        ReleaseSRWLockExclusive(&callbackWindow->iconLock);

        PostMessage(callbackWindow->hwnd, MSG_UPDATE_ICONS, 0, 0);
    } else {
        if (fileInfo.hIcon)
            checkLE(DestroyIcon(fileInfo.hIcon));
        if (hIconSmall)
            checkLE(DestroyIcon(hIconSmall));
    }
    ReleaseSRWLockExclusive(&stopLock);
}

} // namespace
