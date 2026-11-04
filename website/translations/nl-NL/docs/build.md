<a id="building-filespacer-on-windows-x64"></a>

# FileSpacer bouwen op Windows x64

FileSpacer richt zich op Windows 10 of nieuwer, x64. Het gebruikt MSVC/NMAKE en NSIS, geen CMake. Voer bouw- en testopdrachten uit vanuit CMD in de hoofdmap. Codekaart: [ontwikkelaarsoverzicht](README.md).

<a id="dependencies"></a>

## Afhankelijkheden

- Visual Studio 2022 Build Tools met MSVC, ATL en Windows SDK. De launcher verwacht `%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools`.
- Inkscape, normaal `C:\Program Files\Inkscape\bin\inkscape.com`.
- Python met Pillow, geleverd door Windows-Inkscape; de bouw gebruikt de naastgelegen `python.exe`. `ICON_PYTHON` kiest een andere installatie.
- ImageMagick met `magick` op PATH.
- NSIS, normaal `%ProgramFiles(x86)%\NSIS\makensis.exe`, voor pakketten.
- Node.js 22+, Ruby 3.3 en Bundler voor offlinehulp; zie [website- en offlinebouw](../website/README.md#shared-website-and-offline-builds).

<a id="build-and-clean"></a>

## Bouwen en opruimen

Sluit FileSpacer vóór het bouwen en voer uit:

```cmd
build.cmd debug
build.cmd release
build.cmd installer
```

`build.cmd` initialiseert x64 MSVC via `vcvars64.bat` en start NMAKE. Debug gebruikt `FILESPACER_DEBUG`, `/MTd` en het consolesubsysteem. Release gebruikt `/O2`, `/MT` en het Windows-subsysteem. C++ gebruikt `/W4` en C++14. Voeg nieuwe eenheden aan de juiste `OBJECTS`-lijst in `Makefile` toe.

| Uitvoer | Gebruik |
| --- | --- |
| `build\debug\FileSpacer.exe` | Ontwikkeling en regressietests. |
| `build\release\FileSpacer.exe` | Release-toepassing. |
| `FileSpacerFolder.dll` | Native provider voor gemengd sorteren naast het programma; samen bewaren. |
| `build\FileSpacer-install.exe` | Verspreidbaar installatieprogramma. Verpakken start het niet. |

Niet-standaardlocaties voor hulpmiddelen opgeven:

```cmd
set "INKSCAPE=D:\Tools\Inkscape\bin\inkscape.com"
set "MAKENSIS=D:\Tools\NSIS\makensis.exe"
build.cmd installer
```

Een ongewijzigde bouw laat het programma up-to-date. Het installatiedoel verpakt Release-artefacten altijd opnieuw.

```cmd
build.cmd clean
```

Clean verwijdert Debug/Release-uitvoer, gegenereerde ICO en installatieprogramma, met behoud van bronnen en gebruikersgegevens. De volgende bouw maakt het pictogram opnieuw.

<a id="generated-artwork"></a>

## Gegenereerd beeldmateriaal

`generate-icon.py` maakt de tien beelden in `src\res\FileSpacer.ico` uit SVG en optische kleine-formaatmasters. Bij 16–48 px wordt de map op het pixelraster getekend en worden tesseractvlakken overbemonsterd en met Box verkleind. Inkscape rendert 64–256 px. ImageMagick verpakt; de generator controleert gedecodeerde RGBA vóór atomaire ICO-vervanging. NumPy is niet nodig.

De 2D-SVG-vlakken zijn leidend; de generator herberekent geen projectie uit 4D-/camerametadata. Silhouet- of 212 × 212-coördinatenwijzigingen vereisen ook aanpassing van `FOLDER_MASTERS` / `draw_folder`. Houd de [16px-favicon](../website/README.md) gelijk.

Verpakken start `generate-installer-assets.py` voor downloadbadge, zijbalken, resource-DLL en UTF-16-wijzigingsgeschiedenis. Uitvoer staat onder `build\installer-assets`, buiten versiebeheer. `installer\wizard-ui.rc` bepaalt de gedeelde indeling. De x86-`WizardBitmap.dll` schaalt zijbalken met GDI+, met NSIS-lader als terugval. Ze wordt alleen in de tijdelijke plug-inmap uitgepakt. Debug controleren vanuit een x64-ontwikkelprompt:

```cmd
nmake /f Makefile CFG=Debug installer-bitmap
```

## SQLite

De ongewijzigde amalgamatie in `third_party\sqlite` wordt apart als C gebouwd en statisch gekoppeld. Geen SQLite-DLL, CLI of download tijdens bouwen nodig. Zie [herkomst en opties](https://github.com/LeBenco/FileSpacer/blob/main/third_party/sqlite/README.txt).

SQLite gebruikt `/W3`, `_CRT_SECURE_NO_WARNINGS`, `SQLITE_OMIT_LOAD_EXTENSION` en `SQLITE_DQS=0`, met standaard threadveiligheid en statische runtime. Release koppelt met `/OPT:REF /OPT:ICF`. Vanuit een ontwikkelprompt:

```cmd
dumpbin /dependents build\release\FileSpacer.exe
```

Er hoort geen `sqlite3.dll`-afhankelijkheid te zijn; Windows-systeem-DLL’s blijven nodig.

<a id="offline-help-and-installer-contracts"></a>

## Offlinehulp en installatiecontracten

Setup verpakt de gegenereerde map `help`, controleert bronvingerafdruk/bestandshashes en herbouwt ontbrekende, verouderde of beschadigde uitvoer. Node.js moet op PATH staan (`NODE` kan dit vervangen); Ruby/Bundler zijn alleen bij regenereren nodig. De workflow kan geverifieerde Linux-hulp uit dezelfde geteste commit naar Windows overdragen. Geïnstalleerde hulp vereist alleen een browser, geen server of netwerk.

Setup start als `asInvoker`. Installatie voor alle gebruikers start pas bij begin een verhoogde worker via `runas`. De ouder bewaart map, taal en componenten, wacht op de afsluitcode en past persoonlijke koppelingen onder de oorspronkelijke gebruiker toe. De worker mag de persoonlijke standaardbeheerder van de administrator niet wijzigen. Algemene de-installatie vraagt ook verhoging. Annulering behoudt het updater-afsluitcodecontract.

`install-info.ini` bewaart bereik, taal en componenten. In `[Components]` zijn `StartMenu`, `ContextMenu` en `DefaultFileManager` `0` of `1`. Upgrades herstellen keuzes; stille upgrades behouden de effectieve standaardkoppeling, ook na latere wijzigingen. De standaardbeheerder vereist de snelmenucomponent.

De updaterelay bepaalt bereik uit metadata naast het actieve programma. Registerterugval moet exact die map in één bereik matchen. Na alle geslaagde secties schrijft setup UTF-16 `/UpdateResult=` met `[Installation]`, `Mode`, `Executable`, `Complete=1`. De relay valideert programma en bereik vóór vensterherstel. Annulering/falen herstelt de oorspronkelijke toepassing; ongeldig succes bewaart de sessie en meldt een fout. Oude-relayopruiming mag wachtende sessies niet verwijderen. Groep `11-session-maj` dekt dit.

Setup logt naar `%TEMP%\FileSpacer-install.log`, de worker naar `%TEMP%\FileSpacer-install-machine.log`. `/Log="C:\path\setup.log"` vervangt het eerste pad. Elke run vervangt zijn log. Bestandsafsluitcode 5 is een installatiecategorie, geen Windows-foutnummer. Bewaar eerste fout, opruimuitzondering/stack en opdrachtafsluitcodes; loggen verandert geen succes-/foutstatus.

Een NSIS-bouw controleert verpakking, geen echte installatie, UAC, upgrade of de-installatie. Valideer gewijzigde geïnstalleerde paden afzonderlijk.

## Releases

[De releaseworkflow](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/release.yml) start op `vX.Y.Z` of handmatig voor een bestaande tag. Eerst wordt de [regressieworkflow voor de toepassing](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/regression.yml) hergebruikt en moet deze slagen; daarna bouwt hij offlinehulp en Windows-NSIS uit exact de geteste SHA. Hij controleert versie tegen tag en publiceert `FileSpacer-X.Y.Z-install.exe` en `FileSpacer-update.txt` op GitHub Releases. Gewone branchpushes publiceren geen installatieprogramma. GitHubs automatische token volstaat voor de release.

Na publicatie dient een aparte WinGet-taak het installatieprogramma bij de communitycatalogus in. Een eenmalig ingesteld `WINGET_TOKEN` is vereist; fouten draaien de GitHub-release niet terug. Zie [WinGet-distributie](winget.md) voor configuratie, controle en herhalen zonder herbouwen.

Werk vóór publicatie beide versiedefinities in `src/resource.h` bij, voeg de juiste `CHANGELOG.md`-sectie toe en commit de bedoelde wijzigingen. Vul in Git Bash de versie in:

```bash
bash release.sh X.Y.Z
```

Het script vereist een schone werkboom, controleert externe branch/tag, toont wijzigingen en vraagt bevestiging vóór push van `main` en de geannoteerde tag. Controleer daarna workflow en beide bestanden. Foutsamenvattingen en bouwlogboeken blijven workflowartefacten.

## Tests

De [regressiehandleiding](../tests/regression/README.md) beschrijft dezelfde toepassingstests op een lokaal Windows-bureaublad, in Hyper-V en via GitHub Actions, inclusief selectie, vereisten en interpretatie van rapporten. Bijvoorbeeld:

```cmd
tests.cmd -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Voer betrokken tests tijdens ontwikkeling uit. Tests gebruiken Debug en geïsoleerde gegevens; verwijder nooit de productiedatabase ter voorbereiding. Lees de [Shell-architectuur](native-shell-view.md) vóór wijzigingen aan herstel of levensduur.
