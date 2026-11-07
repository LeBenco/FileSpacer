// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
#pragma once
#include <common.h>
#include <windows.h>
#include <unknwn.h>
#include "resource.h"
#include <algorithm>
#include <string>
#include <vector>

namespace filespacer {

// Stable identifiers persisted in the global preferences, independent of labels
// and the dynamically assigned command IDs of individual open windows.
enum TitleMenuEntry {
    TITLE_COPY_PATH, TITLE_RENAME, TITLE_STATE_LOCK, TITLE_PROPERTIES,
    TITLE_CLOSE_PARENTS, TITLE_REOPEN, TITLE_SETTINGS,
    TITLE_RESTORE_LAYOUT, TITLE_REPLACE_LAYOUT,
    // Slot 9 is retired; keep the remaining persisted identifiers stable.
    TITLE_CLOSE_DESCENDANTS = 10, TITLE_CLOSE_OTHERS, TITLE_FIT_CONTENT, TITLE_GROUPS, TITLE_SEPARATOR,
    TITLE_OPEN_WINDOWS, TITLE_ENTRY_COUNT
};
inline std::vector<int> defaultTitleMenu() {
    return {TITLE_COPY_PATH, TITLE_RENAME, TITLE_STATE_LOCK, TITLE_PROPERTIES,
        TITLE_CLOSE_PARENTS, TITLE_REOPEN, TITLE_SEPARATOR, TITLE_OPEN_WINDOWS};
}
inline bool validTitleMenuEntry(int entry) {
    return entry >= 0 && entry < TITLE_ENTRY_COUNT
        && (entry <= TITLE_REPLACE_LAYOUT || entry >= TITLE_CLOSE_DESCENDANTS);
}
inline bool validTitleMenu(const std::vector<int> &items) {
    if (items.size() > 64) return false;
    bool seen[TITLE_ENTRY_COUNT] = {};
    for (int entry : items) {
        if (!validTitleMenuEntry(entry)) return false;
        if (entry != TITLE_SEPARATOR && seen[entry]) return false;
        seen[entry] = true;
    }
    return true;
}
inline UINT titleMenuCommand(int entry) {
    const UINT commands[] = {IDM_CAPTION_COPY_PATH, IDM_RENAME_PROXY, IDM_TOGGLE_STATE_LOCK,
        IDM_CAPTION_PROPERTIES, IDM_CAPTION_CLOSE_PARENTS, IDM_REOPEN_WINDOW, IDM_SETTINGS,
        IDM_RESTORE_LAYOUT, IDM_REPLACE_LAYOUT, 0, IDM_CLOSE_DESCENDANTS,
        IDM_CLOSE_OTHERS, IDM_FIT_CONTENT, IDM_FOLDER_GROUPS};
    return entry >= 0 && entry < TITLE_SEPARATOR ? commands[entry] : 0;
}
void openTitleMenuDialog(HWND owner);
void openFolderGroupsDialog(HWND owner);
bool titleMenuExtensionEnabled();
// Reads the Nilesoft Shell handler registration: its class ID and, when
// requested, its InprocServer32 module path. Either output may be null.
HRESULT findTitleMenuExtension(CLSID *clsid, std::wstring *module);
HRESULT activateTitleMenuExtension(IClassFactory **factory);
// Runs before normal application startup, on the editor's private desktop.
int renderTitleMenu(const wchar_t *directory, const wchar_t *nilesoft, POINT origin);

} // namespace filespacer
