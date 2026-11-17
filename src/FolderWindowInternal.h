// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#pragma once
#include <common.h>
#include <shlobj.h>

namespace filespacer {
inline bool iconPlacementMode(UINT mode) {
    return mode == FVM_ICON || mode == FVM_SMALLICON || mode == FVM_TILE;
}

LONG gestureRoleAt(POINT point, int *itemIndex = nullptr);
} // namespace filespacer
