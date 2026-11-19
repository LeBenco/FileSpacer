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
| None, Ctrl, Shift, Ctrl + Shift | Reserved | Allowed | Background only |
| Alt, Ctrl + Alt, Alt + Shift, Ctrl + Alt + Shift | Allowed | Allowed | Allowed |

Without Alt, a right-button binding starts only on the view background, where
a drag replaces the native right-drag selection. A press released without a
drag is returned to the view as the click it was: both original messages are
posted back to the control that received the press, so the Shell itself clears
the selection and opens its background menu. On an item the press stays native:
context menu and right-button drag. The settings page states this when such a
binding is chosen.

Match modifiers exactly at button-down. Releasing a modifier during the gesture
does not change its action. AltGr is the Ctrl + Alt combination reported by
Windows; middle means pressing the wheel, not scrolling it.

Other gestures act on the view background and items without changing selection.
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

FileSpacer never suspends its own gestures for PowerToys Grab And Move. That
tool intercepts its modifier-and-button presses in a low-level hook before the
application receives them: with the same binding it acts alone, and with
different bindings both stay active. The Shortcuts page only shows a notice
while the saved PowerToys settings enable Grab And Move and do not exclude
FileSpacer (`powerToysOverridesWindowGestures()` in `Settings.cpp`); those
settings are read when the page is shown or activated, never on mouse input.
Grab And Move ignores injected input, so automated tests cannot exercise it.

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
