<a id="default-manager-and-context-menu-integration"></a>

# Standaardbestandsbeheerder en snelmenu-integratie

`Settings.cpp` beheert FileSpacers mapkoppelingen en menu-items voor de alternatieve beheerder. Windows kiest en start nog steeds de standaardactie. Zie [Windows-integratie](user-guide.md#windows-integratie) voor gebruik.

<a id="associations-and-menu-visibility"></a>

## Koppelingen en menuzichtbaarheid

Lees de effectieve `HKEY_CLASSES_ROOT`-koppeling afzonderlijk voor `Directory`, `Drive`, `CompressedFolder`, `NetServer` en `NetShare`. Een standaardverb kan een geordende lijst met spaties of komma’s zijn: gebruik het eerste verb in plaats van de hele tekenreeks te vergelijken. Een serverhoofdmap zoals `\\server` is `NetServer`, geen `Directory`; zonder netwerkklassen behouden die doelen een andere beheerder.

`setDefaultBrowser()` wijzigt elke koppeling afzonderlijk en rapporteert fouten per klasse. `synchronizeContextMenus()` stemt alleen FileSpacer-menuregistraties onder `Software\Classes` van de huidige gebruiker af; het wijzigt nooit de standaardkoppeling of Windows-verbs. Dit levert ook gebruikersoverschrijvingen voor een voor alle gebruikers geïnstalleerde opdracht.

Als FileSpacer de standaard is, behouden itemmenu’s **Openen in FileSpacer** en voegen **Openen in Windows Bestandenverkenner** toe. Anders bieden ze **Openen in FileSpacer**. De mapachtergrond volgt de mapkoppeling en toont alleen de alternatieve beheerder.

Dit onderscheid is essentieel: `CMF_DEFAULTONLY` slaat verbs met `ProgrammaticAccessOnly` over, ook als de koppeling dat verb noemt. Houd FileSpacers **itemverb** zichtbaar zodat de standaardaanroep van de Shell het kiest. Gebruik `ProgrammaticAccessOnly` alleen voor het **achtergrondverb**, dat geen item opent. Behoud de geregistreerde opdracht en `DelegateExecute`-waarden.

Het afzonderlijke verb `filespacer.explorer` start `"%SystemRoot%\explorer.exe" "%1"` (`%V` voor een achtergrond). De standaard herstellen verwijdert dit alternatieve verb en FileSpacers zichtbaarheidsoverride. Herschrijf ongewijzigde waarden niet; meld een echte menuwijziging met `SHCNE_ASSOCCHANGED`.

<a id="startup-settings-and-installer-boundary"></a>

## Grenzen van opstarten, instellingen en installatieprogramma

Normaal opstarten synchroniseert bestaande registraties; draagbaar opstarten installeert niets. Debug `/test` slaat productiesynchronisatie over. Integratie-instellingen synchroniseren na koppelingswijzigingen en bewaren fouten per klasse. Ontbrekende geïnstalleerde netwerkverbs vereisen een installatie-update; meld dan geen volledig geslaagde standaardwijziging.

Het installatieprogramma roept `/sync-context-menus` aan zonder een map te openen of crashherstel te starten en controleert de afsluitcode. Componentverwijdering/de-installatie verwijdert gebruikersoverschrijvingen via het opschonen van gebruikershives. Opstarten noch synchronisatie wijzigt stilzwijgend de gekozen standaardbeheerder.

<a id="testing-the-boundary"></a>

## De grens testen

Groep `29-menu-gestionnaire` gebruikt geïsoleerde registerhandles en echte `SHCreateDefaultContextMenu`. Ze controleert onafhankelijke klassestandaarden, alternatieve verbs, behoud van opdrachten/COM-delegates, foutdoorgifte en native standaardselectie. Beide `ShellExecuteEx`-paden starten een echt testsubproces. Netwerksnelkoppelingen testen de koppelingszoekactie van de Shell voor het doel, niet alleen de `.lnk`-klasse.

Deze controles vervangen geen fysieke dubbelklik in Verkenner of acceptatie van geïnstalleerde upgrades/de-installaties. Groep 19 opent en annuleert apart echte item- en achtergrondmenu’s. Volg de [regressiehandleiding](tests/regression/README.md) voor uitvoering en interpretatie; bewaar uitvoeringsresultaten bij de artefacten.

Referenties: [Shell-werkwoorden](https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers),
[Het standaardmenu opbouwen](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-querycontextmenu),
[Voorgedefinieerde Shell-klassen](https://learn.microsoft.com/en-us/windows/win32/shell/reg-shell-exts),
[Melding van gewijzigde koppelingen](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify).
