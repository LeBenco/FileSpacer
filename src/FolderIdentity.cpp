#include "FolderIdentity.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <winnetwk.h>
#include <bcrypt.h>
#include <vector>
#include <cwchar>
#include <utility>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Mpr.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace filespacer {

static std::wstring hexBytes(const BYTE *bytes, size_t size) {
    const wchar_t digits[] = L"0123456789abcdef";
    std::wstring result;
    result.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}

static std::wstring normalizedPath(std::wstring path) {
    if (_wcsnicmp(path.c_str(), L"\\\\?\\UNC\\", 8) == 0)
        path = L"\\\\" + path.substr(8);
    else if (path.compare(0, 4, L"\\\\?\\") == 0)
        path.erase(0, 4);
    DWORD capacity = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (capacity) {
        std::wstring absolute(capacity, L'\0');
        DWORD length = GetFullPathNameW(path.c_str(), capacity, &absolute[0], nullptr);
        if (length && length < capacity) { absolute.resize(length); path = std::move(absolute); }
    }
    while (path.size() > 3 && path.back() == L'\\')
        path.pop_back();
    int size = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
        path.c_str(), -1, nullptr, 0, nullptr, nullptr, 0);
    if (size) {
        std::wstring upper(static_cast<size_t>(size), L'\0');
        if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
                path.c_str(), -1, &upper[0], size, nullptr, nullptr, 0)) {
            upper.pop_back();
            return upper;
        }
    }
    return path;
}

// Resolve mappings from Windows connection metadata, without probing the remote folder.
static std::wstring networkPath(std::wstring path, bool *remote) {
    if (_wcsnicmp(path.c_str(), L"\\\\?\\UNC\\", 8) == 0)
        path = L"\\\\" + path.substr(8);
    else if (path.compare(0, 4, L"\\\\?\\") == 0)
        path.erase(0, 4);
    *remote = PathIsUNCW(path.c_str()) != FALSE;
    if (*remote) return path;
    if (path.size() < 3 || path[1] != L':' || path[2] != L'\\') return {};
    wchar_t root[] = {path[0], L':', L'\\', 0};
    UINT type = GetDriveTypeW(root);
    if (type != DRIVE_REMOTE && type != DRIVE_UNKNOWN && type != DRIVE_NO_ROOT_DIR)
        return {}; // A known local volume cannot be a disconnected network mapping.
    wchar_t device[] = {path[0], L':', 0};
    DWORD size = 256;
    std::vector<wchar_t> name(size);
    DWORD status = WNetGetConnectionW(device, name.data(), &size);
    if (status == ERROR_MORE_DATA) {
        name.resize(size);
        status = WNetGetConnectionW(device, name.data(), &size);
    }
    if (status == NO_ERROR) {
        *remote = true;
        return std::wstring(name.data()) + path.substr(2);
    }
    *remote = true; // Unknown/disconnected drives must never fall back to a local path key.
    HANDLE enumeration = nullptr;
    if (WNetOpenEnumW(RESOURCE_REMEMBERED, RESOURCETYPE_DISK, 0, nullptr, &enumeration) != NO_ERROR)
        return {};
    std::wstring target;
    std::vector<BYTE> buffer(16384);
    while (target.empty()) {
        DWORD count = MAXDWORD;
        DWORD bytes = static_cast<DWORD>(buffer.size());
        status = WNetEnumResourceW(enumeration, &count, buffer.data(), &bytes);
        if (status == ERROR_MORE_DATA) { buffer.resize(bytes); continue; }
        if (status != NO_ERROR || count == 0) break;
        const auto resources = reinterpret_cast<const NETRESOURCEW *>(buffer.data());
        for (DWORD i = 0; i < count; ++i) {
            if (resources[i].lpLocalName && resources[i].lpRemoteName
                    && lstrcmpiW(resources[i].lpLocalName, device) == 0) {
                target = std::wstring(resources[i].lpRemoteName) + path.substr(2);
                break;
            }
        }
    }
    WNetCloseEnum(enumeration);
    return target;
}

std::wstring getPathFolderIdentity(const wchar_t *path) {
    if (!path || !*path) return {};
    bool remote = false;
    const std::wstring unc = networkPath(path, &remote);
    if (remote) return unc.empty() ? std::wstring{} : L"path:" + normalizedPath(unc);
    // Known non-NTFS volumes use paths, including FAT/exFAT, even if the folder is missing.
    // Identify the local volume, never substitute this key for a failed NTFS ID lookup.
    std::wstring local(path);
    if (local.compare(0, 4, L"\\\\?\\") == 0) local.erase(0, 4);
    if (local.size() < 3 || local[1] != L':' || local[2] != L'\\') return {};
    wchar_t root[] = {local[0], L':', L'\\', 0};
    wchar_t filesystem[32] = {};
    if (!GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, nullptr,
            filesystem, ARRAYSIZE(filesystem))) return {};
    if (lstrcmpiW(filesystem, L"NTFS") == 0) return {};
    return L"path:" + normalizedPath(path);
}

std::wstring getFolderIdentity(IShellItem *item) {
    if (!item)
        return {};
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHGetIDListFromObject(item, &pidl))) {
        bool desktop = ILIsEmpty(pidl);
        CoTaskMemFree(pidl);
        // The Shell desktop is not the physical Desktop directory.
        if (desktop)
            return L"shell:desktop";
    }
    PWSTR name = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name))) {
        std::wstring path(name);
        CoTaskMemFree(name);
        bool remote = false;
        const std::wstring unc = networkPath(path, &remote);
        // A remote server's filesystem type does not change its path-based identity.
        if (remote)
            return unc.empty() ? std::wstring{} : L"path:" + normalizedPath(unc);
        std::wstring handlePath = path;
        if (path.size() >= MAX_PATH && path.size() >= 3 && path[1] == L':' && path[2] == L'\\')
            handlePath = L"\\\\?\\" + path;
        HANDLE file = CreateFileW(handlePath.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (file == INVALID_HANDLE_VALUE) return {};
        wchar_t filesystem[32] = {};
        if (!GetVolumeInformationByHandleW(file, nullptr, 0,
                nullptr, nullptr, nullptr, filesystem, ARRAYSIZE(filesystem))) {
            CloseHandle(file);
            return {};
        }
        if (lstrcmpiW(filesystem, L"NTFS") == 0) {
            FILE_ID_INFO info = {};
            bool identified = GetFileInformationByHandleEx(file, FileIdInfo, &info, sizeof(info)) != FALSE;
            CloseHandle(file);
            if (!identified) return {}; // Never substitute a path for an unavailable NTFS ID.
            wchar_t volume[32] = {};
            swprintf_s(volume, ARRAYSIZE(volume), L"%016llx", info.VolumeSerialNumber);
            return L"ntfs:" + std::wstring(volume) + L":"
                + hexBytes(info.FileId.Identifier, sizeof(info.FileId.Identifier));
        }
        CloseHandle(file);
        // Preserve the existing path strategy for known non-NTFS volumes, including FAT/exFAT.
        return L"path:" + normalizedPath(path);
    }

    if (SUCCEEDED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &name))) {
        std::wstring identity = L"shell:" + std::wstring(name);
        CoTaskMemFree(name);
        return identity;
    }
    return {};
}

std::wstring folderWindowProperty(const std::wstring &identity) {
    if (identity.empty())
        return {};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BYTE digest[32] = {};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0)
        return {};
    status = BCryptHash(algorithm, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(identity.data())),
        static_cast<ULONG>(identity.size() * sizeof(wchar_t)), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return status >= 0 ? L"FileSpacer.Folder." + hexBytes(digest, sizeof(digest)) : std::wstring();
}

} // namespace
