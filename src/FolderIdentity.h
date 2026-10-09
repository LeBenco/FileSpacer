#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <string>

namespace filespacer {

// NTFS: volume + file ID. UNC/mapped drives and known non-NTFS volumes: path. Shell folders: parsing name.
std::wstring getFolderIdentity(IShellItem *item);
// A known UNC target or local non-NTFS volume allows a key before folder parsing.
std::wstring getPathFolderIdentity(const wchar_t *path);
// A bounded name for Win32 window properties and the per-folder creation mutex.
std::wstring folderWindowProperty(const std::wstring &identity);

} // namespace
