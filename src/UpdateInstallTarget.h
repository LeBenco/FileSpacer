// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#pragma once
#include <common.h>
#include <windows.h>
#include <string>

namespace filespacer {

enum class UpdateInstallScope { Unknown, CurrentUser, AllUsers };
struct UpdateInstallTarget {
    std::wstring executable;
    UpdateInstallScope scope = UpdateInstallScope::Unknown;
};

UpdateInstallTarget findUpdateInstallTarget(const std::wstring &executable);
std::wstring updateInstallerParameters(const UpdateInstallTarget &origin,
    const std::wstring &resultFile);
DWORD readUpdateInstallResult(const std::wstring &filename, UpdateInstallTarget &target);

} // namespace filespacer
