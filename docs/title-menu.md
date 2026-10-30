# Title bar menu

Open **Preferences → Display → Interface elements → Customize title bar menu**.
Drag an available action into the menu on the left, or drag a menu entry to
reorder it. Dropping an editable entry anywhere outside the menu removes it;
Escape cancels the drag. Separators can be used more than once. The open-window
list is represented by the taller `<< Open windows >>` block and expands to the
currently open windows in the real menu.

**OK** saves the composition immediately. All FileSpacer windows read it when
opening their next title menu. **Cancel** discards the editor's changes.
**Restore defaults** changes the draft; save it with OK to apply it. The default
composition preserves the existing title menu. Context-dependent actions retain
their existing visibility and availability rules.

The preview uses the active Windows/Nilesoft Shell renderer. There is no theme
selector. Entries added or positioned by Nilesoft Shell are gray and cannot be
dragged; their hover tooltip and the link below the preview point to its
configuration files. A renamed command remains editable if NS does not impose
its position. NS has the final say over the displayed menu. Ambiguous changes
to a separator run are treated conservatively as managed by NS.

The renderer runs in a disposable FileSpacer process on a private desktop.
The editor retains its captured pixels during a drag. If NS's layered background
cannot be captured, it uses an explicit opaque fallback; it does not simulate
the desktop compositor's acrylic effect. No action is invoked from the preview.

## Additional actions

- **Restore the locked layout** restores the saved reference's position, size,
  maximized state and view options through the existing native view. It keeps
  the view and its current selection alive while applying the saved reference.
- **Replace the reference layout** captures the current layout and keeps it
  locked. It requires an existing lock and confirmation.
- **Forget this folder's layout** clears only its saved layout and lock, keeping
  search history. The current window stays in place and does not save a layout
  again until it closes. A new window uses the default settings. Explicitly
  locking a new layout resumes persistence.
- **Close descendant windows** closes the open descendants of the source folder.
  **Close other windows** retains only the source FileSpacer window. Both use
  normal close processing and saved-state persistence, also across processes.
- **Fit the window to its contents** uses the native view's item positions,
  spacing and visible column widths, bounded by the monitor's work area.
- **Save / recall a folder group** stores a named list of open folders. Recall
  opens or activates each folder using its own saved layout and keeps other
  windows open. Deleting a group never deletes its folders. Groups are separate
  from the menu composition and survive a reset of global preference values.

## Commands for Nilesoft Shell

These are stable Windows `WM_COMMAND` identifiers. NS can also add an action
that was omitted from the FileSpacer composition, using its documented
[`window.command`](https://nilesoft.org/docs/functions/window) function. For
example, within a rule restricted to FileSpacer's title menu:

```nss
item(title='Fit window to contents' cmd=window.command(1128))
```

The menu owner forwards the command to the actual FileSpacer window. Normal
context, state-lock and lifecycle checks still apply. NS can rename, hide,
reposition and decorate native entries with its usual configuration rules.

| Action | Command ID |
| --- | ---: |
| Copy path | 1120 |
| Rename | 1008 |
| Toggle layout lock | 1100 |
| Properties | 1121 |
| Close parent windows | 1122 |
| Reopen last window | 1021 |
| Preferences | 1009 |
| Restore locked layout | 1123 |
| Replace reference layout | 1124 |
| Forget folder layout | 1125 |
| Close descendant windows | 1126 |
| Close other windows | 1127 |
| Fit window to contents | 1128 |
| Folder groups | 1129 |

The dynamic open-window list is not a single command: its entries target the
windows that existed when that particular menu was opened.

## Validation

The two focused regressions are under `33-menu-titre`. Debug `/test` keeps
preferences, groups and folder state isolated and excludes NS from this menu
and its editor. A dedicated NS interoperability check can opt in by setting
`FILESPACER_TEST_NILESOFT=1` only in its test process environment. Release builds
ignore this test setting. Native-only regressions do not validate NS rendering.
Nilesoft Shell's visual appearance is checked during manual acceptance,
independently of this native regression suite.

## NS installation and redistribution

No Nilesoft binary is shipped by FileSpacer or its ordinary regression tests.
Optional interoperability probes use an existing NS installation. NS 1.9.15
supports a `config` string value under `HKCU\Software\Nilesoft\Shell` to select
a separate configuration file. Such a probe must preserve and restore the exact
original value and registry state, including after a timeout. Keep fixture rules
separate from the installed configuration. Do not copy the installed DLL into
published test artifacts.

The [v1.9.15 source license](https://github.com/moudey/Shell/blob/v1.9.15/LICENSE)
is MIT and requires preservation of its copyright and permission notice.
The [official binary distribution terms](https://nilesoft.org/legal/eula)
also require the complete original distribution to remain unaltered. Do not
treat a lone DLL taken from an installation as a verified redistributable test
dependency. Any separate redistribution must preserve the complete official
package and its notices. Record its version and archive hash and keep working
configuration files separate. Alternatively, build from a pinned MIT source revision
and preserve the relevant license and third-party notices with that build.
