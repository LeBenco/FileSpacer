<a id="working-on-filespacer"></a>

# Working on FileSpacer

FileSpacer is a native C++/Win32 spatial file manager. Windows supplies the file
view and file operations; FileSpacer supplies folder-window policy, persistence
and its own controls. This documentation is a starting point for navigating the
code, not an API reference. See the [user guide](user-guide.md) for usage.

<a id="start-here"></a>

## Start here

1. [Build the application](build.md) with MSVC/NMAKE.
2. [Run the affected regression tests](tests/regression/README.md), locally,
   in a [dedicated Hyper-V client](tests/client/README.md), or through the
   [GitHub Actions regression workflow](tests/regression/README.md#run-on-github).
3. Read the [Shell architecture](native-shell-view.md) before changing view
   lifetime, restoration or item identities.

<a id="code-map"></a>

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
`FolderWindow` remains one class. Its member definitions are grouped into
`FolderWindowIconSnap`, `FolderWindowInput`, `FolderWindowMenus`,
`FolderWindowViewMenu`, `FolderWindowViewState` and `FolderWindowProvisional`.
Lifecycle coordination and COM entry points stay in `FolderWindow.cpp`.
`FolderWindowInternal.h` shares only the small cross-unit helpers; the private
view-menu preview declaration lives in `FolderWindowViewMenuPreview.h`.

New C++ translation units must be added to the relevant Makefile object list.
Keep UI text in the language resource files and preserve their encodings.

<a id="boundaries-to-preserve"></a>

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

<a id="local-data"></a>

## Local data

Folder layouts, locks, search history and closed-folder history are stored in
`%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. General preferences, title-menu
composition and folder groups are in the Windows Registry under
`HKEY_CURRENT_USER\Software\FileSpacer`. Saved state can contain folder paths,
including network paths. Temporary update files and the session's folder list
are under `%TEMP%\FileSpacerUpdate-{GUID}`.

The database format is recorded in SQLite's `PRAGMA user_version`, independently
of the application release number. Format 8 is written. View blobs have their
own encoding version, currently 8; a database schema change does not implicitly
change their encoding, but a new encoding requires a new database format. In
format 8, encodings 1, 2 and 3 remain readable, and a view that no encoding can
read counts as absent while the rest of that folder's state is read. Empty
databases are initialized from scratch. Format-7
databases are migrated at runtime inside the opening write transaction: legacy
view encodings 1, 2 and 3 are decoded and rewritten, then `user_version` is
updated. All other stored state, including opaque closed-window snapshots and
raw icon sizes, is preserved. A view that none of these encodings can read is
left out and the rest of that folder's state is kept. Any migration error rolls
back the whole change.

Non-empty unversioned databases and formats 6 and earlier are refused without
conversion. Explicit rebuilding requires confirmation and a complete backup;
the confirmation shows the backup path and explains possible data loss. Recovery
salvages compatible readable data from formats 7 and 8 without changing the
source backup. A bad view can be omitted while the rest of its folder state is
kept. Newer formats are refused without offering a reset. There is no installer
migration or downgrade path. General preferences and user files are unaffected
by rebuilding folder state.

For the in-app reset, see [updates and troubleshooting](user-guide.md#updates-and-troubleshooting).
For troubleshooting, `FileSpacer.exe /reset-options` resets general preferences.
`FileSpacer.exe /reset-all` also clears folder state. Neither command deletes files.
Do not edit a live database or reset production data to prepare regression tests.

<a id="focused-notes"></a>

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
