// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "SettingsDialog.h"
#include "UIStrings.h"
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstdlib>

namespace {
std::wstring fixtureExecutable, preferredLanguage, opened;
int errors = 0;
bool failLaunch = false, failLanguage = false;
void require(bool value, const char *description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
DWORD testModule(HMODULE, wchar_t *buffer, DWORD size) {
    require(fixtureExecutable.size() + 1 < size, "module buffer");
    wcscpy_s(buffer, size, fixtureExecutable.c_str());
    return static_cast<DWORD>(fixtureExecutable.size());
}
BOOL testLanguages(DWORD flags, ULONG *count, wchar_t *buffer, ULONG *size) {
    require(flags == MUI_LANGUAGE_NAME, "Windows display language names");
    if (failLanguage) return FALSE;
    *count = 1;
    const ULONG needed = static_cast<ULONG>(preferredLanguage.size() + 2);
    if (buffer) {
        require(*size >= needed, "language buffer");
        wcscpy_s(buffer, *size, preferredLanguage.c_str());
        buffer[preferredLanguage.size() + 1] = L'\0';
    }
    *size = needed;
    return TRUE;
}
BOOL testExecute(SHELLEXECUTEINFOW *info) {
    require((info->fMask & SEE_MASK_NOASYNC) != 0, "Shell invocation completes before returning");
    require(wcscmp(info->lpVerb, L"open") == 0, "native open verb");
    opened = info->lpFile;
    if (failLaunch) { SetLastError(ERROR_NO_ASSOCIATION); return FALSE; }
    return TRUE;
}
int testMessage(HWND, const wchar_t *message, const wchar_t *, UINT) {
    require(message && *message, "localized error message");
    ++errors;
    return IDOK;
}
void writePage(const std::wstring &file) {
    std::unique_ptr<void, decltype(&CloseHandle)> handle(
        CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);
    require(handle.get() != INVALID_HANDLE_VALUE, "create HTML fixture");
}
}

namespace filespacer {
#define GetModuleFileNameW testModule
#define GetUserPreferredUILanguages testLanguages
#define ShellExecuteExW testExecute
#define MessageBoxW testMessage
#include "DocumentationProduction.inc"
#undef GetModuleFileNameW
#undef GetUserPreferredUILanguages
#undef ShellExecuteExW
#undef MessageBoxW
}

int wmain(int argc, wchar_t **argv) {
    require(argc == 2, "isolated run directory");
    const std::wstring directory = std::wstring(argv[1]) + L"\\Documentation é # &";
    require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "isolated application directory");
    fixtureExecutable = directory + L"\\FileSpacer.exe";
    const std::wstring help = directory + L"\\help\\";
    require(CreateDirectoryW((directory + L"\\help").c_str(), nullptr) != FALSE, "help directory");
    writePage(help + L"index.html");
    for (const wchar_t *code : {L"de", L"el", L"es", L"fr", L"it", L"pt"}) {
        require(CreateDirectoryW((help + code).c_str(), nullptr) != FALSE, "translated help directory");
        writePage(help + code + L"\\index.html");
        preferredLanguage = std::wstring(code) + L"-XX";
        filespacer::openDocumentation(nullptr);
        require(opened == help + code + L"\\index.html", "regional language selects installed translation");
    }
    for (const wchar_t *code : {L"en-GB", L"en-US", L"ja-JP"}) {
        preferredLanguage = code;
        filespacer::openDocumentation(nullptr);
        require(opened == help + L"index.html", "English or unsupported language falls back to English");
    }
    require(DeleteFileW((help + L"fr\\index.html").c_str()) != FALSE, "remove translated fixture");
    preferredLanguage = L"fr-CA";
    filespacer::openDocumentation(nullptr);
    require(opened == help + L"index.html", "missing translation falls back to installed English");
    failLanguage = true;
    filespacer::openDocumentation(nullptr);
    require(opened == help + L"index.html", "language API failure falls back to English");
    failLaunch = true;
    filespacer::openDocumentation(nullptr);
    require(errors == 1, "launch failure is reported without a web fallback");
    const LANGID resources[] = {
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), MAKELANGID(LANG_FRENCH, SUBLANG_NEUTRAL),
        MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN), MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT),
        MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH), MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN),
        MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE)
    };
    for (LANGID id : resources) {
        HRSRC resource = FindResourceExW(nullptr, RT_MENU, MAKEINTRESOURCEW(IDR_ITEM_MENU), id);
        require(resource != nullptr, "translated application menu resource");
        HGLOBAL data = LoadResource(nullptr, resource);
        HMENU menu = LoadMenuIndirectW(static_cast<const MENUTEMPLATEW *>(LockResource(data)));
        require(menu != nullptr, "translated application menu");
        require(GetMenuItemID(menu, 0) == IDM_HELP && GetMenuItemID(menu, 1) == IDM_SETTINGS, "documentation immediately precedes settings");
        require(DestroyMenu(menu) != FALSE, "release test menu");
    }
    HACCEL shortcuts = LoadAcceleratorsW(nullptr, MAKEINTRESOURCEW(IDR_ITEM_ACCEL));
    const int count = CopyAcceleratorTableW(shortcuts, nullptr, 0);
    require(count > 0, "application shortcuts");
    std::vector<ACCEL> entries(static_cast<size_t>(count));
    require(CopyAcceleratorTableW(shortcuts, entries.data(), count) == count, "read application shortcuts");
    bool helpShortcut = false;
    for (const ACCEL &entry : entries) {
        if (entry.cmd == IDM_HELP && entry.key == VK_F1 && entry.fVirt == FVIRTKEY)
            helpShortcut = true;
    }
    require(helpShortcut, "F1 opens local documentation");
    std::puts("PASS: regional language selection, English fallback, local paths, launch errors and all seven menu resources. Shell launch and Windows language APIs are simulated.");
}
