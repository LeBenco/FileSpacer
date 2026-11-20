<a id="changelog"></a>

# Versionsverlauf

<a id="150"></a>

## 1.5.0

- Freie Symbolpositionen beim Wechsel zu Inhalt und zurück beibehalten, auch nach erneutem Öffnen des Ordners.
- Drei sich gegenseitig ausschließende Modi für die Symbolanordnung hinzufügen: Automatisch, Am Raster und Frei, mit einer Vorschau der Zielposition beim Ziehen in den Modi Automatisch und Am Raster.
- Das Menü Ansicht schneller öffnen und dabei die Nilesoft-Anpassung beibehalten.
- Die native Anordnung in Zeilen in den Ansichten Kacheln und Inhalt beibehalten, ohne freie Symbolplatzierung, Neuausrichtung oder Snap.
- Die acht Ansichten des Datei-Explorers hinzufügen: Extra große Symbole, Große Symbole, Mittelgroße Symbole, Kleine Symbole, Liste, Details, Kacheln und Inhalt, mit den Tastenkombinationen Strg+Umschalt+1 bis 8.
- Den Befehl **Symbole neu ausrichten** zum einmaligen Ausrichten der aktuellen Anordnung sowie **Snap** beim Ziehen hinzufügen: **S** gedrückt halten, um nur die gezogenen Symbole am Windows-Raster auszurichten, ohne die anderen zu verschieben. Ein durchscheinender Rahmen in Symbolgröße zeigt das Ziel an.
- Datenbank bei Beschädigung neu aufbauen.
- „In FileSpacer öffnen“ in den Ordner-Kontextmenüs aller Programme anbieten.
- Mehrere Anzeige- und Eingabeprobleme beheben: Verlust frei gesetzter Positionen bei kleinen Symbolen, vom Datei-Explorer abweichende Strg+Mausrad-Schritte, automatische Wiederholung der Rücktaste, ungültige Zahlenwerte in den Einstellungen, Ausgangspunkt der Öffnungsanimation bei unterschiedlichen DPI-Werten, Bildlaufposition beim erneuten Öffnen unter Windows 11 sowie langes Einfrieren und verstreute Symbole beim erneuten Öffnen eines großen Ordners bei einer Anzeigeskalierung über 100 %.
- Die Proportionen fensterfüllender Spalten in der Detailansicht beim Schließen und erneuten Öffnen eines Ordners beibehalten.
- Eine Option zum Abschalten des Downloads von Microsoft-Symbolen zur Beschleunigung großer Ordner hinzufügen und beim Download eine Benachrichtigung anzeigen.
- Die täglichen Versuche zur Vorbereitung der Beschleunigung großer Ordner nach drei gleichen Fehlern beenden und die Daten für ersetzte Windows-Versionen entfernen.
- Die Beschleunigung der Auswahl in großen Ordnern auf aktuellen Windows-11-Versionen wiederherstellen, nachdem ein Windows-Update sie deaktiviert hatte.
- Strg+Rücktaste die Einstellung des Quellfensters umkehren lassen, wie Strg bei allen anderen Arten, einen Ordner zu öffnen.
- Beim Einlesen eines großen oder langsamen Ordners einen Fortschrittsbalken in der Adressleiste und auf der Taskleistenschaltfläche anzeigen.

<a id="142--2026-11-12"></a>

## 1.4.2 — 2026-11-12

- Native Rechtsklick-Kontextmenüs für Ordner im Schnellzugriff hinzufügen.
- Das Symbol der Schnellzugriff-Schaltfläche unter Windows 11 korrigieren.
- Die Farben der Navigationsschaltflächen-Symbole an das Windows-Design anpassen.
- Die starke Beschleunigung von „Alles auswählen“ und dem Aufheben der Auswahl tausender Dateien in klassischen Shell-Ansichten auf Windows 11 erweitern.
- Beheben, dass alle Ordnerfenster während langer Kopiervorgänge per Drag-and-drop zwischen FileSpacer-Fenstern nicht mehr reagieren.
- In Ordner-Kontextmenüs nur den Befehl „Öffnen in…“ des jeweils anderen Dateimanagers anzeigen.
- Beim Öffnen von Ordnern eine flüssige Animation mit gepunkteter Umrandung hinzufügen, die sich in den allgemeinen Einstellungen global ein- und ausschalten sowie in der Dauer anpassen lässt.

<a id="141--2026-11-09"></a>

## 1.4.1 — 2026-11-09

- Fehler der Datenbank für Ordnerzustände mit kopierbaren Diagnosedaten, Zugriff auf den Datenordner und nach Möglichkeit Angaben zu beteiligten Prozessen erläutern. Wenn sich die Datenbank weder laden noch wiederherstellen lässt, ein Zurücksetzen nach Bestätigung und mit vollständiger Sicherung anbieten.
- Unnötige Schreibzugriffe auf den Suchverlauf beim Anwenden unveränderter Einstellungen vermeiden, damit eine nicht verfügbare Ordnerdatenbank diesen Schritt vor einer automatischen Aktualisierung nicht blockiert.

- Blockierte Größenanpassung der Symbolleiste nach dem Öffnen ihres Menüs oder der Einstellungen beheben, auch in der Detailansicht mit verzögerter Anpassung der Spaltenbreiten.

<a id="140--2026-11-09"></a>

## 1.4.0 — 2026-11-09

- Gespeicherte freie Symbolpositionen beibehalten, wenn die Shell vor dem Laden des Ordnerinhalts eine leere Aufzählung als abgeschlossen meldet.
- Das Ansichtsmenü und die freie Symbolplatzierung verfügbar halten, wenn der gespeicherte Ordnerzustand vorübergehend nicht gelesen werden kann.
- Das Symbol der Schaltfläche Schnellzugriff direkt aus der Windows-Shell laden.
- Automatische Größenanpassung der Spalten in der Detailansicht mit optionaler Live-Aktualisierung ergänzen.
- Tastenkürzel des Explorers und Anpassungen an die räumliche Bedienung ergänzen (Strg+N und Strg+Rücktaste).
- Das Verkleinern großer Auswahlen durch einen Klick auf ein Element oder den Hintergrund klassischer Shell-Ansichten massiv beschleunigen, mit automatischem stillem Rückgriff auf öffentliche Funktionen, wenn die private Windows-Funktion fehlt.
- Das Auslesen vollständiger und großer teilweiser Auswahlen in Symbolansichten mit freier Platzierung über öffentliche Windows-APIs beschleunigen.
- Die globale Windows-Einstellung für Miniaturansichten über das Ansichtsmenü umschalten.
- Die Scrollposition eines Ordners beim erneuten Öffnen seines Fensters wiederherstellen.
- Neue Fenster anhand der normalen Größe und Position vorhandener FileSpacer-Fenster platzieren, auch wenn diese maximiert, minimiert oder angedockt sind.
- Massive Beschleunigung von „Alles auswählen“ und des Aufhebens der Auswahl von Tausenden Dateien in klassischen Shell-Ansichten. Diese Optimierung nutzt private Windows-Funktionen, deren Verfügbarkeit und Verhalten nicht garantiert sind. Fehlen diese Funktionen oder sind sie inkompatibel, greift FileSpacer automatisch und ohne Meldung auf öffentliche Funktionen zurück; die Leistungsgewinne entfallen dann.

<a id="132--2026-11-04"></a>

## 1.3.2 — 2026-11-04

- Polnisch, Niederländisch und Ukrainisch zur Anwendung hinzufügen.

<a id="131--2026-11-03"></a>

## 1.3.1 — 2026-11-03

- Behebt das Öffnen über den Standard-Dateimanager, bei dem eine andere Anwendung anstelle von FileSpacer ausgewählt wurde.

<a id="130--2026-11-02"></a>

## 1.3.0 — 2026-11-02

- Behebt gleichzeitige Zugriffe auf Ordnerzustände, erhält Änderungen bei verzögerter Wiederherstellung und reduziert Neuzeichnungen großer freier Symbolanordnungen.
- Verhindert unbeabsichtigte Fensterbewegungen und Kontextmenüs nach unterbrochenen Fenstergesten.

- Fügt optionale PowerToys-Peek-Vorschauen hinzu und überlässt kollidierende Fenstergesten Grab And Move.
- Das Titelleistenmenü in sieben Sprachen anpassen, mit Drag-and-drop, Standardeinstellungen und nativer oder Nilesoft-Shell-Darstellung.
- Aktionen für gespeicherte Anordnungen, Anpassung an den Inhalt, Schließen untergeordneter oder anderer Fenster, benannte Ordnergruppen und stabile Nilesoft-Befehle hinzufügen.
- Nähte zwischen den grünen Flächen des Anwendungssymbols korrigieren.

<a id="123--2026-10-30"></a>

## 1.2.3 — 2026-10-30

- Ordnerfenster nach einem Update mit derselben FileSpacer-Installation wiederherstellen.
- Identitäten, Symbolüberlagerungen und Ordnerbenachrichtigungen bei gemischter Sortierung korrigieren. Freie Symbolpositionen erhalten und die native Ansicht verwenden, wenn der Anbieter fehlt oder inkompatibel ist.
- Prozessübergreifend nur ein Einstellungsfenster zulassen sowie Tastenkürzelprüfung, Everything-Pfade, Zugriffstasten und regionale Übersetzungen korrigieren.
- Das Schließen von Ordnerfenstern, Aktivierungsklicks und die Wiederherstellung nach einer Vorschau im Menü Ansicht korrigieren.
- Die Aktualisierung der Sprunglisten korrigieren.
- Mehrsprachige Offline-Hilfe über F1 und Info hinzufügen.

<a id="122--2026-10-28"></a>

## 1.2.2 — 2026-10-28

- Die Verwendung des angepassten Ordneranbieters bei gemischter Sortierung und frei angeordneten Symbolen korrigieren. Bei freier Platzierung den nativen Anbieter verwenden und beim Übernehmen einer sortierten Anordnung den Anbieter wechseln, ohne gespeicherte Symbolpositionen, Auswahl oder Fokus zu verlieren.

<a id="121--2026-10-28"></a>

## 1.2.1 — 2026-10-28

- Kurzzeitig angezeigte Standardsymbole beim Öffnen gemischt sortierter Ordner verringern. Sichtbare Symbole vor dem Einblenden der nativen Ansicht im Windows-Cache vorbereiten. Die Wartezeit für langsame Symbolhandler bleibt begrenzt.

<a id="120--2026-10-28"></a>

## 1.2.0 — 2026-10-28

- Eine optionale gemeinsame Sortierung von Dateien und Ordnern im Dateisystem unter Anzeige → Dateien hinzufügen. Native Windows-Ansicht und gespeicherter Ordnerzustand bleiben erhalten. Die Option ist standardmäßig deaktiviert.
- Die Oberfläche zusätzlich zu Englisch und Französisch auf Deutsch, Italienisch, Spanisch, Portugiesisch (Portugal) und Griechisch anbieten.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

<a id="1110--2026-10-27"></a>

## 1.1.10 — 2026-10-27

- Abstürze beim Beenden beheben, die durch eine vorzeitige Initialisierung threadlokaler Ordnerdaten in spät gestarteten Shell-Arbeitsthreads verursacht wurden.
- In Ordnerkontextmenüs Im Windows-Datei-Explorer öffnen anbieten, wenn FileSpacer der Standard ist, andernfalls In FileSpacer öffnen.
- Das Öffnen von Netzwerkserver- und Freigabeverknüpfungen im Datei-Explorer trotz FileSpacer als Standard korrigieren.
- Das Anwendungssymbol unter Info bei 100 % Skalierung auf 48 Pixel vergrößern und den Text daneben anordnen.
- Titel maximierter Fenster vollständig anzeigen und die Symbolleiste über die verfügbare Breite erstrecken.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

<a id="119--2026-10-27"></a>

## 1.1.9 — 2026-10-27

- Beim Zeigen auf Einträge im Ansichtsmenü eine Vorschau der Ansichtsmodi anzeigen, mit schrittweiser Wiederherstellung bei Abbruch und einer standardmäßig aktivierten Option unter Anzeige.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

<a id="118--2026-10-27"></a>

## 1.1.8 — 2026-10-27

- Das Anwendungssymbol durch eine projizierte Tesserakt-Gestaltung ersetzen und optimierte kleine Größen beibehalten.
- Englische und französische Einstellungsbezeichnungen präzisieren und Fenstersperroptionen zusammenfassen.
- Eine Option zum Ausblenden des Schlosssymbols gesperrter Fenster hinzufügen, ohne ihre Zustandssperre zu ändern.
- In Einstellungsseiten und Tastenkombinationsfeldern native Windows-Theme-Hintergründe verwenden.
- Fensterpositionierung und Taskleistengruppierung mit standardmäßigen Windows-QuickInfos neben den Optionen erklären.
- Die Einstellungen an das Ordnerfenster binden, von dem aus sie geöffnet wurden, und ausstehende Änderungen bei dessen Schließen verwerfen. Bestätigte Wartungsaktionen schließen die Einstellungen, ohne ausstehende Änderungen zu übernehmen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

<a id="117--2026-10-26"></a>

## 1.1.7 — 2026-10-26

- Überlappungen neuer Fenster mit exakter Geometrie minimieren und zunächst freien Platz suchen. Die allgemeine Option in „Fensterüberlappung minimieren“ umbenennen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

<a id="116--2026-10-26"></a>

## 1.1.6 — 2026-10-26

- Offen bleibende Ordnerfenster beheben, wenn ein übergeordneter Ordner gelöscht oder in den Papierkorb verschoben wird.
- Verhindern, dass Sprunglisten nicht mehr verfügbare Ordner zum erneuten Öffnen anbieten.
- Überlappungen bei Fenstern ohne gespeicherte Position verringern und dabei das bei räumlicher Navigation ersetzte Ausgangsfenster ignorieren. Dies standardmäßig aktivieren und unter Allgemein > Ordnernavigation einstellbar machen.
- Die Ordnerzustandswartung von Allgemein nach Info verschieben.
- Neue Ordnerfenster mit einem ausgewogenen Seitenverhältnis von 4:3 öffnen (580 × 435 bei 96 DPI), wobei die bisherige Höhe, konfigurierte Abmessungen und die je Ordner gespeicherte Größe berücksichtigt werden.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

<a id="115--2026-10-26"></a>

## 1.1.5 — 2026-10-26

- Versehentlich erzeugte Verknüpfungen nach Alt + Doppelklick mit anschließender Mausbewegung verhindern. Native Eigenschaften, Auswahl und Fokus erhalten, ohne einen gehaltenen Mausklick nachzustellen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

<a id="114--2026-10-25"></a>

## 1.1.4 — 2026-10-25

Diese Version baut auf 1.1.3 auf, die bereits konfigurierbare Tastatur-/Mausgesten zum Verschieben und Skalieren von Fenstern bietet.

- Die optionale externe Everything-Suche mit automatischer Erkennung, manuellem Programmpfad, Suche im aktuellen Ordner samt Unterordnern oder überall sowie verständlichen Startfehlern hinzufügen.
- Everything und letzte Suchanfragen beim Aktivieren des vorhandenen Suchfelds ohne zusätzliche Schaltfläche anzeigen. Native Suche, Sofortfilter und Wiederverwendung lokaler Suchanfragen beibehalten.
- Standardmäßig fünf letzte Suchanfragen je Ordner sitzungsübergreifend speichern, unabhängig von Everything zwischen null und zwanzig einstellbar. Vorhandene Ordner- und Suchdaten erhalten und den Verlauf nach der bestehenden vorsichtigen Ordnerzustandsrichtlinie bereinigen.
- Das Everything-Programmsymbol in geeigneter Größe anzeigen, mit Hintergrundextraktion und einem bei Änderungen der Programmdatei erneuerten Arbeitsspeichercache.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

<a id="113--2026-10-25"></a>

## 1.1.3 — 2026-10-25

- Eine Einstellungsseite für Tastenkombinationen zum Konfigurieren oder Deaktivieren der Verschiebe- und Skaliergesten hinzufügen. Unterstützt Zusatztasten und linke, mittlere oder rechte Maustaste, direkte Eingabe und Konfliktprüfung.
- Alt + Doppelklick auf Eigenschaften der nativen Explorer-Ansicht überlassen.
- Vollständige Namen auf deckendem Hintergrund für ein einzelnes ausgewähltes Element in der Symbolansicht wiederherstellen. Mehrfachauswahlen behalten die native Windows-Darstellung. Die Einstellungsbezeichnung beschreibt diesen Geltungsbereich jetzt ausdrücklich.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

<a id="112--2026-10-25"></a>

## 1.1.2 — 2026-10-25

- Die optionale Sperre der gespeicherten Fenstergeometrie und Ansicht über das Titelleistenmenü hinzufügen. Vorübergehende Änderungen bleiben möglich. Einzelne Symbolpositionen werden weiterhin normal gespeichert.
- Ein Small-&-Flat-Schlosssymbol auf gesperrten Ordnersymbolen anzeigen, mit nativen mehrstufigen ICO-Dateien, DPI-Skalierung und Kontrastfarben.
- Die globale Deaktivierung bestätigen lassen, Sperren offener und geschlossener Ordner entfernen und normales prozessübergreifendes Speichern wieder aufnehmen.
- Beim erneuten Öffnen eines geschlossenen Ordners über Strg+Umschalt+T oder Sprunglisten in Details und Liste wieder die Zeile nach oben scrollen, die beim Schließen dort stand.
- Die Absturzdiagnose schreibt weiterhin Minidump und Bericht in die lokalen Anwendungsdaten. Die Zuordnung zum früheren vollständigen Namensrendering, dessen automatische Einstellungsänderung und der Neustart entfallen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

<a id="111--2026-10-24"></a>

## 1.1.1 — 2026-10-24

- Das Aktivieren ohne Auswahlverlust wiederherstellen: Der erste Klick auf den leeren Hintergrund eines inaktiven Ordnerfensters aktiviert es, ohne die Auswahl aufzuheben. Die Option ist wieder aktiviert.
- Einen Absturz beim Schließen eines Ordnerfensters während einer Shell-Aktualisierung beheben, etwa beim Aktualisieren, Namensfiltern oder den Shell-Befehlen Aktualisieren/Neu.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

<a id="110--2026-10-24"></a>

## 1.1.0 — 2026-10-24

- Die Ordneransichten auf Implementierungen der Windows Shell über öffentliche COM-Schnittstellen umstellen, statt interne Shell-Steuerelemente anzupassen.
- Räumlichen Fensterzustand, Auswahl und Fokus, Symbolpositionen und Filter an den produktiven SQLite-Speicher und öffentliche Shell-Ereignisse anbinden.
- Native Dateibefehle sowie FileSpacers Alt-Gesten zum Verschieben, Skalieren und Öffnen von Eigenschaften beibehalten.
- Die Systemsteuerung in einem eigenen Windows-Fenster öffnen und dabei Ausgangsordner und Auswahl erhalten.
- Die automatische Auswahl des Ausgangsordners beim Wechsel zu einem übergeordneten Ordner aus ergonomischen Gründen entfernen: Weitere Strg-Auswahlen dürfen nicht versehentlich beide Ordner öffnen.
- Drei Verhaltensunterschiede dieser Version akzeptieren: Die angepasste Darstellung vollständiger ausgewählter Namen auf deckendem Hintergrund, Auswahlbeibehaltung beim Aktivieren und exakt wiederhergestellte Bildlaufpositionen sind nicht verfügbar. Die beiden inaktiven Einstellungen deaktivieren, ihre gespeicherten Werte aber behalten.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

<a id="108--2026-10-22"></a>

## 1.0.8 — 2026-10-22

- Fenster aus dem Dateibereich mit Alt + Ziehen links verschieben und mit Alt + Ziehen rechts skalieren, ohne die Dateiauswahl zu verändern.
- Die klassische Shell-Ansicht unabhängig von der Anzeige vollständiger Namen ausgewählter Elemente aktiv lassen.
- Das Nilesoft-Shell-Theme auf das Ansichtsmenü anwenden.
- Lange Verzögerungen beim Beenden durch passive Shell-Symbolcaches vermeiden, dabei aber Lebensdauerreferenzen explizit aufgerufener Shell-Befehle beibehalten.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

<a id="107--2026-10-20"></a>

## 1.0.7 — 2026-10-20

- Beim Öffnen eines übergeordneten Ordners das ursprüngliche Unterverzeichnis auswählen und die Auswahl bereits geöffneter Fenster beibehalten.
- Die Namensfilterung beim Absenden, Ändern und Leeren einer Suchanfrage korrigieren.
- Das native Windows-Schnellzugriffssymbol wiederherstellen und Schnellzugriffs- sowie Einstellungsschaltflächen zentrieren.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

<a id="106--2026-10-11"></a>

## 1.0.6 — 2026-10-11

- Nach einem Update aus der Anwendung die zuvor geöffneten Ordnerfenster automatisch mit ihren gespeicherten Positionen, Größen und Ansichten wiederherstellen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

<a id="105--2026-10-11"></a>

## 1.0.5 — 2026-10-11

- Unter Info asynchron nach Updates suchen, mit angepasster Installationsschaltfläche und Statusanzeige.
- Updates in einem eigenen abbrechbaren Fortschrittsdialog herunterladen, während Ordnerfenster benutzbar bleiben.
- Größe und SHA-256 des Updates prüfen, bevor FileSpacer zum Anwenden geschlossen wird.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

<a id="104--2026-10-11"></a>

## 1.0.4 — 2026-10-11

- Im Titelleistenmenü einen Befehl zum Schließen übergeordneter Fenster hinzufügen.
- Geschlossene Ordner über das Titelleistenmenü, Strg+Umschalt+T oder Sprunglisten erneut öffnen und dabei Ansicht, Auswahl und Bildlaufposition wiederherstellen.
- Im Schnellzugriff des Explorers angeheftete Ordner auch über die Taskleisten-Sprungliste öffnen.
- Den Wiederöffnungsbefehl nur anzeigen, wenn der zuletzt geschlossene Ordner nicht bereits geöffnet ist, zusammen mit Namen und nativem Symbol.
- Das aktuelle Fenster aus der Fensterwechselliste im Titelleistenmenü ausnehmen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

<a id="103--2026-10-11"></a>

## 1.0.3 — 2026-10-11

- Manuelle und optionale wöchentliche Updateprüfungen über GitHub-Versionen von FileSpacer wiederherstellen, standardmäßig deaktiviert.


- Die native Shell-Behandlung von Alt + Doppelklick und Alt + Eingabetaste auf Ordnereigenschaften beibehalten.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

<a id="102--2026-10-11"></a>

## 1.0.2 — 2026-10-11

- Nilesoft-Shell-Themes unabhängig von der Dateiauswahl auf das Ordnermenü der Titelleiste anwenden und native Shell-Befehle beibehalten.
- Getrennte Nilesoft-Anpassungen für Titelleisten- und Symbolleistenmenüs ermöglichen.
- Den Pfeil des Titelleistenmenüs zentrieren.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

<a id="101--2026-10-09"></a>

## 1.0.1 — 2026-10-09

- Native Minimieren- und Maximieren-Schaltflächen aktivieren und die Titelleistenschaltflächen maximierter Fenster korrigieren.
- Den maximierten Zustand getrennt von der normalen Fenstergeometrie speichern und wiederherstellen.
- Native Taskleistenanimationen und ordnerspezifische Eigenschaften zum erneuten Starten verwenden.
- Gruppierungseinstellungen der Taskleiste auf neue Fenster anwenden und dieses Verhalten in den Einstellungen klarer beschreiben.
- Das Löschen geöffneter Ordner erlauben und deren gespeicherten Zustand entfernen.
- Verhindern, dass Anwendung oder Wiederherstellungshelfer Ordner durch ihr Arbeitsverzeichnis sperren.
- Absolute Ordnerpfade bei der Wiederherstellung nach einem Absturz beibehalten.
- Normale Dateinamen direkt nach Aufheben der Auswahl wiederherstellen, wenn die Anzeige vollständiger Namen aktiviert ist.
- Zwischengespeicherte Shell-Schnittstellen beim Schließen von Ordnerfenstern freigeben und die Verarbeitung von Shell-Benachrichtigungen korrigieren.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

<a id="100--2026-10-09"></a>

## 1.0.0 — 2026-10-09

- Erste FileSpacer-Version für Windows 10 oder neuer, x64, auf Grundlage von ChromaFiler.
- Räumliche Navigation mit einem Fenster pro Ordner und dauerhaftem Fensterzustand, Ansichtseinstellungen, Symbolpositionen und Filtern in SQLite bereitstellen.
- Verschobene und umbenannte Ordner verfolgen und beim erneuten Öffnen ein vorhandenes Fenster aktivieren.
- Das Ausgangsfenster beim Öffnen eines Ordners oder seines übergeordneten Ordners standardmäßig schließen, mit einstellbarer Vorgabe und Strg zum Umkehren.
- Eine Brotkrümelnavigation, eine bearbeitbare Pfadleiste und einen Namensfilter mit optionalem Filtern während der Eingabe hinzufügen.
- Konfigurierbare Symbolleistenelemente, eine untere Statusleiste und Taskleistengruppierung anbieten.
- Die optionale experimentelle vollständige Namensanzeige für ausgewählte Elemente hinzufügen.
- Englische und französische Oberflächen bereitstellen, mit Englisch als Ersatzsprache.
- Absturzwiederherstellung und Wartungsbefehle zum Zurücksetzen von Einstellungen oder gespeichertem Ordnerzustand hinzufügen.
- Miller-Spaltennavigation, integrierten Texteditor, Vorschaufenster und schwebende Ablage entfernen.

[Versionshistorie](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
