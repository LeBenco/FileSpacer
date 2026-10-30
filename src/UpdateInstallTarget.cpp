// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#include <common.h>
#include "UpdateInstallTarget.h"
#include <vector>

namespace filespacer {
namespace {
std::wstring fullPath(const std::wstring &path) {
    // Require an absolute DOS/UNC path before consulting the process directory.
    const bool drive = path.size() >= 3 && path[1] == L':' && path[2] == L'\\';
    const bool unc = path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\';
    if (!drive && !unc) return {};
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()),
        buffer.data(), nullptr);
    return length && length < buffer.size() ? std::wstring(buffer.data(), length) : L"";
}

std::wstring directoryOf(const std::wstring &path) {
    const size_t slash = path.find_last_of(L'\\');
    return slash == std::wstring::npos ? L"" : path.substr(0, slash == 2 ? 3 : slash);
}

bool samePath(const std::wstring &left, const std::wstring &right) {
    return !left.empty() && !right.empty()
        && CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::wstring installationValue(const std::wstring &filename, const wchar_t *key) {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetPrivateProfileStringW(L"Installation", key, L"",
        buffer.data(), static_cast<DWORD>(buffer.size()), filename.c_str());
    return length && length < buffer.size() - 1 ? std::wstring(buffer.data(), length) : L"";
}

UpdateInstallScope scopeFromText(const std::wstring &text) {
    if (text == L"CurrentUser") return UpdateInstallScope::CurrentUser;
    if (text == L"AllUsers") return UpdateInstallScope::AllUsers;
    return UpdateInstallScope::Unknown;
}

bool registeredDirectory(HKEY root, const std::wstring &directory) {
    std::vector<wchar_t> buffer(32768);
    DWORD bytes = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    if (RegGetValueW(root, L"Software\\FileSpacer", L"Install_Dir",
            RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buffer.data(), &bytes) != ERROR_SUCCESS)
        return false;
    // Appending a separator normalizes an optional trailing slash and dot segments.
    return samePath(fullPath(std::wstring(buffer.data()) + L"\\"), fullPath(directory + L"\\"));
}
} // namespace

UpdateInstallTarget findUpdateInstallTarget(const std::wstring &executable) {
    UpdateInstallTarget target;
    target.executable = fullPath(executable);
    if (target.executable.empty()) return target;
    const std::wstring directory = directoryOf(target.executable);
    target.scope = scopeFromText(installationValue(directory + L"\\install-info.ini", L"Mode"));
    if (target.scope == UpdateInstallScope::Unknown) {
        const bool current = registeredDirectory(HKEY_CURRENT_USER, directory);
        const bool all = registeredDirectory(HKEY_LOCAL_MACHINE, directory);
        if (current != all)
            target.scope = current ? UpdateInstallScope::CurrentUser : UpdateInstallScope::AllUsers;
    }
    return target;
}

std::wstring updateInstallerParameters(const UpdateInstallTarget &origin,
        const std::wstring &resultFile) {
    // These are generated file paths, which cannot contain embedded quotes.
    std::wstring parameters = L"/UpdateResult=\"" + resultFile + L"\"";
    if (origin.scope != UpdateInstallScope::Unknown && !origin.executable.empty()) {
        parameters += origin.scope == UpdateInstallScope::AllUsers ? L" /AllUsers" : L" /CurrentUser";
        // NSIS requires /D= last and unquoted, including directories with spaces.
        parameters += L" /D=" + directoryOf(origin.executable);
    }
    return parameters;
}

DWORD readUpdateInstallResult(const std::wstring &filename, UpdateInstallTarget &target) {
    target = UpdateInstallTarget{};
    const DWORD attributes = GetFileAttributesW(filename.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY)
            || installationValue(filename, L"Complete") != L"1") return ERROR_INVALID_DATA;
    UpdateInstallTarget result;
    result.scope = scopeFromText(installationValue(filename, L"Mode"));
    const std::wstring recorded = installationValue(filename, L"Executable");
    result.executable = fullPath(recorded);
    if (result.scope == UpdateInstallScope::Unknown || result.executable.empty()) return ERROR_INVALID_DATA;
    const size_t slash = result.executable.find_last_of(L'\\');
    if (slash == std::wstring::npos
            || lstrcmpiW(result.executable.c_str() + slash + 1, L"FileSpacer.exe") != 0)
        return ERROR_INVALID_DATA;
    const DWORD executableAttributes = GetFileAttributesW(result.executable.c_str());
    if (executableAttributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if ((executableAttributes & FILE_ATTRIBUTE_DIRECTORY)
            || scopeFromText(installationValue(directoryOf(result.executable)
                + L"\\install-info.ini", L"Mode")) != result.scope) return ERROR_INVALID_DATA;
    target = result;
    return ERROR_SUCCESS;
}

} // namespace filespacer
