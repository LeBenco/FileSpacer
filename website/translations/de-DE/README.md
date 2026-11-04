<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

<a href="https://github.com/LeBenco/FileSpacer/releases/latest"><img align="right" src="https://img.shields.io/badge/Herunterladen-neueste%20Version-blue" alt="Herunterladen neueste Version"></a>

# FileSpacer

[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![Telemetrie: keine](https://img.shields.io/badge/Telemetrie-keine-2ea44f)](#privacy)
[![Lizenz: GPL v3](https://img.shields.io/badge/Lizenz-GPLv3-blue.svg)](#lizenz-und-danksagung)

FileSpacer ist ein Dateimanager für Windows nach dem [räumlichen Prinzip](https://en.wikipedia.org/wiki/Spatial_file_manager): Jeder Ordner hat genau ein Fenster, das sich Position, Größe und Ansichtseinstellungen merkt. So entstehen [beständige visuelle Orientierungspunkte](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), mit denen das [räumliche Gedächtnis](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) vertraute Ordner auf einen Blick wiedererkennen kann. In einem navigationsbasierten Dateimanager zeigt dasselbe Fenster nacheinander verschiedene Ordner an. Seine Position und Abmessungen allein verraten daher nicht, welcher Ordner gerade geöffnet ist; dazu muss man den Inhalt betrachten. Indem jeder Ordner seine eigenen Orientierungspunkte behält, kann der räumliche Ansatz die Orientierung zwischen den Dateien erleichtern. Registerkarten sind mit dem räumlichen Modell unvereinbar und werden niemals hinzugefügt.

<img src="docs/images/spatial-windows.png" alt="Drei nicht überlappende Ordnerfenster mit unterschiedlicher Größe, Ansicht und Symbolskalierung, darunter eine freie Symbolanordnung." width="560">

FileSpacer legt Wert auf eine schlanke und übersichtliche Bedienung. Damit sich nicht zu viele Fenster ansammeln – ein bekannter Nachteil dieses Prinzips –, schließt FileSpacer beim Öffnen eines Ordners standardmäßig das Ausgangsfenster. Halten Sie beim Öffnen Strg gedrückt, um es geöffnet zu lassen, wenn Sie mit mehreren Ordnern gleichzeitig arbeiten möchten. FileSpacer hält seine Oberfläche bewusst zurückhaltend, während manche überladenen Oberflächen zwischen all den Schaltflächen gerade noch einen Blick auf eine Datei erlauben.

<a id="system--languages"></a>**System und Sprachen:** Windows 10 oder neuer, x64. In zehn Sprachen verfügbar: Englisch, Französisch, Deutsch, Italienisch, Spanisch, Portugiesisch (Portugal), Griechisch, Polnisch, Niederländisch und Ukrainisch.

<a id="privacy"></a>**Datenschutz:** **Keine Telemetrie, Nutzungsverfolgung oder Werbung.** Update-Prüfungen verbinden sich mit GitHub; wöchentliche Prüfungen sind optional und standardmäßig aus. Herunterladen und Installieren erfordern eine Bestätigung. [Datenschutz](docs/user-guide.md#datenschutz).

**Dokumentation:**

[![Docs](https://img.shields.io/badge/Docs-Benutzerhandbuch-blue)](docs/user-guide.md)
[![Docs](https://img.shields.io/badge/Docs-Versionsverlauf-orange)](CHANGELOG.md)

_FileSpacer ist eine Abspaltung von [ChromaFiler](https://github.com/vanjac/chromafiler), ursprünglich entwickelt von J. van’t Hoog. Weitere Einzelheiten stehen unten unter [Lizenz und Danksagung](#lizenz-und-danksagung)._

## Funktionen

- **Ein Fenster pro Ordner**, das bei bereits geöffneten Ordnern wieder aktiviert wird. Position, Größe, Ansichtseinstellungen und Symbolanordnung bleiben beim erneuten Öffnen erhalten. [Räumliche Navigation](docs/user-guide.md#window-navigation).
- **Symbole frei anordnen.** Ihre Positionen werden für jeden Ordner gespeichert und wiederhergestellt. [Symbolanordnung](docs/user-guide.md#icon-layout).
- **Fenster von jeder Stelle im Dateibereich aus verschieben und skalieren**, ohne die Auswahl zu verlieren. Die Mausgesten lassen sich anpassen; standardmäßig gelten Alt + Ziehen mit der linken bzw. rechten Maustaste. [Fenstergesten](docs/user-guide.md#moving-and-resizing).
- **Zwischen geöffneten Ordnern wechseln**, über das Menü in der Titelleiste; übergeordnete Fenster schließen oder das zuletzt geschlossene Fenster mit Strg+Umschalt+T erneut öffnen. [Fensternavigation](docs/user-guide.md#window-navigation).
- **Den gespeicherten Fensterzustand sperren**, während vorübergehende Änderungen an Position, Größe und Ansicht weiterhin möglich bleiben. [Fensterzustand sperren](docs/user-guide.md#locking-a-saved-layout).
- **Den aktuellen Ordner filtern** und seine letzten Suchanfragen wiederverwenden. [Suche und Verlauf](docs/user-guide.md#search-and-history).
- **Die Auswahl beim Aktivieren eines Fensters beibehalten**, wenn Sie auf dessen Hintergrund klicken. [Auswahl und Aktivierung](docs/user-guide.md#selection-and-activation).
- **Fensterüberlappungen verringern**, wenn Ordner ohne gespeicherte Position geöffnet werden. [Fensterpositionierung](docs/user-guide.md#window-placement).
- **Das Titelleistenmenü anpassen** und Gruppen geöffneter Ordner speichern. [Menü anpassen](docs/user-guide.md#titelleistenmenü-anpassen) · [Ordnergruppen](docs/user-guide.md#ordnergruppen).

## Integration

FileSpacer lässt sich bei der Installation oder später in den Einstellungen zum Standard-Dateimanager machen. Dateien öffnen sich in ihrer üblichen Windows-Anwendung; Netzwerkordner verwenden das gewohnte Windows-Verhalten. [Windows-Integration](docs/user-guide.md#windows-integration).

Mit **Everything** lassen sich erweiterte Suchanfragen aus FileSpacers Suchfeld starten: im aktuellen Ordner samt Unterordnern oder in allen indizierten Dateien. Die Ergebnisse erscheinen in Everything. [Integration und Verwendung](docs/user-guide.md#everything).

Mit **Nilesoft Shell** können FileSpacers Menüs dessen Designs und angepasste Befehle verwenden. Für ein stimmiges Erscheinungsbild empfiehlt sich eine angepasste Konfiguration. [Integration und Konfiguration](docs/user-guide.md#nilesoft-shell).

## Kompilieren

[![Publish FileSpacer](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

Zum Kompilieren werden MSVC/NMAKE, Visual Studio 2022 Build Tools mit ATL und einem Windows SDK, Inkscape sowie ImageMagick verwendet. Für das Installationsprogramm wird NSIS benötigt; SQLite ist enthalten. Siehe [Build-Anleitung](docs/build.md).

Die Regressionstests verbinden PowerShell-Szenarien mit C++- und C#-Hilfsprogrammen für interne Prüfungen und echte Windows-Oberflächen- und Shell-Tests. Der [Regressionsworkflow in GitHub Actions](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) führt diese Anwendungstests auf Anfrage und automatisch vor einer Veröffentlichung aus. Dieselben Fälle lassen sich mit `tests.cmd` auf einem lokalen Windows-Desktop oder in einer eingerichteten Hyper-V-VM ausführen. Fallauswahl, Voraussetzungen und Ergebnisse beschreibt die [Testanleitung](tests/regression/README.md).

## Lizenz und Danksagung

### Urheberrecht und Lizenz
FileSpacer wird unter folgender Lizenz veröffentlicht: <img align="absbottom" src="https://img.shields.io/badge/Lizenz-GPLv3-blue.svg" alt="Lizenz: GPL v3">.<br>
Copyright © 2026 Benco für die FileSpacer-Änderungen. Die vollständige Lizenz steht in [LICENSE](LICENSE) im Quelltext oder in `LICENSE.txt` im Installationsordner. FileSpacer wird ohne Gewährleistung bereitgestellt; Weitergabe und Änderung sind nach Maßgabe dieser Lizenz erlaubt.

### Ursprung und Danksagung
FileSpacer ist eine Abspaltung von [ChromaFiler](https://github.com/vanjac/chromafiler), entwickelt von J. van’t Hoog (vanjac). Vielen Dank an vanjac für diese Grundlage! Ursprünglicher ChromaFiler-Code © 2023 J. van’t Hoog.

### Drittanbieterhinweise
Siehe [Drittanbieterhinweise](THIRD_PARTY_NOTICES.md) und [Datenschutzerklärung](docs/privacy-policy.md).
