#pragma once
#include <common.h>
#include "FolderStateStore.h"
#include <windows.h>
#include <shobjidl.h>

namespace filespacer {

enum class FolderOpenResult;
bool getClosedWindowInfo(IShellItem *item, const wchar_t *title, ClosedWindow *window);
std::wstring closedWindowName(const ClosedWindow &window);
std::wstring closedWindowPath(const ClosedWindow &window);
FolderOpenResult reopenLastWindow(HWND owner);
void updateReopenJumpLists();
void refreshReopenJumpLists();
void scheduleReopenJumpList(const std::wstring &appID);

} // namespace
