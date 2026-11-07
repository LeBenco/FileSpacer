<a id="title-bar-menu-implementation"></a>

# Implementatie van het titelbalkmenu

Zie de [gebruikershandleiding](user-guide.md#het-titelbalkmenu-aanpassen) voor bewerken en mapgroepen. Begin in `TitleMenu.h`, `TitleMenuDialog.cpp`, `TitleMenuRenderer.cpp` en `FolderGroups.cpp`; het mapvenster verwerkt de live opdrachten.

<a id="composition-and-rendering"></a>

## Samenstelling en weergave

Voorkeuren bewaren stabiele `TitleMenuEntry`-waarden, onafhankelijk van vertaalde labels en dynamische venster-ID’s. `validTitleMenu()` accepteert maximaal 64 items, herhaalde scheidingstekens en geen dubbele acties. Het open-vensteritem is een blok dat bij het openen uit de actuele vensterlijst wordt opgebouwd, geen opdracht voor een permanent venster.

De editor slaat direct op bij OK; Annuleren verwerpt het concept. Standaardwaarden herstellen wijzigt alleen het concept. Andere vensters lezen de samenstelling bij de volgende menuopening. Behoud context-/beschikbaarheidscontroles bij verplaatsen of extern aanroepen van acties.

FileSpacer stelt het menu samen, daarna past Nilesoft Shell regels toe. Door Nilesoft toegevoegde of geplaatste items zijn alleen-lezen. Een hernoemd item blijft bewerkbaar als Nilesoft de positie niet beheert; onduidelijke reeksen scheidingstekens gelden voorzichtigheidshalve als extern beheerd.

Het voorbeeld gebruikt de echte Windows/Nilesoft-renderer in een tijdelijk FileSpacer-proces op een privébureaublad. Het voert getoonde acties nooit uit. Bewaar tijdens slepen de vastgelegde menupixels. Kan de gelaagde Nilesoft-achtergrond niet worden vastgelegd, gebruik dan de expliciete ondoorzichtige terugvalweergave in plaats van compositoracryl te simuleren.

<a id="action-boundaries"></a>

## Actiegrenzen

- Vergrendelde indeling herstellen past opgeslagen geometrie/weergave toe via de bestaande weergave zonder selectie te verliezen. Referentie vervangen legt de huidige indeling vast onder een bestaande vergrendeling en vereist bevestiging.
- Onderliggende/andere vensters sluiten gebruikt normale sluiting en opslag, ook tussen processen. Aan inhoud aanpassen gebruikt native posities, tussenruimte en zichtbare kolombreedten, begrensd door het monitorwerkgebied.
- Mapgroepen bewaren benoemde lijsten los van het menu. Oproepen opent/activeert elke map met haar eigen indeling en houdt andere vensters open. Groepen verwijderen verwijdert geen mappen; algemene voorkeuren resetten wist geen groepen.

<a id="nilesoft-command-bridge"></a>

## Nilesoft-opdrachtbrug

Dit zijn stabiele `WM_COMMAND`-ID’s, verschillend van opgeslagen `TitleMenuEntry`-waarden en dynamische vensterlijst-ID’s. Nilesoft kan een ontbrekende actie aanroepen met [`window.command`](https://nilesoft.org/docs/functions/window). Binnen een regel beperkt tot het FileSpacer-titelmenu:

```nss
item(title='Venster aan inhoud aanpassen' cmd=window.command(1128))
```

De menu-eigenaar stuurt de opdracht door naar het echte mapvenster; levensduur-, context- en vergrendelingscontroles blijven gelden. Zie de [configuratiereferentie](nilesoft-shell.md) voor bereik en uiterlijk.

| Actie | Opdracht-ID |
| --- | ---: |
| Pad kopiëren | 1120 |
| Naam wijzigen | 1008 |
| Vensterstatus vergrendelen | 1100 |
| Eigenschappen | 1121 |
| Bovenliggende vensters sluiten | 1122 |
| Laatst gesloten venster opnieuw openen | 1021 |
| FileSpacer-instellingen | 1009 |
| Vergrendelde indeling herstellen | 1123 |
| Referentie-indeling vervangen | 1124 |
| Onderliggende vensters sluiten | 1126 |
| Andere vensters sluiten | 1127 |
| Venster aan inhoud aanpassen | 1128 |
| Mapgroep opslaan / oproepen | 1129 |

## Tests

Groep `33-menu-titre` test native samenstelling en acties in Debug `/test` met geïsoleerde voorkeuren, groepen en mapstatus. Dit sluit Nilesoft uit van menu/editor. Een expliciete interoperabiliteitstest kan `FILESPACER_TEST_NILESOFT=1` in de eigen procesomgeving gebruiken; Release negeert dit. Native tests valideren Nilesoft-weergave niet.

Interoperabiliteitstests gebruiken een bestaande Nilesoft-installatie en aparte testconfiguratie. Bewaar en herstel de exacte geïnstalleerde register-/configuratiestatus, ook na time-outs. Geen Nilesoft-binary wordt met FileSpacer of gewone tests verspreid. Kopieer geen geïnstalleerde DLL’s naar gepubliceerde testartefacten; behoud toepasselijke licentie-/distributievermeldingen voor apart geleverde afhankelijkheden.
