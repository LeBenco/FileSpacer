#pragma once
#include <common.h>
#include <windows.h>
#include <string>

namespace filespacer {

enum class FolderOpenResult;
bool isUpdateSessionPath(const wchar_t *filename);
bool createUpdateWindowSession(const wchar_t *filename);
DWORD inspectUpdateWindowSession(const wchar_t *filename, bool *hasWindows);
// Main STA callback: capture only the folder windows belonging to this process.
bool saveUpdateWindows(const wchar_t *filename);
// Consume once, open each folder independently, and use its normal saved state.
FolderOpenResult restoreUpdateWindows(const wchar_t *filename);
DWORD launchUpdateSession(const std::wstring &executable, const std::wstring &filename);

} // namespace filespacer
