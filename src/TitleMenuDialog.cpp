// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "TitleMenu.h"
#include "Settings.h"
#include "UIStrings.h"
#include "dialog.h"
#include <windowsx.h>
#include <commctrl.h>
#include <ole2.h>
#include <shlobj.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <atlbase.h>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <strsafe.h>
#undef min
#undef max

namespace filespacer {
namespace {
constexpr UINT RENDER_FRAME = WM_APP + 84;
constexpr int PREVIEW_X = 48, PREVIEW_Y = 41, IMAGE_X = PREVIEW_X - 194;
constexpr int SEPARATOR = TITLE_SEPARATOR, WINDOWS_BLOCK = TITLE_OPEN_WINDOWS;
constexpr int LABEL_COUNT = TITLE_ENTRY_COUNT;
struct Payload { DWORD process; int command; int sourceIndex; };
struct App;
void startDrag(App &app, int command, int sourceIndex);
void refreshPreview(App &app);
void refreshCatalog(App &app);
void layoutEditor(App &app);
void trackBlockedTip(App &app, bool visible);
void setInsertion(App &app, int index);
int insertionAt(App &app, POINTL point);
bool readPayload(IDataObject *data, CLIPFORMAT format, Payload &payload) {
    FORMATETC requested = {format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium = {};
    if (!data || FAILED(data->GetData(&requested, &medium))) return false;
    bool valid = false;
    if (medium.tymed == TYMED_HGLOBAL && GlobalSize(medium.hGlobal) >= sizeof(payload)) {
        const void *bytes = GlobalLock(medium.hGlobal);
        if (bytes) {
            std::memcpy(&payload, bytes, sizeof(payload));
            GlobalUnlock(medium.hGlobal);
            valid = payload.process == GetCurrentProcessId()
                && validTitleMenuEntry(payload.command);
        }
    }
    ReleaseStgMedium(&medium);
    return valid;
}

class Transfer : public IDataObject, public IDropSource {
    ULONG refs = 1;
    CLIPFORMAT format;
    Payload payload;
    ~Transfer() = default;
public:
    bool released = false;
    Transfer(CLIPFORMAT value, Payload item) : format(value), payload(item) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (id == IID_IUnknown || id == IID_IDataObject) *object = static_cast<IDataObject *>(this);
        else if (id == IID_IDropSource) *object = static_cast<IDropSource *>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG left = --refs; if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC *requested, STGMEDIUM *medium) override {
        if (!requested || !medium) return E_POINTER;
        const HRESULT valid = QueryGetData(requested);
        if (FAILED(valid)) return valid;
        *medium = {};
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(payload));
        if (!memory) return E_OUTOFMEMORY;
        void *bytes = GlobalLock(memory);
        if (!bytes) { GlobalFree(memory); return E_OUTOFMEMORY; }
        std::memcpy(bytes, &payload, sizeof(payload));
        GlobalUnlock(memory);
        medium->tymed = TYMED_HGLOBAL; medium->hGlobal = memory;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *value) override {
        if (!value) return E_POINTER;
        if (value->cfFormat != format) return DV_E_FORMATETC;
        if (!(value->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;
        if (value->dwAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
        return value->lindex == -1 ? S_OK : DV_E_LINDEX;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *, STGMEDIUM *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *output) override {
        if (!output) return E_POINTER; output->ptd = nullptr; return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC **output) override {
        if (!output) return E_POINTER; *output = nullptr;
        FORMATETC value = {format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        return direction == DATADIR_GET ? SHCreateStdEnumFmtEtc(1, &value, output) : E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *, DWORD, IAdviseSink *, DWORD *) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **output) override {
        if (output) *output = nullptr; return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape) return DRAGDROP_S_CANCEL;
        released = !(keys & MK_LBUTTON);
        return released ? DRAGDROP_S_DROP : S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

struct RenderedRow { RECT rect; int source; bool managed; bool locked = false; unsigned type; std::wstring text; };
struct App {
    HWND window = nullptr, catalog = nullptr, preview = nullptr, nilesoftNotice = nullptr, settingsLink = nullptr;
    HWND blockedTip = nullptr;
    HFONT font = nullptr; // Borrowed from the dialog.
    HBITMAP frame = nullptr;
    std::unique_ptr<std::remove_pointer<HBITMAP>::type, decltype(&DeleteObject)> backdrop{nullptr, DeleteObject};
    std::unique_ptr<std::remove_pointer<HDESK>::type, decltype(&CloseDesktop)> desktop{nullptr, CloseDesktop};
    TOOLINFOW blockedTool = {};
    const wchar_t *labels[LABEL_COUNT] = {};
    std::vector<int> items, beforeMove, nsForcedCommands;
    std::vector<RenderedRow> rows;
    std::wstring directory, desktopName, nilesoftPath;
    RECT arrowRect = {}, blockRect = {};
    SIZE frameSize = {}, menuSize = {};
    POINT frameInset = {}, pressedPoint = {}, catalogPoint = {};
    int previewWidth = 500, previewHeight = 500, scrollY = 0, blockExtra = 0, insertion = -1;
    int fullStackHeight = 0, stackSeparatorHeight = 0, renderError = 0, pendingMove = -1;
    int pressedRow = -1, catalogPressed = -1;
    CLIPFORMAT format = 0;
    unsigned drops = 0;
    bool blockedTipActive = false, dragging = false, closePending = false;
    bool frameAlpha = false, frameDecorated = false, frameSimplified = false, nilesoftModified = false;
    bool menuActive = false, measuringStack = false, moveRejected = false, rendering = false;
    ~App() {
        if (frame) DeleteObject(frame);
        if (!directory.empty()) {
            wchar_t path[MAX_PATH + 128] = {};
            for (const auto *file : {L"request.tsv", L"frame.tsv", L"frame.bmp"})
                if (SUCCEEDED(StringCchPrintfW(path, _countof(path), L"%s\\render\\%s", directory.c_str(), file)))
                    DeleteFileW(path);
            if (SUCCEEDED(StringCchPrintfW(path, _countof(path), L"%s\\render", directory.c_str())))
                RemoveDirectoryW(path);
            RemoveDirectoryW(directory.c_str());
        }
    }
};
std::wstring catalogLabel(const wchar_t *label) {
    const std::wstring text(label);
    return text.substr(0, text.find(L'\t'));
}
void layoutEditor(App &app) {
    {
        int textWidth = 0;
        for (const auto *label : app.labels) {
            const auto text = catalogLabel(label);
            textWidth = std::max(textWidth, ListView_GetStringWidth(app.catalog, text.c_str()));
        }
        const int catalogWidth = textWidth + 24;
        int contentRight = std::max(IMAGE_X + 350, static_cast<int>(PREVIEW_X - app.frameInset.x + app.frameSize.cx));
        const int contentHeight = std::max(160, static_cast<int>(PREVIEW_Y - app.frameInset.y + app.frameSize.cy + 12));
        RECT item = {}; ListView_GetItemRect(app.catalog, 0, &item, LVIR_BOUNDS);
        const int catalogHeight = ListView_GetItemCount(app.catalog) * std::max(18L, item.bottom - item.top) + 6;
        const int extraSeparators = std::max(0, static_cast<int>(std::count(app.items.begin(), app.items.end(), SEPARATOR)) - 2);
        const int reservedHeight = app.fullStackHeight + extraSeparators * app.stackSeparatorHeight;
        app.previewWidth = contentRight + 12;
        const int textAreaWidth = app.previewWidth + 80 + catalogWidth;
        RECT notice = {0, 0, textAreaWidth, 0};
        HDC textDC = GetDC(app.nilesoftNotice); HGDIOBJ oldFont = SelectObject(textDC, app.font);
        DrawTextW(textDC, getString(IDS_TITLE_MENU_NS_NOTICE), -1, &notice, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(textDC, oldFont); ReleaseDC(app.nilesoftNotice, textDC);
        SIZE link = {};
        SendMessageW(app.settingsLink, LM_GETIDEALSIZE, textAreaWidth, reinterpret_cast<LPARAM>(&link));
        const int linkHeight = std::max(22L, link.cy);
        const int noticeHeight = app.nilesoftModified ? 12 + notice.bottom + 8 + linkHeight : 0;
        int maximumHeight = 500;
        MONITORINFO monitor = {sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromWindow(app.window, MONITOR_DEFAULTTONEAREST), &monitor)) {
            maximumHeight = std::max(200, std::min(500, static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top - noticeHeight - 150)));
        }
        app.previewHeight = std::min(maximumHeight, std::max({contentHeight, catalogHeight, reservedHeight}));
        const bool scrollable = contentHeight > app.previewHeight;
        app.previewWidth = contentRight + 12 + (scrollable ? GetSystemMetrics(SM_CXVSCROLL) : 0);
        const int catalogX = 24 + app.previewWidth + 80;
        const int groupY = 20 + app.previewHeight + noticeHeight + 16;
        const int arrowX = (24 + contentRight + catalogX - 64) / 2;
        app.arrowRect = {arrowX, 20 + app.previewHeight / 2 - 22, arrowX + 64, 20 + app.previewHeight / 2 + 22};
        const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
        ShowScrollBar(app.preview, SB_VERT, scrollable);
        SetWindowPos(app.preview, nullptr, 24, 20, app.previewWidth, app.previewHeight, flags);
        SetWindowPos(app.catalog, nullptr, catalogX, 20, catalogWidth, app.previewHeight, flags);
        ListView_SetColumnWidth(app.catalog, 0, catalogWidth - 4);
        SetWindowPos(app.nilesoftNotice, nullptr, 24, 32 + app.previewHeight, catalogX + catalogWidth - 24, notice.bottom, flags);
        SetWindowPos(app.settingsLink, nullptr, 24, 40 + app.previewHeight + notice.bottom, catalogX + catalogWidth - 24, linkHeight, flags);
        SetWindowPos(GetDlgItem(app.window, IDC_TITLE_MENU_DEFAULT), nullptr, 24, groupY, 190, 26, flags);
        SetWindowPos(GetDlgItem(app.window, IDOK), nullptr, catalogX + catalogWidth - 174, groupY, 80, 26, flags);
        SetWindowPos(GetDlgItem(app.window, IDCANCEL), nullptr, catalogX + catalogWidth - 84, groupY, 84, 26, flags);
        ShowWindow(app.nilesoftNotice, app.nilesoftModified ? SW_SHOW : SW_HIDE);
        ShowWindow(app.settingsLink, app.nilesoftModified ? SW_SHOW : SW_HIDE);
        RECT desired = {0, 0, catalogX + catalogWidth + 24, groupY + 42};
        AdjustWindowRectEx(&desired, static_cast<DWORD>(GetWindowLongPtrW(app.window, GWL_STYLE)), FALSE, 0);
        SetWindowPos(app.window, nullptr, 0, 0, desired.right - desired.left, desired.bottom - desired.top,
            flags | SWP_NOMOVE);
        RedrawWindow(app.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    ShowWindow(app.nilesoftNotice, app.nilesoftModified ? SW_SHOW : SW_HIDE);
    ShowWindow(app.settingsLink, app.nilesoftModified ? SW_SHOW : SW_HIDE);
    RECT viewport = {}; GetClientRect(app.preview, &viewport);
    SCROLLINFO scroll = {sizeof(scroll), SIF_RANGE | SIF_PAGE | SIF_POS};
    scroll.nMax = std::max(160, static_cast<int>(PREVIEW_Y - app.frameInset.y + app.frameSize.cy + 12)) - 1;
    scroll.nPage = static_cast<UINT>(viewport.bottom); scroll.nPos = app.scrollY;
    app.scrollY = SetScrollInfo(app.preview, SB_VERT, &scroll, TRUE);
}

int catalogCommand(HWND catalog, int row) {
    if (row < 0) return -1;
    LVITEMW item = {}; item.mask = LVIF_PARAM; item.iItem = row;
    return ListView_GetItem(catalog, &item) ? static_cast<int>(item.lParam) : -1;
}

void refreshCatalog(App &app) {
    const int selected = catalogCommand(app.catalog, ListView_GetNextItem(app.catalog, -1, LVNI_SELECTED));
    SendMessageW(app.catalog, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(app.catalog);
    for (int command = 0; command < LABEL_COUNT; ++command) {
        if (!validTitleMenuEntry(command)) continue;
        if (command != SEPARATOR && std::find(app.items.begin(), app.items.end(), command) != app.items.end()) continue;
        LVITEMW item = {}; item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(app.catalog); item.lParam = command;
        const auto text = catalogLabel(app.labels[command]);
        item.pszText = const_cast<wchar_t *>(text.c_str());
        const int row = ListView_InsertItem(app.catalog, &item);
        if (command == selected && row >= 0)
            ListView_SetItemState(app.catalog, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    SendMessageW(app.catalog, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(app.catalog, nullptr, TRUE);
}

class Target : public IDropTarget {
    ULONG refs = 1;
    App &app;
    int index;
    bool catalog;
    bool surface;
    bool removing = false;
    Payload incoming = {};
    bool valid = false;
    DWORD allowed = DROPEFFECT_NONE;
    ~Target() = default;
    DWORD desiredEffect() const {
        if (!valid || app.rendering || (removing && incoming.sourceIndex < 0)) return DROPEFFECT_NONE;
        if (incoming.sourceIndex < 0 && incoming.command != SEPARATOR
                && std::find(app.items.begin(), app.items.end(), incoming.command) != app.items.end()) return DROPEFFECT_NONE;
        return (removing || incoming.sourceIndex >= 0 ? DROPEFFECT_MOVE : DROPEFFECT_COPY) & allowed;
    }
    void position(POINTL point) {
        POINT local = {point.x, point.y}; ScreenToClient(app.preview, &local);
        RECT menu = {PREVIEW_X, PREVIEW_Y - app.scrollY,
            PREVIEW_X + app.menuSize.cx, PREVIEW_Y - app.scrollY + app.menuSize.cy};
        removing = catalog || (surface && incoming.sourceIndex >= 0 && !PtInRect(&menu, local));
        if (surface) index = insertionAt(app, point);
        setInsertion(app, removing ? -1 : index);
    }
public:
    Target(App &owner, int position, bool remove, bool persistentSurface = false)
        : app(owner), index(position), catalog(remove), surface(persistentSurface) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **object) override {
        if (!object) return E_POINTER; *object = nullptr;
        if (id != IID_IUnknown && id != IID_IDropTarget) return E_NOINTERFACE;
        *object = static_cast<IDropTarget *>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG left = --refs; if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *data, DWORD, POINTL point, DWORD *effect) override {
        if (!effect) return E_POINTER;
        allowed = *effect;
        valid = readPayload(data, app.format, incoming);
        position(point);
        *effect = desiredEffect();

        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL point, DWORD *effect) override {
        if (!effect) return E_POINTER;
        position(point);
        *effect = desiredEffect(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        setInsertion(app, -1); valid = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject *data, DWORD, POINTL point, DWORD *effect) override {
        if (!effect) return E_POINTER;
        allowed = *effect;
        valid = readPayload(data, app.format, incoming);
        position(point);
        *effect = desiredEffect();
        if (!*effect || !IsWindow(app.window)) return S_OK;
        int position = std::max(0, std::min(index, static_cast<int>(app.items.size())));
        app.moveRejected = false;
        if (incoming.sourceIndex >= 0 && !removing) {
            app.beforeMove = app.items; app.pendingMove = incoming.command;
        }
        if (incoming.sourceIndex >= 0) {
            if (incoming.sourceIndex >= static_cast<int>(app.items.size())
                    || app.items[static_cast<size_t>(incoming.sourceIndex)] != incoming.command) {
                *effect = DROPEFFECT_NONE; return S_OK;
            }
            app.items.erase(app.items.begin() + incoming.sourceIndex);
            if (incoming.sourceIndex < position) --position;
        }
        if (!removing) app.items.insert(app.items.begin() + position, incoming.command);
        refreshCatalog(app);
        ++app.drops; setInsertion(app, -1);

        if (!app.nilesoftPath.empty() && app.pendingMove >= 0) {
            // The rendering dispatches messages under the DoDragDrop loop. The
            // editor outlives the dialog; this target may be revoked meanwhile.
            AddRef();
            App &editor = app;
            refreshPreview(editor);
            const bool rejected = editor.moveRejected;
            Release();
            if (rejected) *effect = DROPEFFECT_NONE;
        } else PostMessageW(app.window, RENDER_FRAME, 0, 0);
        return S_OK;
    }
};

void startDrag(App &app, int command, int sourceIndex) {
    if (app.dragging || app.rendering || !validTitleMenuEntry(command)) return;
    trackBlockedTip(app, false);
    app.dragging = true;
    const unsigned drops = app.drops;
    auto transfer = new Transfer(app.format, {GetCurrentProcessId(), command, sourceIndex});
    CComPtr<IDataObject> data; data.Attach(transfer);
    CComQIPtr<IDropSource> source(data);
    DWORD effect = DROPEFFECT_NONE;
    DoDragDrop(data, source, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
    app.dragging = false;
    setInsertion(app, -1);
    // A release away from any accepted target removes an editable menu row.
    // Escape and a rejected NS reorder never remove it.
    if (transfer->released && drops == app.drops && sourceIndex >= 0 && !app.closePending) {
        POINT point = {}; GetCursorPos(&point);
        RECT menu = {PREVIEW_X, PREVIEW_Y - app.scrollY,
            PREVIEW_X + app.menuSize.cx, PREVIEW_Y - app.scrollY + app.menuSize.cy};
        ScreenToClient(app.preview, &point);
        if (!PtInRect(&menu, point) && sourceIndex < static_cast<int>(app.items.size())
                && app.items[static_cast<size_t>(sourceIndex)] == command) {
            app.items.erase(app.items.begin() + sourceIndex);
            refreshCatalog(app); PostMessageW(app.window, RENDER_FRAME, 0, 0);
        }
    }
    if (app.closePending) PostMessageW(app.window, WM_CLOSE, 0, 0);
}
LSTATUS readRegistryString(HKEY key, const wchar_t *subkey, const wchar_t *value, std::wstring &text) {
    DWORD bytes = 0;
    LSTATUS status = RegGetValueW(key, subkey, value, RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
    auto read = [&]() {
        text.assign(std::max(bytes, static_cast<DWORD>(sizeof(wchar_t))) / sizeof(wchar_t), L'\0');
        bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
        return RegGetValueW(key, subkey, value, RRF_RT_REG_SZ, nullptr, &text[0], &bytes);
    };
    if (status == ERROR_SUCCESS) status = read();
    // An expanded REG_EXPAND_SZ value may exceed the size reported first.
    if (status == ERROR_MORE_DATA) status = read();
    if (status != ERROR_SUCCESS) { text.clear(); return status; }
    text.resize(wcslen(text.c_str()));
    return ERROR_SUCCESS;
}

std::wstring installedNilesoft() {
#ifdef FILESPACER_DEBUG
    if (!titleMenuExtensionEnabled()) return L"";
#endif
    std::wstring path;
    if (FAILED(findTitleMenuExtension(nullptr, &path))) return L"";
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES ? path : L"";
}

std::wstring nilesoftSettingsDirectory(const std::wstring &module) {
    // NS selects an existing HKCU config file before falling back to shell.nss
    // beside its module. Follow that selection when opening its settings.
    std::wstring file = module, config;
    if (readRegistryString(HKEY_CURRENT_USER, L"Software\\Nilesoft\\Shell", L"config", config) == ERROR_SUCCESS
            && !config.empty()) {
        const DWORD attributes = GetFileAttributesW(config.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring full(GetFullPathNameW(config.c_str(), 0, nullptr, nullptr), L'\0');
            const DWORD length = full.empty() ? 0
                : GetFullPathNameW(config.c_str(), static_cast<DWORD>(full.size()), &full[0], nullptr);
            if (length && length < full.size()) file.assign(full, 0, length);
        }
    }
    const size_t separator = file.find_last_of(L"\\/");
    return file.substr(0, separator == 2 && file[1] == L':' ? 3 : separator);
}

HBITMAP loadFrame(const std::wstring &path, LONG width, LONG height, void **pixels) {
    if (width <= 0 || height <= 0) return nullptr;
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb")) return nullptr;
    BITMAPFILEHEADER header = {}; BITMAPINFO info = {};
    const bool valid = fread(&header, sizeof(header), 1, file) == 1
        && fread(&info.bmiHeader, sizeof(info.bmiHeader), 1, file) == 1
        && header.bfType == 0x4d42 && header.bfOffBits == sizeof(header) + sizeof(info.bmiHeader)
        && info.bmiHeader.biSize == sizeof(info.bmiHeader) && info.bmiHeader.biWidth == width
        && info.bmiHeader.biHeight == -height && info.bmiHeader.biPlanes == 1
        && info.bmiHeader.biBitCount == 32 && info.bmiHeader.biCompression == BI_RGB;
    // The renderer writes this exact top-down BGRA format. Keep its alpha and
    // orientation explicit; GetObject does not preserve the height sign.
    HBITMAP frame = valid ? CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, pixels, nullptr, 0) : nullptr;
    const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (frame && fread(*pixels, 1, bytes, file) != bytes) { DeleteObject(frame); frame = nullptr; }
    fclose(file); return frame;
}

// Waits for the private renderer while the dialog keeps dispatching its
// messages. Commands that would start another rendering or validate the draft
// are ignored meanwhile (app.rendering); Cancel and closing abort the wait.
// The deadline is a watchdog only: it can fail the preview, never pass it.
bool waitForRenderer(App &app, HANDLE process) {
    const ULONGLONG deadline = GetTickCount64() + 8000;
    bool finished = false;
    app.rendering = true;
    for (;;) {
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) break;
        const DWORD wait = MsgWaitForMultipleObjectsEx(1, &process, static_cast<DWORD>(deadline - now),
            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0) { finished = true; break; }
        if (wait != WAIT_OBJECT_0 + 1) break;
        MSG message = {};
        bool alive = IsWindow(app.window) != FALSE;
        while (alive && !app.closePending && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { PostQuitMessage(static_cast<int>(message.wParam)); alive = false; break; }
            if (!IsDialogMessageW(app.window, &message)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            alive = IsWindow(app.window) != FALSE;
        }
        // A destroyed dialog leaves its state alone; a pending close ends the wait.
        if (!alive || app.closePending) break;
    }
    app.rendering = false;
    if (!finished) {
        // A failed private experiment must not leave a hidden menu process running.
        TerminateProcess(process, 99);
        WaitForSingleObject(process, 2000);
    }
    return finished;
}

void refreshPreview(App &app) {
    trackBlockedTip(app, false);
    if (!app.measuringStack && !app.fullStackHeight) {
        std::vector<int> model = std::move(app.items), forced = std::move(app.nsForcedCommands);
        app.items = {0, 1, 2, 3, SEPARATOR, 4, 5, 6, SEPARATOR, 7, 8, 10, 11, 12, 13, WINDOWS_BLOCK};
        app.measuringStack = true;
        refreshPreview(app);
        app.measuringStack = false; app.items = std::move(model); app.nsForcedCommands = std::move(forced);
        if (app.renderError) return;
    }
    if (app.items.empty() && app.nilesoftPath.empty()) {
        // TrackPopupMenu does not display an empty native menu. Retain the editor
        // drop surface so it is possible to add the first command again.
        if (app.frame) DeleteObject(app.frame);
        app.frame = nullptr; app.frameAlpha = false; app.frameSize = {}; app.rows.clear();
        app.frameInset = {}; app.menuSize = {}; app.frameDecorated = false;
        app.frameSimplified = false;
        app.blockRect = {}; app.blockExtra = 0; app.scrollY = 0;
        app.nilesoftModified = false; layoutEditor(app);
        app.menuActive = true; app.renderError = 0;
        InvalidateRect(app.preview, nullptr, TRUE);

        return;
    }
    const std::wstring directory = app.directory + L"\\render";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        app.renderError = 30;  return;
    }
    FILE *request = nullptr;
    if (_wfopen_s(&request, (directory + L"\\request.tsv").c_str(), L"w, ccs=UTF-8")) {
        app.renderError = 31;  return;
    }
    for (size_t i = 0; i < app.items.size(); ++i)
        fwprintf_s(request, L"%u\t%u\t%s\n", app.items[i] < SEPARATOR ? titleMenuCommand(app.items[i]) : static_cast<unsigned>(0xe000 + i),
            app.items[i] == SEPARATOR ? 1U : (app.items[i] == WINDOWS_BLOCK ? 2U : 0U),
            app.items[i] == SEPARATOR ? L"" : app.labels[app.items[i]]);
    const bool requestFailed = ferror(request) != 0;
    const int requestClosed = fclose(request);
    if (requestFailed || requestClosed) {
        app.renderError = 31;  return;
    }
    if (!app.desktop) {
        app.desktopName = L"FileSpacerMenuPreview-" + std::to_wstring(GetCurrentProcessId());
        app.desktop.reset(CreateDesktopW(app.desktopName.c_str(), nullptr, nullptr, 0,
            DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS | DESKTOP_ENUMERATE, nullptr));
    }
    if (!app.desktop) { app.renderError = 32;  return; }
    // NS evaluates the real executable name, separately from this editor.
    wchar_t modulePath[32768] = {};
    const DWORD moduleLength = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
    if (!checkLE(moduleLength && moduleLength < _countof(modulePath))) { app.renderError = 33; return; }
    const std::wstring executable = std::wstring(modulePath);
    std::wstring command = L"\"" + executable + L"\" /render-title-menu \"" + directory + L"\" \"" + app.nilesoftPath + L"\"";
    RECT position = {}; GetWindowRect(app.window, &position);
    command += L" " + std::to_wstring(position.left) + L" " + std::to_wstring(position.top);
    STARTUPINFOW start = {sizeof(start)};
    start.lpDesktop = &app.desktopName[0];
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, app.directory.c_str(), &start, &process)) {
        app.renderError = 33;  return;
    }
    std::unique_ptr<void, decltype(&CloseHandle)> processHandle(process.hProcess, CloseHandle);
    std::unique_ptr<void, decltype(&CloseHandle)> threadHandle(process.hThread, CloseHandle);
    if (!waitForRenderer(app, processHandle.get())) {
        app.renderError = 34;
        // A close requested during a drag is posted by startDrag once it ends.
        if (app.closePending && !app.dragging) PostMessageW(app.window, WM_CLOSE, 0, 0);
        return;
    }
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processHandle.get(), &exitCode) || exitCode) {
        app.renderError = static_cast<int>(exitCode ? exitCode : 35);
         return;
    }
    FILE *rows = nullptr;
    if (_wfopen_s(&rows, (directory + L"\\frame.tsv").c_str(), L"r, ccs=UTF-8")) {
        app.renderError = 36;  return;
    }
    wchar_t line[2048] = {};
    if (!fgetws(line, _countof(line), rows)) { fclose(rows); app.renderError = 37; return; }
    long width = 0, height = 0; int alpha = 0, decorated = 0, simplified = 0;
    POINT inset = {};
    SIZE menuSize = {};
    if (swscanf_s(line, L"%ld\t%ld\t%d\t%ld\t%ld\t%d\t%ld\t%ld\t%d", &width, &height, &alpha,
            &inset.x, &inset.y, &decorated, &menuSize.cx, &menuSize.cy, &simplified) != 9) {
        fclose(rows); app.renderError = 38; return;
    }
    std::vector<RenderedRow> rendered;
    while (fgetws(line, _countof(line), rows)) {
        std::wistringstream stream(line);
        unsigned id = 0, type = 0; int source = -1, managed = 0, locked = 0;
        RECT rect = {};
        if (!(stream >> id >> source >> managed >> locked >> type >> rect.left >> rect.top >> rect.right >> rect.bottom)) {
            fclose(rows); app.renderError = 39; return;
        }
        if (source >= static_cast<int>(app.items.size()) || source < -1) { fclose(rows); app.renderError = 39; return; }
        wchar_t separator = 0; stream.get(separator);
        std::wstring text; std::getline(stream, text);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) text.pop_back();
        rendered.push_back({rect, source, managed != 0, locked != 0, type, text});
    }
    fclose(rows);
    if (!app.nilesoftPath.empty()) {
        for (const auto &row : rendered) if (row.locked && row.source >= 0) {
            const int itemCommand = app.items[static_cast<size_t>(row.source)];
            if (itemCommand != SEPARATOR && std::find(app.nsForcedCommands.begin(), app.nsForcedCommands.end(), itemCommand) == app.nsForcedCommands.end())
                app.nsForcedCommands.push_back(itemCommand);
        }
        if (app.pendingMove >= 0 && std::find(app.nsForcedCommands.begin(), app.nsForcedCommands.end(), app.pendingMove) != app.nsForcedCommands.end()) {
            app.items = app.beforeMove; app.beforeMove.clear(); app.pendingMove = -1;
            app.moveRejected = true; refreshCatalog(app);
            refreshPreview(app); return;
        }
        for (auto &row : rendered) if (row.source >= 0
                && std::find(app.nsForcedCommands.begin(), app.nsForcedCommands.end(), app.items[static_cast<size_t>(row.source)]) != app.nsForcedCommands.end())
            row.locked = true;
    }
    app.pendingMove = -1; app.beforeMove.clear();
    void *pixels = nullptr;
    HBITMAP frame = loadFrame(directory + L"\\frame.bmp", width, height, &pixels);
    if (!frame || width <= 0 || height <= 0 || rendered.empty()) {
        if (frame) DeleteObject(frame);
        app.renderError = 40;  return;
    }
    auto pixel = [pixels, width](int x, int y) {
        return static_cast<BYTE *>(pixels)
            + (static_cast<std::ptrdiff_t>(y) * width + static_cast<std::ptrdiff_t>(x)) * 4;
    };
    const RECT bounds = {0, 0, width, height};
    GdiFlush();
    // Cache disabled pixels once, so partial repaints use the same background and alpha.
    for (const auto &row : rendered) if (row.locked && !(row.type & MFT_SEPARATOR)) {
        RECT area = row.rect; OffsetRect(&area, inset.x, inset.y);
        if (!IntersectRect(&area, &area, &bounds)) continue;
        BYTE background[4] = {};
        std::memcpy(background, pixel(std::min(area.left + 8, area.right - 1), std::min(area.top + 1, area.bottom - 1)), sizeof(background));
        for (int y = area.top; y < area.bottom; ++y) for (int x = area.left; x < area.right; ++x) {
            BYTE *target = pixel(x, y);
            for (int channel = 0; channel < (alpha ? 4 : 3); ++channel)
                target[channel] = static_cast<BYTE>((target[channel] * 112 + background[channel] * 143 + 127) / 255);
        }
    }
    if (app.measuringStack) {
        const bool windowsBlock = std::any_of(rendered.begin(), rendered.end(), [&app](const RenderedRow &row) {
            return row.source >= 0 && app.items[static_cast<size_t>(row.source)] == WINDOWS_BLOCK;
        });
        app.fullStackHeight = PREVIEW_Y - inset.y + height + (windowsBlock ? 12 : 0) + 12;
        app.stackSeparatorHeight = 0;
        for (const auto &row : rendered) if (row.type & MFT_SEPARATOR)
            app.stackSeparatorHeight = std::max(app.stackSeparatorHeight, static_cast<int>(row.rect.bottom - row.rect.top));
        DeleteObject(frame); app.renderError = 0; return;
    }
    if (app.frame) DeleteObject(app.frame);
    app.frame = frame; app.frameAlpha = alpha != 0;
    app.frameInset = inset; app.frameDecorated = decorated != 0;
    app.frameSimplified = simplified != 0;
    app.menuSize = menuSize;
    app.frameSize = {width, height}; app.rows = std::move(rendered);
    app.blockRect = {}; app.blockExtra = 0;
    for (auto &row : app.rows) {
        if (row.source >= 0 && app.items[static_cast<size_t>(row.source)] == WINDOWS_BLOCK) {
            app.blockRect = row.rect; app.blockExtra = 12;
            row.rect.bottom += app.blockExtra; row.text = getString(IDS_TITLE_MENU_WINDOWS_BLOCK);
        } else if (app.blockExtra) OffsetRect(&row.rect, 0, app.blockExtra);
    }
    app.frameSize.cy += app.blockExtra; app.menuSize.cy += app.blockExtra;
    app.nilesoftModified = false;
    if (!app.nilesoftPath.empty()) {
        app.nilesoftModified = app.rows.size() != app.items.size();
        for (size_t index = 0; index < app.rows.size(); ++index)
            if (app.rows[index].managed || app.rows[index].locked || app.rows[index].source != static_cast<int>(index))
                app.nilesoftModified = true;
    }
    layoutEditor(app);
    app.menuActive = true; app.renderError = 0;
    InvalidateRect(app.preview, nullptr, TRUE);

}

int insertionAt(App &app, POINTL point) {
    POINT local = {point.x, point.y}; ScreenToClient(app.preview, &local);
    local.y += app.scrollY - PREVIEW_Y;
    // Locked NS rows are not insertion anchors; choose the next editable
    // source row. NS reapplies its own positions on the following rendering.
    for (const auto &row : app.rows) {
        if (row.source >= 0 && !row.locked && local.y < (row.rect.top + row.rect.bottom) / 2)
            return row.source;
    }
    return static_cast<int>(app.items.size());
}

void fillColor(HDC dc, const RECT &rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

RECT insertionRect(const App &app, int index) {
    int y = app.menuSize.cy ? app.menuSize.cy - 2 : 24;
    for (const auto &row : app.rows) if (!row.locked && row.source == index) { y = row.rect.top; break; }
    return {PREVIEW_X, PREVIEW_Y + y, PREVIEW_X + (app.menuSize.cx ? app.menuSize.cx : 300), PREVIEW_Y + y + 2};
}

struct PaintBuffer {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    bool ready = false;
    PaintBuffer(HDC target, const RECT &area) {
        dc = CreateCompatibleDC(target);
        bitmap = CreateCompatibleBitmap(target, area.right - area.left, area.bottom - area.top);
        if (dc && bitmap) previous = SelectObject(dc, bitmap);
        if (previous && previous != HGDI_ERROR)
            ready = SetWindowOrgEx(dc, area.left, area.top, nullptr) != FALSE;
    }
    PaintBuffer(const PaintBuffer &) = delete;
    PaintBuffer &operator=(const PaintBuffer &) = delete;
    ~PaintBuffer() {
        if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }
};

void setInsertion(App &app, int index) {
    if (app.insertion == index) return;
    if (app.preview && app.insertion >= 0) {
        RECT old = insertionRect(app, app.insertion); OffsetRect(&old, 0, -app.scrollY);
        InvalidateRect(app.preview, &old, FALSE);
    }
    app.insertion = index;
    if (app.preview && index >= 0) {
        RECT next = insertionRect(app, index); OffsetRect(&next, 0, -app.scrollY);
        InvalidateRect(app.preview, &next, FALSE);
    }
}

void drawMenuFrame(App &app, HDC dc) {
    HDC memory = CreateCompatibleDC(dc); HGDIOBJ previous = SelectObject(memory, app.frame);
    const int x = PREVIEW_X - app.frameInset.x, y = PREVIEW_Y - app.frameInset.y;
    const int width = app.frameSize.cx, rawHeight = app.frameSize.cy - app.blockExtra;
    auto copy = [&](int dx, int dy, int w, int h, int sx, int sy, int sw, int sh) {
        if (w <= 0 || h <= 0 || sw <= 0 || sh <= 0) return;
        if (app.frameAlpha) {
            BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            AlphaBlend(dc, dx, dy, w, h, memory, sx, sy, sw, sh, blend);
        } else StretchBlt(dc, dx, dy, w, h, memory, sx, sy, sw, sh, SRCCOPY);
    };
    if (!app.blockExtra) copy(x, y, width, rawHeight, 0, 0, width, rawHeight);
    else {
        const int top = app.frameInset.y + app.blockRect.top;
        const int bottom = app.frameInset.y + app.blockRect.bottom;
        const int left = app.frameInset.x + app.blockRect.left, right = app.frameInset.x + app.blockRect.right;
        const int height = bottom - top + app.blockExtra;
        copy(x, y, width, top, 0, 0, width, top);
        copy(x, y + bottom + app.blockExtra, width, rawHeight - bottom, 0, bottom, width, rawHeight - bottom);
        copy(x, y + top, left, height, 0, top, left, bottom - top);
        copy(x + right, y + top, width - right, height, right, top, width - right, bottom - top);
        RECT block = app.blockRect; block.bottom += app.blockExtra; OffsetRect(&block, PREVIEW_X, PREVIEW_Y);
        FillRect(dc, &block, GetSysColorBrush(COLOR_BTNFACE));
        FrameRect(dc, &block, GetSysColorBrush(COLOR_3DSHADOW));
        HGDIOBJ font = SelectObject(dc, app.font); SetBkMode(dc, TRANSPARENT);
        const bool locked = std::any_of(app.rows.begin(), app.rows.end(), [&app](const RenderedRow &row) {
            return row.locked && row.source >= 0 && app.items[static_cast<size_t>(row.source)] == WINDOWS_BLOCK;
        });
        SetTextColor(dc, GetSysColor(locked ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
        DrawTextW(dc, getString(IDS_TITLE_MENU_WINDOWS_BLOCK), -1, &block, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, font);
    }
    SelectObject(memory, previous); DeleteDC(memory);
    HGDIOBJ brush = SelectObject(dc, GetStockObject(DC_BRUSH));
    HGDIOBJ pen = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCBrushColor(dc, GetSysColor(COLOR_GRAYTEXT)); SetDCPenColor(dc, GetSysColor(COLOR_GRAYTEXT));
    for (const auto &row : app.rows) if (row.locked && !(row.type & MFT_SEPARATOR)) {
        const int right = PREVIEW_X + row.rect.left - 6;
        const int middle = PREVIEW_Y + (row.rect.top + row.rect.bottom) / 2;
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, right - 7, middle - 7, right - 1, middle, 5, 5);
        SelectObject(dc, GetStockObject(DC_BRUSH));
        Rectangle(dc, right - 8, middle - 2, right + 1, middle + 5);
    }
    SelectObject(dc, pen); SelectObject(dc, brush);
}

void trackBlockedTip(App &app, bool visible) {
    const bool changed = visible != app.blockedTipActive;
    app.blockedTipActive = visible;
    if (visible) {
        POINT point = {}; GetCursorPos(&point);
        SendMessageW(app.blockedTip, TTM_TRACKPOSITION, 0, MAKELPARAM(point.x + 16, point.y + 20));
    }
    if (changed || !visible)
        SendMessageW(app.blockedTip, TTM_TRACKACTIVATE, visible, reinterpret_cast<LPARAM>(&app.blockedTool));
    if (!visible) ShowWindow(app.blockedTip, SW_HIDE);
    }

LRESULT CALLBACK previewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    App *app = reinterpret_cast<App *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    if (message == WM_VSCROLL || message == WM_MOUSEWHEEL) {
        trackBlockedTip(*app, false);
        SCROLLINFO scroll = {sizeof(scroll), SIF_ALL}; GetScrollInfo(window, SB_VERT, &scroll);
        int next = app->scrollY;
        if (message == WM_MOUSEWHEEL) next -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
        else switch (LOWORD(wParam)) {
            case SB_LINEUP: next -= 24; break;
            case SB_LINEDOWN: next += 24; break;
            case SB_PAGEUP: next -= static_cast<int>(scroll.nPage); break;
            case SB_PAGEDOWN: next += static_cast<int>(scroll.nPage); break;
            case SB_TOP: next = 0; break;
            case SB_BOTTOM: next = scroll.nMax; break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: next = scroll.nTrackPos; break;
            default: return 0;
        }
        scroll.fMask = SIF_POS; scroll.nPos = next;
        app->scrollY = SetScrollInfo(window, SB_VERT, &scroll, TRUE);
        InvalidateRect(window, nullptr, FALSE);  return 0;
    }
    if (message == WM_MOUSEMOVE && !app->dragging) {
        POINT point = {GET_X_LPARAM(lParam) - PREVIEW_X, GET_Y_LPARAM(lParam) + app->scrollY - PREVIEW_Y};
        const bool locked = !app->nilesoftPath.empty() && std::any_of(app->rows.begin(), app->rows.end(), [point](const RenderedRow &row) {
            return row.locked && PtInRect(&row.rect, point);
        });
        trackBlockedTip(*app, locked);
        SetCursor(LoadCursorW(nullptr, locked ? IDC_NO : IDC_ARROW));
        if (locked) {
            TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
        }
    }
    if (message == WM_MOUSELEAVE) { trackBlockedTip(*app, false); return 0; }
    if (message == WM_ERASEBKGND) return 1; // The buffered scene covers the paint region.
    if (message == WM_PAINT) {
        PAINTSTRUCT paint = {}; HDC target = BeginPaint(window, &paint);
        PaintBuffer buffer(target, paint.rcPaint);
        HDC dc = buffer.ready ? buffer.dc : target;
        RECT area = {}; GetClientRect(window, &area);
        FillRect(dc, &area, GetSysColorBrush(COLOR_BTNFACE));
        RECT outline = area; InflateRect(&outline, -1, -1);
        std::unique_ptr<std::remove_pointer<HTHEME>::type, decltype(&CloseThemeData)> theme(
            OpenThemeData(window, L"Button"), CloseThemeData);
        DTBGOPTS options = {sizeof(options), DTBG_OMITCONTENT, {}};
        if (!theme || FAILED(DrawThemeBackgroundEx(theme.get(), dc, BP_GROUPBOX, GBS_NORMAL, &outline, &options)))
            DrawEdge(dc, &outline, EDGE_ETCHED, BF_RECT);
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 2, 2, area.right - 2, area.bottom - 2);
        OffsetViewportOrgEx(dc, 0, -app->scrollY, nullptr);
        if (app->backdrop) {
            HDC image = CreateCompatibleDC(dc);
            HGDIOBJ previous = SelectObject(image, app->backdrop.get());
            // Keep the pictured dropdown and menu together. Reveal only the
            // last 32 pixels to the left of the menu, fading into dialog gray.
            for (int x = PREVIEW_X - 32; x < PREVIEW_X; ++x) {
                BLENDFUNCTION fade = {AC_SRC_OVER, 0, static_cast<BYTE>((x - PREVIEW_X + 32) * 255 / 31), 0};
                AlphaBlend(dc, x, 16, 1, 290, image, x - IMAGE_X, 0, 1, 290, fade);
            }
            BitBlt(dc, PREVIEW_X, 16, IMAGE_X + 350 - PREVIEW_X, 290, image, 194, 0, SRCCOPY);
            SelectObject(image, previous); DeleteDC(image);
        }
        if (app->frame) drawMenuFrame(*app, dc);
        else if (app->menuActive && app->items.empty()) {
            HGDIOBJ previous = SelectObject(dc, app->font);
            SetBkMode(dc, TRANSPARENT); SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
            RECT hint = {PREVIEW_X, PREVIEW_Y, area.right - 24, area.bottom - 24};
            DrawTextW(dc, getString(IDS_TITLE_MENU_EMPTY), -1,
                &hint, DT_LEFT | DT_TOP | DT_WORDBREAK);
            SelectObject(dc, previous);
        }
        if (app->insertion >= 0) {
            const RECT marker = insertionRect(*app, app->insertion);
            FillRect(dc, &marker, GetSysColorBrush(COLOR_HIGHLIGHT));
        }
        RestoreDC(dc, saved);
        if (buffer.ready) BitBlt(target, paint.rcPaint.left, paint.rcPaint.top,
            paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
            dc, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
        EndPaint(window, &paint); return 0;
    }
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) {
        SetFocus(window);
        if (app->rendering) return 0;
        app->pressedPoint = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; app->pressedRow = -1;
        POINT itemPoint = {app->pressedPoint.x - PREVIEW_X, app->pressedPoint.y - PREVIEW_Y + app->scrollY};
        for (size_t i = 0; i < app->rows.size(); ++i)
            if (!app->rows[i].locked && PtInRect(&app->rows[i].rect, itemPoint)) { app->pressedRow = static_cast<int>(i); break; }
        if (app->pressedRow >= 0) SetCapture(window);
        return 0;
    }
    if (message == WM_LBUTTONUP || message == WM_CANCELMODE) {
        if (message == WM_CANCELMODE) trackBlockedTip(*app, false);
        app->pressedRow = -1; if (GetCapture() == window) ReleaseCapture(); return 0;
    }
    if (message == WM_MOUSEMOVE && (wParam & MK_LBUTTON) && app->pressedRow >= 0) {
        const auto &row = app->rows[static_cast<size_t>(app->pressedRow)];
        if (abs(GET_X_LPARAM(lParam) - app->pressedPoint.x) >= GetSystemMetrics(SM_CXDRAG)
                || abs(GET_Y_LPARAM(lParam) - app->pressedPoint.y) >= GetSystemMetrics(SM_CYDRAG)) {
            const int source = row.source;
            app->pressedRow = -1; ReleaseCapture();
            if (source >= 0 && source < static_cast<int>(app->items.size()))
                startDrag(*app, app->items[static_cast<size_t>(source)], source);
        }
        return 0;
    }
    if (message == WM_CAPTURECHANGED) { trackBlockedTip(*app, false); app->pressedRow = -1; return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK catalogInput(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    App &app = *reinterpret_cast<App *>(data);
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) {
        if (app.rendering) return 0;
        app.catalogPoint = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        LVHITTESTINFO hit = {}; hit.pt = app.catalogPoint;
        const int row = ListView_SubItemHitTest(window, &hit);
        app.catalogPressed = catalogCommand(window, row);
        if (row >= 0) {
            ListView_SetItemState(window, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(window, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            SetFocus(window); SetCapture(window);
        }
        return 0;
    }
    if (message == WM_LBUTTONUP || message == WM_CANCELMODE) {
        app.catalogPressed = -1; if (GetCapture() == window) ReleaseCapture(); return 0;
    }
    if (message == WM_MOUSEMOVE && (wParam & MK_LBUTTON) && app.catalogPressed >= 0) {
        if (abs(GET_X_LPARAM(lParam) - app.catalogPoint.x) >= GetSystemMetrics(SM_CXDRAG)
                || abs(GET_Y_LPARAM(lParam) - app.catalogPoint.y) >= GetSystemMetrics(SM_CYDRAG)) {
            const int command = app.catalogPressed; app.catalogPressed = -1; ReleaseCapture();
            startDrag(app, command, -1);
        }
        return 0;
    }
    if (message == WM_CAPTURECHANGED) { app.catalogPressed = -1; return 0; }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, catalogInput, 1);
    return DefSubclassProc(window, message, wParam, lParam);
}

INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    App *app = reinterpret_cast<App *>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        app = reinterpret_cast<App *>(lParam); app->window = window;
        SetWindowLongPtrW(window, DWLP_USER, lParam);
        app->font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
        const UINT labels[] = {IDS_CAPTION_COPY_PATH, IDS_CAPTION_RENAME, IDS_LOCK_WINDOW_STATE,
            IDS_CAPTION_PROPERTIES, IDS_CAPTION_CLOSE_PARENTS, IDS_REOPEN_MENU, IDS_SETTINGS_CAPTION,
            IDS_TITLE_MENU_RESTORE, IDS_TITLE_MENU_REPLACE, 0,
            IDS_TITLE_MENU_CLOSE_DESCENDANTS, IDS_TITLE_MENU_CLOSE_OTHERS, IDS_TITLE_MENU_FIT,
            IDS_TITLE_MENU_GROUPS, IDS_TITLE_MENU_SEPARATOR, IDS_TITLE_MENU_OPEN_WINDOWS};
        for (int i = 0; i < LABEL_COUNT; ++i) app->labels[i] = labels[i] ? getString(labels[i]) : L"";
        app->catalog = GetDlgItem(window, IDC_TITLE_MENU_ACTIONS);
        app->nilesoftNotice = GetDlgItem(window, IDC_TITLE_MENU_NS_NOTICE);
        app->settingsLink = GetDlgItem(window, IDC_TITLE_MENU_NS_LINK);
        SetWindowTextW(app->nilesoftNotice, getString(IDS_TITLE_MENU_NS_NOTICE));
        SetWindowTextW(app->settingsLink, getString(IDS_TITLE_MENU_NS_LINK));
        app->nilesoftPath = installedNilesoft();
        app->items = settings::getTitleMenu();
        app->format = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"FileSpacer.TitleMenuEntry"));
        app->backdrop.reset(static_cast<HBITMAP>(LoadImageW(GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(IDR_TITLE_MENU_BACKDROP), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
        WNDCLASSW type = {}; type.lpfnWndProc = previewProc; type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"FileSpacer Title Menu Editor";
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { EndDialog(window, IDCANCEL); return TRUE; }
        app->preview = CreateWindowExW(0, type.lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            24, 20, 300, 300, window, reinterpret_cast<HMENU>(IDC_TITLE_MENU_PREVIEW), type.hInstance, app);
        app->blockedTip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            window, nullptr, type.hInstance, nullptr);
        app->blockedTool.cbSize = sizeof(app->blockedTool);
        app->blockedTool.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE | TTF_TRANSPARENT;
        app->blockedTool.hwnd = app->preview;
        app->blockedTool.uId = reinterpret_cast<UINT_PTR>(app->preview);
        app->blockedTool.lpszText = const_cast<wchar_t *>(getString(IDS_TITLE_MENU_NS_TOOLTIP));
        SendMessageW(app->blockedTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&app->blockedTool));
        SendMessageW(app->blockedTip, TTM_SETMAXTIPWIDTH, 0, 420);
        SendMessageW(app->catalog, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW column = {}; column.mask = LVCF_WIDTH; column.cx = 300;
        ListView_InsertColumn(app->catalog, 0, &column);
        SetWindowSubclass(app->catalog, catalogInput, 1, reinterpret_cast<DWORD_PTR>(app));
        CComPtr<IDropTarget> menuTarget, removeTarget;
        menuTarget.Attach(new Target(*app, 0, false, true));
        removeTarget.Attach(new Target(*app, -1, true));
        if (FAILED(RegisterDragDrop(app->preview, menuTarget)) || FAILED(RegisterDragDrop(window, removeTarget))) {
            EndDialog(window, IDCANCEL); return TRUE;
        }
        // The first rendering runs once the dialog is shown and its owner disabled,
        // since the wait dispatches messages.
        refreshCatalog(*app);
        PostMessageW(window, RENDER_FRAME, 0, 0);
        return TRUE;
    }
    if (!app) return FALSE;
    switch (message) {
        case RENDER_FRAME:
            if (app->rendering) return TRUE;
            if (!wParam) refreshPreview(*app);
            EnableWindow(GetDlgItem(window, IDOK), !app->renderError);
            if (app->renderError && !app->closePending) MessageBoxW(window, getString(IDS_TITLE_MENU_RENDER_ERROR),
                getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_TITLE_MENU_DEFAULT) {
                if (app->rendering) return TRUE;
                app->items = defaultTitleMenu(); app->nsForcedCommands.clear(); app->pendingMove = -1;
                refreshCatalog(*app); PostMessageW(window, RENDER_FRAME, 0, 0); return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                if (app->dragging || app->rendering || app->renderError) return TRUE;
                if (settings::setTitleMenu(app->items) != ERROR_SUCCESS) {
                    MessageBoxW(window, getString(IDS_TITLE_MENU_SAVE_ERROR), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
                    return TRUE;
                }
                // Windows reread the composition when opening the menu: no broadcast.
                EndDialog(window, IDOK); return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) { SendMessageW(window, WM_CLOSE, 0, 0); return TRUE; }
            break;
        case WM_NOTIFY: {
            const NMHDR *notice = reinterpret_cast<NMHDR *>(lParam);
            if (notice->hwndFrom == app->settingsLink && (notice->code == NM_CLICK || notice->code == NM_RETURN)) {
                const std::wstring directory = nilesoftSettingsDirectory(app->nilesoftPath);
                const std::wstring argument = L"\"" + directory + L"\"";
                const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", L"explorer.exe", argument.c_str(), nullptr, SW_SHOWNORMAL));
                if (result <= 32) MessageBoxW(window, getString(IDS_TITLE_MENU_SETTINGS_ERROR), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
            SetBkColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_BTNFACE));
            SetTextColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_BTNTEXT));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        case WM_PAINT: {
            PAINTSTRUCT paint = {}; HDC dc = BeginPaint(window, &paint);
            const LONG x = app->arrowRect.left, y = app->arrowRect.top;
            const POINT arrow[] = {{x,y+22},{x+22,y},{x+22,y+12},{x+42,y+12},{x+42,y},
                {x+64,y+22},{x+42,y+44},{x+42,y+32},{x+22,y+32},{x+22,y+44}};
            HGDIOBJ brush = SelectObject(dc, GetStockObject(DC_BRUSH));
            HGDIOBJ pen = SelectObject(dc, GetStockObject(DC_PEN));
            SetDCBrushColor(dc, GetSysColor(COLOR_BTNTEXT)); SetDCPenColor(dc, GetSysColor(COLOR_BTNTEXT));
            Polygon(dc, arrow, _countof(arrow)); SelectObject(dc, pen); SelectObject(dc, brush);
            EndPaint(window, &paint); return TRUE;
        }
        case WM_CLOSE:
            if (app->dragging || app->rendering) app->closePending = true;
            else EndDialog(window, IDCANCEL);
            return TRUE;
        case WM_DESTROY:
            RevokeDragDrop(window); RevokeDragDrop(app->preview);
            if (app->blockedTip) DestroyWindow(app->blockedTip);
            return TRUE;
    }
    return FALSE;
}
} // namespace

bool titleMenuExtensionEnabled() {
#ifdef FILESPACER_DEBUG
    // Ordinary /test regressions use native menus. A dedicated interoperability
    // run can opt in without using the user's settings or folder-state database.
    if (settings::testMode) {
        wchar_t value[2] = {};
        return GetEnvironmentVariableW(L"FILESPACER_TEST_NILESOFT", value, _countof(value)) == 1 && value[0] == L'1';
    }
#endif
    return true;
}

HRESULT findTitleMenuExtension(CLSID *clsid, std::wstring *module) {
    std::wstring text;
    LSTATUS status = readRegistryString(HKEY_CLASSES_ROOT,
        L"Directory\\shellex\\ContextMenuHandlers\\ @nilesoft.shell", nullptr, text);
    if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
    CLSID parsed = {};
    const HRESULT result = CLSIDFromString(text.c_str(), &parsed);
    if (FAILED(result)) return result;
    if (module) {
        status = readRegistryString(HKEY_CLASSES_ROOT, (L"CLSID\\" + text + L"\\InprocServer32").c_str(),
            nullptr, *module);
        if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
    }
    if (clsid) *clsid = parsed;
    return S_OK;
}

HRESULT activateTitleMenuExtension(IClassFactory **factory) {
    if (!factory) return E_POINTER;
    *factory = nullptr;
    if (!titleMenuExtensionEnabled()) return S_FALSE;
    CLSID clsid = {};
    const HRESULT found = findTitleMenuExtension(&clsid, nullptr);
    if (FAILED(found)) return found;
    return CoGetClassObject(clsid, CLSCTX_INPROC_SERVER, nullptr, IID_IClassFactory,
        reinterpret_cast<void **>(factory));
}

void openTitleMenuDialog(HWND owner) {
    App app;
    GUID id = {}; wchar_t name[40] = {}, path[MAX_PATH] = {};
    const DWORD length = checkLE(GetTempPathW(_countof(path), path));
    bool ready = length && length < _countof(path) && checkHR(CoCreateGuid(&id))
        && StringFromGUID2(id, name, _countof(name));
    if (ready) {
        app.directory = std::wstring(path) + L"FileSpacerMenu-" + name;
        ready = checkLE(CreateDirectoryW(app.directory.c_str(), nullptr)) != FALSE;
        if (!ready) app.directory.clear(); // Nothing was created to remove.
    }
    if (!ready) {
        MessageBoxW(owner, getString(IDS_TITLE_MENU_RENDER_ERROR), getString(IDS_ERROR_CAPTION),
            MB_OK | MB_ICONERROR);
        return;
    }
    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_TITLE_MENU), owner,
        dialogProc, reinterpret_cast<LPARAM>(&app));
}
} // namespace filespacer
