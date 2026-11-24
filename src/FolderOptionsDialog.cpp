#include "FolderOptionsDialog.h"
#include "UIStrings.h"
#include <commctrl.h>
#include <prsht.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <dwmapi.h>
#include <atlbase.h>
#include <oleacc.h>
#include <shlwapi.h>
#include <algorithm>
#include <climits>
#include <memory>
#include <string>
#include <vector>

namespace filespacer {
namespace {
// Experimental adaptation of the Windows Folder Options dialog. These resource
// and control IDs belong to shell32, not to a documented Windows UI contract.
// Check classes and containment before hiding anything; unknown layouts retain
// the unmodified native UI. Only the machine's option definitions are read;
// user settings are neither read nor written by this adapter.
constexpr UINT GENERAL_PAGE = 29959;
constexpr UINT GENERAL_PAGE_NEW = 29968;
constexpr UINT VIEW_PAGE = 29960;
constexpr UINT SEARCH_PAGE = 29961;
const wchar_t PAGE_LAYOUT[] = L"FileSpacer.FolderOptions.Layout";
const wchar_t ADVANCED_FILTER[] = L"FileSpacer.FolderOptions.Advanced";
const wchar_t PAGE_WIDTH[] = L"FileSpacer.FolderOptions.Width";
HWND optionsSheet = nullptr;
DWORD explorerProcess = 0;
std::vector<HWND> previousSheets;
bool optionsClosed = false;
LONG extraWidth = 0;
LONG targetPageWidth = 0;

bool hasClass(HWND window, const wchar_t *expected) {
    wchar_t name[64] = {};
    return window && GetClassNameW(window, name, ARRAYSIZE(name))
        && lstrcmpiW(name, expected) == 0;
}

bool contains(const RECT &outer, const RECT &inner) {
    return inner.left >= outer.left && inner.right <= outer.right
        && inner.top >= outer.top && inner.bottom <= outer.bottom;
}

std::wstring optionText(HKEY key, const wchar_t *value) {
    wchar_t text[2048] = {};
    DWORD size = sizeof(text);
    if (RegGetValueW(key, nullptr, value, RRF_RT_REG_SZ, nullptr, text, &size) != ERROR_SUCCESS)
        return {};
    return text;
}

// A group may be removed only when all its immediate options belong to the
// same setting family. Moving a navigation option into a larger group in a
// future Windows version must not hide that larger group.
bool isAdvancedGroup(HKEY key, bool navigation) {
    DWORD count = 0;
    bool hasNavigationSetting = false;
    for (DWORD index = 0;; ++index) {
        wchar_t name[256] = {};
        DWORD length = ARRAYSIZE(name);
        const LSTATUS result = RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_NO_MORE_ITEMS) return count >= 2 && (!navigation || hasNavigationSetting);
        if (result != ERROR_SUCCESS) return false;
        CRegKey child;
        if (child.Open(key, name, KEY_READ) != ERROR_SUCCESS) return false;
        const auto type = optionText(child, L"Type");
        if (navigation && type == L"checkbox") {
            // Windows 11 routes the Network and This PC options through
            // handlers instead of a direct registry destination.
            const auto handler = optionText(child, L"CLSID");
            if (lstrcmpiW(handler.c_str(), L"{726bd46b-a435-43e0-a054-1a11ad3af044}") == 0
                    || lstrcmpiW(handler.c_str(), L"{ecf2dcaa-e718-4e0c-b5eb-3be48efc4d5a}") == 0) {
                ++count;
                continue;
            }
        }
        const auto path = optionText(child, L"RegPath"), value = optionText(child, L"ValueName");
        DWORD root = 0, size = sizeof(root);
        if (RegGetValueW(child, nullptr, L"HKeyRoot", RRF_RT_REG_DWORD, nullptr, &root, &size)
                != ERROR_SUCCESS || root != 0x80000001u) return false;
        const bool advanced = lstrcmpiW(path.c_str(),
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced") == 0;
        if (!navigation) {
            if (!advanced || lstrcmpiW(value.c_str(), L"TypeAhead") != 0
                    || type != L"radio") return false;
        } else if (type != L"checkbox"
                || !((advanced && _wcsnicmp(value.c_str(), L"NavPane", 7) == 0)
                    || (lstrcmpiW(path.c_str(), L"Software\\Classes\\CLSID\\{031E4825-7B94-4dc3-B131-E946B44C8DD5}") == 0
                        && lstrcmpiW(value.c_str(), L"System.IsPinnedToNameSpaceTree") == 0))) return false;
        if (navigation && advanced && lstrcmpiW(value.c_str(), L"NavPaneExpandToCurrentFolder") == 0)
            hasNavigationSetting = true;
        ++count;
    }
}

// Match the setting's destination, not a translated label, a resource number,
// or the definition's subkey name. The definition schema is still a Shell
// implementation detail; an unknown destination remains in the native tree.
void findAdvancedLabels(HKEY key, std::vector<std::wstring> &labels, unsigned depth = 0) {
    if (depth > 8) return;
    const auto type = optionText(key, L"Type");
    const auto path = optionText(key, L"RegPath");
    const auto value = optionText(key, L"ValueName");
    wchar_t text[2048] = {};
    const auto source = optionText(key, L"Text");
    const bool hasLabel = !source.empty()
        && SUCCEEDED(SHLoadIndirectString(source.c_str(), text, ARRAYSIZE(text), nullptr));
    DWORD root = 0, size = sizeof(root);
    bool remove = hasLabel && type == L"group"
        && (isAdvancedGroup(key, false) || isAdvancedGroup(key, true));
    if (hasLabel && type == L"checkbox" && RegGetValueW(key, nullptr, L"HKeyRoot", RRF_RT_REG_DWORD,
            nullptr, &root, &size) == ERROR_SUCCESS && root == 0x80000001u) {
        const bool advanced = lstrcmpiW(path.c_str(),
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced") == 0;
        remove = (advanced && (lstrcmpiW(value.c_str(), L"ShowStatusBar") == 0
            || lstrcmpiW(value.c_str(), L"ShowPreviewHandlers") == 0
            || lstrcmpiW(value.c_str(), L"SeparateProcess") == 0
            || lstrcmpiW(value.c_str(), L"AlwaysShowMenus") == 0
            || lstrcmpiW(value.c_str(), L"AutoCheckSelect") == 0
            || lstrcmpiW(value.c_str(), L"IconsOnly") == 0
            || lstrcmpiW(value.c_str(), L"ShowSyncProviderNotifications") == 0
            || lstrcmpiW(value.c_str(), L"PersistBrowsers") == 0))
            || (lstrcmpiW(path.c_str(), L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\CabinetState") == 0
                && lstrcmpiW(value.c_str(), L"FullPath") == 0);
    }
    if (remove && *text && std::find(labels.begin(), labels.end(), text) == labels.end())
        labels.emplace_back(text);
    for (DWORD index = 0;; ++index) {
        wchar_t name[256] = {};
        DWORD length = ARRAYSIZE(name);
        const LSTATUS result = RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_NO_MORE_ITEMS) break;
        if (result != ERROR_SUCCESS) return;
        CRegKey child;
        if (child.Open(key, name, KEY_READ) == ERROR_SUCCESS)
            findAdvancedLabels(child, labels, depth + 1);
    }
}

struct AdvancedItem { HTREEITEM item; std::wstring label; };

bool readAdvancedItems(HWND tree, IAccessible *accessible, HTREEITEM item,
        std::vector<AdvancedItem> &items, unsigned depth = 0) {
    if (depth > 8) return false;
    for (; item; item = TreeView_GetNextSibling(tree, item)) {
        if (items.size() >= 512 || optionsClosed || !IsWindow(tree)) return false;
        const UINT id = TreeView_MapHTREEITEMToAccID(tree, item);
        CComBSTR name;
        if (!id || FAILED(accessible->get_accName(CComVariant(static_cast<LONG>(id)), &name))
                || !name) return false;
        items.push_back({item, std::wstring(name, name.Length())});
        if (!readAdvancedItems(tree, accessible, TreeView_GetChild(tree, item), items, depth + 1))
            return false;
    }
    return true;
}

void filterAdvancedOptions(HWND tree) {
    if (GetPropW(tree, ADVANCED_FILTER)) return;
    CRegKey definitions;
    if (definitions.Open(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", KEY_READ) != ERROR_SUCCESS) return;
    std::vector<std::wstring> labels;
    findAdvancedLabels(definitions, labels);
    if (labels.empty() || !TreeView_GetRoot(tree)) return;
    // COM can pump WinEvents. Mark this tree before crossing that boundary.
    if (!SetPropW(tree, ADVANCED_FILTER, optionsSheet)) return;
    CComPtr<IAccessible> accessible;
    std::vector<AdvancedItem> items;
    if (FAILED(AccessibleObjectFromWindow(tree, static_cast<DWORD>(OBJID_CLIENT), IID_PPV_ARGS(&accessible)))
            || !accessible || !readAdvancedItems(tree, accessible, TreeView_GetRoot(tree), items)) {
        RemovePropW(tree, ADVANCED_FILTER);
        return;
    }
    // Accessibility exposes the Windows label, but no setting/action ID. Use
    // the labels resolved above only when each identifies exactly one row.
    // Public tree messages carry handles/integers, never remote pointers or
    // private Shell data. Deleting a group also removes all of its children.
    for (const auto &label : labels) {
        HTREEITEM match = nullptr;
        size_t count = 0;
        for (const auto &item : items) if (item.label == label) { match = item.item; ++count; }
        if (count == 1 && !optionsClosed && GetAncestor(tree, GA_ROOT) == optionsSheet)
            TreeView_DeleteItem(tree, match);
    }
    if (!optionsClosed && GetAncestor(tree, GA_ROOT) == optionsSheet)
        SetPropW(tree, ADVANCED_FILTER, tree);
}

// Group boxes and their contents are siblings, not parent and children.
// Find the unique group containing both known buttons, without translated text
// or pixel coordinates. The top portion of General also contains its combobox.
void hideSection(HWND page, int firstID, int secondID, bool includeTop) {
    if (GetPropW(page, PAGE_LAYOUT)) return;
    const HWND first = GetDlgItem(page, firstID), second = GetDlgItem(page, secondID);
    RECT firstRect = {}, secondRect = {}, groupRect = {};
    if (!hasClass(first, WC_BUTTONW) || !hasClass(second, WC_BUTTONW)
            || !GetWindowRect(first, &firstRect) || !GetWindowRect(second, &secondRect)) return;
    HWND group = nullptr;
    for (HWND child = GetWindow(page, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RECT bounds = {};
        if (hasClass(child, WC_BUTTONW)
                && (static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE)) & BS_TYPEMASK) == BS_GROUPBOX
                && GetWindowRect(child, &bounds)
                && contains(bounds, firstRect) && contains(bounds, secondRect)) {
            if (group) return; // An ambiguous layout must stay native.
            group = child;
            groupRect = bounds;
        }
    }
    if (!group) return;
    RECT comboRect = {};
    if (includeTop) {
        const HWND combo = GetDlgItem(page, 30108);
        if (!hasClass(combo, WC_COMBOBOXW) || !GetWindowRect(combo, &comboRect)) return;
    }
    HWND notice = nullptr;
    bool ambiguousNotice = false;
    LONG hiddenTop = LONG_MAX, visibleTop = LONG_MAX;
    for (HWND child = GetWindow(page, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RECT bounds = {};
        if (!GetWindowRect(child, &bounds)) continue;
        if (contains(groupRect, bounds) || (includeTop && bounds.bottom <= groupRect.bottom)) {
            hiddenTop = (std::min)(hiddenTop, bounds.top);
            // Reuse the displaced opening-location label or the View page's
            // description. Both are native text controls owned by Explorer.
            if (hasClass(child, WC_STATICW)
                    && (GetWindowLongPtrW(child, GWL_STYLE) & SS_TYPEMASK) == SS_LEFT
                    && (includeTop ? bounds.right <= comboRect.left && bounds.top < comboRect.bottom
                                      && bounds.bottom > comboRect.top
                                   : GetDlgCtrlID(child) == 30127)) {
                ambiguousNotice = notice != nullptr;
                notice = child;
            }
            ShowWindow(child, SW_HIDE);
        } else if (static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE)) & WS_VISIBLE) {
            visibleTop = (std::min)(visibleTop, bounds.top);
        }
    }
    if (hiddenTop == LONG_MAX || visibleTop == LONG_MAX || visibleTop <= hiddenTop) return;
    const LONG shift = visibleTop - hiddenTop;
    const int defaultsID = includeTop ? 30107 : 30121;
    const HWND defaults = GetDlgItem(page, defaultsID);
    RECT defaultsRect = {}, pageRect = {}, noticeRect = {};
    LONG viewReserve = 0;
    if (notice && !ambiguousNotice && hasClass(defaults, WC_BUTTONW)
            && GetWindowRect(defaults, &defaultsRect) && GetClientRect(page, &pageRect)) {
        POINT bottom = {0, pageRect.bottom};
        if (ClientToScreen(page, &bottom)) {
            const LONG buttonHeight = defaultsRect.bottom - defaultsRect.top;
            const LONG margin = buttonHeight / 3;
            noticeRect = {groupRect.left, bottom.y - margin - 2 * buttonHeight,
                groupRect.right, bottom.y - margin};
            if (includeTop && defaultsRect.bottom - shift + margin > noticeRect.top)
                notice = nullptr;
            else if (!includeTop)
                viewReserve = (std::max)(0L, defaultsRect.bottom + margin - noticeRect.top);
        } else notice = nullptr;
    } else notice = nullptr;
    for (HWND child = GetWindow(page, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        if (!(static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE)) & WS_VISIBLE)) continue;
        RECT bounds = {};
        if (!GetWindowRect(child, &bounds)) continue;
        POINT origin = {bounds.left, bounds.top};
        if (!ScreenToClient(page, &origin)) continue;
        const LONG offset = !includeTop && child == defaults ? viewReserve : shift;
        const LONG addedHeight = !includeTop && GetDlgCtrlID(child) == 30120 ? shift - viewReserve : 0;
        SetWindowPos(child, nullptr, origin.x, origin.y - offset,
            bounds.right - bounds.left, bounds.bottom - bounds.top + addedHeight,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS);
    }
    SetPropW(page, PAGE_LAYOUT, page); // Tab switches must not shift the layout again.
    if (notice) {
        POINT origin = {noticeRect.left, noticeRect.top};
        const auto text = getString(IDS_FOLDER_OPTIONS_SCOPE);
        if (text && ScreenToClient(page, &origin)
                && SendMessageW(notice, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text)))
            SetWindowPos(notice, nullptr, origin.x, origin.y,
                noticeRect.right - noticeRect.left, noticeRect.bottom - noticeRect.top,
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOCOPYBITS);
    }
    // Overlapping sibling group boxes must not copy old pixels over the labels.
    // Repaint the complete page only after every control is at its final position.
    RedrawWindow(page, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

enum class WidthChange { Stretch, Shift };

void adjustChildWidth(HWND child, LONG delta, WidthChange change) {
    RECT bounds = {};
    if (!GetWindowRect(child, &bounds)) return;
    POINT origin = {bounds.left, bounds.top};
    if (!ScreenToClient(GetParent(child), &origin)) return;
    SetWindowPos(child, nullptr, origin.x + (change == WidthChange::Shift ? delta : 0), origin.y,
        bounds.right - bounds.left + (change == WidthChange::Stretch ? delta : 0), bounds.bottom - bounds.top,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS);
}

void widenPage(HWND page, bool general) {
    if (!extraWidth) return;
    // A page can be created lazily or sized again by the property sheet on
    // activation. Match the page width, and extend its child layout just once.
    RECT bounds = {};
    const bool resized = GetWindowRect(page, &bounds) && bounds.right - bounds.left != targetPageWidth;
    if (resized)
        adjustChildWidth(page, targetPageWidth - (bounds.right - bounds.left), WidthChange::Stretch);
    if (GetPropW(page, PAGE_WIDTH)) {
        if (resized) RedrawWindow(page, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        return;
    }
    for (HWND child = GetWindow(page, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        const int id = GetDlgCtrlID(child);
        if (id == (general ? 30107 : 30121) && hasClass(child, WC_BUTTONW))
            adjustChildWidth(child, extraWidth, WidthChange::Shift);
        else if ((!general && id == 30120 && hasClass(child, WC_TREEVIEWW))
                || (general && hasClass(child, WC_BUTTONW)
                    && (static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE)) & BS_TYPEMASK) == BS_GROUPBOX))
            adjustChildWidth(child, extraWidth, WidthChange::Stretch);
    }
    SetPropW(page, PAGE_WIDTH, page);
    RedrawWindow(page, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void fitAdvancedTree(HWND page, HWND tree) {
    if (extraWidth || !IsWindowVisible(tree) || GetPropW(tree, ADVANCED_FILTER) != tree) return;
    SCROLLINFO scroll = {sizeof(scroll), SIF_RANGE | SIF_PAGE};
    RECT client = {}, sheet = {}, pageBounds = {}, tabBounds = {};
    const HWND tabs = PropSheet_GetTabControl(optionsSheet);
    if (!hasClass(tabs, WC_TABCONTROLW) || !GetScrollInfo(tree, SB_HORZ, &scroll)
            || !GetClientRect(tree, &client) || scroll.nMin != 0 || !scroll.nPage
            || scroll.nPage != static_cast<UINT>(client.right) || scroll.nMax < client.right
            || !GetWindowRect(optionsSheet, &sheet) || !GetWindowRect(page, &pageBounds)
            || !GetWindowRect(tabs, &tabBounds)) return;
    MONITORINFO monitor = {sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromWindow(optionsSheet, MONITOR_DEFAULTTONEAREST), &monitor)) return;
    const LONG width = sheet.right - sheet.left;
    const LONG extra = (std::min)(scroll.nMax - client.right + 1,
        monitor.rcWork.right - monitor.rcWork.left - width);
    if (extra <= 0) return;
    const LONG left = (std::max)(monitor.rcWork.left, (std::min)(sheet.left, monitor.rcWork.right - width - extra));
    if (!SetWindowPos(optionsSheet, nullptr, left, sheet.top, width + extra, sheet.bottom - sheet.top,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS)) return;
    extraWidth = extra;
    targetPageWidth = pageBounds.right - pageBounds.left + extra;
    adjustChildWidth(tabs, extra, WidthChange::Stretch);
    for (HWND child = GetWindow(optionsSheet, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RECT button = {};
        if (hasClass(child, WC_BUTTONW) && GetWindowRect(child, &button) && button.top >= tabBounds.bottom)
            adjustChildWidth(child, extra, WidthChange::Shift);
    }
}

void adaptPage(HWND page) {
    if (GetParent(page) != optionsSheet) return;
    const bool general = GetDlgItem(page, 30100) && GetDlgItem(page, 30101);
    if (general) hideSection(page, 30100, 30101, true);
    else {
        const HWND tree = GetDlgItem(page, 30120);
        if (!hasClass(tree, WC_TREEVIEWW)) return;
        hideSection(page, 30122, 30123, false);
        filterAdvancedOptions(tree);
        fitAdvancedTree(page, tree);
    }
    if (GetPropW(page, PAGE_LAYOUT)) widenPage(page, general);
}

bool isGeneralPage(UINT_PTR id) {
    return id == GENERAL_PAGE || id == GENERAL_PAGE_NEW;
}

void removeSearchPage(HWND sheet) {
    HWND tabs = PropSheet_GetTabControl(sheet);
    if (!hasClass(tabs, WC_TABCONTROLW)) return;
    const int count = TabCtrl_GetItemCount(tabs);
    // Do not assume the third tab is Search: verify all three resource IDs.
    if (count == 3 && isGeneralPage(PropSheet_IndexToId(sheet, 0))
            && PropSheet_IndexToId(sheet, 1) == VIEW_PAGE
            && PropSheet_IndexToId(sheet, 2) == SEARCH_PAGE)
        PropSheet_RemovePage(sheet, 2, nullptr);
}

bool isOptionsSheet(HWND window) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != explorerProcess || !IsWindowVisible(window) || IsIconic(window)
            || !hasClass(window, L"#32770")) return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
            && cloaked) return false;
    HWND tabs = PropSheet_GetTabControl(window);
    if (!hasClass(tabs, WC_TABCONTROLW)) return false;
    const int count = TabCtrl_GetItemCount(tabs);
    return (count == 2 || count == 3)
        && isGeneralPage(PropSheet_IndexToId(window, 0))
        && PropSheet_IndexToId(window, 1) == VIEW_PAGE;
}

void adaptSheet(HWND window) {
    if (!isOptionsSheet(window) || (optionsSheet && optionsSheet != window)) return;
    // A previous sheet may coexist with the one opened by this request. Reuse
    // it only when Windows activates it; never take the first enumerated match.
    if (!optionsSheet && window != GetAncestor(GetForegroundWindow(), GA_ROOT)
            && std::find(previousSheets.begin(), previousSheets.end(), window) != previousSheets.end()) return;
    optionsSheet = window;
    // These messages carry integer indices/handles only, never cross-process
    // pointers. Out-of-context WinEvents run here, outside the sheet's handlers.
    removeSearchPage(window);
    const LONG previousWidth = extraWidth;
    for (HWND page = GetWindow(window, GW_CHILD); page; page = GetWindow(page, GW_HWNDNEXT))
        if (hasClass(page, L"#32770")) adaptPage(page);
    if (extraWidth != previousWidth) {
        // General may already have been visited in this pass before View told
        // us how wide the sheet needs to be.
        for (HWND page = GetWindow(window, GW_CHILD); page; page = GetWindow(page, GW_HWNDNEXT))
            if (GetPropW(page, PAGE_LAYOUT)) widenPage(page, GetDlgItem(page, 30100) != nullptr);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    SetPropW(window, L"FileSpacer.FolderOptions", window);
}

void CALLBACK optionsEvent(HWINEVENTHOOK, DWORD event, HWND window, LONG object,
        LONG child, DWORD, DWORD) {
    if (optionsClosed) return;
    if (event == EVENT_SYSTEM_FOREGROUND) {
        adaptSheet(GetAncestor(window, GA_ROOT));
        return;
    }
    if (!window || object != OBJID_WINDOW || child != CHILDID_SELF) return;
    if (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_HIDE) {
        if (window == optionsSheet
                && (event == EVENT_OBJECT_DESTROY || !IsWindowVisible(window))) {
            optionsClosed = true;
            optionsSheet = nullptr;
            PostQuitMessage(0);
        }
        return;
    }
    if (event == EVENT_OBJECT_SHOW) adaptSheet(GetAncestor(window, GA_ROOT));
}

void findOptionsSheet() {
    if (optionsSheet || optionsClosed) return;
    adaptSheet(GetAncestor(GetForegroundWindow(), GA_ROOT));
    if (optionsSheet) return;
    struct Candidate { HWND window = nullptr; bool ambiguous = false; } candidate;
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto &found = *reinterpret_cast<Candidate *>(data);
        if (isOptionsSheet(window)
                && std::find(previousSheets.begin(), previousSheets.end(), window) == previousSheets.end()) {
            if (found.window) { found.ambiguous = true; return FALSE; }
            found.window = window;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&candidate));
    if (candidate.window && !candidate.ambiguous) adaptSheet(candidate.window);
}

int showError(DWORD error) {
    auto text = getErrorMessage(error);
    MessageBoxW(nullptr, text.get(), getString(IDS_APP_NAME), MB_OK | MB_ICONERROR);
    return 1;
}
}

void openFolderOptionsDialog(HWND owner) {
    // A separate invocation confines the hook and Shell modal loop to this
    // dialog; it does not initialize FileSpacer preferences, recovery or views.
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, &path[0], static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return;
    path.resize(length);
    SHELLEXECUTEINFOW launch = {sizeof(launch)};
    launch.fMask = SEE_MASK_NOASYNC;
    launch.hwnd = owner;
    launch.lpVerb = L"open";
    launch.lpFile = path.c_str();
    launch.lpParameters = L"/folder-options";
    launch.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&launch)) {
        const DWORD error = GetLastError();
        auto text = getErrorMessage(error);
        MessageBoxW(owner, text.get(), getString(IDS_APP_NAME), MB_OK | MB_ICONERROR);
    }
}

int runFolderOptionsDialog() {
    struct Apartment {
        HRESULT status = OleInitialize(nullptr);
        ~Apartment() { if (SUCCEEDED(status)) OleUninitialize(); }
    };
    const Apartment apartment;
    if (FAILED(apartment.status)) return showError(static_cast<DWORD>(apartment.status));

    GetWindowThreadProcessId(GetShellWindow(), &explorerProcess);
    if (!explorerProcess) return showError(ERROR_NOT_FOUND);
    EnumWindows([](HWND window, LPARAM) -> BOOL {
        if (isOptionsSheet(window)) previousSheets.push_back(window);
        return TRUE;
    }, 0);
    // The Shell hosts its global Folder Options sheet in Explorer. This public
    // accessibility hook observes that process without loading any code into it.
    std::unique_ptr<std::remove_pointer<HWINEVENTHOOK>::type, decltype(&UnhookWinEvent)> hook(
        SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, nullptr, optionsEvent,
            explorerProcess, 0, WINEVENT_OUTOFCONTEXT), UnhookWinEvent);
    if (!hook) return showError(GetLastError());
    std::unique_ptr<std::remove_pointer<HWINEVENTHOOK>::type, decltype(&UnhookWinEvent)> foregroundHook(
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, optionsEvent,
            explorerProcess, 0, WINEVENT_OUTOFCONTEXT), UnhookWinEvent);
    if (!foregroundHook) return showError(GetLastError());

    wchar_t systemDirectory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory));
    if (!length || length >= ARRAYSIZE(systemDirectory)) return showError(ERROR_PATH_NOT_FOUND);
    const std::wstring executable = std::wstring(systemDirectory) + L"\\rundll32.exe";
    SHELLEXECUTEINFOW launch = {sizeof(launch)};
    launch.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    launch.lpVerb = L"open";
    launch.lpFile = executable.c_str();
    launch.lpParameters = L"shell32.dll,Options_RunDLL 7";
    launch.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&launch)) return showError(GetLastError());
    std::unique_ptr<void, decltype(&CloseHandle)> launcher(launch.hProcess, CloseHandle);
    findOptionsSheet(); // Includes an existing singleton reused by Windows.

    // Await public window events. Ten seconds is a failure watchdog for startup,
    // never a delay after which a page is presumed ready or an adaptation succeeds.
    const ULONGLONG deadline = GetTickCount64() + 10000;
    bool launcherExited = !launcher;
    for (;;) {
        if (optionsClosed) return 0;
        if (optionsSheet && !IsWindow(optionsSheet)) return 0;
        const ULONGLONG now = GetTickCount64();
        if (!optionsSheet && now >= deadline) return showError(ERROR_TIMEOUT);
        const DWORD timeout = optionsSheet ? INFINITE : static_cast<DWORD>(deadline - now);
        HANDLE process = launcher.get();
        const DWORD count = launcherExited ? 0 : 1;
        const DWORD waited = MsgWaitForMultipleObjectsEx(count, &process, timeout,
            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (waited == WAIT_FAILED) return showError(GetLastError());
        if (count && waited == WAIT_OBJECT_0) {
            launcherExited = true;
            findOptionsSheet();
        }
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return 0;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}
}
