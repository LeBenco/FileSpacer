#pragma once
#include <common.h>

#include "UpdateManifest.h"
#include <atomic>
#include <Windows.h>

namespace filespacer {

struct UpdateInfo : ReleaseManifest {
    bool isNewer = false;
};

constexpr DWORD makeVersion(BYTE v1, BYTE v2, BYTE v3, BYTE v4) {
    return (static_cast<DWORD>(v1) << 24) | (static_cast<DWORD>(v2) << 16)
        | (static_cast<DWORD>(v3) << 8) | v4;
}

void autoUpdateCheck();
void waitForUpdateThread();
void stopUpdateChecks();
void resumeUpdateChecks();
DWORD checkUpdate(UpdateInfo *info, const std::atomic_bool *cancel = nullptr);
void openUpdate(const UpdateInfo &info);

} // namespace
