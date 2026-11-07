<a id="search-history-and-everything"></a>

# Zoeken, geschiedenis en Everything

Begin in `SearchBox`, `FolderWindow::applyNameFilter`, `ShellViewFilter` en `Everything`. Interactiedetails staan in de [gebruikershandleiding](user-guide.md#search-and-history).

<a id="local-filtering-and-suggestions"></a>

## Lokaal filteren en suggesties

SearchBox scheidt expliciet indienen van de typevertraging van 150 ms. Het lokale filter loopt via de Shell; suggesties gebruiken een niet-activerende Win32-pop-up en een native door de eigenaar getekende keuzelijst. Het tekstveld houdt de focus. Eén geschiedenisopslag en één itemopbouwer bedienen Everything en recente zoekopdrachten.

Een suggestie wordt pas geselecteerd na toetsenbordnavigatie of een muisdruk. Alleen aanwijzen selecteert Everything niet; bewerken wist een eerdere selectie. Enter zonder suggestie blijft lokaal. Escape sluit zonder tekst te wissen. Tab, focusverlies, verplaatsing/deactivering en vernietiging van de eigenaar sluiten de pop-up. Houd teksten toegankelijk en grenzen binnen het monitorwerkgebied.

Alleen geslaagd expliciet lokaal indienen, een recente zoekopdracht kiezen of extern starten registreert een niet-lege zoekopdracht. Alleen typen mag de geschiedenis niet vullen.

<a id="launching-everything"></a>

## Everything starten

`EverythingEnabled` is standaard false. Een lege `EverythingPath` betekent automatische detectie; `EverythingScope` kiest de huidige bestandssysteemmap met onderliggende mappen (0) of overal (1). Detectie controleert beide registerweergaven, App Paths, voidtools-installatievermeldingen en standaardprogrammalocaties.

Geef één `-search`-argument door. Mapbereik voegt een geciteerd pad eindigend op `\` toe en groepeert de zoekopdracht met `<…>` om OR-voorrang te behouden. De zoekopdracht blijft Everything-syntaxis. Ingebedde aanhalingstekens gebruiken Everything’s drievoudige aanhalingstekens, los van Windows CRT-citering van de programmanaam. `CreateProcessW` ontvangt een apart absoluut programmapad en beschrijfbare Unicode-opdrachtbuffer, zonder opdrachtinterpreter.

Procesaanmaak bewijst alleen geslaagd starten, geen indexering of bruikbare resultaten. Wis bij geslaagd starten het lokale filter; behoud bij fouten zoekopdracht/filter en meld de fout.

<a id="history-persistence"></a>

## Geschiedenis opslaan

`SearchHistoryLimit` is standaard 5 en accepteert 0–20. De SQLite-tabel `search_history(sequence,key,query)` staat los van weergavestatus en gesloten-venstergeschiedenis. Volgnummers ordenen ook binnen dezelfde seconde betrouwbaar. Samenvoegen van statusopslag stelt schrijven uit; normaal sluiten schrijft het weg.

Een limietwijziging kort alle opgeslagen mappen transactioneel in, ook gesloten mappen. Meldingen bevatten de toegepaste limiet en normale/testnaamruimte, zodat vertraagde berichten geen gewiste cache herstellen of productiegegevens vanuit een testvenster wijzigen.

Geschiedenis volgt wijzigingen van padidentiteit. De verwijdertrigger van `folder_state` verwijdert haar bij bevestigde verwijdering, geldige vervaldatum, reset of quotaverwijdering. Schrijven vereist bestaande mapstatus en mag geen verlopen map terugmaken. Toegangsfalen alleen is geen verwijdering; het bewaarbeleid geldt.

<a id="everything-icon-loading"></a>

## Everything-pictogram laden

De suggestie gebruikt het hoofdpictogram van het ingestelde programma, geen meegeleverd logo. Een Windows-threadpooltaak bepaalt het pad, leest metadata en koppelt resources met `LoadLibraryExW`-data/afbeeldingsresourcevlaggen. Ze voert geen module-imports uit. `LoadIconWithScaleDown` kiest een DPI-passend beeld; geef de resourcemodule direct vrij en behoud alleen de eigen `HICON`.

Elke volgende opening controleert pad, aanmaak-/wijzigingstijden en grootte op de achtergrond. Instellingen-/DPI-wijzigingen maken de cache per veld ongeldig. Updates die al die waarden behouden worden niet gedetecteerd. Tekenen gebruikt een niet-blokkerende cachevergrendeling; een ontbrekend pictogram verhindert de actie niet. Sluiten annuleert publicatie zonder op de taak te wachten; gedeelde status en handles blijven geldig tot voltooiing.

<a id="tests-and-references"></a>

## Tests en referenties

Groep `25-recherche-integration` scheidt opslag-/argumentcontroles, native weergavegedrag en de echte Everything-toepassing. Volg de [vereisten en isolatieregels](tests/regression/README.md#external-applications) vóór de externe test. Vergelijk getoonde zoekopdracht-/resultaatidentiteiten, inclusief onderliggende mappen en een treffer buiten mapbereik; procesaanmaak of een gelijk resultaataantal volstaat niet.

- [Everything-opdrachtregelopties](https://www.voidtools.com/support/everything/command_line_options/)
- [Everything-zoeksyntaxis](https://www.voidtools.com/support/everything/searching/)
- [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
