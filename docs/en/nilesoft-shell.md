<a id="nilesoft-shell-configuration-reference"></a>

# Nilesoft Shell configuration reference

For an overview, see [Nilesoft Shell in the user guide](user-guide.md#nilesoft-shell).

If you use [Nilesoft Shell](https://nilesoft.org/), you can also apply its themes and menu customizations to FileSpacer through `shell.nss` and its imported configuration files. For this optional integration, configuration adjustments are recommended so the menus fit FileSpacer's appearance. The examples below cover rules to add, modify or remove menu items, conditions specific to FileSpacer, and the recommended square corners.

<a id="scope-system-menu-rules"></a>

## Scope system-menu rules

For rules intended for Windows Explorer, the desktop or the taskbar, add `process.is_explorer &&` to their existing `where` condition. To preserve their scope in other applications while excluding FileSpacer, use `process.name != 'FileSpacer' &&` instead. Apply the filter to generic `menu`, `item`, `modify` and `remove` rules, including those in imported files.

<a id="apply-themes-to-filespacer"></a>

## Apply themes to FileSpacer

Allow FileSpacer in `settings.exclude.where` and keep your theme import. For the usual exclusion condition:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Theme properties can also use process-specific expressions: for example, `dark = if(process.name == 'FileSpacer', true, sys.dark)` inside `theme { ... }` selects dark mode for FileSpacer and the system preference elsewhere.

After saving an NSS file, **Ctrl + right-click** on the desktop or taskbar forces Nilesoft to reload its configuration in Explorer, without restarting Explorer or unregistering the extension. Nilesoft also reloads when it detects a left button held during menu opening; opening a FileSpacer button can therefore apply edits immediately. This depends on the opening gesture, not on a faster reload mechanism in FileSpacer.

<a id="menu-opening-delay"></a>

## Menu opening delay

To remove Nilesoft Shell’s [menu opening delay](https://nilesoft.org/docs/configuration/settings), set `showdelay = 0` in the existing `settings` block of `shell.nss`. To apply this only to FileSpacer, use `showdelay = if(process.name == 'FileSpacer', 0, 200)` instead, replacing `200` with the delay to keep for other applications, in milliseconds.

<a id="target-a-specific-menu"></a>

## Target a specific menu

Declare these variables before the imports that use them:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Use the corresponding `where` and `type` properties on your rules:

| Menu                      | `where`              | `type`        |
| ------------------------- | -------------------- | ------------- |
| Title-bar drop-down       | `filespacer_title`   | `'*'`         |
| Toolbar **…** and **View** | `filespacer_toolbar` | `'*'` |
| View only | `filespacer_view` | `'*'` |
| Toolbar **…** only | `filespacer_more` | `'*'` |
| Selected files or folders | `filespacer_context` | `'file\|dir'` |
| Folder background         | `filespacer_context` | `'back'`      |

Use `mode='multiple'` for rules that should remain available across selection states. `process.name` omits `.exe`. `window.parent.name` identifies a window class.

`filespacer_toolbar` still covers both toolbar menus. Use `filespacer_view` for View alone or `filespacer_more` for **…** alone. The internal marker `window.parent.title` is stable and is not translated. To keep Terminal outside View, add `!filespacer_view &&` to its existing `where` condition, enclosing that condition in parentheses:

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

<a id="target-an-existing-menu-entry"></a>

## Target an existing menu entry

Nilesoft's general `modify(find=...)` mechanism matches an entry by its label. Combine it with `where` to limit the change to a menu. `|` separates alternative labels and `*` is a wildcard:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin' title='Copy folder path')
```

See the Nilesoft [configuration reference](https://nilesoft.org/docs/configuration/).

<a id="recommended-menu-appearance"></a>

## Recommended menu appearance

Square corners are recommended for the three menus: the title-bar drop-down, **…** and **View**, to match FileSpacer’s buttons. Add this setting to your existing `theme { ... }` block:

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

<a id="use-filespacer-actions"></a>

## Use FileSpacer actions

Use [`window.command`](https://nilesoft.org/docs/functions/window) with one of the IDs below to add a FileSpacer action, even if it is absent from FileSpacer’s own menu composition. Keep the rule restricted to the title-bar menu with the `filespacer_title` condition defined above. The action’s usual requirements still apply: restoring a locked layout, for example, requires an existing lock.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Restore the locked layout' cmd=window.command(1123))
```

| Action | ID |
| --- | ---: |
| Restore the locked layout | 1123 |
| Replace the reference layout | 1124 |
| Close descendant windows | 1126 |
| Close other windows | 1127 |
| Fit the window to its contents | 1128 |
| Save / recall a folder group | 1129 |
