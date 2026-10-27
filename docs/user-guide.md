# Using FileSpacer

## Window navigation

Each folder has one window that remembers its position, size and view settings.
Reopening an already-open folder activates its window. Moving or renaming a
folder preserves its saved layout.

- Opening a folder or its parent closes the source window by default. Hold Ctrl
  to reverse this once, or change the default in **Settings > General**. Opening
  several selected folders keeps the source window open.
- The menu beside the folder title switches between open folders. **Close Parent
  Windows** closes its open ancestors while leaving other folders open.
- **Ctrl+Shift+T**, or **Reopen Last Closed Window** in the title-bar menu or
  taskbar Jump List, reopens a closed folder without closing the current one.
  Its layout and selection are restored; Details and List views also restore
  the first visible row. The taskbar command can reopen earlier closures in turn.
- The taskbar Jump List also opens folders pinned in File Explorer's Quick access.

Going to a parent does not select the folder you came from. An already-open
parent keeps its selection. Control Panel opens in its own Windows window.

## View and layout

### Icon layout

Turn off **Auto arrange icons** in an icon view to position files and subfolders
freely. Their positions are saved independently of the window's size and position,
including when its saved state is locked.

### View options

In **Settings > Display**, two options are enabled by default:

- **Show the full name of a selected file** keeps one selected name readable in
  Icons view. It does not change multiple selections or other view modes.
- **Preview display modes in the View menu** previews a mode as you hover over
  it. Click to keep it; Escape or leaving the preview restores the initial view.
  Previewing does not replace the saved layout.

### Window placement

**Settings > General > Place new windows in free space** is enabled by default.
Folders without a saved position open in available space, with overlap reduced
when no free space remains. Saved positions take priority. Turning this off uses
Windows' default placement without moving existing windows.

### Selection and activation

With **Settings > General > Keep the current selection when clicking an inactive
window** enabled, clicking an inactive folder's background activates it without
clearing the selection. Start a selection rectangle with a second press. Item
clicks, right-clicks and window gestures retain their usual behavior.

### Lock a folder window's saved state

Enable **Settings > General > Enable window state locking** (off by default), then choose
**Lock window state** in the menu beside a folder's title. Locking captures its
position, size and view settings; a padlock identifies a locked folder.

You can still move, resize and change the view temporarily. Reopening the folder
restores the locked layout. Individual icon positions continue to save normally.
**Unlock window state** keeps the current layout. The padlock can be hidden in
General settings without removing the lock.

Disabling the feature asks for confirmation and removes all saved locks,
including those on closed folders. Enabling it again does not restore them.

## Window shortcuts

Drag in the file area, on its background or an item, using these defaults:

| Action | Shortcut |
| --- | --- |
| Move the window | Alt + left drag |
| Resize from the bottom-right corner | Alt + right drag |

The selection is preserved. Escape cancels the gesture and restores the original
geometry. Text fields, headers, scrollbars, toolbars and menus keep their normal
behavior. Alt + double-click on an item opens its Properties.

In **Settings > Shortcuts**, enable or disable each action and expand it to
change its binding. Use the direct-input area to record a key/button combination.
Left and right buttons require Alt; the middle button also supports combinations
without Alt. Modifiers must match exactly. Reserved or duplicate bindings cannot
be applied.

The default-setting button resets one action. **Revert** restores the last
applied shortcuts; **Cancel** discards pending edits. **Apply** or **OK** makes
the changes available to open FileSpacer windows.

## Search and history

Type in the search field and press Enter or click the magnifier to filter the
current folder. **Settings > General > Filter while typing** applies the filter
as you type. This local filter never launches Everything automatically.

Focusing the field shows recent searches and, when enabled,
**Search in Everything**. Use Up/Down and Enter to choose an entry, or click it.
Enter without a selected suggestion submits the local filter. Escape closes
the list without erasing your text; Ctrl+F focuses the field again.

History keeps five different submitted searches per folder by default. Change
the limit in General settings from 0 to 20; 0 clears and disables history.
Typing alone does not add a history entry. See [Everything](#everything) for
searching subfolders or all indexed files.

## Integration

### Windows integration

The installer offers **Make FileSpacer the default file manager**, unchecked by
default. It also installs the Folder Context Menu component. You can change the
default later in **Settings > Integration** with **Set as default browser**;
**Reset default browser** returns folder opening to Windows File Explorer.

The context-menu component offers **Open in Windows File Explorer** when
FileSpacer is the default, and **Open in FileSpacer** otherwise. It applies to
folders, drives, compressed folders, network locations and folder backgrounds.
Installing only this component leaves the default application unchanged.

**File Explorer options** on the same settings page opens Windows folder
preferences, some of which also affect FileSpacer's file view.

### Everything

Install [Everything](https://www.voidtools.com/) separately, then enable it in
**Settings > Integration**. Leave its executable path empty for automatic
detection, use **Detect**, or choose its `.exe` manually for a portable install.

Choose the search scope: the current folder and its subfolders, or **Everywhere**.
Virtual folders have no filesystem scope; use Everywhere for them.

Enter a query in FileSpacer's search field and choose **Search in Everything**
from its suggestions. Results open in Everything, using its index and search
syntax. A successful launch clears FileSpacer's field and local filter; a launch
error keeps both so you can retry.

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) can theme FileSpacer's menus and customize
their entries. It is optional and installed separately.

Allow FileSpacer in your Nilesoft configuration's exclusion rule and keep your
theme import. Rules intended only for Explorer should be restricted to Explorer
so they do not also change FileSpacer's menus.

See the [configuration reference and examples](nilesoft-shell.md) for the exact
settings, menu-specific rules and recommended appearance. After changing the
configuration, unregister and register Nilesoft Shell again to apply it to newly
opened menus.

## Settings and updates

### Interface and preferences

**Ctrl+,** opens Settings, even with the toolbar hidden. Toolbar and status-bar
visibility can be changed there. Closing the folder that opened Settings discards
pending edits. The interface follows Windows' display language, with English as
the fallback; changing the language requires restarting.

**Settings > About > Maintenance > Reset all folder state** closes open folder
windows and clears their saved layouts and history, keeping global preferences
and your files. This also removes saved window-state locks.

### Updates

**Settings > About** checks for a new release. **Install FileSpacer vX.Y.Z** asks
for confirmation, downloads and verifies the installer, then closes folder
windows normally before starting setup. You can cancel the download; updates
are never installed automatically.

After installation, FileSpacer requests reopening the previously open folders
with their saved layouts. Cancelling setup or UAC also requests reopening them.
Automatic weekly checks are optional and off by default; enable them in About
or the first-run welcome dialog. Older releases may offer the GitHub release
page instead of an installer.
