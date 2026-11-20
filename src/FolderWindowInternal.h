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
// The view presets, in the order of the View menu and of the Ctrl+Shift shortcuts.
struct ViewPreset { FOLDERVIEWMODE mode = FVM_AUTO; int size = -1; };
static constexpr ViewPreset VIEW_PRESETS[] = {
    {FVM_ICON, 256}, {FVM_ICON, 96}, {FVM_ICON, 48}, {FVM_SMALLICON, 16},
    {FVM_LIST, -1}, {FVM_DETAILS, -1}, {FVM_TILE, -1}, {FVM_CONTENT, -1}
};

bool gridPlacement(IFolderView *folderView);
LONG gestureRoleAt(POINT point, int *itemIndex = nullptr);
} // namespace filespacer
