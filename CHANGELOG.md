# Changelog

## 1.0.8 — 2026-10-22

- Move folder windows with Alt + left drag and resize them with Alt + right drag from the main file area, preserving the file selection.
- Keep the classic Shell view enabled independently of the selected-item full-name display preference.
- Apply Nilesoft Shell theming to the view-mode menu.
- Avoid long shutdown delays caused by passive Shell icon caches while retaining the lifetime references acquired by explicitly invoked Shell commands.
- Isolate Shell view hosting and extend regression coverage for Alt gestures, menu reentrancy and Shell/STA lifetime.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Select the originating child folder when opening a parent or ancestor, preserving selection in already-open windows.
- Fix folder name filtering when submitting, changing or clearing a query.
- Restore the native Windows Quick access icon and center the Quick access and configuration buttons.
- Remove temporary diagnostics and use Benco in project credits.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Automatically restore open folder windows after an in-app update, preserving their saved positions, sizes and view settings.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Check for updates asynchronously from the About page, with an adaptive installation button and status text.
- Download updates in a separate cancellable progress dialog while folder windows remain usable.
- Verify installer size and SHA-256 before gracefully closing FileSpacer and starting setup.
- Publish installer download metadata alongside each release.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Add a title-bar command to close open parent-folder windows.
- Reopen closed folders from the title-bar menu, Ctrl+Shift+T or taskbar Jump Lists, restoring their saved view, selection and scroll position.
- Add Explorer Quick access folders with native Shell icons to taskbar Jump Lists.
- Show the reopening command only when the last closed folder is not already open, using its name and native icon.
- Exclude the current window from the title-bar window-switching list.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Restore manual update checks and optional weekly checks using FileSpacer GitHub releases, disabled by default.

- Publish the update manifest alongside each release installer.

- Preserve native Shell handling of Alt+double-click and Alt+Enter on folders.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Apply Nilesoft Shell themes to the title-bar folder menu independently of file selection, preserving native Shell commands.
- Allow separate Nilesoft customization of the title-bar and toolbar menus.
- Center the title-bar drop-down arrow.
- Document Nilesoft Shell configuration, theme application and menu filtering.
- Credit Benco in copyright notices.
- Allow the release workflow to replace the installer of an existing release.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Enable native minimize and maximize controls and fix caption buttons in maximized windows.
- Save and restore maximized state separately from normal window geometry, with automatic database migration.
- Use native taskbar animations and per-folder taskbar relaunch properties.
- Apply taskbar grouping preferences to newly created windows and clarify this behavior in Settings.
- Allow deletion of open folders and remove their saved state.
- Stop the application and its recovery helper from locking folders through their working directory.
- Preserve absolute folder paths and debug test mode during crash recovery.
- Restore normal filename labels immediately after deselection when full-name display is enabled.
- Release cached Shell interfaces when folder windows close and correct Shell notification handling.
- Support manual release publication and include the version in installer filenames.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

## 1.0.0 — 2026-10-09

- Initial FileSpacer release for Windows 10 or later, x64, based on ChromaFiler.
- Provide spatial navigation with one window per folder and persistent window position, size, view settings and icon positions stored in SQLite.
- Follow folder moves and renames and activate an existing window when its folder is opened again.
- Close the source window by default when opening a folder or its parent, with a configurable default and Ctrl to reverse the behavior.
- Add a breadcrumb and editable path bar, plus a folder name filter with optional filtering while typing.
- Provide configurable toolbar elements, a bottom status bar and taskbar grouping preferences.
- Add optional experimental full-name display for selected items.
- Provide English and French interfaces, with English as the fallback.
- Add crash recovery and maintenance commands to reset preferences or saved folder state.
- Remove Miller-column navigation, the built-in text editor, preview windows and the floating tray.
- Provide MSVC/NMAKE build scripts, an NSIS installer, automated GitHub releases and project documentation.

[Release history](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)

