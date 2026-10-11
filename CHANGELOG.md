# Changelog

## 1.0.2 — 2026-10-11

- Apply Nilesoft Shell themes to the title-bar folder menu independently of file selection, preserving native Shell commands.
- Allow separate Nilesoft customization of the title-bar and toolbar menus.
- Center the title-bar drop-down arrow.
- Document Nilesoft Shell configuration, theme application and menu filtering.
- Credit Benjamin Cohen Boulakia in copyright notices.
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
