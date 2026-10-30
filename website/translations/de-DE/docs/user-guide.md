# FileSpacer verwenden

## Fensternavigation

Jeder Ordner hat ein Fenster, das sich Position, Größe und Ansichtseinstellungen merkt. Wird ein bereits geöffneter Ordner erneut geöffnet, aktiviert FileSpacer dessen Fenster. Beim Verschieben oder Umbenennen des Ordners bleibt die gespeicherte Anordnung erhalten.

- Beim Öffnen eines Ordners oder seines übergeordneten Ordners wird das Ausgangsfenster standardmäßig geschlossen. Halten Sie Strg gedrückt, um dies einmalig umzukehren, oder ändern Sie den Standard unter **Einstellungen > Allgemein**. Beim Öffnen mehrerer ausgewählter Ordner bleibt das Ausgangsfenster geöffnet.
- Über das Menü neben dem Ordnertitel wechseln Sie zwischen geöffneten Ordnern. **Übergeordnete Fenster schließen** schließt die geöffneten Vorgänger dieses Ordners, lässt andere Ordner aber geöffnet.
- **Strg+Umschalt+T** oder **Zuletzt geschlossenes Fenster erneut öffnen** im Titelleistenmenü oder in der Sprungliste der Taskleiste öffnet einen geschlossenen Ordner erneut, ohne das aktuelle Fenster zu schließen. Anordnung und Auswahl werden wiederhergestellt; in den Ansichten Details und Liste auch die zuvor erste sichtbare Zeile. Der Taskleistenbefehl kann nacheinander weiter zurückliegende Schließvorgänge rückgängig machen.
- Über die Sprungliste der Taskleiste lassen sich auch Ordner öffnen, die im Schnellzugriff des Datei-Explorers angeheftet sind.

Beim Wechsel zum übergeordneten Ordner wird der Ausgangsordner nicht ausgewählt. Ein bereits geöffnetes übergeordnetes Fenster behält seine Auswahl. Die Systemsteuerung öffnet sich in einem eigenen Windows-Fenster.

![Das Ordnertitelmenü mit Fensternavigation und Sperre des gespeicherten Fensterzustands.](images/window-menu.png)

## Ansicht und Anordnung

### Symbolanordnung

In einer Symbolansicht können Sie Dateien und Unterordner frei platzieren, wenn **Symbole automatisch anordnen** deaktiviert ist. Ihre Positionen werden unabhängig von Fenstergröße und -position gespeichert, auch bei gesperrtem Fensterzustand.

### Ansichtsoptionen

**Vollständigen Namen einer ausgewählten Datei anzeigen** unter **Einstellungen > Anzeige** zeigt einen einzelnen ausgewählten Namen in der Symbolansicht vollständig an. Die Option ist standardmäßig aktiviert; Mehrfachauswahl und andere Ansichten bleiben unverändert.

![Ein langer ausgewählter Dateiname wird in der Symbolansicht vollständig angezeigt.](images/selected-name.png)

**Ansichtsmodi im Ansichtsmenü vorab anzeigen**, ebenfalls standardmäßig aktiviert, zeigt beim Darüberfahren eine Vorschau. Ein Klick übernimmt die Ansicht; Esc oder das Verlassen der Vorschau stellt die ursprüngliche Ansicht wieder her. Die gespeicherte Anordnung wird dabei nicht überschrieben.

**Ordner und Dateien beim Sortieren mischen** sortiert beide gemeinsam, wie unter [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) und [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Die Option ist unter **Einstellungen > Anzeige > Dateien** standardmäßig deaktiviert. Sie gilt für Details, Liste und automatisch angeordnete Symbole; freie Anordnungen bleiben erhalten. Virtuelle Ordner und Archive behalten die Windows-Sortierung.

![Ordner und Dateien in einer gemeinsamen alphabetischen Reihenfolge.](images/mixed-sorting.png)

Auch die Sichtbarkeit von Symbolleiste und Statusleiste lässt sich in den Einstellungen ändern.

### Fensterpositionierung

**Einstellungen > Allgemein > Neue Fenster auf freiem Platz anordnen** ist standardmäßig aktiviert. Ordner ohne gespeicherte Position öffnen sich auf freiem Platz; ist keiner mehr verfügbar, werden Überlappungen möglichst klein gehalten. Gespeicherte Positionen haben Vorrang. Bei deaktivierter Option verwendet FileSpacer die Standardpositionierung von Windows, ohne vorhandene Fenster zu verschieben.

### Auswahl und Aktivierung

Wenn **Einstellungen > Allgemein > Auswahl beim Klicken auf inaktives Fenster beibehalten** aktiviert ist, aktiviert ein Klick auf den Hintergrund eines inaktiven Ordnerfensters dieses Fenster, ohne die Auswahl aufzuheben. Mit einem zweiten Drücken beginnen Sie ein Auswahlrechteck. Klicks auf Elemente, Rechtsklicks und Fenstergesten verhalten sich wie gewohnt.

## Fenster verwalten

### Verschieben und Größenänderung

Fenster lassen sich durch Ziehen im Dateibereich verschieben oder in der Größe ändern, auch über einem Element. Standardmäßig gelten:

| Aktion | Tastenkombination |
| --- | --- |
| Fenster verschieben | Alt + Ziehen mit der linken Maustaste |
| Größe von der unteren rechten Ecke aus ändern | Alt + Ziehen mit der rechten Maustaste |

![Fenster mit Alt und der linken Maustaste verschieben.](images/move-window.gif)

![Fenstergröße mit Alt und der rechten Maustaste ändern.](images/resize-window.gif)

Die Auswahl bleibt erhalten. Esc bricht den Vorgang ab. Textfelder, Spaltenköpfe, Bildlaufleisten, Symbolleisten und Menüs funktionieren wie gewohnt; Alt + Doppelklick öffnet die Eigenschaften eines Elements.

Unter **Einstellungen > Tastenkombinationen** lässt sich jede Aktion einzeln konfigurieren. Linke und rechte Maustaste erfordern Alt; die mittlere funktioniert auch ohne Alt. Zusatztasten müssen genau übereinstimmen; reservierte oder doppelte Belegungen werden abgelehnt. Änderungen gelten auch für offene Fenster. Zurücksetzen stellt die zuletzt übernommenen Belegungen wieder her, Abbrechen verwirft ausstehende Änderungen.

### Gespeicherten Fensterzustand sperren

Eine Sperre hält Position, Größe und Ansicht eines Ordners für das erneute Öffnen fest. **Einstellungen > Allgemein > Sperren des Fensterzustands aktivieren** ist standardmäßig deaktiviert. Nach dem Aktivieren steht neben dem Ordnertitel **Fensterzustand sperren** zur Verfügung. Ein Schloss am Ordnersymbol kennzeichnet die Sperre.

<img src="images/locked-title.png" alt="Nahaufnahme des Ordnertitels mit Schlosssymbol." width="280">

Vorübergehende Änderungen bleiben möglich; beim erneuten Öffnen kehrt der gesperrte Zustand zurück. Einzelne Symbolpositionen werden weiterhin gespeichert. **Fensterzustand entsperren** behält die aktuelle Anordnung bei. Das Ausblenden des Schlosses hebt die Sperre nicht auf.

Das Deaktivieren der Funktion erfordert eine Bestätigung und entfernt alle Sperren, auch bei geschlossenen Ordnern. Erneutes Aktivieren stellt sie nicht wieder her.

## Suche und Verlauf

Geben Sie den Namen einer Datei oder eines Ordners ganz oder teilweise in das Suchfeld ein und drücken Sie die Eingabetaste oder klicken Sie auf die Lupe, um den aktuellen Ordner zu filtern. **Einstellungen > Allgemein > Beim Eingeben filtern** wendet den Filter bereits während der Eingabe an. Dieser lokale Filter startet Everything niemals automatisch.

Beim Aktivieren des Felds werden die letzten Suchanfragen und, falls aktiviert, **In Everything suchen** angezeigt. Wählen Sie mit Pfeil nach oben/unten und der Eingabetaste oder per Mausklick einen Eintrag aus. Ohne ausgewählten Vorschlag wendet die Eingabetaste den lokalen Filter an. Esc schließt die Liste, ohne den Text zu löschen; Strg+F aktiviert das Feld erneut.

Standardmäßig speichert jeder Ordner fünf unterschiedliche bestätigte Suchanfragen. In den allgemeinen Einstellungen können Sie die Grenze zwischen 0 und 20 festlegen; 0 leert und deaktiviert den Verlauf. Reines Tippen legt keinen Eintrag an. Siehe [Everything](#everything) für die Suche in Unterordnern oder allen indizierten Dateien.

![Suchvorschläge mit Everything und letzten Suchanfragen zu fiktiven Projektdateien.](images/search.png)

## Integration

### Windows-Integration

FileSpacer lässt sich bei der Installation oder später unter **Einstellungen > Integration** mit **Als Standard-Dateibrowser festlegen** zum Standard-Dateimanager machen. **Standard-Dateibrowser zurücksetzen** übergibt das Öffnen von Ordnern wieder an den Datei-Explorer.

Das Kontextmenü von Ordnern bietet **In FileSpacer öffnen**, beziehungsweise **Im Windows-Datei-Explorer öffnen**, wenn FileSpacer der Standard ist. Das gilt auch für Laufwerke, ZIP-Ordner, Netzwerkadressen und Ordnerhintergründe; die Standardanwendung muss dafür nicht geändert werden.

Dateien öffnen sich in der zugeordneten Windows-Anwendung. Auch Netzwerkordner, Vorschauen und installierte Kontextmenübefehle verwenden das gewohnte Windows-Verhalten. **Datei-Explorer-Optionen** öffnet gemeinsame Ordnereinstellungen, von denen einige auch für FileSpacer gelten.

### Everything

Ein vorhandenes [Everything](https://www.voidtools.com/) lässt sich einbinden, um erweiterte Suchanfragen direkt aus FileSpacers Suchfeld zu starten. Die Ergebnisse erscheinen in Everything.

Die Option wird unter **Einstellungen > Integration** aktiviert. Everything wird automatisch oder mit **Erkennen** gefunden; eine portable Ausgabe lässt sich manuell auswählen. Gesucht wird im aktuellen Ordner samt Unterordnern oder **Überall**. Virtuelle Ordner erfordern Überall.

**In Everything suchen** übergibt die eingegebene Anfrage in Everything-Syntax. Nach einem erfolgreichen Start werden FileSpacers Suchfeld und lokaler Filter geleert; bei einem Fehler bleiben beide für einen neuen Versuch erhalten.

![Eine Suchanfrage in FileSpacer und daneben die passenden Ergebnisse in Everything.](images/everything-search.png)

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) kann FileSpacers Menüs gestalten und ihre Befehle anpassen. Für ein stimmiges Erscheinungsbild empfiehlt sich eine angepasste Konfiguration, insbesondere mit eckigen Menüs an Titel- und Symbolleiste.

Die [Konfigurationsreferenz](nilesoft-shell.md) beschreibt die Freigabe für FileSpacer, das Beibehalten des Designs und die Beschränkung Explorer-spezifischer Regeln. Nach Änderungen wendet erneutes Ab- und Anmelden von Nilesoft Shell die Konfiguration auf neu geöffnete Menüs an.

Das folgende Modern-Design vergleicht die standardmäßigen Ecken mit den empfohlenen eckigen Menüs.

**Navigationsmenü**

| Standardecken | Empfohlene eckige Menüs |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Navigationsmenü der Titelleiste mit den standardmäßig abgerundeten Ecken des Modern-Themes." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Dasselbe Navigationsmenü mit den empfohlenen eckigen Menürändern." width="320"> |

**Hauptmenü (…)**

| Standardecken | Empfohlene eckige Menüs |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Hauptmenü mit den standardmäßig abgerundeten Ecken des Modern-Themes." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Dasselbe Hauptmenü mit den empfohlenen eckigen Menürändern." width="320"> |

## Einstellungen und Updates

Strg+, öffnet die Einstellungen auch bei ausgeblendeter Symbolleiste. Die Oberfläche folgt der Windows-Anzeigesprache, ersatzweise Englisch; ein Sprachwechsel erfordert einen Neustart. F1 öffnet das installierte Handbuch, das auch offline verfügbar ist.

Unter **Einstellungen > Info** lassen sich Updates suchen. Herunterladen und Installieren erfordern eine Bestätigung; die Installation erfolgt niemals automatisch. Anschließend werden die zuvor geöffneten Ordner mit ihrer gespeicherten Anordnung wiederhergestellt. Die optionalen wöchentlichen Prüfungen sind standardmäßig aus und können unter Info aktiviert werden.

## Lokale Daten

Ordneranordnungen, Sperren, Suchanfragen und der Verlauf geschlossener Ordner liegen lokal in `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. Allgemeine Einstellungen stehen in der Windows-Registrierung unter `HKEY_CURRENT_USER\Software\FileSpacer`. Der gespeicherte Zustand kann Ordnerpfade einschließlich Netzwerkpfaden enthalten. Temporäre Update-Dateien und die Liste der wieder zu öffnenden Ordner bleiben unter `%TEMP%\FileSpacerUpdate-{GUID}`.

**Einstellungen > Info > Wartung > Alle Ordnerzustände zurücksetzen** schließt die Ordnerfenster und löscht deren gespeicherte Anordnungen, Verläufe und Sperren. Dateien und allgemeine Einstellungen bleiben erhalten.

Zur Fehlerbehebung setzt `FileSpacer.exe /reset-options` allgemeine Einstellungen zurück; `/reset-all` löscht zusätzlich die gespeicherten Ordnerzustände. Beide Befehle lassen Dateien, Windows-Dateizuordnungen und vorhandene Diagnoseberichte unverändert.

## Datenschutz

FileSpacer enthält weder Telemetrie noch Nutzungsverfolgung oder Werbung. Absturzberichte werden nicht automatisch versendet. Diagnoseberichte bleiben unter `%LOCALAPPDATA%\FileSpacer`; ihre Weitergabe ist freiwillig und kann Dateipfade oder Teile des Prozessspeichers offenlegen.

Update-Prüfungen und Downloads verbinden sich per HTTPS mit GitHub. GitHub erhält übliche Verbindungsinformationen, darunter IP-Adresse und Anwendungsversion, aber keine Ordnerzustände, Dateipfade oder Diagnoseberichte.

Zugangsdaten für Netzlaufwerke werden wie im Datei-Explorer von Windows entgegengenommen und verarbeitet. FileSpacer liest oder speichert sie nicht. Bei einer Everything-Suche werden die Anfrage und bei einer Ordnersuche dessen Pfad an die lokale Everything-Anwendung übergeben.
