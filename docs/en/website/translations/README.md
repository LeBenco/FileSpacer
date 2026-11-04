# Website translations

The repository's main README and documentation remain English sources. These
complete Markdown variants supply the website and localized GitHub references;
they do not add language links or controls to the repository README.

Each translated `docs/<language>/` directory contains `locale.json` (including
its language-region locale) and four source files: `README.md`, `user-guide.md`,
`CHANGELOG.md`, and `THIRD_PARTY_NOTICES.md`. Privacy details and local-data
maintenance belong in separate user-guide sections; `privacy-policy.md`
remains a short localized policy for GitHub.
The directory also contains `nilesoft-shell.md`, a complete localized reference linked
from the user guide and kept as a separate GitHub document. A missing reference
fails generation instead of silently linking readers to English.
The generator discovers these directories. Its English input remains the
root README, changelog and third-party notices plus `docs/en/user-guide.md`, with
interface labels from `website/locale.json`. Technical documents also live under
their language directory. Documents with colliding basenames keep a subdirectory:
the translated technical index is `technical/README.md`, and the short build
entry point is `building/BUILD.md`. Test and website guides retain their respective
subdirectories. Existing document and image basenames are unchanged.

When updating a translation, keep the heading order and hierarchy identical to
its English reference. Each locale's generated `source-state.json` records the
English content hashes and section IDs used for its last synchronization.
English edits are published immediately. Translations retain their existing
text and section IDs; outdated documents produce workflow warnings and a summary
table instead of blocking publication. Text-only edits are detected too.

After translating the changed documents, record their English reference from
CMD at the repository root (replace the locale as appropriate):

```cmd
node website\generate.mjs --record-translation-sources fr-FR
```

Omit the locale to record all translations. Commit the generated source-state
files with the updated Markdown. Recording validates matching headings and links;
it does not translate text or infer whether its meaning is up to date. Do not
record an English edit until its translation is complete. Missing documents,
broken links or media and translation structure inconsistent with its recorded
reference remain errors.

The recorded English IDs keep existing section links working when another
language's headings change. Links can use either a translated heading slug or
the original English slug. Referenced user
documentation links to the selected language's GitHub file. Polish, Dutch and
Ukrainian also provide translated developer documents outside the website;
their links use those files. Other languages retain the English technical sources.
Keep explicit HTML anchor IDs unchanged across languages.

Use explicit relative references to the language's own `images/` directory,
including existing suffixes such as `spatial-windows-de.png`. The generator copies
the referenced files without inferring another filename, while
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
| Polski | Eksplorator plików | folder | pasek zadań | skróty klawiaturowe |
| Nederlands | Bestandenverkenner | map | taakbalk | sneltoetsen |
| Українська | Файловий провідник | папка | панель завдань | сполучення клавіш |

Style references: [Microsoft File Explorer](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows)
for large commercial software, [Everything search](https://www.voidtools.com/support/everything/searching/)
for a smaller utility, and GNOME Files in [French](https://help.gnome.org/gnome-help/files-browse.html.fr)
and [German](https://help.gnome.org/gnome-help/files-browse.html.de) for open-source documentation.

Terminology references:

- French: [File Explorer](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/fr-fr/accessibility/windows/keyboard-shortcuts-in-windows).
- German: [File Explorer](https://support.microsoft.com/de-de/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/de-de/accessibility/windows/keyboard-shortcuts-in-windows).
- Italian: [File Explorer](https://support.microsoft.com/it-it/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/it-it/accessibility/windows/keyboard-shortcuts-in-windows).
- Spanish: [File Explorer](https://support.microsoft.com/es-es/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/es-es/accessibility/windows/keyboard-shortcuts-in-windows).
- European Portuguese: [File Explorer](https://support.microsoft.com/pt-pt/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/pt-pt/accessibility/windows/keyboard-shortcuts-in-windows).
- Greek: [File Explorer](https://support.microsoft.com/el-gr/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/el-gr/accessibility/windows/keyboard-shortcuts-in-windows).
- Polish: [File Explorer](https://support.microsoft.com/pl-pl/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/pl-pl/accessibility/windows/keyboard-shortcuts-in-windows).
- Dutch: [File Explorer](https://support.microsoft.com/nl-nl/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/nl-nl/accessibility/windows/keyboard-shortcuts-in-windows).
- Ukrainian: [File Explorer](https://support.microsoft.com/uk-ua/windows/experience/fileexplorer/file-explorer-in-windows), [keyboard shortcuts](https://support.microsoft.com/uk-ua/accessibility/windows/keyboard-shortcuts-in-windows).
- [Microsoft localization style guides](https://learn.microsoft.com/en-us/globalization/reference/microsoft-style-guides).
- [Windows icon handlers in Portuguese](https://learn.microsoft.com/pt-pt/windows/win32/shell/how-to-create-icon-handlers).
- [CC0 public-domain terminology in Greek](https://creativecommons.org/publicdomain/zero/1.0/deed.el).
