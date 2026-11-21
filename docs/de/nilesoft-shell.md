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

Nach dem Speichern einer NSS-Datei erzwingt **Strg + Rechtsklick** auf Desktop oder Taskleiste das Neuladen der Nilesoft-Konfiguration im Explorer, ohne Explorer neu zu starten oder die Erweiterung abzumelden. Nilesoft lädt auch neu, wenn beim Öffnen eines Menüs die linke Maustaste gedrückt ist; eine FileSpacer-Schaltfläche kann Änderungen daher sofort übernehmen. Das hängt von der Öffnungsgeste ab, nicht von einem schnelleren Neulademechanismus in FileSpacer.

<a id="menu-opening-delay"></a>

## Verzögerung beim Öffnen von Menüs

Um die [Verzögerung beim Öffnen von Menüs](https://nilesoft.org/docs/configuration/settings) von Nilesoft Shell aufzuheben, setzen Sie `showdelay = 0` im bestehenden `settings`-Block von `shell.nss`. Soll dies nur für FileSpacer gelten, verwenden Sie stattdessen `showdelay = if(process.name == 'FileSpacer', 0, 200)`. Ersetzen Sie `200` durch die Verzögerung in Millisekunden, die für andere Anwendungen beibehalten werden soll.

<a id="target-a-specific-menu"></a>

## Ein bestimmtes Menü ansprechen

Deklarieren Sie diese Variablen vor den Imports, die sie verwenden:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Verwenden Sie in den Regeln die passenden Eigenschaften `where` und `type`:

| Menü | `where` | `type` |
| --- | --- | --- |
| Dropdownmenü der Titelleiste | `filespacer_title` | `'*'` |
| Symbolleiste **…** und **Ansicht** | `filespacer_toolbar` | `'*'` |
| Nur **Ansicht** | `filespacer_view` | `'*'` |
| Nur Symbolleiste **…** | `filespacer_more` | `'*'` |
| Ausgewählte Dateien oder Ordner | `filespacer_context` | `'file\|dir'` |
| Ordnerhintergrund | `filespacer_context` | `'back'` |

Verwenden Sie `mode='multiple'` für Regeln, die unabhängig vom Auswahlzustand verfügbar bleiben sollen. `process.name` enthält kein `.exe`. `window.parent.name` bezeichnet eine Fensterklasse.

`filespacer_toolbar` umfasst weiterhin beide Symbolleistenmenüs. Verwenden Sie `filespacer_view` nur für Ansicht oder `filespacer_more` nur für **…**. Die interne Kennung `window.parent.title` ist stabil und wird nicht übersetzt. Damit Terminal außerhalb von Ansicht verfügbar bleibt, ergänzen Sie die bestehende `where`-Bedingung um `!filespacer_view &&` und setzen Sie die bisherige Bedingung in Klammern:

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

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

Für die drei Menüs – das Dropdownmenü der Titelleiste, **…** und **Ansicht** – empfehlen sich rechtwinklige Ecken, passend zu den Schaltflächen von FileSpacer. Ergänzen Sie Ihren bestehenden Block `theme { ... }` um diese Einstellung:

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
