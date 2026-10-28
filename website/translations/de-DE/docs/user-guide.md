# FileSpacer verwenden

## Fensternavigation

Jeder Ordner hat ein Fenster, das sich Position, Größe und Ansichtseinstellungen merkt. Wird ein bereits geöffneter Ordner erneut geöffnet, aktiviert FileSpacer dessen Fenster. Beim Verschieben oder Umbenennen des Ordners bleibt die gespeicherte Anordnung erhalten.

- Beim Öffnen eines Ordners oder seines übergeordneten Ordners wird das Ausgangsfenster standardmäßig geschlossen. Halten Sie Strg gedrückt, um dies einmalig umzukehren, oder ändern Sie den Standard unter **Einstellungen > Allgemein**. Beim Öffnen mehrerer ausgewählter Ordner bleibt das Ausgangsfenster geöffnet.
- Über das Menü neben dem Ordnertitel wechseln Sie zwischen geöffneten Ordnern. **Übergeordnete Fenster schließen** schließt die geöffneten Vorgänger dieses Ordners, lässt andere Ordner aber geöffnet.
- **Strg+Umschalt+T** oder **Zuletzt geschlossenes Fenster erneut öffnen** im Titelleistenmenü oder in der Sprungliste der Taskleiste öffnet einen geschlossenen Ordner erneut, ohne das aktuelle Fenster zu schließen. Anordnung und Auswahl werden wiederhergestellt; in den Ansichten Details und Liste auch die zuvor erste sichtbare Zeile. Der Taskleistenbefehl kann nacheinander weiter zurückliegende Schließvorgänge rückgängig machen.
- Über die Sprungliste der Taskleiste lassen sich auch Ordner öffnen, die im Schnellzugriff des Datei-Explorers angeheftet sind.

Beim Wechsel zum übergeordneten Ordner wird der Ausgangsordner nicht ausgewählt. Ein bereits geöffnetes übergeordnetes Fenster behält seine Auswahl. Die Systemsteuerung öffnet sich in einem eigenen Windows-Fenster.

![Das Ordnertitelmenü mit Fensternavigation und Sperre des gespeicherten Fensterzustands.](images/window-menu.png)

*Das Titelleistenmenü, hier mit gesperrtem Fensterzustand.*

## Ansicht und Anordnung

### Symbolanordnung

Deaktivieren Sie in einer Symbolansicht **Symbole automatisch anordnen**, um Dateien und Unterordner frei zu platzieren. Ihre Positionen werden unabhängig von Fenstergröße und -position gespeichert, auch bei gesperrtem Fensterzustand.

### Ansichtsoptionen

Die Sichtbarkeit der Symbolleiste und der Statusleiste lässt sich unter **Einstellungen** ändern.

Unter **Einstellungen > Anzeige** sind zwei Optionen standardmäßig aktiviert:

- **Vollständigen Namen einer ausgewählten Datei anzeigen** hält einen ausgewählten Dateinamen in der Symbolansicht lesbar. Mehrfachauswahlen und andere Ansichten werden dadurch nicht verändert.
- **Ansichtsmodi im Ansichtsmenü vorab anzeigen** zeigt beim Zeigen auf einen Modus eine Vorschau. Klicken Sie, um sie zu übernehmen; Esc oder das Verlassen der Vorschau stellt die ursprüngliche Ansicht wieder her. Eine Vorschau ersetzt nicht die gespeicherte Anordnung.

**Ordner und Dateien beim Sortieren mischen** unter **Einstellungen > Anzeige > Dateien** ist standardmäßig deaktiviert. Aktivieren Sie die Option, um Ordner und Dateien gemeinsam zu sortieren, wie unter [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) und [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Sie gilt für Details, Liste und automatisch angeordnete Symbolansichten. Freie Symbolanordnungen bleiben erhalten. Virtuelle Ordner und Archive behalten die Windows-Sortierung bei.

![Ein Ordnerfenster mit Beispieldateien und -ordnern in einer gemeinsamen alphabetischen Reihenfolge.](images/mixed-sorting.png)

*Ordner und Dateien werden gemeinsam alphabetisch sortiert.*

### Fensterpositionierung

**Einstellungen > Allgemein > Neue Fenster auf freiem Platz anordnen** ist standardmäßig aktiviert. Ordner ohne gespeicherte Position öffnen sich auf freiem Platz; ist keiner mehr verfügbar, werden Überlappungen möglichst klein gehalten. Gespeicherte Positionen haben Vorrang. Bei deaktivierter Option verwendet FileSpacer die Standardpositionierung von Windows, ohne vorhandene Fenster zu verschieben.

### Auswahl und Aktivierung

Wenn **Einstellungen > Allgemein > Auswahl beim Klicken auf inaktives Fenster beibehalten** aktiviert ist, aktiviert ein Klick auf den Hintergrund eines inaktiven Ordnerfensters dieses Fenster, ohne die Auswahl aufzuheben. Mit einem zweiten Drücken beginnen Sie ein Auswahlrechteck. Klicks auf Elemente, Rechtsklicks und Fenstergesten verhalten sich wie gewohnt.

## Fenster verwalten

### Verschieben und Größenänderung

Ziehen Sie im Dateibereich, auf dem Hintergrund oder einem Element. Standardmäßig gelten diese Gesten:

| Aktion | Tastenkombination |
| --- | --- |
| Fenster verschieben | Alt + Ziehen mit der linken Maustaste |
| Größe an der unteren rechten Ecke ändern | Alt + Ziehen mit der rechten Maustaste |

**Fenster verschieben — Alt + Ziehen mit der linken Maustaste**

![Ein Ordnerfenster wird bei gedrückter Alt-Taste mit der linken Maustaste im Dateibereich verschoben.](images/move-window.gif)

**Fenstergröße ändern — Alt + Ziehen mit der rechten Maustaste**

![Ein Ordnerfenster wird bei gedrückter Alt-Taste mit der rechten Maustaste im Dateibereich vergrößert.](images/resize-window.gif)

Die Auswahl bleibt erhalten. Esc bricht die Geste ab und stellt die ursprüngliche Größe und Position wieder her. Textfelder, Überschriften, Bildlaufleisten, Symbolleisten und Menüs behalten ihr normales Verhalten. Alt + Doppelklick auf ein Element öffnet dessen Eigenschaften.

Unter **Einstellungen > Tastenkombinationen** aktivieren oder deaktivieren Sie die einzelnen Aktionen und klappen sie auf, um ihre Belegung zu ändern. Im Bereich zur direkten Eingabe lässt sich eine Kombination aus Tasten und Maustaste aufzeichnen. Die linke und rechte Maustaste benötigen Alt; mit der mittleren Maustaste sind auch Kombinationen ohne Alt möglich. Die Zusatztasten müssen genau übereinstimmen. Reservierte oder doppelt belegte Kombinationen lassen sich nicht übernehmen.

Die Schaltfläche für die Standardeinstellung setzt eine Aktion zurück. **Zurücksetzen** stellt die zuletzt übernommenen Tastenkombinationen wieder her; **Abbrechen** verwirft ausstehende Änderungen. Mit **Übernehmen** oder **OK** werden die Änderungen in den geöffneten FileSpacer-Fenstern wirksam.

### Gespeicherten Fensterzustand sperren

Aktivieren Sie **Einstellungen > Allgemein > Sperren des Fensterzustands aktivieren** (standardmäßig deaktiviert). Wählen Sie anschließend **Fensterzustand sperren** im Menü neben dem Ordnertitel. Die Sperre hält Position, Größe und Ansichtseinstellungen fest. Ein **Schlosssymbol auf dem Ordnersymbol in der Titelleiste**, neben dem Ordnernamen, kennzeichnet den gesperrten Zustand.

<img src="images/locked-title.png" alt="Nahaufnahme der Titelleiste: Ein Schlosssymbol überlagert das Ordnersymbol neben Studio project." width="420">

*Das Schlosssymbol kennzeichnet den gesperrten gespeicherten Zustand.*

Sie können das Fenster weiterhin vorübergehend verschieben, skalieren oder seine Ansicht ändern. Beim erneuten Öffnen wird der gesperrte Zustand wiederhergestellt. Einzelne Symbolpositionen werden weiter normal gespeichert. **Fensterzustand entsperren** behält die aktuelle Anordnung bei. Das Schlosssymbol lässt sich in den allgemeinen Einstellungen ausblenden, ohne die Sperre aufzuheben.

Beim Deaktivieren der Funktion wird nach einer Bestätigung gefragt. Anschließend werden alle gespeicherten Sperren entfernt, auch die geschlossener Ordner. Erneutes Aktivieren stellt sie nicht wieder her.

## Suche und Verlauf

Geben Sie Text in das Suchfeld ein und drücken Sie die Eingabetaste oder klicken Sie auf die Lupe, um den aktuellen Ordner zu filtern. **Einstellungen > Allgemein > Beim Eingeben filtern** wendet den Filter bereits während der Eingabe an. Dieser lokale Filter startet Everything niemals automatisch.

Beim Aktivieren des Felds werden die letzten Suchanfragen und, falls aktiviert, **In Everything suchen** angezeigt. Wählen Sie mit Pfeil nach oben/unten und der Eingabetaste oder per Mausklick einen Eintrag aus. Ohne ausgewählten Vorschlag wendet die Eingabetaste den lokalen Filter an. Esc schließt die Liste, ohne den Text zu löschen; Strg+F aktiviert das Feld erneut.

Standardmäßig speichert jeder Ordner fünf unterschiedliche bestätigte Suchanfragen. In den allgemeinen Einstellungen können Sie die Grenze zwischen 0 und 20 festlegen; 0 leert und deaktiviert den Verlauf. Reines Tippen legt keinen Eintrag an. Siehe [Everything](#everything) für die Suche in Unterordnern oder allen indizierten Dateien.

![Suchvorschläge mit Everything und letzten Suchanfragen zu fiktiven Projektdateien.](images/search.png)

*Everything ist eine eigene Aktion; die letzten Suchanfragen filtern den aktuellen Ordner.*

## Integration

### Windows-Integration

Das Installationsprogramm bietet **FileSpacer als Standard-Dateimanager festlegen** an; die Option ist standardmäßig nicht ausgewählt. Es installiert außerdem die Komponente für das Ordnerkontextmenü. Unter **Einstellungen > Integration** lässt sich die Vorgabe später mit **Als Standard-Dateibrowser festlegen** ändern; **Standard-Dateibrowser zurücksetzen** übergibt das Öffnen von Ordnern wieder an den Windows-Datei-Explorer.

Die Kontextmenükomponente bietet **Im Windows-Datei-Explorer öffnen** an, wenn FileSpacer der Standard ist, andernfalls **In FileSpacer öffnen**. Sie gilt für Ordner, Laufwerke, komprimierte Ordner, Netzwerkorte und Ordnerhintergründe. Die alleinige Installation dieser Komponente ändert die Standardanwendung nicht.

**Datei-Explorer-Optionen** auf derselben Einstellungsseite öffnet die Windows-Ordneroptionen. Einige davon wirken sich auch auf die Dateiansicht von FileSpacer aus.

### Everything

Installieren Sie [Everything](https://www.voidtools.com/) separat und aktivieren Sie es unter **Einstellungen > Integration**. Lassen Sie den Programmpfad zur automatischen Erkennung leer, wählen Sie **Erkennen** oder geben Sie bei einer portablen Installation die `.exe`-Datei selbst an.

Wählen Sie den Suchbereich: aktueller Ordner mit Unterordnern oder **Überall**. Virtuelle Ordner haben keinen Suchbereich im Dateisystem; verwenden Sie dafür Überall.

Geben Sie eine Suchanfrage in das FileSpacer-Suchfeld ein und wählen Sie **In Everything suchen** aus den Vorschlägen. Die Ergebnisse werden in Everything geöffnet, mit dessen Index und Suchsyntax. Nach erfolgreichem Start leert FileSpacer das Suchfeld und hebt den lokalen Filter auf. Bei einem Startfehler bleiben beide für einen erneuten Versuch erhalten.

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) kann die FileSpacer-Menüs gestalten und ihre Einträge anpassen. Es ist optional und wird separat installiert.

Lassen Sie FileSpacer in der Ausschlussregel Ihrer Nilesoft-Konfiguration zu und behalten Sie den Import Ihres Themes bei. Regeln, die nur für den Explorer gedacht sind, sollten auf ihn beschränkt werden, damit sie nicht auch die FileSpacer-Menüs verändern.

Die [Konfigurationsreferenz mit Beispielen](nilesoft-shell.md) beschreibt die genauen Einstellungen, menüspezifischen Regeln und die empfohlene Darstellung. Nach einer Konfigurationsänderung müssen Sie die Registrierung von Nilesoft Shell aufheben und es erneut registrieren, damit die Änderung für neu geöffnete Menüs gilt.

Das folgende Modern-Theme zeigt links die standardmäßig abgerundeten Ecken und rechts die empfohlenen eckigen Menüs.

**Navigationsmenü**

| Standardecken | Empfohlene eckige Menüs |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Navigationsmenü der Titelleiste mit den standardmäßig abgerundeten Ecken des Modern-Themes." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Dasselbe Navigationsmenü mit den empfohlenen eckigen Menürändern." width="320"> |

**Hauptmenü (…)**

| Standardecken | Empfohlene eckige Menüs |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Hauptmenü mit den standardmäßig abgerundeten Ecken des Modern-Themes." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Dasselbe Hauptmenü mit den empfohlenen eckigen Menürändern." width="320"> |

## Einstellungen und Updates

**Einstellungen.** **Strg+,** öffnet die Einstellungen auch bei ausgeblendeter Symbolleiste. Die Oberfläche folgt der Windows-Anzeigesprache; ist sie nicht verfügbar, wird Englisch verwendet. Ein Sprachwechsel erfordert einen Neustart.

**Updates.** Suchen Sie unter **Einstellungen > Info** nach einer neuen Version und wählen Sie **FileSpacer vX.Y.Z installieren**. Die Installation erfordert eine Bestätigung; Updates werden niemals automatisch installiert. Nach der Installation öffnet FileSpacer die zuvor geöffneten Ordner mit ihrer gespeicherten Anordnung erneut. Die optionalen wöchentlichen Prüfungen sind standardmäßig deaktiviert und lassen sich unter Info aktivieren.

**Wartung.** **Einstellungen > Info > Wartung > Alle Ordnerzustände zurücksetzen** schließt geöffnete Ordnerfenster und löscht deren gespeicherte Anordnungen, Verläufe und Fenstersperren. Ihre Dateien und globalen Einstellungen bleiben erhalten.

**Dokumentation.** Wählen Sie **Dokumentation** oberhalb von **FileSpacer-Einstellungen** im Hauptmenü, drücken Sie **F1** oder verwenden Sie die Schaltfläche unter **Einstellungen > Info**. Das installierte Handbuch wird im Browser in der Windows-Anzeigesprache geöffnet; ersatzweise wird Englisch verwendet. Seiten, Bilder und Suche funktionieren offline. Über die Sprachauswahl können Sie die Übersetzung wechseln.
