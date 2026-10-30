# Website translations

The repository's main README and documentation remain English. These complete
Markdown variants are inputs for the website only; they do not add language
links or controls to the repository README.

Each language-region directory contains `locale.json` and the same four source
paths used by the English site: `README.md`, `docs/user-guide.md`,
`CHANGELOG.md`, and `THIRD_PARTY_NOTICES.md`. Privacy details and local-data
maintenance belong in separate user-guide sections; `docs/privacy-policy.md`
remains a short localized policy for GitHub.
The directory also contains `docs/nilesoft-shell.md`, a complete localized reference linked
from the user guide and kept as a separate GitHub document. A missing reference
fails generation instead of silently linking readers to English.
The generator discovers these directories. Its English input remains the
original documents, with interface labels from `website/locale.json`.

Keep the heading order and hierarchy identical to English. The generator uses
the English heading IDs in every language, so switching language preserves the
section being read and existing public anchors keep working. Links can use
either a translated heading slug or the original English slug. Referenced user
documentation links to the selected language's GitHub file. Developer documents
outside the website still link to the English GitHub source.

Keep image references under their original English filenames. The generator
substitutes real language-specific captures such as `spatial-windows-de.png`, while
the neutral application logo is shared. It fails when an included capture or
animation is missing. Do not translate text inside exported images.

The generated site is built separately for each language with the same pinned
Just the Docs theme. Each build has its own navigation, search index, interface
labels and media; the deployment workflow discovers the build matrix from the
generator's `locales.json` output.

## Terminology and style

Use the terminology of the localized Windows documentation for operating-system
concepts. Preserve the labels actually displayed by FileSpacer when giving an
exact settings path. For concepts not named by Windows, use established software
terminology and natural phrasing. Preserve the meaning, examples, links and
restrained humour of the English text rather than translating word for word.
License files, command names, registry paths, identifiers and executable names
remain unchanged.

Prefer short descriptions of behavior and useful subtleties over instructions
for self-explanatory controls. In French, use neutral descriptions or infinitives
for actions and avoid repeated direct address. This is an editorial choice, not
a universal documentation convention: Microsoft and GNOME use direct address in
French, Everything favors concise English commands, and German documentation
commonly retains formal address. Follow each language's idiom rather than forcing
French sentence patterns onto it. Place screenshots after the behavior they
illustrate; keep descriptive alternative text instead of a redundant caption.

The Everything screenshot uses its available Brazilian Portuguese translation
alongside FileSpacer and fictional names in European Portuguese. Everything does
not provide a European Portuguese translation in its official
[language list](https://www.voidtools.com/support/everything/supported_languages/).

| Language | File Explorer | Folder | Taskbar | Keyboard shortcuts |
| --- | --- | --- | --- | --- |
| Français | Explorateur de fichiers | dossier | barre des tâches | raccourcis clavier |
| Deutsch | Datei-Explorer | Ordner | Taskleiste | Tastenkombinationen |
| Italiano | Esplora file | cartella | barra delle applicazioni | scelte rapide da tastiera |
| Español | Explorador de archivos | carpeta | barra de tareas | métodos abreviados de teclado |
| Português (Portugal) | Explorador de Ficheiros | pasta | barra de tarefas | atalhos de teclado |
| Ελληνικά | Εξερεύνηση αρχείων | φάκελος | γραμμή εργασιών | συντομεύσεις πληκτρολογίου |

Style comparison checked on 2026-10-30: [Microsoft File Explorer](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows)
for large commercial software, [Everything search](https://www.voidtools.com/support/everything/searching/)
for a smaller utility, and GNOME Files in [French](https://help.gnome.org/gnome-help/files-browse.html.fr)
and [German](https://help.gnome.org/gnome-help/files-browse.html.de) for open-source documentation.

Terminology references checked on 2026-10-28:

- French: [File Explorer](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/fr-fr/accessibility/windows/keyboard-shortcuts-in-windows).
- German: [File Explorer](https://support.microsoft.com/de-de/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/de-de/accessibility/windows/keyboard-shortcuts-in-windows).
- Italian: [File Explorer](https://support.microsoft.com/it-it/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/it-it/accessibility/windows/keyboard-shortcuts-in-windows).
- Spanish: [File Explorer](https://support.microsoft.com/es-es/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/es-es/accessibility/windows/keyboard-shortcuts-in-windows).
- European Portuguese: [File Explorer](https://support.microsoft.com/pt-pt/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/pt-pt/accessibility/windows/keyboard-shortcuts-in-windows).
- Greek: [File Explorer](https://support.microsoft.com/el-gr/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/el-gr/accessibility/windows/keyboard-shortcuts-in-windows).
- [Microsoft localization style guides](https://learn.microsoft.com/en-us/globalization/reference/microsoft-style-guides).
- [Windows icon handlers in Portuguese](https://learn.microsoft.com/pt-pt/windows/win32/shell/how-to-create-icon-handlers).
- [CC0 public-domain terminology in Greek](https://creativecommons.org/publicdomain/zero/1.0/deed.el).
