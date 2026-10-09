#pragma once
#include <common.h>

#include <windows.h>

namespace filespacer {

enum SettingsPage {
    SETTINGS_GENERAL, SETTINGS_DISPLAY, SETTINGS_BROWSER, SETTINGS_ABOUT,
    NUM_SETTINGS_PAGES
};

void openSettingsDialog(HWND owner, SettingsPage page = SETTINGS_GENERAL);
// Used by preferences and the welcome dialog.
void changeDefaultBrowser(HWND owner, bool value);

} // namespace
