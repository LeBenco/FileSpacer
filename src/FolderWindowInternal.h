// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#pragma once
#include <common.h>
#include <shlobj.h>

namespace filespacer {
static constexpr int BOTTOM_STATUS_HEIGHT_DIP = 23;
inline bool iconPlacementMode(UINT mode) {
    return mode == FVM_ICON || mode == FVM_SMALLICON;
}

bool gridPlacement(IFolderView *folderView);
LONG gestureRoleAt(POINT point, int *itemIndex = nullptr);
} // namespace filespacer
