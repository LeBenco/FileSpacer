<a id="window-gestures"></a>

# Window gestures

The [user guide](user-guide.md#moving-and-resizing) covers configuration.
Implementation starts in `WindowShortcuts.h`, `ShortcutSettingsPage.cpp`,
`Settings.cpp`, `FolderWindow` and `WindowResizeGesture`.

<a id="binding-and-input-rules"></a>

## Binding and input rules

Defaults are Alt + left drag to move and Alt + right drag to resize. Each action
can be disabled independently. The two enabled actions cannot share a binding.

| Modifier keys | Left | Middle | Right |
| --- | --- | --- | --- |
| None, Ctrl, Shift, Ctrl + Shift | Reserved | Allowed | Reserved |
| Alt, Ctrl + Alt, Alt + Shift, Ctrl + Alt + Shift | Allowed | Allowed | Allowed |

Match modifiers exactly at button-down. Releasing a modifier during the gesture
does not change its action. AltGr is the Ctrl + Alt combination reported by
Windows; middle means pressing the wheel, not scrolling it.

Gestures act on the view background and items without changing selection.
Editors, rename controls, headers, scrollbars, toolbars and menus retain native
input. Escape, capture loss and cancellation abort a captured gesture. Left
movement uses the Windows move loop; other movement and all resizing use the
shared capture engine, respecting native size limits and geometry persistence.

Double-click and Properties remain owned by the Shell. If a gesture consumed
the first click of a subsequent native double-click, deliver that deferred
click to the view before the double-click. Do not replace Explorer's selection
or Properties behavior with a manual invocation.

<a id="preferences-and-powertoys-precedence"></a>

## Preferences and PowerToys precedence

The settings page keeps drafts until Apply/OK and rejects reserved or
conflicting bindings. Per-action defaults affect one draft; Revert restores
the last applied pair. Both bindings are committed atomically as the versioned
`WindowShortcuts` DWORD. Invalid stored data falls back to defaults. Windows
read the current pair when a gesture starts, including across processes.

`settings::windowShortcutApplies()` checks PowerToys Grab And Move only after
the input matches a configured binding. If its saved configuration enables it
for that target window, FileSpacer suppresses its own gestures without deleting
the configured shortcuts. This does not depend on PowerToys currently running.
Preserve the distinction between target exclusions and foreground-window
modifier exclusions; see `powerToysOverridesWindowGestures()` in `Settings.cpp`.

<a id="tests"></a>

## Tests

```cmd
tests.cmd -Pattern "24-*"
```

Group 24 separates page/Registry binding validation, capture-engine geometry and
cancellation, real Shell gestures across processes, and native Properties
comparison. The binding matrix checks valid/conflicting pairs and corrupt
stored values; HWND-only tests do not replace Shell input acceptance. Groups
15–17 cover further gesture/interaction boundaries. Use the runner's isolated
preferences/database and preserve the real outcome and exit codes.
