#pragma once
#include <common.h>

#include "ItemWindow.h"
#include <ExDisp.h>

namespace filespacer {

enum class FolderOpenResult { Failed, Created, Activated, Pending };
// The only creation path for folder windows; item must have SFGAO_FOLDER.
FolderOpenResult openFolderWindow(IShellItem *item, HMONITOR monitor, int showCmd);
CComPtr<IShellItem> resolveLink(IShellItem *linkItem);
// A failed access has a usable result; navigation callbacks alone do not.
bool isFolderAccessFailure(HRESULT result);
void recordFolderPathFailure(const wchar_t *path, HRESULT result);
void recordFolderItemFailure(IShellItem *item, HRESULT result);
// displays error message if item can't be found
CComPtr<IShellItem> itemFromPath(wchar_t *path);

void debugDisplayNames(HWND hwnd, IShellItem *item);

} // namespace
