<a id="default-manager-and-context-menu-integration"></a>

# Default-manager and context-menu integration

`Settings.cpp` manages FileSpacer's folder associations and alternate-manager
menu entries. Windows still selects and invokes the default action. For usage,
see [Windows integration](user-guide.md#windows-integration).

<a id="associations-and-menu-visibility"></a>

## Associations and menu visibility

Read the effective `HKEY_CLASSES_ROOT` association independently for `Directory`,
`Drive`, `CompressedFolder`, `NetServer` and `NetShare`. A default verb can be a
space- or comma-separated ordered list: use its first verb rather than comparing
the whole string. A server root such as `\\server` is `NetServer`, not
`Directory`; omitting network classes leaves those targets with another manager.

`setDefaultBrowser()` redirects each class's canonical `open` command to
FileSpacer and selects `open` as the default verb. The label remains **Open**.
Only the current user's overrides are written. The original values, types and
absence are saved under `Software\Classes\FileSpacer.OpenVerbBackup` before any change.
The journal stays in the same Classes hive as the overrides, including when
the installer mounts an offline user's `UsrClass.dat`.
Disabling FileSpacer as the default restores those values, but preserves values
subsequently replaced by another application. Existing empty keys are preserved;
empty keys created by FileSpacer are removed during restoration.

The explicit `filespacer` and `filespacer.explorer` verbs are independent of that
association. Their shared `IExplorerCommandState` implementation, shipped in
`FileSpacerFolder.dll`, controls visibility in the process hosting the menu:

- FileSpacer shows **Open in Windows File Explorer**.
- Windows Explorer shows **Open in FileSpacer**.
- Other hosts show neither extra command.

The base `open` command has no FileSpacer visibility filter, including in
`CMF_DEFAULTONLY` menus. Default invocation therefore remains available when
both explicit commands are hidden. FileSpacer's internal folder activation
continues to use its spatial-window logic.

The Explorer command invokes `"%SystemRoot%\explorer.exe" "%1"` (`%V` for a
background). The FileSpacer command retains its installed command and COM
delegate. Avoid rewriting unchanged menu values; notify `SHCNE_ASSOCCHANGED`
after an actual menu change. No foreground-window heuristic or resident watcher
is involved.

<a id="startup-settings-and-installer-boundary"></a>

## Startup, settings and installer boundary

Normal startup synchronizes existing registrations and migrates the old
`filespacer` default to `open`; portable startup installs nothing. Debug `/test` skips production synchronization. Integration settings
synchronize after association changes and retain separate errors for each class.
Missing installed network verbs require an installation update, not a claim
that setting the default succeeded fully.

The installer uses `/set-default-browser` for the explicit default-manager
choice and `/sync-context-menus` to reconcile the menus. Both run without
opening a folder or starting crash recovery; their exit codes are checked.
Component removal and uninstall call `/restore-open-verbs` before deleting the
executable, including for user hives mounted by the existing cleanup. Startup
does not reclaim a default changed by another application.

<a id="testing-the-boundary"></a>

## Testing the boundary

Group `29-menu-gestionnaire` uses isolated Registry handles and the real
`SHCreateDefaultContextMenu`. It checks independent class defaults, host-filter
registrations,
restoration and external replacements, preservation of commands/COM delegates,
failure propagation and native default-only selection. Both `ShellExecuteEx` invocation paths launch a real
test subprocess. Network shortcut resolution exercises the Shell's association
lookup for the target, not just the `.lnk` file's class.

These checks do not constitute physical Explorer double-click or installed
upgrade/uninstall acceptance. Group 19 separately opens/cancels real item and
background menus. Follow the [regression guide](tests/regression/README.md)
for execution and interpretation; keep run-specific results in its artifacts.

References: [Shell verbs](https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers),
[default-menu construction](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-querycontextmenu),
[predefined Shell classes](https://learn.microsoft.com/en-us/windows/win32/shell/reg-shell-exts),
[association-change notification](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify).
