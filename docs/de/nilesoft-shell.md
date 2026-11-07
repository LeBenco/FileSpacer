<a id="nilesoft-shell-configuration-reference"></a>

# Konfigurationsreferenz für Nilesoft Shell

Einen Überblick bietet [Nilesoft Shell im Benutzerhandbuch](user-guide.md#nilesoft-shell).

Wenn Sie [Nilesoft Shell](https://nilesoft.org/) verwenden, können Sie dessen Themes und Menüanpassungen auch über `shell.nss` und die dort importierten Konfigurationsdateien auf FileSpacer anwenden. Für diese optionale Integration empfiehlt es sich, die Konfiguration anzupassen, damit die Menüs optisch zu FileSpacer passen. Die folgenden Beispiele zeigen Regeln zum Hinzufügen, Ändern und Entfernen von Einträgen, Bedingungen für FileSpacer und die empfohlenen rechtwinkligen Ecken.

<a id="scope-system-menu-rules"></a>

## Regeln für Systemmenüs eingrenzen

Ergänzen Sie bei Regeln für den Datei-Explorer, den Desktop oder die Taskleiste die bestehende `where`-Bedingung um `process.is_explorer &&`. Soll die Regel weiterhin für andere Anwendungen gelten, FileSpacer jedoch ausschließen, verwenden Sie stattdessen `process.name != 'FileSpacer' &&`. Wenden Sie den Filter auf allgemeine `menu`-, `item`-, `modify`- und `remove`-Regeln an, auch in importierten Dateien.

<a id="apply-themes-to-filespacer"></a>

## Themes auf FileSpacer anwenden

Lassen Sie FileSpacer in `settings.exclude.where` zu und behalten Sie den Theme-Import bei. Für die übliche Ausschlussbedingung:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Theme-Eigenschaften können auch prozessabhängige Ausdrücke verwenden. Beispielsweise wählt `dark = if(process.name == 'FileSpacer', true, sys.dark)` innerhalb von `theme { ... }` für FileSpacer den dunklen Modus und andernorts die Systemeinstellung.

Bei FileSpacer gelten Theme-Änderungen nach dem Aufheben und erneuten Einrichten der Registrierung von Nilesoft Shell für neu geöffnete Menüs. Ein Neustart des Explorers ist nicht erforderlich.

<a id="target-a-specific-menu"></a>

## Ein bestimmtes Menü ansprechen

Deklarieren Sie diese Variablen vor den Imports, die sie verwenden:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Verwenden Sie in den Regeln die passenden Eigenschaften `where` und `type`:

| Menü | `where` | `type` |
| --- | --- | --- |
| Dropdownmenü der Titelleiste | `filespacer_title` | `'*'` |
| Symbolleiste **…** | `filespacer_toolbar` | `'*'` |
| Ausgewählte Dateien oder Ordner | `filespacer_context` | `'file\|dir'` |
| Ordnerhintergrund | `filespacer_context` | `'back'` |

Verwenden Sie `mode='multiple'` für Regeln, die unabhängig vom Auswahlzustand verfügbar bleiben sollen. `process.name` enthält kein `.exe`. `window.parent.name` bezeichnet eine Fensterklasse.

<a id="target-an-existing-menu-entry"></a>

## Einen bestehenden Menüeintrag ansprechen

Der allgemeine Nilesoft-Mechanismus `modify(find=...)` findet einen Eintrag anhand seiner Beschriftung. Kombinieren Sie ihn mit `where`, um die Änderung auf ein Menü zu beschränken. `|` trennt alternative Beschriftungen. `*` ist ein Platzhalter:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Pfad kopieren' title='Ordnerpfad kopieren')
```

Siehe die [Konfigurationsreferenz von Nilesoft](https://nilesoft.org/docs/configuration/).

<a id="recommended-menu-appearance"></a>

## Empfohlene Menüdarstellung

Für die Menüs der Titelleiste und der Symbolleiste empfehlen sich rechtwinklige Ecken, passend zu den Schaltflächen von FileSpacer. Ergänzen Sie Ihren bestehenden Block `theme { ... }` um diese Einstellung:

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

Wenn Sie bereits einen eigenen Eckenradius verwenden, ersetzen Sie `default` durch dessen bisherigen Wert.

<a id="use-filespacer-actions"></a>

## FileSpacer-Aktionen verwenden

Mit [`window.command`](https://nilesoft.org/docs/functions/window) und einer der folgenden IDs lässt sich eine FileSpacer-Aktion hinzufügen, auch wenn sie in dessen eigener Menüzusammenstellung fehlt. Die Regel mit der oben definierten Bedingung `filespacer_title` auf das Titelleistenmenü beschränken. Die üblichen Voraussetzungen gelten weiterhin: Ein gesperrtes Layout kann beispielsweise nur wiederhergestellt werden, wenn bereits eine Sperre besteht.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Gesperrtes Fensterlayout wiederherstellen' cmd=window.command(1123))
```

| Aktion | ID |
| --- | ---: |
| Gesperrtes Fensterlayout wiederherstellen | 1123 |
| Referenzlayout ersetzen | 1124 |
| Unterordnerfenster schließen | 1126 |
| Andere Fenster schließen | 1127 |
| Fenster an Inhalt anpassen | 1128 |
| Ordnergruppe speichern / öffnen | 1129 |
