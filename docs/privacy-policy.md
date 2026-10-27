# Privacy policy

FileSpacer does not include telemetry, analytics, or advertising. It does not
automatically send usage information or crash reports to the developer.

## Update checks

Manual update checks, and automatic weekly checks if enabled, request the latest
FileSpacer version manifest from GitHub over HTTPS. Automatic checks are off by
default. GitHub and its download infrastructure receive the usual connection
information, including your IP address and a FileSpacer version user agent. The
request does not contain file paths, folder state, or diagnostic reports.
When an installable update is available, FileSpacer asks for confirmation before
downloading its installer from GitHub over HTTPS. It checks the download's size
and SHA-256 against the release manifest before starting setup. The installer
requires your interaction; updates are never installed automatically. Releases
without installer metadata open their release page in your default browser.

## Local data

Global preferences are stored in the current user's Windows Registry under
`HKEY_CURRENT_USER\Software\FileSpacer`. Folder window positions, sizes, view
settings, icon positions, saved-state locks, recent searches and closed-folder
history are stored in a SQLite database at
`%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. SQLite may create associated
journal files in the same directory. Saved state can include folder identities
and paths, including network paths.

An update temporarily stores its installer, a copy of the update launcher and a
SQLite list of open folder paths under `%TEMP%\FileSpacerUpdate-{GUID}`. This
local list is used to reopen folders after installation or cancellation; it is
not uploaded.

Crash protection may save diagnostic text reports and minidumps in
`%LOCALAPPDATA%\FileSpacer`. These stay on the computer unless the user chooses
to share them. Diagnostics can contain file paths, process details, and portions
of process memory. The same folder contains a settings lock file.

Debug builds can use separate test preferences and local test data with `/test`.

## Windows and external applications

Opening network folders, resolving Shell items, displaying previews or thumbnails
provided by Windows, and invoking context-menu extensions use Windows and any
installed Shell components. Those components and remote servers may access the
network according to their own behavior and policies.

Files are opened with their associated application, or with the application
picker provided by Windows. Their handling by those applications is outside
FileSpacer's control.

If Everything integration is enabled, choosing its search action passes the
visible query to the separately installed Everything application. Folder-scoped
searches also pass the current folder path. Local filtering does not launch
Everything; the external application's index and settings control its results.

## Resetting preferences

`FileSpacer.exe /reset-options` resets global preferences.
`FileSpacer.exe /reset-all` also clears saved folder state. Neither command deletes
the user's files or changes Windows file associations. These commands do not
remove previously saved diagnostic reports.
