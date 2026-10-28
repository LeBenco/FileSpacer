# Datenschutzerklärung

FileSpacer enthält weder Telemetrie noch Nutzungsanalyse oder Werbung. Es sendet keine Nutzungsdaten oder Absturzberichte automatisch an den Entwickler.

## Updateprüfungen

Manuelle Updateprüfungen und aktivierte automatische wöchentliche Prüfungen rufen das Versionsmanifest von FileSpacer über HTTPS von GitHub ab. Automatische Prüfungen sind standardmäßig deaktiviert. GitHub und seine Download-Infrastruktur erhalten die üblichen Verbindungsdaten, einschließlich Ihrer IP-Adresse und eines User-Agents mit der FileSpacer-Version. Die Anfrage enthält keine Dateipfade, Ordnerzustände oder Diagnoseberichte.

Ist ein installierbares Update verfügbar, bittet FileSpacer um Bestätigung, bevor es das Installationsprogramm über HTTPS von GitHub herunterlädt. Vor dem Start der Installation werden Dateigröße und SHA-256-Prüfsumme mit dem Versionsmanifest verglichen. Das Installationsprogramm erfordert Ihre Mitwirkung; Updates werden niemals automatisch installiert. Bei Versionen ohne Installationsmetadaten wird die Versionsseite im Standardbrowser geöffnet.

## Lokale Daten

Globale Einstellungen werden in der Windows-Registrierung des aktuellen Benutzers unter `HKEY_CURRENT_USER\Software\FileSpacer` gespeichert. Fensterpositionen und -größen, Ansichtseinstellungen, Symbolpositionen, Zustandssperren, letzte Suchanfragen und der Verlauf geschlossener Ordner liegen in einer SQLite-Datenbank unter `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. SQLite kann zugehörige Journaldateien im selben Verzeichnis anlegen. Der gespeicherte Zustand kann Ordneridentitäten und -pfade enthalten, einschließlich Netzwerkpfaden.

Ein Update speichert sein Installationsprogramm, eine Kopie des Update-Starters und eine SQLite-Liste der geöffneten Ordnerpfade vorübergehend unter `%TEMP%\FileSpacerUpdate-{GUID}`. Diese lokale Liste dient dazu, die Ordner nach der Installation oder einem Abbruch wieder zu öffnen; sie wird nicht hochgeladen.

Der Absturzschutz kann Diagnoseberichte und Minidumps unter `%LOCALAPPDATA%\FileSpacer` ablegen. Sie bleiben auf dem Computer, sofern der Benutzer sie nicht ausdrücklich weitergibt. Diagnosedaten können Dateipfade, Prozessinformationen und Teile des Prozessspeichers enthalten. Derselbe Ordner enthält eine Sperrdatei für die Einstellungen.

Debug-Versionen können mit `/test` getrennte Testeinstellungen und lokale Testdaten verwenden.

## Windows und externe Anwendungen

Das Öffnen von Netzwerkordnern, das Auflösen von Shell-Elementen, Windows-Vorschauen oder Miniaturansichten und das Aufrufen von Kontextmenüerweiterungen verwenden Windows und die installierten Shell-Komponenten. Diese Komponenten und entfernte Server können entsprechend ihrem eigenen Verhalten und ihren eigenen Richtlinien auf das Netzwerk zugreifen.

Dateien werden mit ihrer zugeordneten Anwendung oder mit der Windows-App-Auswahl geöffnet. Wie diese Anwendungen die Dateien verarbeiten, liegt außerhalb der Kontrolle von FileSpacer.

Bei aktivierter Everything-Integration übergibt die Suchaktion die sichtbare Anfrage an die separat installierte Everything-Anwendung. Auf einen Ordner beschränkte Suchen übergeben auch dessen Pfad. Der lokale Filter startet Everything nicht; dessen eigener Index und dessen Einstellungen bestimmen die Ergebnisse.

## Einstellungen zurücksetzen

`FileSpacer.exe /reset-options` setzt die globalen Einstellungen zurück. `FileSpacer.exe /reset-all` löscht außerdem den gespeicherten Ordnerzustand. Keiner der beiden Befehle löscht Benutzerdateien oder ändert Windows-Dateizuordnungen. Bereits gespeicherte Diagnoseberichte werden nicht entfernt.
