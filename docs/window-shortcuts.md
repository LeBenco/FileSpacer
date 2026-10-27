# Window shortcut implementation and validation

For everyday use and configuration, see [Moving and resizing in the user guide](user-guide.md#moving-and-resizing).

The **Shortcuts / Raccourcis** settings tab configures moving and resizing folder
windows. Defaults are Alt + left drag to move and Alt + right drag to resize.
Each action can be disabled independently. Item operations and Properties remain
owned by the native Explorer view; left double-click is not a configurable action.
If a gesture consumed the first click, a subsequent native double-click delivers
that deferred click to the view first. Explorer performs its usual selection and
Properties handling; no application-specific Properties command is invoked.

| Modifier keys | Left | Middle | Right |
|---|---|---|---|
| None, Ctrl, Shift, Ctrl + Shift | Reserved | Allowed | Reserved |
| Alt, Ctrl + Alt, Alt + Shift, Ctrl + Alt + Shift | Allowed | Allowed | Allowed |

Modifiers must match exactly when the button is pressed. Releasing a modifier
after starting a gesture does not change its action. AltGr uses the Ctrl + Alt
combination reported by Windows. The middle button means pressing the wheel,
not scrolling it.

Both sections start collapsed. The checkbox enables an action without opening
its section. The title or chevron expands a single shared editor beneath that
action. The default-setting button resets only that action; Revert restores the
last applied shortcuts for this page. Cancel discards unapplied changes through
the normal property-sheet contract.

Direct input records the keys and button in the designated area and closes the
area when the click ends. An invalid selection remains visible, with the usual
red warning and disabled Apply button. Reserved gestures and identical enabled
bindings cannot be saved. Swapping bindings remains pending until Apply or OK.

Gestures apply to the file-list background and its items, preserving selection.
Text fields, rename controls, headers, scrollbars, toolbars and menus retain
their native input. Escape, capture loss and cancellation abort a captured
gesture. Left-button movement retains the Windows move loop. Movement with
middle/right and all resizing share the application capture engine, preserving
normal size limits and geometry persistence.

The pair of bindings is stored atomically as the versioned `WindowShortcuts`
DWORD under the existing settings key. Invalid persisted data falls back to the
defaults. Existing windows read the current pair when a gesture begins, so
applied changes also work in other FileSpacer processes.

## Validation

Run from CMD, with production FileSpacer instances closed:

```cmd
tests.cmd -Pattern "24-*"
```

The native page/registry test enumerates all 2,304 pairs of action options:
1,584 valid pairs and 720 rejected pairs. It also checks exact modifier matching,
corrupt persisted values, pending edits, Revert, per-action defaults, swaps and
all 24 direct-input combinations. It runs with French and English resources.
The shared capture engine has 96 real HWND geometry/cancellation scenarios per
language. These do not stand in for Shell integration.

The separate Shell test sends real mouse and modifier input for all 32 enabled
action/binding choices, on both background and an unselected item in two
processes (64 scenarios), checking geometry, selection and the other window.
Existing Alt, resize, settings, activation-click, navigation and persistence
regressions remain available separately. The native Properties comparison checks
the target and selection with shortcuts disabled, with defaults, and with left
resize enabled. Test preferences and databases use
the runner's isolated namespace and are restored afterward.
