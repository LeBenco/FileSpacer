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

`setDefaultBrowser()` changes each association independently and reports errors
per class. `synchronizeContextMenus()` only reconciles FileSpacer-owned menu
registrations under the current user's `Software\Classes`; it never changes
the default association or Windows-owned verbs. This also supplies user-level
overrides for a FileSpacer command installed for all users.

When FileSpacer is the default, item menus retain **Open in FileSpacer** and add
**Open in Windows File Explorer**. Otherwise they offer **Open in FileSpacer**.
The directory background follows the directory association and shows only the
alternative manager command.

The visibility distinction matters: `CMF_DEFAULTONLY` omits verbs marked
`ProgrammaticAccessOnly`, even when the association names that verb. Keep
FileSpacer's **item** verb visible so Shell default invocation still selects it.
Apply `ProgrammaticAccessOnly` only to its **background** verb, which is not used
to open an item. Preserve the registered command and `DelegateExecute` values.

The separate `filespacer.explorer` verb invokes
`"%SystemRoot%\explorer.exe" "%1"` (`%V` for a background). Resetting the default
removes this alternate verb and FileSpacer's visibility override. Avoid rewriting
unchanged values; notify `SHCNE_ASSOCCHANGED` after an actual menu change.

<a id="startup-settings-and-installer-boundary"></a>

## Startup, settings and installer boundary

Normal startup synchronizes existing registrations; portable startup installs
nothing. Debug `/test` skips production synchronization. Integration settings
synchronize after association changes and retain separate errors for each class.
Missing installed network verbs require an installation update, not a claim
that setting the default succeeded fully.

The installer calls `/sync-context-menus` without opening a folder or starting
crash recovery, and checks its exit code. Component removal/uninstall removes
FileSpacer's per-user overrides through the user-hive cleanup. Neither startup
nor synchronization silently changes a user's default-manager choice.

<a id="testing-the-boundary"></a>

## Testing the boundary

Group `29-menu-gestionnaire` uses isolated Registry handles and the real
`SHCreateDefaultContextMenu`. It checks independent class defaults, alternate
verbs, preservation of commands/COM delegates, failure propagation and native
default-only selection. Both `ShellExecuteEx` invocation paths launch a real
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
