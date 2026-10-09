#pragma once
#include <common.h>

#include <memory>
#include <Windows.h>

namespace filespacer {

#if 0 // Deferred until FileSpacer has its own public services.
struct UpdateInfo {
    DWORD version;
    bool isNewer;
    std::unique_ptr<char[]> url;
};
#endif


constexpr DWORD makeVersion(BYTE v1, BYTE v2, BYTE v3, BYTE v4) {
    return (v1 << 24) | (v2 << 16) | (v3 << 8) | v4;
}

#if 0 // Deferred until FileSpacer has its own public services.
void autoUpdateCheck();
void waitForUpdateThread();
DWORD checkUpdate(UpdateInfo *info);
void openUpdate(const UpdateInfo &info);
#endif


} // namespace
