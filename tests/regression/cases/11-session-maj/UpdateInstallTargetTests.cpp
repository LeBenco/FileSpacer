// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "UpdateInstallTarget.h"
#include <atlbase.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using namespace filespacer;
namespace {
int checks = 0;
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s (%d)\n", message, checks); std::exit(1); }
}

void writeText(const std::wstring &filename, const std::wstring &text) {
    FILE *raw = nullptr;
    require(_wfopen_s(&raw, filename.c_str(), L"wb") == 0, "open fixture file");
    std::unique_ptr<FILE, decltype(&std::fclose)> file(raw, std::fclose);
    const wchar_t bom = 0xFEFF;
    require(std::fwrite(&bom, sizeof(bom), 1, file.get()) == 1
        && std::fwrite(text.data(), sizeof(wchar_t), text.size(), file.get()) == text.size(), "write UTF-16 fixture");
    require(std::fclose(file.release()) == 0, "close fixture file");
}

class RegistryFixture {
public:
    RegistryFixture() {
        path = L"Software\\FileSpacerTests\\UpdateTarget-" + std::to_wstring(GetCurrentProcessId())
            + L"-" + std::to_wstring(GetTickCount64());
        require(parent.Create(HKEY_CURRENT_USER, path.c_str()) == ERROR_SUCCESS, "create registry namespace");
        require(user.Create(parent, L"User") == ERROR_SUCCESS && machine.Create(parent, L"Machine") == ERROR_SUCCESS,
            "create isolated hives");
        require(RegOverridePredefKey(HKEY_CURRENT_USER, user) == ERROR_SUCCESS, "override HKCU in this process");
        require(RegOverridePredefKey(HKEY_LOCAL_MACHINE, machine) == ERROR_SUCCESS, "override HKLM in this process");
    }
    ~RegistryFixture() {
        require(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) == ERROR_SUCCESS
            && RegOverridePredefKey(HKEY_LOCAL_MACHINE, nullptr) == ERROR_SUCCESS, "restore predefined hives");
        user.Close(); machine.Close(); parent.Close();
        require(RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()) == ERROR_SUCCESS, "remove registry namespace");
    }
    void set(bool all, const std::wstring &directory) {
        CRegKey key;
        require(key.Create(all ? machine : user, L"Software\\FileSpacer") == ERROR_SUCCESS, "create installation registration");
        require(key.SetStringValue(L"Install_Dir", directory.c_str()) == ERROR_SUCCESS, "set installation registration");
    }
private:
    CRegKey parent, user, machine;
    std::wstring path;
};

std::wstring install(const std::wstring &directory, const wchar_t *mode) {
    require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "create installation fixture");
    const std::wstring executable = directory + L"\\FileSpacer.exe";
    writeText(executable, L"Inert fixture; never executed.");
    writeText(directory + L"\\install-info.ini", std::wstring(L"[Installation]\r\nMode=") + mode + L"\r\n");
    return executable;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
    require(argc == 2, "pass an isolated fixture directory");
    const std::wstring root = std::wstring(argv[1]) + L"\\install-targets";
    require(CreateDirectoryW(root.c_str(), nullptr) != FALSE, "create test directory");
    {
        RegistryFixture registry;
        const std::wstring userDir = root + L"\\user copy";
        const std::wstring machineDir = root + L"\\machine \u00e9 \u6587";
        const std::wstring user = install(userDir, L"CurrentUser");
        const std::wstring machine = install(machineDir, L"AllUsers");
        registry.set(false, userDir); registry.set(true, machineDir);
        require(findUpdateInstallTarget(user).scope == UpdateInstallScope::CurrentUser, "personal metadata wins");
        const std::wstring resultFile = root + L"\\result with spaces.ini";
        const std::wstring prefix = L"/UpdateResult=\"" + resultFile + L"\"";
        require(updateInstallerParameters(findUpdateInstallTarget(user), resultFile)
            == prefix + L" /CurrentUser /D=" + userDir, "personal installation parameters");
        require(findUpdateInstallTarget(machine).scope == UpdateInstallScope::AllUsers, "machine metadata wins despite HKCU");
        require(DeleteFileW((userDir + L"\\install-info.ini").c_str()) != FALSE
            && DeleteFileW((machineDir + L"\\install-info.ini").c_str()) != FALSE, "remove metadata for registry fallback");
        require(findUpdateInstallTarget(user).scope == UpdateInstallScope::CurrentUser, "exact HKCU fallback");
        require(findUpdateInstallTarget(machine).scope == UpdateInstallScope::AllUsers, "exact HKLM fallback");
        registry.set(false, machineDir + L"\\.");
        require(findUpdateInstallTarget(machine).scope == UpdateInstallScope::Unknown, "ambiguous registration stays unknown");
        writeText(machineDir + L"\\install-info.ini", L"[Installation]\r\nMode=AllUsers\r\n");
        require(findUpdateInstallTarget(machine).scope == UpdateInstallScope::AllUsers, "metadata resolves registry ambiguity");
        const auto origin = findUpdateInstallTarget(machine);
        require(updateInstallerParameters(origin, resultFile) == prefix + L" /AllUsers /D=" + machineDir,
            "preserve scope and directory; NSIS /D is last and unquoted");
        const std::wstring local = install(root + L"\\local build", L"invalid");
        require(updateInstallerParameters(findUpdateInstallTarget(local), resultFile) == prefix,
            "unregistered local build leaves installation choices to setup");
        UpdateInstallTarget result;
        require(readUpdateInstallResult(resultFile, result) == ERROR_FILE_NOT_FOUND, "missing setup result");
        writeText(resultFile, L"[Installation]\r\nMode=AllUsers\r\nExecutable=" + machine + L"\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_INVALID_DATA, "partial result rejected");
        writeText(resultFile, L"[Installation]\r\nComplete=1\r\nMode=AllUsers\r\nExecutable=" + machine + L"\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_SUCCESS && result.executable == machine,
            "explicit machine result is selected despite personal registration");
        writeText(userDir + L"\\install-info.ini", L"[Installation]\r\nMode=CurrentUser\r\n");
        writeText(resultFile, L"[Installation]\r\nComplete=1\r\nMode=CurrentUser\r\nExecutable=" + user + L"\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_SUCCESS && result.executable == user,
            "changed scope and directory follow the actual setup result");
        writeText(resultFile, L"[Installation]\r\nComplete=1\r\nMode=AllUsers\r\nExecutable=" + user + L"\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_INVALID_DATA && result.executable.empty(),
            "inconsistent scope clears the output and fails");
        writeText(resultFile, L"[Installation]\r\nComplete=1\r\nMode=AllUsers\r\nExecutable=FileSpacer.exe\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_INVALID_DATA, "relative result rejected");
        writeText(resultFile, L"[Installation]\r\nComplete=1\r\nMode=AllUsers\r\nExecutable=" + machineDir + L"\\uninstall.exe\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_INVALID_DATA, "another executable rejected");
        require(DeleteFileW(machine.c_str()) != FALSE, "remove installed fixture executable");
        writeText(resultFile, L"[Installation]\r\nComplete=1\r\nMode=AllUsers\r\nExecutable=" + machine + L"\r\n");
        require(readUpdateInstallResult(resultFile, result) == ERROR_FILE_NOT_FOUND, "missing installed executable rejected");
    }
    std::printf("PASS: %d checks; actual Win32 metadata, isolated registry fallback, setup parameters and result validation.\n", checks);
    return 0;
}
