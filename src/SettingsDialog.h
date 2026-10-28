#pragma once
#include <common.h>

#include <windows.h>

namespace filespacer {

enum SettingsPage {
    SETTINGS_GENERAL, SETTINGS_DISPLAY, SETTINGS_SHORTCUTS, SETTINGS_BROWSER, SETTINGS_ABOUT,
    NUM_SETTINGS_PAGES
};

void openSettingsDialog(HWND owner, SettingsPage page = SETTINGS_GENERAL);
void openDocumentation(HWND owner);
void requestSettingsDialogClose();
INT_PTR CALLBACK shortcutsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void notifyWindowSettingsChanged();
// Used by preferences and the welcome dialog.
void changeDefaultBrowser(HWND owner, bool value);

} // namespace
