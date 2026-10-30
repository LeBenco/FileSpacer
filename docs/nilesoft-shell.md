# Nilesoft Shell configuration reference

For an overview, see [Nilesoft Shell in the user guide](user-guide.md#nilesoft-shell).

If you use [Nilesoft Shell](https://nilesoft.org/), you can also apply its themes and menu customizations to FileSpacer through `shell.nss` and its imported configuration files. For this optional integration, configuration adjustments are recommended so the menus fit FileSpacer's appearance. The examples below cover rules to add, modify or remove menu items, conditions specific to FileSpacer, and the recommended square corners.

## Scope system-menu rules

For rules intended for Windows Explorer, the desktop or the taskbar, add `process.is_explorer &&` to their existing `where` condition. To preserve their scope in other applications while excluding FileSpacer, use `process.name != 'FileSpacer' &&` instead. Apply the filter to generic `menu`, `item`, `modify` and `remove` rules, including those in imported files.

## Apply themes to FileSpacer

Allow FileSpacer in `settings.exclude.where` and keep your theme import. For the usual exclusion condition:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Theme properties can also use process-specific expressions: for example, `dark = if(process.name == 'FileSpacer', true, sys.dark)` inside `theme { ... }` selects dark mode for FileSpacer and the system preference elsewhere.

When using FileSpacer instead of File Explorer, theme changes apply directly to newly opened menus after unregistering and registering Nilesoft Shell again, without restarting Explorer.

## Target a specific menu

Declare these variables before the imports that use them:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Use the corresponding `where` and `type` properties on your rules:

| Menu                      | `where`              | `type`        |
| ------------------------- | -------------------- | ------------- |
| Title-bar drop-down       | `filespacer_title`   | `'*'`         |
| Toolbar **…**             | `filespacer_toolbar` | `'*'`         |
| Selected files or folders | `filespacer_context` | `'file\|dir'` |
| Folder background         | `filespacer_context` | `'back'`      |

Use `mode='multiple'` for rules that should remain available across selection states. `process.name` omits `.exe`; `window.parent.name` identifies a window class.

## Target an existing menu entry

Nilesoft's general `modify(find=...)` mechanism matches an entry by its label. Combine it with `where` to limit the change to a menu; `|` separates alternative labels and `*` is a wildcard:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin' title='Copy folder path')
```

See the Nilesoft [configuration reference](https://nilesoft.org/docs/configuration/).

## Recommended menu appearance

Square corners are recommended for the title-bar and toolbar menus to match FileSpacer's buttons. Using the variables above, add this setting to your existing `theme { ... }` block:

```nss
border.radius = if(
    process.name == 'FileSpacer' &&
    (
        window.parent.name == 'FileSpacer Item Window' ||
        window.parent.name == 'FileSpacer Toolbar Host'
    ),
    0,
    default
)
```

If you already use a custom radius, replace `default` with its previous value.
