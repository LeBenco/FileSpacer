# Privacy policy

FileSpacer does not include telemetry, analytics, advertising, or an active
update-checking service. It does not automatically send usage information or
crash reports to the developer.

## Local data

Global preferences are stored in the current user's Windows Registry under
`HKEY_CURRENT_USER\Software\FileSpacer`. Folder window positions, sizes, view
settings, and icon positions are stored in a SQLite database at
`%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. SQLite may create associated
journal files in the same directory. Saved state can include folder identities
and paths, including network paths.

Crash protection may save diagnostic text reports and minidumps in
`%LOCALAPPDATA%\FileSpacer`. These stay on the computer unless the user chooses
to share them. Diagnostics can contain file paths, process details, and portions
of process memory. The same folder contains a settings lock file. Recovery
counters are stored in the current user's Registry.

Debug builds can use separate test preferences and local test data with `/test`.

## Windows and external applications

Opening network folders, resolving Shell items, displaying previews or thumbnails
provided by Windows, and invoking context-menu extensions use Windows and any
installed Shell components. Those components and remote servers may access the
network according to their own behavior and policies.

Files are opened with their associated application, or with the application
picker provided by Windows. Their handling by those applications is outside
FileSpacer's control.

## Resetting preferences

`FileSpacer.exe /reset-options` resets global preferences.
`FileSpacer.exe /reset-all` also clears saved folder state. Neither command deletes
the user's files or changes Windows file associations. These commands do not
remove previously saved diagnostic reports.
