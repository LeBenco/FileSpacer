<a id="working-on-filespacer"></a>

# Aan FileSpacer werken

FileSpacer is een native ruimtelijke C++/Win32-bestandsbeheerder. Windows levert bestandsweergave en -bewerkingen; FileSpacer levert vensterbeleid, opslag en eigen besturingselementen. Deze documentatie helpt de code te vinden en is geen API-referentie. Gebruik: [gebruikershandleiding](user-guide.md).

<a id="start-here"></a>

## Hier beginnen

1. [Bouw de toepassing](build.md) met MSVC/NMAKE.
2. [Voer de relevante regressietests uit](../tests/regression/README.md), lokaal, in een [speciaal ingerichte Hyper-V-client](../tests/client/README.md), of via de [GitHub Actions-regressieworkflow](../tests/regression/README.md#run-on-github).
3. Lees de [Shell-architectuur](native-shell-view.md) vóór wijzigingen aan levensduur, herstel of itemidentiteiten.

<a id="code-map"></a>

## Codekaart

Alle toepassingsbronnen staan onder `src/`.

| Gebied | Beginpunten |
| --- | --- |
| Opstarten, proceslevensduur en herstel | `main.cpp`, `ProcessLifetime`, `CrashRecovery` |
| Mapvensters en ruimtelijke navigatie | `ItemWindow`, `FolderWindow`, `CreateItemWindow` |
| Native bestandsweergave | `ShellViewHost`, `ShellViewSettings`, `ShellViewSelection`, `ShellViewIcons`, `ShellViewFilter` |
| Mapidentiteit en opgeslagen status | `FolderIdentity`, `FolderStateStore`, `ClosedWindows` |
| Vensterplaatsing en gebaren | `WindowPlacement`, `WindowResizeGesture`, `WindowShortcuts.h` |
| Voorkeuren en vertalingen | `Settings`, `SettingsDialog`, `ShortcutSettingsPage`, `dialog.*.rc`, `resource.*.rc` |
| Zoeken en voorbeelden | `SearchBox`, `Everything`, `Peek` |
| Titelmenu en mapgroepen | `TitleMenu.h`, `TitleMenuDialog`, `TitleMenuRenderer`, `FolderGroups.cpp` |
| Gemengd sorteren | `MixedFolder`, `MixedFolderProvider` |
| Updates | `Update*` |

Bouw, installatieprogramma, regressieharnas en website hebben aparte ingangen: `Makefile` / `build.cmd`, `installer/`, `tests/` en `website/`. Voeg nieuwe C++-vertaaleenheden toe aan de juiste objectlijst. Houd UI-tekst in taalresources en behoud hun coderingen.

<a id="boundaries-to-preserve"></a>

## Te behouden grenzen

- Een map heeft één venster, ook tussen FileSpacer-processen. Identiteit wordt vóór de Shell-weergave verkregen. `FolderIdentity` gebruikt native identiteit voor lokaal NTFS, genormaliseerde UNC-paden voor externe mappen, paden voor andere lokale bestandssystemen en Shell-parsenamen voor virtuele mappen.
- Algemene voorkeuren horen in het register, mapstatus in SQLite. Geometrie, weergave en pictogramposities zijn afzonderlijk aanwezige delen van een momentopname. Ontbrekende gegevens mogen bekende delen niet wissen. De hoofdverbinding hoort bij de UI-thread; achtergrondlezers gebruiken eigen verbindingen. Houd schrijftransacties kort en buiten Shell-/netwerkaanroepen.
- Een gefilterde of onvoltooide weergave is geen volledige pictogramindeling. Sla haar niet als zodanig op. Vergrendeling beschermt geometrie en weergave; pictogramposities blijven opgeslagen worden.
- Toegangsfalen bewijst geen verwijdering. Behoud status tenzij verwijdering bevestigd is of bewaar-/quotaregels gelden.
- Shell-aanroepen en modale menu’s kunnen berichten verwerken en hun eigenaar sluiten vóór terugkeer. Een COM-verwijzing bewaart het C++-object, niet zijn venster of weergave. Controleer levensduur en observatiegeneratie opnieuw.

<a id="focused-notes"></a>

## Gerichte notities

- [Zoeken, geschiedenis en Everything](search-integration.md)
- [Venstergebaren](window-shortcuts.md)
- [Titelmenu en opdrachten](title-menu.md)
- [Standaardbeheerder en snelmenu’s](context-menu-integration.md)
- [Website en offlinehulp genereren](../website/README.md)
- [Vertalingen onderhouden](../website/translations/README.md)

Engels is de brontaal van de technische documentatie; Poolse, Nederlandse en Oekraïense vertalingen zijn ook beschikbaar. Beschrijf huidig gedrag, beperkingen en nuttige debuggingingangen. Bewaar metingen, logboeken en validatieoordelen per uitvoering bij testartefacten, niet in deze gidsen.
