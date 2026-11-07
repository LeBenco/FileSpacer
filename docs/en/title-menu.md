# Title bar menu implementation

For editing and folder-group usage, see the
[user guide](user-guide.md#customize-the-title-bar-menu). Start in `TitleMenu.h`,
`TitleMenuDialog.cpp`, `TitleMenuRenderer.cpp` and `FolderGroups.cpp`; the live
commands are handled by the folder window.

## Composition and rendering

Preferences store stable `TitleMenuEntry` values, independent of translated
labels and dynamic open-window command IDs. `validTitleMenu()` accepts at most
64 entries, permits repeated separators and rejects duplicate actions. The
open-window entry is a block expanded from the live window list when opening
the real menu, not one command targeting a permanent window.

The editor saves immediately on OK; Cancel discards its draft. Restore defaults
changes only the draft. Other FileSpacer windows read the composition on the
next menu opening. Keep each action's context/availability checks when moving
it in the composition or invoking it externally.

FileSpacer composes the menu, then Nilesoft Shell applies its rules. Entries
added or positioned by Nilesoft are read-only in the editor. A renamed entry
remains editable when Nilesoft does not control its position; ambiguous separator
runs are treated conservatively as externally managed.

The preview uses the actual Windows/Nilesoft renderer in a disposable FileSpacer
process on a private desktop. It never invokes previewed actions. During a
drag, retain the captured menu pixels. If Nilesoft's layered background cannot
be captured, use the explicit opaque fallback rather than simulating compositor
acrylic.

## Action boundaries

- Restore the locked layout applies saved geometry/view options through the
  existing view without destroying its selection. Replace reference captures
  the current layout under an existing lock and requires confirmation.
- Close descendant/other windows uses normal close processing and persistence,
  including across processes. Fit to contents uses native item positions,
  spacing and visible column widths, bounded by the monitor work area.
- Folder groups store named folder lists independently of menu composition.
  Recall opens/activates each folder with its own saved layout, retaining other
  windows. Removing a group never deletes folders; resetting global preference
  values does not clear groups.

## Nilesoft command bridge

The following are stable `WM_COMMAND` identifiers. They differ from persisted
`TitleMenuEntry` values and dynamically assigned window-list IDs. Nilesoft can
invoke an action omitted from the composition with
[`window.command`](https://nilesoft.org/docs/functions/window). Inside a rule
restricted to FileSpacer's title menu:

```nss
item(title='Fit window to contents' cmd=window.command(1128))
```

The menu owner forwards it to the actual folder window; lifecycle, context and
lock checks still apply. See the [configuration reference](nilesoft-shell.md)
for menu scoping and visual settings.

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
| Close descendant windows | 1126 |
| Close other windows | 1127 |
| Fit window to contents | 1128 |
| Folder groups | 1129 |

## Tests

Group `33-menu-titre` exercises native composition and actions in Debug `/test`
mode, with isolated preferences, groups and folder state. This mode excludes
Nilesoft from the menu/editor. An explicit interoperability test can opt in with
`FILESPACER_TEST_NILESOFT=1` in its own process environment; Release ignores it.
Native-only tests do not validate Nilesoft rendering.

Interoperability checks use an existing Nilesoft installation and separate
fixture configuration. Preserve and restore the exact installed Registry and
configuration state, including after timeout. No Nilesoft binary is distributed
with FileSpacer or its ordinary tests. Do not copy installed DLLs into published
test artifacts; preserve the applicable license/distribution notices for any
separately supplied dependency.
