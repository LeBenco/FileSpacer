#pragma once
#include <common.h>
#include <windows.h>
#include <string>

namespace filespacer {

// Empty means no installed executable was found. A manual path takes precedence.
std::wstring findEverything();
std::wstring everythingArguments(const std::wstring &query, const std::wstring &folder);
DWORD launchEverything(const std::wstring &executable, const std::wstring &arguments);
// Reads only resources; the caller owns the returned icon and must DestroyIcon it.
HICON extractEverythingIcon(const std::wstring &executable, int pixels);

} // namespace
