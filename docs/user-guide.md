# Using FileSpacer

- Opening one folder, including its parent, closes the source window by default.
  Hold Ctrl to reverse the behavior for that operation, or change the default in
  Settings > General. Opening several selected folders keeps the source window.
- Opening a folder that already has a FileSpacer window activates that window.
- Files open through the normal Windows application association mechanism.
- Toolbar and status-bar visibility can be changed in Settings. Ctrl+, opens
  Settings even when the toolbar is hidden.
- English and French resources are selected from the Windows display language,
  with English as the fallback. Changing the language requires restarting.
- Settings > General > Reset all folder state clears saved folder positions,
  sizes, and view settings, keeping global application preferences. It closes
  open FileSpacer folder windows before clearing the database.

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

Automatic update checking, public help links, and donation links remain disabled.
Their code and resource entries are retained in disabled blocks for later work.
