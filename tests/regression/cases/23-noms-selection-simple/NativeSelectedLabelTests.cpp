#include <common.h>
#include "ShellViewHost.h"
#include "ShellViewLabels.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>
using namespace filespacer;
namespace {
constexpr UINT READY = WM_APP + 1, DONE = WM_APP + 2, RUN = WM_APP + 3;
constexpr int ITEM_COUNT = 20;
struct Raster {
    int width = 0, height = 0;
    std::vector<DWORD> pixels;
};
class LabelTests : public IServiceProvider, public ICommDlgBrowser2, public IExplorerBrowserEvents {
    LONG references = 1;
    ShellViewHost host;
    CComPtr<IShellView> view;
    HWND window = nullptr, viewWindow = nullptr, list = nullptr;
    ULONG_PTR generation = 0;
    bool navigation = false, enumerated = false, queued = false, running = false, closing = false;
    int failures = 0;
    std::array<POINT, ITEM_COUNT> positions = {};
    bool expect(bool value, const char *description) {
        printf("%s: %s\n", value ? "PASS" : "FAIL", description);
        if (!value) ++failures;
        return value;
    }
    void queue() {
        if (navigation && enumerated && !queued && !running)
            queued = PostMessageW(window, RUN, 0, 0) != FALSE;
    }
    static LRESULT CALLBACK observe(HWND target, UINT message, WPARAM wp, LPARAM lp,
            UINT_PTR id, DWORD_PTR data) {
        auto owner = reinterpret_cast<LabelTests *>(data);
        if (message == WM_NCDESTROY) RemoveWindowSubclass(target, observe, id);
        if (message == WM_NOTIFY && lp) {
            auto header = reinterpret_cast<const NMHDR *>(lp);
            wchar_t name[32] = {};
            if (header->code == NM_CUSTOMDRAW && GetClassNameW(header->hwndFrom, name, 32)
                    && lstrcmpW(name, WC_LISTVIEWW) == 0)
                owner->list = header->hwndFrom;
        }
        return DefSubclassProc(target, message, wp, lp);
    }
    Raster render(bool enabled) {
        setSingleSelectionLabels(view, enabled);
        RECT client = {};
        if (!GetClientRect(list, &client)) { expect(false, "read render bounds"); return {}; }
        Raster image; image.width = client.right; image.height = client.bottom;
        const auto releaseScreen = [this](HDC dc) { ReleaseDC(list, dc); };
        std::unique_ptr<std::remove_pointer<HDC>::type, decltype(releaseScreen)> screen(GetDC(list), releaseScreen);
        std::unique_ptr<std::remove_pointer<HDC>::type, decltype(&DeleteDC)> dc(
            CreateCompatibleDC(screen.get()), DeleteDC);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = image.width; info.bmiHeader.biHeight = -image.height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void *bits = nullptr;
        std::unique_ptr<std::remove_pointer<HBITMAP>::type, decltype(&DeleteObject)> bitmap(
            CreateDIBSection(screen.get(), &info, DIB_RGB_COLORS, &bits, nullptr, 0), DeleteObject);
        if (dc && bitmap) {
            struct Selection {
                HDC target;
                HGDIOBJ previous;
                ~Selection() { SelectObject(target, previous); }
            } selection = {dc.get(), SelectObject(dc.get(), bitmap.get())};
            FillRect(dc.get(), &client, GetSysColorBrush(COLOR_WINDOW));
            // Documented ListView WM_PAINT behavior: a supplied HDC receives the
            // real control's rendering, including the production custom draw.
            SendMessageW(list, WM_PAINT, reinterpret_cast<WPARAM>(dc.get()), 0);
            const auto start = static_cast<const DWORD *>(bits);
            image.pixels.assign(start, start + image.width * image.height);
            for (auto &pixel : image.pixels) pixel &= 0x00ffffff;
        } else expect(false, "allocate real-control render surface");
        return image;
    }
    static size_t differences(const Raster &a, const Raster &b, RECT area) {
        if (a.pixels.empty() || a.width != b.width || a.height != b.height) return SIZE_MAX;
        size_t count = 0;
        for (LONG y = (std::max)(0L, area.top); y < area.bottom && y < a.height; ++y)
            for (LONG x = (std::max)(0L, area.left); x < area.right && x < a.width; ++x)
                if (a.pixels[static_cast<size_t>(y) * a.width + x]
                        != b.pixels[static_cast<size_t>(y) * b.width + x]) ++count;
        return count;
    }
    void select(const std::vector<int> &items, int focused) {
        CComQIPtr<IFolderView2> folder(view);
        expect(SUCCEEDED(view->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "clear native selection");
        for (int item : items)
            expect(SUCCEEDED(folder->SelectItem(item, SVSI_SELECT | SVSI_NOTAKEFOCUS)), "select exact item");
        if (focused >= 0) {
            const bool keepSelected = std::find(items.begin(), items.end(), focused) != items.end();
            expect(SUCCEEDED(folder->SelectItem(focused, SVSI_FOCUSED | SVSI_NOTAKEFOCUS
                | (keepSelected ? SVSI_SELECT : 0))), "set independent focus item");
        }
    }
    void unchanged(const std::vector<int> &selected, int focused) {
        CComQIPtr<IFolderView2> folder(view);
        int actualFocus = -1, count = -1;
        folder->GetFocusedItem(&actualFocus); folder->ItemCount(SVGIO_SELECTION, &count);
        bool exact = count == static_cast<int>(selected.size());
        for (int i = 0; i < ITEM_COUNT; ++i) {
            CComHeapPtr<ITEMID_CHILD> item; POINT position = {};
            const bool wanted = std::find(selected.begin(), selected.end(), i) != selected.end();
            exact = exact && ((ListView_GetItemState(list, i, LVIS_SELECTED) & LVIS_SELECTED) != 0) == wanted;
            if (FAILED(folder->Item(i, &item)) || FAILED(folder->GetItemPosition(item, &position))
                    || position.x != positions[i].x || position.y != positions[i].y) exact = false;
        }
        expect(exact && actualFocus == focused, "exact selection, independent focus and all positions preserved");
    }
    void nativeCase(const std::vector<int> &selected, int focused, const char *description) {
        select(selected, focused);
        const HWND keyboard = GetFocus();
        const Raster native = render(false), adapted = render(true);
        RECT all = {0, 0, native.width, native.height};
        expect(differences(native, adapted, all) == 0, description);
        expect(GetFocus() == keyboard, "painting does not take keyboard focus");
        unchanged(selected, focused);
    }
    void run() {
        CComQIPtr<IFolderView2> folder(view);
        int count = -1; UINT mode = 0;
        if (!expect(SUCCEEDED(folder->ItemCount(SVGIO_ALLVIEW, &count)) && count == ITEM_COUNT,
                "real Shell fixture is enumerated")
                || !expect(SUCCEEDED(folder->GetCurrentViewMode(&mode)) && mode == FVM_ICON,
                    "production spatial profile uses icon view")) { finish(); return; }
        folder->SetCurrentFolderFlags(FWF_AUTOARRANGE, 0);
        for (int i = 0; i < ITEM_COUNT; ++i) {
            CComHeapPtr<ITEMID_CHILD> item; folder->Item(i, &item);
            POINT point = {35 + (i % 6) * 110, 20 + (i / 6) * 100};
            PCUITEMID_CHILD ids[] = {item};
            expect(SUCCEEDED(folder->SelectAndPositionItems(1, ids, &point, SVSI_POSITIONITEM)), "position fixture");
            folder->GetItemPosition(item, &positions[i]);
        }
        view->UIActivate(SVUIA_ACTIVATE_FOCUS);
        RedrawWindow(viewWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        if (!expect(list != nullptr, "observe the actual notification emitter")) { finish(); return; }
        select({0}, 0);
        const Raster native = render(false), adapted = render(true), disabled = render(false);
        RECT label = {}; ListView_GetItemRect(list, 0, &label, LVIR_LABEL);
        const size_t changed = differences(native, adapted, label);
        printf("Single selected label: changed pixels=%zu, height=%ld\n", changed, label.bottom - label.top);
        expect(label.bottom - label.top > 60 && changed > 50 && changed != SIZE_MAX,
            "single selected long name has an opaque native label background");
        RECT all = {0, 0, native.width, native.height};
        expect(differences(native, disabled, all) == 0, "disabling restores the exact native image");
        unchanged({0}, 0);
        nativeCase({0, 1}, 1, "multiple selection is pixel-identical to native rendering");
        nativeCase({}, 1, "focus without selection is pixel-identical to native rendering");
        select({0}, 1);
        const Raster separateNative = render(false), separateAdapted = render(true);
        RECT focused = {}; ListView_GetItemRect(list, 1, &focused, LVIR_LABEL);
        expect(differences(separateNative, separateAdapted, focused) == 0,
            "unselected focused label is left to the Shell when another item is selected");
        unchanged({0}, 1);
        select({0}, 0);
        const Raster returnedNative = render(false), returnedSingle = render(true);
        expect(differences(returnedNative, returnedSingle, label) > 50,
            "returning to a single selection restores the opaque label");
        expect(SUCCEEDED(folder->SelectItem(0, SVSI_EDIT | SVSI_SELECT | SVSI_FOCUSED)), "begin native rename editor");
        expect(ListView_GetEditControl(list) != nullptr, "native rename editor remains available");
        render(true);
        ListView_CancelEditLabel(list);
        expect(ListView_GetEditControl(list) == nullptr, "native rename editor cancels normally");
        expect(SUCCEEDED(view->Refresh()), "refresh with optional drawing installed");
        finish();
    }
    void finish() { PostMessageW(window, WM_CLOSE, 0, 0); }
public:
    int exitCode = 3;
    STDMETHODIMP QueryInterface(REFIID id, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IServiceProvider)) *object = static_cast<IServiceProvider *>(this);
        else if (id == __uuidof(IExplorerBrowserEvents)) *object = static_cast<IExplorerBrowserEvents *>(this);
        else if (id == __uuidof(ICommDlgBrowser) || id == __uuidof(ICommDlgBrowser2)) *object = static_cast<ICommDlgBrowser2 *>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override { return InterlockedDecrement(&references); }
    STDMETHODIMP QueryService(REFGUID service, REFIID id, void **object) override {
        if (service == SID_SExplorerBrowserFrame) return QueryInterface(id, object);
        *object = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP OnDefaultCommand(IShellView *) override { return S_FALSE; }
    STDMETHODIMP OnStateChange(IShellView *, ULONG) override { return S_OK; }
    STDMETHODIMP IncludeObject(IShellView *, PCUITEMID_CHILD) override { return S_OK; }
    STDMETHODIMP GetDefaultMenuText(IShellView *, LPWSTR, int) override { return S_FALSE; }
    STDMETHODIMP GetViewFlags(DWORD *flags) override { *flags = CDB2GVF_NOSELECTVERB; return S_OK; }
    STDMETHODIMP Notify(IShellView *, DWORD) override { return S_OK; }
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE) override { return S_OK; }
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE) override { navigation = true; queue(); return S_OK; }
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE) override { expect(false, "navigation"); finish(); return S_OK; }
    STDMETHODIMP OnViewCreated(IShellView *source) override {
        view = source;
        expect(SUCCEEDED(source->GetWindow(&viewWindow)), "get public view window");
        expect(SetWindowSubclass(viewWindow, observe, 0, reinterpret_cast<DWORD_PTR>(this)) != FALSE, "observe native notifications");
        expect(SUCCEEDED(host.observeView(source, window, READY, DONE, generation)), "observe public enumeration completion");
        return S_OK;
    }
    bool start(const std::wstring &fixture) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = procedure; wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"FileSpacerSingleSelectionLabelTests";
        RegisterClassW(&wc);
        window = CreateWindowW(wc.lpszClassName, L"FileSpacer native selected labels", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 650, nullptr, nullptr, wc.hInstance, this);
        if (!window) return false;
        RECT bounds = {0, 0, 780, 600}; FOLDERSETTINGS fs = {FVM_ICON, FWF_NONE};
        if (FAILED(host.initialize(window, bounds, fs, static_cast<IServiceProvider *>(this), this))) return false;
        CComPtr<IShellItem> folder;
        if (FAILED(SHCreateItemFromParsingName(fixture.c_str(), nullptr, IID_PPV_ARGS(&folder)))) return false;
        ShowWindow(window, SW_SHOW);
        SetTimer(window, 1, 60000, nullptr); // Failure watchdog only.
        return SUCCEEDED(host.navigate(folder));
    }
    bool translate(MSG *message) { return view && view->TranslateAccelerator(message) == S_OK; }
    static LRESULT CALLBACK procedure(HWND target, UINT message, WPARAM wp, LPARAM lp) {
        auto owner = reinterpret_cast<LabelTests *>(GetWindowLongPtrW(target, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            owner = static_cast<LabelTests *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            owner->window = target; SetWindowLongPtrW(target, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
        }
        if (!owner) return DefWindowProcW(target, message, wp, lp);
        if (message == DONE && owner->host.isViewNotification(wp)) { owner->enumerated = true; owner->queue(); return 0; }
        if (message == READY) return 0;
        if (message == RUN && !owner->running && !owner->closing) { owner->running = true; owner->run(); return 0; }
        if (message == WM_TIMER) { owner->expect(false, "watchdog"); owner->finish(); return 0; }
        if (message == WM_CLOSE) {
            owner->closing = true; KillTimer(target, 1);
            if (owner->viewWindow) RemoveWindowSubclass(owner->viewWindow, observe, 0);
            owner->view.Release(); owner->expect(SUCCEEDED(owner->host.destroy()), "destroy with optional callback installed");
            owner->exitCode = owner->failures ? 3 : 0;
            printf("RESULT: %s selected-label rendering; failures=%d\n", owner->exitCode ? "FAIL" : "PASS", owner->failures);
            DestroyWindow(target); return 0;
        }
        if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(target, message, wp, lp);
    }
};
}
int wmain() {
    if (FAILED(OleInitialize(nullptr))) return 2;
    wchar_t executable[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, executable, MAX_PATH); PathRemoveFileSpecW(executable);
    const std::wstring fixture = std::wstring(executable) + L"\\labels-fixture";
    if (!CreateDirectoryW(fixture.c_str(), nullptr)) { OleUninitialize(); return 2; }
    for (int i = 0; i < ITEM_COUNT; ++i) {
        wchar_t leaf[160]; swprintf_s(leaf, L"%02d - A long selected name extending over the icons below, with opaque background and native text preserved.txt", i);
        const std::wstring path = fixture + L"\\" + leaf;
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) { OleUninitialize(); return 2; }
        std::unique_ptr<void, decltype(&CloseHandle)> ownedFile(file, CloseHandle);
    }
    int result = 3;
    { LabelTests test; if (test.start(fixture)) { MSG message; while (GetMessageW(&message, nullptr, 0, 0) > 0) if (!test.translate(&message)) { TranslateMessage(&message); DispatchMessageW(&message); } result = test.exitCode; } }
    OleUninitialize(); return result;
}
