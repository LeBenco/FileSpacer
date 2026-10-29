<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

FileSpacer ist ein Dateimanager für Windows nach dem [räumlichen Prinzip](https://en.wikipedia.org/wiki/Spatial_file_manager): Jeder Ordner hat genau ein Fenster, das sich Position, Größe und Ansichtseinstellungen merkt. So entstehen [beständige visuelle Orientierungspunkte](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), mit denen das [räumliche Gedächtnis](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) vertraute Ordner auf einen Blick wiedererkennen kann. In einem navigationsbasierten Dateimanager zeigt dasselbe Fenster nacheinander verschiedene Ordner an. Seine Position und Abmessungen allein verraten daher nicht, welcher Ordner gerade geöffnet ist; dazu muss man den Inhalt betrachten. Indem jeder Ordner seine eigenen Orientierungspunkte behält, kann der räumliche Ansatz die Orientierung zwischen den Dateien erleichtern.

Damit sich nicht zu viele Fenster ansammeln – ein bekannter Nachteil dieses Prinzips –, schließt FileSpacer beim Öffnen eines Ordners standardmäßig das Ausgangsfenster. Halten Sie beim Öffnen Strg gedrückt, um es geöffnet zu lassen, wenn Sie mit mehreren Ordnern gleichzeitig arbeiten möchten.

FileSpacer hält seine Oberfläche bewusst zurückhaltend, während manche überladenen Oberflächen zwischen all den Schaltflächen gerade noch einen Blick auf eine Datei erlauben.

Registerkarten sind mit dem räumlichen Modell unvereinbar und werden niemals hinzugefügt.

Windows 10 oder neuer, x64. Verfügbar auf Englisch, Französisch, Deutsch, Italienisch, Spanisch, Portugiesisch (Portugal) und Griechisch.

[Neueste Version herunterladen](https://github.com/LeBenco/FileSpacer/releases/latest)
· [Benutzerhandbuch](docs/user-guide.md)
· [Versionsverlauf](CHANGELOG.md)

![Ein FileSpacer-Ordnerfenster mit frei angeordneten Beispieldateien und -ordnern.](docs/images/overview.png)

*Jeder Ordner behält sein Fenster und seine Symbolanordnung.*

## Funktionen

- **Symbole frei anordnen.** Ihre Positionen werden für jeden Ordner gespeichert und wiederhergestellt. [Symbolanordnung](docs/user-guide.md#icon-layout).
- **Fenster von jeder Stelle im Dateibereich aus verschieben und skalieren**, ohne die Auswahl zu verlieren. Die Mausgesten lassen sich anpassen; standardmäßig gelten Alt + Ziehen mit der linken bzw. rechten Maustaste. [Fenstergesten](docs/user-guide.md#moving-and-resizing).
- **Zwischen geöffneten Ordnern wechseln**, über das Menü in der Titelleiste; übergeordnete Fenster schließen oder das zuletzt geschlossene Fenster mit Strg+Umschalt+T erneut öffnen. [Fensternavigation](docs/user-guide.md#window-navigation).
- **Den gespeicherten Fensterzustand sperren**, während vorübergehende Änderungen an Position, Größe und Ansicht weiterhin möglich bleiben. [Fensterzustand sperren](docs/user-guide.md#locking-a-saved-layout).
- **Den aktuellen Ordner filtern** und seine letzten Suchanfragen wiederverwenden. [Suche und Verlauf](docs/user-guide.md#search-and-history).
- **Die Auswahl beim Aktivieren eines Fensters beibehalten**, wenn Sie auf dessen Hintergrund klicken. [Auswahl und Aktivierung](docs/user-guide.md#selection-and-activation).
- **Fensterüberlappungen verringern**, wenn Ordner ohne gespeicherte Position geöffnet werden. [Fensterpositionierung](docs/user-guide.md#window-placement).

## Integration

Wenn Sie **Nilesoft Shell** verwenden, können Sie damit auch die FileSpacer-Menüs gestalten und ihre Befehle mit menüspezifischen Regeln anpassen. Für eine stimmige Darstellung empfiehlt es sich, die Konfiguration anzupassen. [Integration und Konfiguration](docs/user-guide.md#nilesoft-shell).

**Everything** kann mit der in FileSpacer eingegebenen Suchanfrage den aktuellen Ordner samt Unterordnern oder den gesamten Index durchsuchen. Die Ergebnisse werden in Everything geöffnet, das separat installiert werden muss. [Einrichtung und Verwendung](docs/user-guide.md#everything).

FileSpacer lässt sich außerdem als Standardanwendung zum Öffnen von Ordnern festlegen. [Windows-Integration](docs/user-guide.md#windows-integration).

## Kompilieren

Zum Kompilieren werden MSVC/NMAKE, Visual Studio 2022 Build Tools mit ATL und einem Windows SDK, Inkscape sowie ImageMagick verwendet. Für das Installationsprogramm wird NSIS benötigt; SQLite ist enthalten. Siehe [Build-Anleitung](docs/build.md).

Die Regressionstests verbinden PowerShell-Szenarien mit C++- und C#-Hilfsprogrammen für interne Prüfungen und echte Windows-Oberflächen- und Shell-Tests. Führen Sie sie mit `tests.cmd` lokal oder in einer eingerichteten Hyper-V-VM aus. Siehe [Testanleitung](tests/regression/README.md).

---

FileSpacer ist eine Abspaltung von [ChromaFiler](https://github.com/vanjac/chromafiler), entwickelt von J. van’t Hoog. Vielen Dank an vanjac für diese Grundlage!

Veröffentlicht unter der [GNU GPL v3.0](LICENSE). Copyright © 2026 Benco für die FileSpacer-Änderungen; ursprünglicher ChromaFiler-Code © 2023 J. van’t Hoog. Siehe [Drittanbieterhinweise](THIRD_PARTY_NOTICES.md) und [Datenschutzerklärung](docs/privacy-policy.md).
