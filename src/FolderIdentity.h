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
// The window property is an ASCII prefix followed by a hexadecimal digest: the
// same characters form the persisted folder-state key.
inline std::string folderStateKey(const std::wstring &property) {
    std::string key;
    for (wchar_t character : property) key.push_back(static_cast<char>(character));
    return key;
}

} // namespace
