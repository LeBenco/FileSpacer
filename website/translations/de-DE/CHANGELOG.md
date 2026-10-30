# Versionsverlauf

## Noch nicht veröffentlicht

## 1.2.3 — 2026-10-30

- Identitäten, Symbolüberlagerungen und Ordnerbenachrichtigungen bei gemischter Sortierung korrigieren; freie Symbolpositionen erhalten und die native Ansicht verwenden, wenn der Anbieter fehlt oder inkompatibel ist.
- Prozessübergreifend nur ein Einstellungsfenster zulassen sowie Tastenkürzelprüfung, Everything-Pfade, Zugriffstasten und regionale Übersetzungen korrigieren.
- Das Schließen von Ordnerfenstern, Aktivierungsklicks und die Wiederherstellung nach einer Vorschau im Menü Ansicht korrigieren.
- Sprunglistenaktualisierungen und Installationsdiagnosen korrigieren; eigene Hilfedateien bei Updates und der Deinstallation erhalten.
- Mehrsprachige Offlinehilfe über F1 und Info mitliefern sowie übersetzte Einrichtungsanleitungen und Nilesoft-Shell-Referenzen aktualisieren.

## 1.2.2 — 2026-10-28

- Die Verwendung des angepassten Ordneranbieters bei gemischter Sortierung und frei angeordneten Symbolen korrigieren. Bei freier Platzierung den nativen Anbieter verwenden und beim Übernehmen einer sortierten Anordnung den Anbieter wechseln, ohne gespeicherte Symbolpositionen, Auswahl oder Fokus zu verlieren.

## 1.2.1 — 2026-10-28

- Kurzzeitig angezeigte Standardsymbole beim Öffnen gemischt sortierter Ordner verringern. Sichtbare Symbole vor dem Einblenden der nativen Ansicht im Windows-Cache vorbereiten; die Wartezeit für langsame Symbolhandler bleibt begrenzt.

## 1.2.0 — 2026-10-28

- Eine optionale gemeinsame Sortierung von Dateien und Ordnern im Dateisystem unter Anzeige → Dateien hinzufügen. Native Windows-Ansicht und gespeicherter Ordnerzustand bleiben erhalten. Die Option ist standardmäßig deaktiviert.
- Die Oberfläche zusätzlich zu Englisch und Französisch auf Deutsch, Italienisch, Spanisch, Portugiesisch (Portugal) und Griechisch anbieten.
- Titel und Untertitel des Installationsprogramms als größeren, DPI-angepassten Windows-Text mit ClearType-Glättung darstellen.
- Das Online-Benutzerhandbuch um englische und französische Bildschirmaufnahmen, Fenstergesten und Vergleiche der Nilesoft-Shell-Menüs ergänzen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

## 1.1.10 — 2026-10-27

- Die Windows-Bestätigung nur bei Installation oder Deinstallation für alle Benutzer anfordern. Persönliche Integrationsentscheidungen auf den Benutzer anwenden, der das Installationsprogramm gestartet hat.
- Auf der ersten Installationsseite den vollständigen Versionsverlauf in einem Bildlaufbereich neben den Installationsoptionen anzeigen.
- Das Zurücksetzen der Komponentenauswahl bei Upgrades beheben. Gespeicherte Auswahlen wiederherstellen und bei unbeaufsichtigten Upgrades den aktuellen Standard-Dateimanager beibehalten.
- In allen Installations- und Deinstallationsschritten dieselbe FileSpacer-Seitenleiste mit großem Logo und hellgrau-weißer Farbgebung verwenden. Dem Installationsprogramm ein eigenes Symbol aus den nativen Anwendungsbitmaps und einem für jede Größe separat optimierten Download-Pfeil geben; am rechten und unteren Ordnerrand bleibt ein schmaler gelber Saum erhalten.
- Abstürze beim Beenden beheben, die durch eine vorzeitige Initialisierung threadlokaler Ordnerdaten in spät gestarteten Shell-Arbeitsthreads verursacht wurden.
- In Ordnerkontextmenüs Im Windows-Datei-Explorer öffnen anbieten, wenn FileSpacer der Standard ist, andernfalls In FileSpacer öffnen. Die Menüs beim Wechsel der Vorgabe, nach der Installation und beim Start aktualisieren.
- Das Öffnen von Netzwerkserver- und Freigabeverknüpfungen im Datei-Explorer trotz FileSpacer als Standard korrigieren. Zum Aktivieren der Netzwerkintegration die Kontextmenükomponente aktualisieren und FileSpacer erneut als Standard auswählen.
- Das Anwendungssymbol unter Info bei 100 % Skalierung auf 48 Pixel vergrößern und den Text daneben anordnen.
- Titel maximierter Fenster vollständig anzeigen und die Symbolleiste über die verfügbare Breite erstrecken.
- Regressionstests in einem eingerichteten Hyper-V-Client ausführen können, während Anwendung und Testhilfsprogramme auf dem Host kompiliert werden.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

## 1.1.9 — 2026-10-27

- Beim Zeigen auf Einträge im Ansichtsmenü eine Vorschau der Ansichtsmodi anzeigen, mit schrittweiser Wiederherstellung bei Abbruch und einer standardmäßig aktivierten Option unter Anzeige.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

## 1.1.8 — 2026-10-27

- Anwendungssymbol und README-Logo durch eine Tesseraktprojektion mit einem Verhältnis von 11/20 zwischen violetten und grünen Würfelkanten ersetzen; optimierte kleine Symbolgrößen beibehalten.
- Englische und französische Einstellungsbezeichnungen präzisieren und Fenstersperroptionen zusammenfassen.
- Eine Option zum Ausblenden des Schlosssymbols gesperrter Fenster hinzufügen, ohne ihre Zustandssperre zu ändern.
- In Einstellungsseiten und Tastenkombinationsfeldern native Windows-Theme-Hintergründe verwenden.
- Fensterpositionierung und Taskleistengruppierung mit standardmäßigen Windows-QuickInfos neben den Optionen erklären.
- Die Einstellungen an das Ordnerfenster binden, von dem aus sie geöffnet wurden, und ausstehende Änderungen bei dessen Schließen verwerfen. Bestätigte Wartungsaktionen schließen die Einstellungen, ohne ausstehende Änderungen zu übernehmen.
- Die README nach räumlicher Navigation und Funktionen gliedern, mit Forschungsreferenzen, Downloadlink zur neuesten Version, eigenen Build- und Nilesoft-Shell-Anleitungen und ausführlicherer Benutzerdokumentation.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

## 1.1.7 — 2026-10-26

- Überlappungen neuer Fenster mit exakter Geometrie minimieren und zunächst freien Platz suchen. Die allgemeine Option in „Fensterüberlappung minimieren“ umbenennen.
- In der englischen und französischen Installation die optionale Auswahl als Standard-Dateimanager anbieten, mit Hinweisen zum späteren Aktivieren oder Zurücksetzen in den Integrationseinstellungen von FileSpacer.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

## 1.1.6 — 2026-10-26

- Offen bleibende Ordnerfenster beheben, wenn ein übergeordneter Ordner gelöscht oder in den Papierkorb verschoben wird.
- Verhindern, dass Sprunglisten nicht mehr verfügbare Ordner zum erneuten Öffnen anbieten.
- Überlappungen bei Fenstern ohne gespeicherte Position verringern und dabei das bei räumlicher Navigation ersetzte Ausgangsfenster ignorieren. Dies standardmäßig aktivieren und unter Allgemein > Ordnernavigation einstellbar machen.
- Die Ordnerzustandswartung von Allgemein nach Info verschieben.
- Neue Ordnerfenster mit einem ausgewogenen Seitenverhältnis von 4:3 öffnen (580 × 435 bei 96 DPI), wobei die bisherige Höhe, konfigurierte Abmessungen und die je Ordner gespeicherte Größe berücksichtigt werden.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

## 1.1.5 — 2026-10-26

- Versehentlich erzeugte Verknüpfungen nach Alt + Doppelklick mit anschließender Mausbewegung verhindern. Native Eigenschaften, Auswahl und Fokus erhalten, ohne einen gehaltenen Mausklick nachzustellen.
- Die Regressionstests für native Doppelklickzeiten und physische Mausziele korrigieren. Eigenschaften, Auswahl, Fokus, Auswahlanker, Geometrie und Ordnerinhalt mit und ohne Fenstergesten vergleichen, einschließlich Strg-/Umschalt-Kombinationen.
- Das echte Everything-Testprogramm und seine Start-INI isolieren, damit die installierte Administratoreinstellung des Benutzers keine Testsuchen umleiten oder Rechteerhöhungen auslösen kann. Unveränderte installierte Einstellungen prüfen.
- Den Desktop während Oberflächentests wach halten und den Bildschirmschoner vorübergehend aussetzen. Beim Aufräumen den ursprünglichen Laufzeitzustand wiederherstellen, auch nach Testfehlern.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

## 1.1.4 — 2026-10-25

Diese Version baut auf 1.1.3 auf, die bereits konfigurierbare Tastatur-/Mausgesten zum Verschieben und Skalieren von Fenstern bietet.

- Die optionale externe Everything-Suche mit automatischer Erkennung, manuellem Programmpfad, Suche im aktuellen Ordner samt Unterordnern oder überall sowie verständlichen Startfehlern hinzufügen.
- Everything und letzte Suchanfragen beim Aktivieren des vorhandenen Suchfelds ohne zusätzliche Schaltfläche anzeigen. Native Suche, Sofortfilter und Wiederverwendung lokaler Suchanfragen beibehalten.
- Standardmäßig fünf letzte Suchanfragen je Ordner sitzungsübergreifend speichern, unabhängig von Everything zwischen null und zwanzig einstellbar. Vorhandene Ordner- und Suchdaten erhalten und den Verlauf nach der bestehenden vorsichtigen Ordnerzustandsrichtlinie bereinigen.
- Das Everything-Programmsymbol in geeigneter Größe anzeigen, mit Hintergrundextraktion und einem bei Änderungen der Programmdatei erneuerten Arbeitsspeichercache.
- Regressionstests für isolierte Speicherung, Migration und echte Shell-/Everything-Interaktion hinzufügen. Den Shell-Testleser gegen wechselnde Ansichten bei aufeinanderfolgenden Filtern absichern.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

## 1.1.3 — 2026-10-25

- Eine Einstellungsseite für Tastenkombinationen zum Konfigurieren oder Deaktivieren der Verschiebe- und Skaliergesten hinzufügen. Unterstützt Zusatztasten und linke, mittlere oder rechte Maustaste, direkte Eingabe und Konfliktprüfung.
- Alt + Doppelklick auf Eigenschaften der nativen Explorer-Ansicht überlassen.
- Vollständige Namen auf deckendem Hintergrund für ein einzelnes ausgewähltes Element in der Symbolansicht wiederherstellen. Mehrfachauswahlen behalten die native Windows-Darstellung; die Einstellungsbezeichnung beschreibt diesen Geltungsbereich jetzt ausdrücklich.
- Umfassende Tests der Tastenkombinationen und echter Mausgesten mit französischen und englischen Einstellungsressourcen ergänzen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

## 1.1.2 — 2026-10-25

- Die optionale Sperre der gespeicherten Fenstergeometrie und Ansicht über das Titelleistenmenü hinzufügen. Vorübergehende Änderungen bleiben möglich; einzelne Symbolpositionen werden weiterhin normal gespeichert.
- Ein Small-&-Flat-Schlosssymbol auf gesperrten Ordnersymbolen anzeigen, mit nativen mehrstufigen ICO-Dateien, DPI-Skalierung und Kontrastfarben.
- Die globale Deaktivierung bestätigen lassen, Sperren offener und geschlossener Ordner entfernen und normales prozessübergreifendes Speichern wieder aufnehmen.
- Die Regressionstests auf Sperrübergänge, Speicherfehler, Shell-Wiederherstellung und native Symboldarstellung erweitern.
- Beim erneuten Öffnen eines geschlossenen Ordners über Strg+Umschalt+T oder Sprunglisten in Details und Liste wieder die Zeile nach oben scrollen, die beim Schließen dort stand.
- Die Absturzdiagnose schreibt weiterhin Minidump und Bericht in die lokalen Anwendungsdaten. Die Zuordnung zum früheren vollständigen Namensrendering, dessen automatische Einstellungsänderung und der Neustart entfallen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

## 1.1.1 — 2026-10-24

- Das Aktivieren ohne Auswahlverlust wiederherstellen: Der erste Klick auf den leeren Hintergrund eines inaktiven Ordnerfensters aktiviert es, ohne die Auswahl aufzuheben. Die Option ist wieder aktiviert.
- Einen Absturz beim Schließen eines Ordnerfensters während einer Shell-Aktualisierung beheben, etwa beim Aktualisieren, Namensfiltern oder den Shell-Befehlen Aktualisieren/Neu.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

## 1.1.0 — 2026-10-24

- Die Ordneransichten auf Implementierungen der Windows Shell über öffentliche COM-Schnittstellen umstellen, statt interne Shell-Steuerelemente anzupassen.
- Räumlichen Fensterzustand, Auswahl und Fokus, Symbolpositionen und Filter an den produktiven SQLite-Speicher und öffentliche Shell-Ereignisse anbinden.
- Native Dateibefehle sowie FileSpacers Alt-Gesten zum Verschieben, Skalieren und Öffnen von Eigenschaften beibehalten.
- Die Systemsteuerung in einem eigenen Windows-Fenster öffnen und dabei Ausgangsordner und Auswahl erhalten.
- Die automatische Auswahl des Ausgangsordners beim Wechsel zu einem übergeordneten Ordner aus ergonomischen Gründen entfernen: Weitere Strg-Auswahlen dürfen nicht versehentlich beide Ordner öffnen.
- Drei Verhaltensunterschiede dieser Version akzeptieren: Die angepasste Darstellung vollständiger ausgewählter Namen auf deckendem Hintergrund, Auswahlbeibehaltung beim Aktivieren und exakt wiederhergestellte Bildlaufpositionen sind nicht verfügbar. Die beiden inaktiven Einstellungen deaktivieren, ihre gespeicherten Werte aber behalten.
- Die Regressionstests auf die produktive native Shell-Konfiguration ausrichten. Explorative Profilvergleiche, Einbettungsprüfungen und migrationsbezogene Berichte ausmustern.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

## 1.0.8 — 2026-10-22

- Fenster aus dem Dateibereich mit Alt + Ziehen links verschieben und mit Alt + Ziehen rechts skalieren, ohne die Dateiauswahl zu verändern.
- Die klassische Shell-Ansicht unabhängig von der Anzeige vollständiger Namen ausgewählter Elemente aktiv lassen.
- Das Nilesoft-Shell-Theme auf das Ansichtsmenü anwenden.
- Lange Verzögerungen beim Beenden durch passive Shell-Symbolcaches vermeiden, dabei aber Lebensdauerreferenzen explizit aufgerufener Shell-Befehle beibehalten.
- Das Hosting der Shell-Ansicht isolieren und Tests für Alt-Gesten, wiederholte Menüaufrufe während laufender Verarbeitung und die Shell-/STA-Lebensdauer erweitern.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Beim Öffnen eines übergeordneten Ordners das ursprüngliche Unterverzeichnis auswählen und die Auswahl bereits geöffneter Fenster beibehalten.
- Die Namensfilterung beim Absenden, Ändern und Leeren einer Suchanfrage korrigieren.
- Das native Windows-Schnellzugriffssymbol wiederherstellen und Schnellzugriffs- sowie Einstellungsschaltflächen zentrieren.
- Vorübergehende Diagnosen entfernen und Benco in den Projektangaben verwenden.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Nach einem Update aus der Anwendung die zuvor geöffneten Ordnerfenster automatisch mit ihren gespeicherten Positionen, Größen und Ansichten wiederherstellen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Unter Info asynchron nach Updates suchen, mit angepasster Installationsschaltfläche und Statusanzeige.
- Updates in einem eigenen abbrechbaren Fortschrittsdialog herunterladen, während Ordnerfenster benutzbar bleiben.
- Größe und SHA-256-Prüfsumme des Installationsprogramms prüfen, bevor FileSpacer geordnet geschlossen und die Installation gestartet wird.
- Download-Metadaten zum Installationsprogramm mit jeder Version veröffentlichen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Im Titelleistenmenü einen Befehl zum Schließen übergeordneter Fenster hinzufügen.
- Geschlossene Ordner über das Titelleistenmenü, Strg+Umschalt+T oder Sprunglisten erneut öffnen und dabei Ansicht, Auswahl und Bildlaufposition wiederherstellen.
- Im Schnellzugriff des Explorers angeheftete Ordner auch über die Taskleisten-Sprungliste öffnen.
- Den Wiederöffnungsbefehl nur anzeigen, wenn der zuletzt geschlossene Ordner nicht bereits geöffnet ist, zusammen mit Namen und nativem Symbol.
- Das aktuelle Fenster aus der Fensterwechselliste im Titelleistenmenü ausnehmen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Manuelle und optionale wöchentliche Updateprüfungen über GitHub-Versionen von FileSpacer wiederherstellen, standardmäßig deaktiviert.

- Das Updatemanifest gemeinsam mit jedem Installationsprogramm veröffentlichen.

- Die native Shell-Behandlung von Alt + Doppelklick und Alt + Eingabetaste auf Ordnereigenschaften beibehalten.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Nilesoft-Shell-Themes unabhängig von der Dateiauswahl auf das Ordnermenü der Titelleiste anwenden und native Shell-Befehle beibehalten.
- Getrennte Nilesoft-Anpassungen für Titelleisten- und Symbolleistenmenüs ermöglichen.
- Den Pfeil des Titelleistenmenüs zentrieren.
- Nilesoft-Shell-Konfiguration, Theme-Anwendung und Menüfilter dokumentieren.
- Benco in den Copyrightangaben nennen.
- Dem Veröffentlichungsworkflow erlauben, das Installationsprogramm einer vorhandenen Version zu ersetzen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Native Minimieren- und Maximieren-Schaltflächen aktivieren und die Titelleistenschaltflächen maximierter Fenster korrigieren.
- Maximierten Zustand getrennt von der normalen Fenstergeometrie speichern und wiederherstellen, mit automatischer Datenbankmigration.
- Native Taskleistenanimationen und ordnerspezifische Eigenschaften zum erneuten Starten verwenden.
- Gruppierungseinstellungen der Taskleiste auf neue Fenster anwenden und dieses Verhalten in den Einstellungen klarer beschreiben.
- Das Löschen geöffneter Ordner erlauben und deren gespeicherten Zustand entfernen.
- Verhindern, dass Anwendung oder Wiederherstellungshelfer Ordner durch ihr Arbeitsverzeichnis sperren.
- Absolute Ordnerpfade und den Debug-Testmodus bei der Wiederherstellung nach Abstürzen erhalten.
- Normale Dateinamen direkt nach Aufheben der Auswahl wiederherstellen, wenn die Anzeige vollständiger Namen aktiviert ist.
- Zwischengespeicherte Shell-Schnittstellen beim Schließen von Ordnerfenstern freigeben und die Verarbeitung von Shell-Benachrichtigungen korrigieren.
- Manuelle Veröffentlichungen ermöglichen und die Versionsnummer in die Dateinamen der Installationsprogramme aufnehmen.

[Alle Änderungen](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

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
- MSVC-/NMAKE-Buildskripte, ein NSIS-Installationsprogramm, automatisierte GitHub-Veröffentlichungen und Projektdokumentation bereitstellen.

[Versionshistorie](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
