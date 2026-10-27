# Using FileSpacer

## Window navigation

Each folder has a single FileSpacer window, with its position, size and view
settings saved for the next opening. The window follows its folder when it is
moved or renamed, and closes when the folder is deleted.

- Opening one folder, including its parent, closes the source window by default.
  Hold Ctrl to reverse the behavior for that operation, or change the default in
  Settings > General. Opening several selected folders keeps the source window.
- Opening a folder that already has a FileSpacer window activates that window.
- Navigating up or opening an ancestor does not automatically select the folder
  you came from. An existing window keeps its current selection.
- The menu beside the folder title lists the other open FileSpacer windows,
  including those in other FileSpacer processes. Choose an entry to activate
  that window. The current window is excluded from the list.
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
  and restores its selection and focused item once the Shell has loaded it. In Details and List views it also scrolls back to the row that was at the top when the window was closed; icon views open at the top as before.
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

## Interface and preferences

- Files open through the normal Windows application association mechanism.
- Toolbar and status-bar visibility can be changed in Settings. Ctrl+, opens
  Settings even when the toolbar is hidden.
- English and French resources are selected from the Windows display language,
  with English as the fallback. Changing the language requires restarting.
- Settings > About > Maintenance > Reset all folder state clears saved folder positions,
  sizes, and view settings, keeping global application preferences. It closes
  open FileSpacer folder windows before clearing the database.

## Icon layout

In an icon view, turn off **Auto arrange icons** in the folder's view options
to place files and subfolders freely. Their positions are saved with the folder
and restored when it is reopened, independently of the window's position and
size. Moving or renaming the folder preserves its saved spatial state.

[Window state locking](#lock-a-folder-windows-saved-state) keeps a reference for
window geometry and view settings; individual icon positions continue to save.

## Window placement

Settings > General > Place new windows in free space is enabled by default.
Folders without a valid saved position first look for a free position, then
minimize the area covered by other visible FileSpacer windows on the chosen
monitor. Minimized windows and the source being replaced during spatial navigation are ignored.
Saved positions keep priority. Disabling the option restores Windows' default
placement without moving existing windows.

## Selection and activation

With **Keep the current selection when clicking an inactive window** enabled
in **Settings > General**, the first click
on the empty background of an inactive folder window only activates it and keeps
its selection. Clicking an item, right-clicking, Alt gestures and clicks in the
already active window behave as usual; a selection rectangle started by that
first click requires a second press.

## Windows integration

The installer offers **Make FileSpacer the default file manager**, unchecked by
default. Selecting it also enables the folder context-menu component required
for opening folders through FileSpacer. Installing the context-menu component
alone adds **Open in FileSpacer** to folders and drives without changing the
default application.

To enable this later, open **Settings > Integration** and choose **Set as default
browser**. Choose **Reset default browser** to return folder opening to Windows
File Explorer. These actions update the current user's folder, drive and
compressed-folder associations immediately.

The context-menu command always offers the other file manager: **Open in Windows
File Explorer** when FileSpacer is the default, and **Open in FileSpacer** otherwise.
This also applies to the background of a folder. The installer and Integration
settings update these menus immediately; FileSpacer checks them again at each
normal startup, so changes made by another tool take effect at the next launch.
The default folder-opening command is preserved. No background monitor is used.
See [the integration note](context-menu-integration.md) for implementation and
validation details.

The same page offers **File Explorer options**: some Windows folder preferences
also affect FileSpacer's native folder view.

## Search and Everything

The existing search field filters the current Shell view. Enter or the magnifier
submits the filter; **Settings > General > Filter while typing** also applies it
during typing. Neither action automatically uses Everything.

Clicking or focusing the search field opens a suggestion list: **Search in
Everything** when enabled, followed by the current folder's recent searches,
newest first. A separator appears only when both sections contain entries. There
is no chevron or extra button. Typing continues in the field, including instant
filtering when enabled, and no entry is selected automatically.

Up/Down navigate the list, skipping the separator. Enter accepts an explicitly
selected entry; otherwise it submits the local search. Selecting a recent search
fills the same field and applies the local filter. Escape closes the list without
changing the query. Click the field again, use Ctrl+F, or press Down to reopen it.
Tab closes the list and continues through the magnifier to the folder view. An
empty list is never displayed.

In **Settings > Integration**, enable Everything independently of history. Leave
its executable path empty for automatic detection, use **Detect**, or browse/type
an absolute `.exe` path without enclosing quotes. Portable installations outside
the standard locations need a manual path. Everything opens as an external
application, using the visible query. The default scope is the current filesystem
folder and its subfolders; **Everywhere** removes that restriction. A virtual Shell
folder has no filesystem scope: choose Everywhere to search from it.

A successful launch clears the local field and filter. A launch error displays a
dialog and keeps both. Everything must be installed separately; its own index and
settings control the external results.

The Everything entry shows the icon from its executable, loaded in the background
at the appropriate display size. It is cached in memory and refreshed at the next
list opening when the executable's path, dates or size change. Loading an icon
does not delay typing or opening the list; an unavailable icon leaves the action
usable.

History retains five different explicit submissions per folder by default, across
sessions. Typing alone does not add entries, even with instant filtering enabled.
Repeated submissions move the query to the front. The history limit in General
accepts 0 through 20; zero clears and disables history while local search and
Everything remain available. Reducing the limit also trims closed folders.
History follows the folder's existing identity on rename/move and is removed with
its saved state. Temporary access failures follow the existing conservative state
expiration rules; they do not immediately delete history.

## Lock a folder window's saved state

Enable **Settings > General > Enable window state locking** (off by default).
This makes **Lock window state** available in the menu beside the window title;
no folder is locked automatically. Locking captures the current normal position,
size, maximized state and folder view settings, including icon size, sorting,
grouping and columns. A small padlock badge from the Small & Flat set overlays the bottom-right
corner of the folder icon. Its embedded multi-resolution ICO scales with DPI on
all supported Windows versions; high-contrast mode uses the system text color.
The title's accessible name and its menu button's tooltip explain the lock.

The badge is visible by default. To hide it without removing the lock, clear
**Settings > General > Show a padlock in the title bar of locked folders**.

You can still move, resize and change the view of a locked window. Those changes
remain temporary: reopening the folder restores the captured reference. Individual
file and subfolder icon positions continue to be saved normally. Selection, focus,
scrolling, contents, global options and search history retain their usual behavior.
**Unlock window state** saves the current visible settings without restoring the
old reference. The lock follows the existing folder identity (including local
NTFS renames and moves); the normal expiration and storage quota still apply.

Disabling the feature asks for confirmation. Cancel keeps the existing locks;
confirm permanently removes all locks, including closed folders, while preserving
other saved state. Open windows in all processes immediately resume normal saving
without moving. Enabling the feature again does not restore previous locks.
Resetting all folder state also removes its locks.

## Move and resize folder windows

The following are the default shortcuts. Change or disable each action in
**Settings > Shortcuts**; see [Window mouse shortcuts](window-shortcuts.md)
for configuration and supported combinations.

Hold Alt and start a drag in the main file area, on the background or on an item:

- The left mouse button moves the window.
- The right mouse button resizes it from its bottom-right corner, keeping the
  top-left corner fixed. The diagonal resize cursor appears during resizing.
- The existing file selection is preserved, including when starting on an
  unselected item. Escape cancels the gesture and restores the initial geometry.

Toolbars, the path and search fields, column headers, scrollbars and rename
edit controls retain their normal Windows behavior. Alt + double-click on an
item opens its Properties. Without Alt, the file view handles clicks and drags
normally. Moving and resizing this way updates the folder's saved geometry,
unless its window state is locked.

File operations, rendering and change notifications use Windows' native folder
view. FileSpacer manages the spatial windows and their saved state; see the
[Shell architecture and accepted differences](native-shell-view.md).

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



## View behavior in 1.1

Control Panel opens in its native Windows window and leaves the source
FileSpacer folder open, with its selection.

For ergonomic reasons, navigating to a parent or ancestor no longer selects
the originating folder automatically. This avoids adding an unintended folder
to a subsequent Ctrl selection or opening multiple folders accidentally.
An already-open parent keeps its existing selection.

The public Shell integration uses Windows' native selected-name rendering.
FileSpacer's custom opaque full-name rendering is unavailable; its Settings
control is disabled and labeled accordingly, and the saved preference is
retained. Scroll restoration on reopening is anchored on the first displayed
row in Details and List views; icon views and other layouts open at the top,
with the selection and focus restored.
