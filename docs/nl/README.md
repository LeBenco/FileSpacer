<p align="center">
    <img height="200" alt="FileSpacer" src="../../src/res/FileSpacer.svg" />
</p>

# FileSpacer

[![Nieuwste versie downloaden](https://img.shields.io/badge/Downloaden-nieuwste%20versie-blue)](https://github.com/LeBenco/FileSpacer/releases/latest) &emsp;
[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![Geen telemetrie](https://img.shields.io/badge/telemetrie-geen-2ea44f)](#privacy)
[![Licentie: GPL v3](https://img.shields.io/badge/Licentie-GPLv3-blue.svg)](#license-and-credits)

FileSpacer is een bestandsbeheerder voor Windows volgens het [ruimtelijke model](https://en.wikipedia.org/wiki/Spatial_file_manager): elke map heeft één venster dat zijn positie, afmetingen en weergave-instellingen onthoudt. Dit biedt [vaste visuele herkenningspunten](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), waardoor het [ruimtelijk geheugen](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) vertrouwde mappen in één oogopslag helpt herkennen. In een navigerende bestandsbeheerder toont hetzelfde venster achtereenvolgens verschillende mappen: positie en afmetingen identificeren de huidige map niet, dus moet u naar de inhoud kijken. Door herkenningspunten per map te bewaren kan de ruimtelijke aanpak de cognitieve belasting bij het terugvinden van bestanden verminderen. Tabbladen passen niet bij het ruimtelijke model en zullen nooit worden toegevoegd.

<img src="images/spatial-windows-nl.png" alt="Drie niet-overlappende mapvensters met verschillende afmetingen, weergaven en pictogramgroottes, waaronder een vrije indeling." width="560">

FileSpacer kiest voor een lichte, overzichtelijke ervaring. Om de opeenstapeling van vensters, een bekend nadeel van het ruimtelijke model, te beperken, sluit FileSpacer standaard het bronvenster wanneer een map wordt geopend. Houd Ctrl ingedrukt om het open te houden wanneer u met meerdere mappen wilt werken. De interface blijft bewust bescheiden, anders dan sommige volle interfaces waar tussen de knoppen nog net een bestand te zien is.

<a id="system--languages"></a>**Systeem en talen:** Windows 10 of nieuwer, x64. Tien talen: Engels, Frans, Duits, Italiaans, Spaans, Portugees (Portugal), Grieks, Pools, Nederlands en Oekraïens.

<a id="privacy"></a>**Privacy:** **Geen telemetrie, gebruiksregistratie of reclame**. Updatecontroles maken contact met GitHub; wekelijkse controles zijn optioneel en staan standaard uit. Downloaden en installeren vereisen bevestiging. [Privacybeleid](user-guide.md#privacy).

**Documentatie:**

[![Documentatie](https://img.shields.io/badge/Docs-Handleiding-blue)](docs/user-guide.md)
[![Documentatie](https://img.shields.io/badge/Docs-Wijzigingen-orange)](CHANGELOG.md)

_FileSpacer is een fork van [ChromaFiler](https://github.com/vanjac/chromafiler), oorspronkelijk gemaakt door J. van't Hoog. Zie de [vermeldingen](#license-and-credits) voor details._

## Functies

- **Eén venster per map**, hergebruikt als de map al open is. Positie, afmetingen, weergave-instellingen en pictogramindeling worden hersteld. [Ruimtelijke navigatie](user-guide.md#window-navigation).
- **Pictogrammen vrij plaatsen**, met opgeslagen en herstelde posities per map. [Pictogramindeling](user-guide.md#icon-layout).
- **Vensters verplaatsen en het formaat wijzigen** vanuit het hele bestandsgebied, met behoud van selectie. Muisgebaren zijn instelbaar; standaard Alt + slepen met links en Alt + slepen met rechts. [Venstergebaren](user-guide.md#moving-and-resizing).
- **Schakelen tussen geopende mappen** via het titelbalkmenu, bovenliggende vensters sluiten of het laatst gesloten venster opnieuw openen met Ctrl+Shift+T. [Vensternavigatie](user-guide.md#window-navigation).
- **Een opgeslagen vensterindeling vergrendelen**, terwijl tijdelijke wijzigingen in positie, afmetingen en weergave mogelijk blijven. [Vergrendeling](user-guide.md#locking-a-saved-layout).
- **De huidige map filteren** en recente zoekopdrachten hergebruiken. [Zoeken en geschiedenis](user-guide.md#search-and-history).
- **Selectie behouden bij activering** door op de achtergrond te klikken. [Selectie en activering](user-guide.md#selection-and-activation).
- **Overlap beperken** bij mappen zonder opgeslagen positie. [Vensterplaatsing](user-guide.md#window-placement).
- **Het titelbalkmenu aanpassen** met indelings- en vensteracties en benoemde groepen geopende mappen opslaan. [Menu aanpassen](user-guide.md#customize-the-title-bar-menu) · [Mapgroepen](user-guide.md#folder-groups).

## Integratie

FileSpacer kan tijdens installatie of later in Instellingen als standaardbestandsbeheerder worden ingesteld. Bestanden openen in hun gebruikelijke Windows-toepassingen; netwerkmappen gebruiken de Windows-afhandeling. [Windows-integratie](user-guide.md#windows-integration).

Met **Everything** start u geavanceerde zoekopdrachten vanuit het FileSpacer-zoekveld, in de huidige map met submappen of in alle geïndexeerde bestanden. Resultaten openen in Everything. [Integratie en gebruik](user-guide.md#everything).

Met **Nilesoft Shell** kunnen de menu’s thema’s en aangepaste opdrachten gebruiken. Configuratieaanpassingen worden aanbevolen voor een consistent uiterlijk. [Integratie en configuratie](user-guide.md#nilesoft-shell).

## Bouwen

[![FileSpacer publiceren](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

Bouwen gebruikt MSVC/NMAKE, Visual Studio 2022 Build Tools met ATL en een Windows SDK, Inkscape en ImageMagick. NSIS is nodig voor het installatieprogramma; SQLite is meegeleverd. Zie de [bouwhandleiding](build.md).

Regressietests combineren PowerShell-scenario’s met C++- en C#-helpers voor interne controles en echte Windows-interface- en Shell-tests. De [regressieworkflow in GitHub Actions](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) voert deze toepassingstests op verzoek en automatisch vóór een publicatie uit. Dezelfde gevallen kunnen met `tests.cmd` op een lokaal Windows-bureaublad of in een geconfigureerde Hyper-V-VM worden uitgevoerd. De [testhandleiding](tests/regression/README.md) beschrijft de casusselectie, vereisten en resultaten.

## Licentie en dankbetuigingen

### Auteursrecht en licentie
FileSpacer wordt verspreid onder de <img align="absbottom" src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="Licentie: GPL v3">-licentie.<br>
Copyright © 2026 Benco voor de wijzigingen aan FileSpacer. De volledige licentie staat in [LICENSE](../../LICENSE) in de broncode, of in `LICENSE.txt` in de installatiemap. FileSpacer wordt zonder garantie geleverd; verspreiding en wijziging zijn onder deze licentie toegestaan.

### Fork en dank aan het oorspronkelijke project
FileSpacer is een fork van [ChromaFiler](https://github.com/vanjac/chromafiler) (oorspronkelijke ChromaFiler-code © 2023 J. van't Hoog), gemaakt door J. van't Hoog (vanjac). Veel dank aan vanjac voor de basis!

### Vermeldingen van derden
Zie de [vermeldingen van derden](THIRD_PARTY_NOTICES.md) en het [privacybeleid](privacy-policy.md).
