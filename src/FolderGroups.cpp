// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "TitleMenu.h"
#include "Settings.h"
#include "CreateItemWindow.h"
#include "UIStrings.h"
#include "dialog.h"
#include <ExDisp.h>
#include <shlobj.h>
#include <shlguid.h>
#include <atlbase.h>
#include <algorithm>
#include <string>
#include <vector>

namespace filespacer {
namespace {
bool captureFolderGroup(std::vector<std::wstring> &paths) {
    CComPtr<IShellWindows> windows;
    if (FAILED(windows.CoCreateInstance(CLSID_ShellWindows))) return false;
    long count = 0;
    if (FAILED(windows->get_Count(&count))) return false;
    for (long i = 0; i < count; ++i) {
        CComPtr<IDispatch> dispatch;
        if (FAILED(windows->Item(CComVariant(i), &dispatch)) || !dispatch) continue;
        CComQIPtr<IWebBrowserApp> browser(dispatch);
        SHANDLE_PTR handle = 0;
        if (!browser || FAILED(browser->get_HWND(&handle))) continue;
        const HWND window = reinterpret_cast<HWND>(handle);
        wchar_t name[64] = {};
        if (!IsWindowVisible(window) || !GetClassNameW(window, name, _countof(name))
                || lstrcmpW(name, L"FileSpacer Item Window") != 0) continue;
#ifdef FILESPACER_DEBUG
        if ((GetPropW(window, L"FileSpacer.PlacementTest") != nullptr) != settings::testMode) continue;
#endif
        CComQIPtr<IServiceProvider> provider(dispatch);
        CComPtr<IShellBrowser> shellBrowser;
        CComPtr<IShellView> shellView;
        if (!provider || FAILED(provider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&shellBrowser)))
                || FAILED(shellBrowser->QueryActiveShellView(&shellView))) return false;
        CComQIPtr<IFolderView> view(shellView);
        CComPtr<IPersistFolder2> folder;
        CComHeapPtr<ITEMIDLIST_ABSOLUTE> id;
        CComHeapPtr<wchar_t> path;
        if (!view || FAILED(view->GetFolder(IID_PPV_ARGS(&folder)))
                || FAILED(folder->GetCurFolder(&id)) || !id
                || FAILED(SHGetNameFromIDList(id, SIGDN_DESKTOPABSOLUTEPARSING, &path))) return false;
        if (std::find(paths.begin(), paths.end(), path.m_pData) == paths.end()) paths.emplace_back(path.m_pData);
    }
    return !paths.empty();
}

struct GroupsDialog { HWND owner = nullptr; std::vector<settings::FolderGroup> groups; };

bool refreshGroups(HWND window, GroupsDialog &state, const wchar_t *select = nullptr) {
    if (settings::getFolderGroups(state.groups) != ERROR_SUCCESS) {
        MessageBoxW(window, getString(IDS_GROUPS_ERROR), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
        return false;
    }
    SendDlgItemMessageW(window, IDC_GROUPS_LIST, LB_RESETCONTENT, 0, 0);
    int selected = -1;
    for (size_t i = 0; i < state.groups.size(); ++i) {
        const auto &group = state.groups[i];
        SendDlgItemMessageW(window, IDC_GROUPS_LIST, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(group.name.c_str()));
        if (select && lstrcmpiW(select, group.name.c_str()) == 0) selected = static_cast<int>(i);
    }
    SendDlgItemMessageW(window, IDC_GROUPS_LIST, LB_SETCURSEL, selected, 0);
    EnableWindow(GetDlgItem(window, IDC_GROUPS_RECALL), selected >= 0);
    EnableWindow(GetDlgItem(window, IDC_GROUPS_DELETE), selected >= 0);
    return true;
}

INT_PTR CALLBACK groupsProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto state = reinterpret_cast<GroupsDialog *>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<GroupsDialog *>(lParam); SetWindowLongPtrW(window, DWLP_USER, lParam);
        SendDlgItemMessageW(window, IDC_GROUPS_NAME, EM_SETLIMITTEXT, 128, 0);
        EnableWindow(GetDlgItem(window, IDC_GROUPS_SAVE), FALSE);
        if (!refreshGroups(window, *state)) EndDialog(window, IDCANCEL);
        return TRUE;
    }
    if (!state) return FALSE;
    if (message == WM_CLOSE) { EndDialog(window, IDCANCEL); return TRUE; }
    if (message != WM_COMMAND) return FALSE;
    const int command = LOWORD(wParam);
    const LRESULT selected = SendDlgItemMessageW(window, IDC_GROUPS_LIST, LB_GETCURSEL, 0, 0);
    if (command == IDCANCEL) { EndDialog(window, IDCANCEL); return TRUE; }
    if (command == IDC_GROUPS_NAME && HIWORD(wParam) == EN_CHANGE) {
        EnableWindow(GetDlgItem(window, IDC_GROUPS_SAVE), GetWindowTextLengthW(GetDlgItem(window, IDC_GROUPS_NAME)) > 0);
        return TRUE;
    }
    if (command == IDC_GROUPS_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
        const bool valid = selected >= 0 && static_cast<size_t>(selected) < state->groups.size();
        EnableWindow(GetDlgItem(window, IDC_GROUPS_RECALL), valid);
        EnableWindow(GetDlgItem(window, IDC_GROUPS_DELETE), valid);
        if (valid) SetDlgItemTextW(window, IDC_GROUPS_NAME, state->groups[static_cast<size_t>(selected)].name.c_str());
        return TRUE;
    }
    if (command == IDC_GROUPS_SAVE) {
        wchar_t name[129] = {}; GetDlgItemTextW(window, IDC_GROUPS_NAME, name, _countof(name));
        settings::FolderGroup group; group.name = name;
        const size_t first = group.name.find_first_not_of(L" \t");
        if (first == std::wstring::npos) return TRUE;
        group.name = group.name.substr(first, group.name.find_last_not_of(L" \t") - first + 1);
        const bool replacing = std::any_of(state->groups.begin(), state->groups.end(),
            [&](const settings::FolderGroup &old) { return lstrcmpiW(old.name.c_str(), group.name.c_str()) == 0; });
        if (replacing && MessageBoxW(window, getString(IDS_GROUPS_REPLACE), getString(IDS_APP_NAME),
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return TRUE;
        if (!IsWindow(state->owner)) { EndDialog(window, IDCANCEL); return TRUE; }
        if (!captureFolderGroup(group.paths) || !IsWindow(state->owner)
                || settings::setFolderGroup(group) != ERROR_SUCCESS) {
            if (IsWindow(window)) MessageBoxW(window, getString(IDS_GROUPS_ERROR), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
            return TRUE;
        }
        refreshGroups(window, *state, group.name.c_str());
        return TRUE;
    }
    if (selected < 0 || static_cast<size_t>(selected) >= state->groups.size()) return FALSE;
    const auto group = state->groups[static_cast<size_t>(selected)];
    if (command == IDC_GROUPS_DELETE) {
        if (MessageBoxW(window, getString(IDS_GROUPS_DELETE_CONFIRM), getString(IDS_APP_NAME),
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return TRUE;
        if (settings::removeFolderGroup(group.name) != ERROR_SUCCESS)
            MessageBoxW(window, getString(IDS_GROUPS_ERROR), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
        else { SetDlgItemTextW(window, IDC_GROUPS_NAME, L""); refreshGroups(window, *state); }
        return TRUE;
    }
    if (command == IDC_GROUPS_RECALL || (command == IDC_GROUPS_LIST && HIWORD(wParam) == LBN_DBLCLK)) {
        // Leave the modal loop before opening/activating the folders of the group.
        EndDialog(window, IDC_GROUPS_RECALL);
        // The caller consumes the selected group after DialogBox returns.
        state->groups.clear(); state->groups.push_back(group);
        return TRUE;
    }
    return FALSE;
}
} // namespace

void openFolderGroupsDialog(HWND owner) {
    GroupsDialog state; state.owner = owner;
    const INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_FOLDER_GROUPS),
        owner, groupsProc, reinterpret_cast<LPARAM>(&state));
    if (result != IDC_GROUPS_RECALL || state.groups.empty()) return;
    bool failed = false;
    for (const auto &path : state.groups[0].paths) {
        CComPtr<IShellItem> item;
        if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item)))) { failed = true; continue; }
        if (openFolderWindow(item, nullptr, SW_SHOWNORMAL) == FolderOpenResult::Failed) failed = true;
    }
    if (failed) MessageBoxW(IsWindow(owner) ? owner : nullptr, getString(IDS_GROUPS_OPEN_ERROR),
        getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
}
} // namespace filespacer
