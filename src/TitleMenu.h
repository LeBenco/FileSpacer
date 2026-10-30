// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#pragma once
#include <common.h>
#include <windows.h>
#include <unknwn.h>
#include "resource.h"
#include <algorithm>
#include <vector>

namespace filespacer {

// Stable identifiers persisted in the global preferences, independent of labels
// and the dynamically assigned command IDs of individual open windows.
enum TitleMenuEntry {
    TITLE_COPY_PATH, TITLE_RENAME, TITLE_STATE_LOCK, TITLE_PROPERTIES,
    TITLE_CLOSE_PARENTS, TITLE_REOPEN, TITLE_SETTINGS,
    TITLE_RESTORE_LAYOUT, TITLE_REPLACE_LAYOUT, TITLE_FORGET_LAYOUT,
    TITLE_CLOSE_DESCENDANTS, TITLE_CLOSE_OTHERS, TITLE_FIT_CONTENT, TITLE_GROUPS, TITLE_SEPARATOR,
    TITLE_OPEN_WINDOWS, TITLE_ENTRY_COUNT
};
inline std::vector<int> defaultTitleMenu() {
    return {TITLE_COPY_PATH, TITLE_RENAME, TITLE_STATE_LOCK, TITLE_PROPERTIES,
        TITLE_CLOSE_PARENTS, TITLE_REOPEN, TITLE_SEPARATOR, TITLE_OPEN_WINDOWS};
}
inline bool validTitleMenu(const std::vector<int> &items) {
    if (items.size() > 64) return false;
    bool seen[TITLE_ENTRY_COUNT] = {};
    for (int entry : items) {
        if (entry < 0 || entry >= TITLE_ENTRY_COUNT) return false;
        if (entry != TITLE_SEPARATOR && seen[entry]) return false;
        seen[entry] = true;
    }
    return true;
}
inline UINT titleMenuCommand(int entry) {
    const UINT commands[] = {IDM_CAPTION_COPY_PATH, IDM_RENAME_PROXY, IDM_TOGGLE_STATE_LOCK,
        IDM_CAPTION_PROPERTIES, IDM_CAPTION_CLOSE_PARENTS, IDM_REOPEN_WINDOW, IDM_SETTINGS,
        IDM_RESTORE_LAYOUT, IDM_REPLACE_LAYOUT, IDM_FORGET_LAYOUT, IDM_CLOSE_DESCENDANTS,
        IDM_CLOSE_OTHERS, IDM_FIT_CONTENT, IDM_FOLDER_GROUPS};
    return entry >= 0 && entry < TITLE_SEPARATOR ? commands[entry] : 0;
}
void openTitleMenuDialog(HWND owner);
void openFolderGroupsDialog(HWND owner);
bool titleMenuExtensionEnabled();
HRESULT activateTitleMenuExtension(IClassFactory **factory);
// Runs before normal application startup, on the editor's private desktop.
int renderTitleMenu(const wchar_t *directory, const wchar_t *nilesoft, POINT origin);

} // namespace filespacer
