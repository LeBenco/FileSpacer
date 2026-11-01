#pragma once
#include <common.h>
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>

namespace filespacer {

struct PeekInstallation {
    std::wstring executable;
    bool multipleSelection = false;
};

PeekInstallation findPeek();
bool powerToysPeekEnabled();
DWORD openPeekSettings();
std::vector<std::wstring> peekPaths(IShellItemArray *selection);
DWORD launchPeek(const PeekInstallation &installation, const std::vector<std::wstring> &paths);

} // namespace filespacer
