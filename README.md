# FileSpacer

FileSpacer is a file manager for Microsoft Windows with a fully spatial interface: folders open in separate windows that retain their position, size, and display settings. By default, opening a folder closes the window it was opened from, avoiding the accumulation of windows and keeping the desktop uncluttered.

Windows File Explorer does not provide all the behaviors needed for a consistent, efficient spatial workflow. FileSpacer aims to provide them:

- Each folder is represented by a single window, permanently and unambiguously tied to that folder.
- Folders retain their spatial state: window position, size, and view settings.
- A folder window follows its folder when it is moved or renamed.
- When a folder is deleted, its window closes.
- Windows' Auto-arrange feature can be turned off, allowing icons to be positioned freely. Their positions are saved with the folder settings and restored the next time the folder is opened.
- By default, opening a folder closes the source window. This behavior can be reversed in Settings; holding Ctrl reverses it for an individual operation.

FileSpacer is compatible with [Nilesoft Shell](https://nilesoft.org/) for menu theming and targeted customization.

FileSpacer is a fork of [ChromaFiler](https://github.com/vanjac/chromafiler), created by J. van't Hoog. It started as a small fork to remove the Miller-column approach, which I am not particularly fond of, and quickly grew into a project of its own. Many thanks to vanjac: his work provided an excellent foundation!

## Build and use

Windows 10 or later, x64. The interface includes English and French resources. See [BUILD.md](BUILD.md) for the MSVC/NMAKE build and NSIS installer, and the [user guide](docs/user-guide.md) for navigation and maintenance commands.

## Nilesoft Shell

Configure `shell.nss` and its imported files in the Nilesoft Shell installation directory.

### Scope system-menu rules

For rules intended for Windows Explorer, the desktop or the taskbar, add `process.is_explorer &&` to their existing `where` condition. To preserve their scope in other applications while excluding FileSpacer, use `process.name != 'FileSpacer' &&` instead. Apply the filter to generic `menu`, `item`, `modify` and `remove` rules, including those in imported files.

### Apply themes to FileSpacer

Allow FileSpacer in `settings.exclude.where` and keep your theme import. For the usual exclusion condition:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Theme properties can also use process-specific expressions: for example, `dark = if(process.name == 'FileSpacer', true, sys.dark)` inside `theme { ... }` selects dark mode for FileSpacer and the system preference elsewhere.

When using FileSpacer instead of File Explorer, theme changes apply directly to newly opened menus after unregistering and registering Nilesoft Shell again, without restarting Explorer.

### Target a specific menu

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

### Target an existing menu entry

Nilesoft's general `modify(find=...)` mechanism matches an entry by its label. Combine it with `where` to limit the change to a menu; `|` separates alternative labels and `*` is a wildcard:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin' title='Copy folder path')
```

See the Nilesoft [configuration reference](https://nilesoft.org/docs/configuration/).

### Recommended menu appearance

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

## License and privacy

FileSpacer is distributed under the [GNU General Public License v3.0](LICENSE). Copyright (c) 2026 Benco, for the FileSpacer modifications. Original ChromaFiler code: Copyright (c) 2023 J. van't Hoog.

See [third-party notices](THIRD_PARTY_NOTICES.md) and the [privacy policy](docs/privacy-policy.md).
