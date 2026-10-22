# Using FileSpacer

- Opening one folder, including its parent, closes the source window by default.
  Hold Ctrl to reverse the behavior for that operation, or change the default in
  Settings > General. Opening several selected folders keeps the source window.
- Opening a folder that already has a FileSpacer window activates that window.
- The title-bar menu's Close Parent Windows command closes the open FileSpacer
  windows for all ancestors of the current folder, keeping the current window
  and unrelated folders open. It also works across FileSpacer processes and is
  disabled when no parent window is open. Positions and view settings are saved
  through normal window closing.
- Reopen the last closed folder from the title-bar menu or with Ctrl+Shift+T.
  The menu shows the folder's name and Shell icon, and shows the command only
  when the history has a folder to reopen and that folder is not already open.
  This applies to every
  folder, independently of the startup-folder setting. Only the latest
  closure is considered; an older entry is not offered instead, and the history
  is retained. The current window stays open. A reopened folder keeps its saved
  geometry and view settings,
  and restores its selection and scroll position once the Shell has loaded it.
  The menu icon loads in the background, including custom folder icons; a generic
  folder icon is used if the Shell cannot retrieve it.
- The taskbar icon's Jump List also offers Reopen Last Closed Window, including
  the name and Shell icon of the folder to reopen, even when no FileSpacer window
  remains open.
  This task is omitted when the latest closed folder is already open or the
  history is empty, using the same rule as the title-bar menu and Ctrl+Shift+T.
  Its Quick access category shows Explorer's pinned Quick access folders with
  their Shell icons; clicking one opens it in FileSpacer. The list refreshes
  on startup, activation and changes to the closed-window history. Windows
  controls the separation between categories and the Tasks heading; a leading
  task separator is ignored. Items pinned directly in this Jump List move to
  Windows' own Pinned category. The history is shared across processes
  and survives restarts, keeping up to 20 different folders in closing order.
  Repeated use reopens earlier closures. Closing a source window automatically
  during navigation does not enter the history. An unavailable folder remains
  in the history so it can be retried; resetting all saved folder settings also
  clears the history.
- Files open through the normal Windows application association mechanism.
- Toolbar and status-bar visibility can be changed in Settings. Ctrl+, opens
  Settings even when the toolbar is hidden.
- English and French resources are selected from the Windows display language,
  with English as the fallback. Changing the language requires restarting.
- Settings > General > Reset all folder state clears saved folder positions,
  sizes, and view settings, keeping global application preferences. It closes
  open FileSpacer folder windows before clearing the database.

## Move and resize folder windows

Hold Alt and start a drag in the main file area, on the background or on an item:

- The left mouse button moves the window.
- The right mouse button resizes it from its bottom-right corner, keeping the
  top-left corner fixed. The diagonal resize cursor appears during resizing.
- The existing file selection is preserved, including when starting on an
  unselected item. Escape cancels the gesture and restores the initial geometry.

Toolbars, the path and search fields, column headers, scrollbars and rename
edit controls retain their normal Windows behavior. Alt + double-click on an
item opens its Properties. Without Alt, the file view handles clicks and drags
normally. Moving and resizing this way updates the folder's saved geometry.

FileSpacer uses the classic Shell view for all display modes. The full-name
display preference changes selected-item labels and does not switch the view
implementation.

## Command-line maintenance

Close all FileSpacer instances before running either command from its executable
folder:

```cmd
FileSpacer.exe /reset-options
FileSpacer.exe /reset-all
```

The first command resets global preferences. The second also resets saved folder
state. Neither deletes files or changes Windows associations. Resetting preferences
also restores the default setting for experimental full-name display.

## Updates

Settings > About checks for a new release in the background. When an installable
update is available, the button becomes Install FileSpacer vX.Y.Z. Installing
asks for confirmation, downloads and verifies the installer, then closes folder
windows normally before starting setup. The installer requires your interaction.

The download has its own progress dialog. Cancel or its close button stops the
download and leaves folder windows open. A download or verification failure
does not close them. If a process prevents normal shutdown, setup is not started;
some windows may already have closed. No process is forcibly terminated.

After installation, FileSpacer reopens the folders that were open when shutdown
started, with their saved geometry and view settings. Setup or UAC cancellation
also requests restoration with the original executable. Existing folder windows
are reused rather than duplicated.

Automatic weekly checks can be enabled on the same page or in the first-run
welcome dialog. They are off by default and notify you when a newer release is
available. Older releases without installer metadata offer their GitHub release
page instead. Updates are never installed without your confirmation.

