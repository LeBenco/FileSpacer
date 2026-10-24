#pragma once
#include <common.h>
#include "FolderStateStore.h"
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <vector>

namespace filespacer {

enum class FolderOpenResult;
bool getClosedWindowInfo(IShellItem *item, const wchar_t *title, ClosedWindow *window);
std::wstring closedWindowName(const ClosedWindow &window);
std::wstring closedWindowPath(const ClosedWindow &window);
FolderOpenResult reopenLastWindow(HWND owner);
void updateReopenJumpLists();
void refreshReopenJumpLists();
void scheduleReopenJumpList(const std::wstring &appID);
// Pinned Quick access folders, through the Explorer namespace and property shared
// by the toolbar menu and the Jump Lists. On failure *stage names the step
// (IDS_QUICK_ACCESS_OPEN/KEY/ENUM/READ); strict fails on an unreadable entry
// instead of skipping it.
HRESULT enumerateQuickAccessFolders(std::vector<CComPtr<IShellItem>> *folders, UINT *stage,
    bool strict);

} // namespace
