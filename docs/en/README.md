# Working on FileSpacer

FileSpacer is a native C++/Win32 spatial file manager. Windows supplies the file
view and file operations; FileSpacer supplies folder-window policy, persistence
and its own controls. This documentation is a starting point for navigating the
code, not an API reference. See the [user guide](user-guide.md) for usage.

## Start here

1. [Build the application](build.md) with MSVC/NMAKE.
2. [Run the affected regression tests](tests/regression/README.md), locally,
   in a [dedicated Hyper-V client](tests/client/README.md), or through the
   [GitHub Actions regression workflow](tests/regression/README.md#run-on-github).
3. Read the [Shell architecture](native-shell-view.md) before changing view
   lifetime, restoration or item identities.

## Code map

All application sources are under `src/`.

| Area | Starting points |
| --- | --- |
| Startup, process lifetime and recovery | `main.cpp`, `ProcessLifetime`, `CrashRecovery` |
| Folder windows and spatial navigation | `ItemWindow`, `FolderWindow`, `CreateItemWindow` |
| Native file view | `ShellViewHost`, `ShellViewSettings`, `ShellViewSelection`, `ShellViewIcons`, `ShellViewFilter` |
| Folder identity and saved state | `FolderIdentity`, `FolderStateStore`, `ClosedWindows` |
| Window placement and gestures | `WindowPlacement`, `WindowResizeGesture`, `WindowShortcuts.h` |
| Global preferences and translations | `Settings`, `SettingsDialog`, `ShortcutSettingsPage`, `dialog.*.rc`, `resource.*.rc` |
| Search and previews | `SearchBox`, `Everything`, `Peek` |
| Title menu and folder groups | `TitleMenu.h`, `TitleMenuDialog`, `TitleMenuRenderer`, `FolderGroups.cpp` |
| Mixed file/folder sorting | `MixedFolder`, `MixedFolderProvider` |
| Updates | `Update*` |

The build, installer, regression harness and website have separate entry points:
`Makefile` / `build.cmd`, `installer/`, `tests/`, and `website/` respectively.
New C++ translation units must be added to the relevant Makefile object list.
Keep UI text in the language resource files and preserve their encodings.

## Boundaries to preserve

- A folder has one window, including across FileSpacer processes. Identity is
  acquired before loading its Shell view. `FolderIdentity` uses native identity
  for local NTFS folders, normalized UNC paths for remote folders, paths for
  other local filesystems, and Shell parsing names for virtual folders.
- Global preferences belong in the Registry; per-folder state belongs in
  SQLite. Geometry, view settings and icon positions are independently present
  parts of a snapshot. Missing data must not erase a known part. The main
  database connection belongs to the UI thread; background readers use their
  own connections. Keep write transactions short and outside Shell/network calls.
- A filtered or not-yet-ready view is not a complete icon layout. Never save it
  as one. A locked layout protects geometry and view settings; icon positions
  continue to be saved.
- Failure to access a folder is not proof of deletion. Preserve its state unless
  deletion is confirmed or the store's retention/quota rules apply.
- Shell calls and modal menus can dispatch messages and close their owner before
  returning. A COM reference keeps the C++ object alive, not its window or view.
  Recheck lifetime and observation generation before continuing.

## Local data

Folder layouts, locks, search history and closed-folder history are stored in
`%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. General preferences, title-menu
composition and folder groups are in the Windows Registry under
`HKEY_CURRENT_USER\Software\FileSpacer`. Saved state can contain folder paths,
including network paths. Temporary update files and the session's folder list
are under `%TEMP%\FileSpacerUpdate-{GUID}`.

Use the [user guide's reset procedures](user-guide.md#local-data) for maintenance.
Do not edit a live database or reset production data to prepare regression tests.

## Focused notes

- [Search, history and Everything](search-integration.md)
- [Window gestures](window-shortcuts.md)
- [Title menu composition and commands](title-menu.md)
- [Default-manager and context-menu integration](context-menu-integration.md)
- [Website and offline help generation](website/README.md)
- [Translation maintenance](website/translations/README.md)

English is the source for technical documentation; Polish, Dutch and Ukrainian
translations are also available in the website translation directories. Document the current
behavior, constraints and useful debugging entry points. Keep run-specific
measurements, logs and validation verdicts with their test artifacts rather than
in these guides.
