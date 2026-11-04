<a id="native-shell-view-architecture"></a>

# Architectuur van de native Shell-weergave

Lees dit vóór wijzigingen aan weergavehosting, statusherstel of de grens tussen Shell-items en ruimtelijke vensters. Zie [Aan FileSpacer werken](technical/README.md) voor de bredere codekaart.

<a id="responsibilities"></a>

## Verantwoordelijkheden

`ShellViewHost` bevat `CLSID_ExplorerBrowser`. Windows beheert opsomming, weergave, pictogrammen, modi en bestandsbewerkingen. `FolderWindow` beheert ruimtelijk navigatiebeleid en coördineert opslag. Productie gebruikt `FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING` zonder `FVO_VISTALAYOUT` af te dwingen; virtuele mappen kiezen hun eerste weergave. `EBO_NOPERSISTVIEWSTATE` houdt mapopslag onder FileSpacers controle.

| Component | Verantwoordelijkheid |
| --- | --- |
| `ShellViewHost` | Browserlevensduur, navigatie, toegang en publieke gebeurtenissen. |
| `ShellViewSettings` | Modus, pictogramgrootte, vlaggen, kolommen, sortering en groepering vastleggen/toepassen. |
| `ShellViewSelection` | Selectie, focus en eenmalige heropen-/scrollmomentopname. |
| `ShellViewIcons` | Native identiteiten en vrije pictogramposities. |
| `ShellViewFilter` | Naamfilter via `IFolderFilter`. |
| `FolderStateStore` | Onafhankelijke statusdelen, transacties, vergrendeling en bewaring. |

Weergavebewerkingen gebruiken publieke COM-interfaces in plaats van Shell-kindbesturingselementen te zoeken of te subclassen. De labelachtergrond is een beperkte uitzondering, hieronder beschreven. Tests mogen besturingselementen inspecteren; die instrumentatie is geen toepassingsafhankelijkheid.

<a id="readiness-restoration-and-lifetime"></a>

## Gereedheid, herstel en levensduur

Navigatievoltooiing, weergaveaanmaak en voltooide opsomming zijn aparte gebeurtenissen. `OnViewCreated` betekent niet dat de inhoud gereed is. Gebruik publieke signalen; tijd of itemaantal bewijst geen gereedheid.

Meldingen bevatten een observatiegeneratie. Maak die ongeldig vóór afmelden of vernietigen en negeer oude berichten. Verbruik eenmalig herstel vóór herintredende COM-aanroepen. Veroorzaken weergave-instellingen een nieuwe opsomming, wacht dan op de volgende voltooiing vóór positie-/selectieherstel of opslag.

Shell-aanroepen, menu’s en vernieuwen kunnen berichten verwerken. Bewaar COM-verwijzingen en controleer ook of eigenaar en weergave nog bestaan. Een verwijzing beschermt het C++-object, niet `HWND` of browserresources. Gebruik `FolderWindow::refreshView()` en `invokeViewCommand()` zodat navigatiedieptebewaking vernietiging veilig uitstelt. Een verouderde voortzetting mag niet herstellen of opslag plannen.

Geometrie, weergave en pictogramposities zijn aparte momentopnamedelen. Vastleggingsfalen laat bekende delen intact. Gefilterde positievastlegging geeft `S_FALSE`; sla alleen op bij `S_OK`. Lege, gefilterde of onvolledig opgesomde inhoud is geen volledige indeling. Tijdelijke Beeld-voorbeelden mogen evenmin blijvend worden opgeslagen.

Selectie en itemfocus worden apart eenmalig hersteld zonder toetsenbordfocus te nemen. `SelectItem(nullptr, SVSI_DESELECTOTHERS)` mag bij wissen geen `SVSI_NOTAKEFOCUS` bevatten; die vlag hoort bij een gericht item. Omhoog navigeren selecteert de bron niet automatisch en vervangt geen selectie van een geopende ouder.

<a id="mixed-filefolder-sorting"></a>

## Gemengd sorteren van bestanden en mappen

Bij gemengd sorteren levert `MixedFolder` een Shell-knooppunt in lokale FileSpacer-gegevens met COM-registratie per gebruiker. `FileSpacerFolder.dll` implementeert `IShellFolder2` en `IPersistFolder2` en maakt de native weergave via `SHCreateShellFolderView`. `IExplorerBrowser` blijft host. Virtuele mappen en archieven houden hun Windows-provider. Lever toepassing en DLL samen.

Alleen de providerroot heeft een eigen knooppuntidentiteit. Kinderen behouden de relatieve native PIDL’s ongewijzigd. Opsomming, metadata, pictogrammen, menu’s, overdrachtsobjecten en acties delegeren naar de onderliggende map. Kolomvergelijking laat mapvoorrang weg; identiteit blijft native. Vrije plaatsing houdt de provider actief en bewaart posities zonder automatisch schikken af te dwingen.

Het private contract `IMixedFolder::NativeItem` kruist de FileSpacer-DLL-grens. Gebruik `nativeShellItem()` / `nativeViewSelection()` voor het echte bestandssysteemitem buiten de provider. SQLite bewaart native kind-ID’s voor dezelfde selectie en posities na voorkeurwijzigingen. Verpak of herinterpreteer kind-PIDL’s niet. De native weergave volgt de echte root via `SFVM_GETNOTIFY`, zonder dubbele knooppuntmeldingen.

Registratie en knooppunt zijn geïsoleerd in `/test`. Tests verwijderen alleen hun eigen registratie en map. Normale de-installatie verwijdert FileSpacers registratie.

<a id="selected-name-background-and-scrolling"></a>

## Achtergrond van geselecteerde namen en scrollen

`ShellViewLabels.h` vult optioneel een ondoorzichtige achtergrond achter één geselecteerd pictogramlabel. Windows tekent en vergroot de tekst. Meervoudige selectie, alleen-focusitems en andere modi blijven native.

De aanpassing subclasst alleen het publieke venster van `IShellView::GetWindow` op de eigen thread. Ze leest `NM_CUSTOMDRAW` van een geverifieerde ListView van die weergave met gedocumenteerde leesoperaties. Controleer echte selectie, niet alleen `CDIS_SELECTED`. Behoud `DefSubclassProc` en resultaat; voeg geen `CDRF_SKIPDEFAULT` toe. De callback bewaart geen FileSpacer-pointer en verwijdert zichzelf bij `WM_NCDESTROY`.

Dit meldingspad is implementatieafhankelijke weergaveaanpassing, geen algemeen Shell-contract voor labeluitbreiding. Vallen voorwaarden of meldingen weg, laat standaardtekenen intact. Maak er geen eigen multilabelrenderer of weergavebediening van.

Heropenen van Details/Lijst herstelt een eerste-zichtbare-rijanker, geen exacte pixels. `ShellViewSelection` vindt het via `IFolderView::Items` en posities binnen het clientgebied onder de toegankelijke kolomkop. Herstel maakt eerst het laatste item en dan het anker zichtbaar met onderdrukt hertekenen, zonder focus te nemen. Pictogramweergaven gebruiken ongescrolde posities en openen bovenaan. `IFolderView2::GetVisibleItem` is geen viewportquery; neem geen schuifpositie in `GetViewStateStream` aan.

<a id="activation-and-external-folders"></a>

## Activering en externe mappen

Bij selectiebehoud activeert een gewone linksklik op de achtergrond een inactieve weergave zonder wissen. De lus gebruikt `WM_ACTIVATE` / `WA_CLICKACTIVE` en het echte Shell-doel vóór het verwerpen van de eerste achtergrondklik. Een titelklik mag geen latere clientklik onderdrukken. Items, rechtsklikken, kopteksten, schuifbalken en actieve vensters blijven normaal. `WM_MOUSEACTIVATE` op het buitenframe is niet betrouwbaar via Shell-vensters.

Configuratiescherm opent extern via `IOpenControlPanel::Open`. Vergelijk Shell-identiteiten, geen vertaalde namen of gedecodeerde PIDL’s. Behoud bronmap en selectie.

<a id="tests-and-references"></a>

## Tests en referenties

De [regressiehandleiding](tests/regression/README.md) koppelt gedrag aan groepen. Begin met 18 voor echte Shell/SQLite-opslag, 19 voor opdrachten, 21 voor activering, 23 voor labels, 28 voor Beeld-voorbeelden en `30-tri-melange` voor providers. Geïnjecteerde fout-/levensduurtests dekken paden die een geslaagde echte-weergavetest niet bewijst.

- [IExplorerBrowser](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorerbrowser)
- [IFolderView2](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ifolderview2)
- [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-folderviewoptions)
- [Native mapweergaven](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreateshellfolderview)
- [Naamruimteknooppunten](https://learn.microsoft.com/en-us/windows/win32/shell/nse-junction)
- [SFVM_GETNOTIFY](https://learn.microsoft.com/en-us/windows/win32/shell/sfvm-getnotify)
- [IOpenControlPanel::Open](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iopencontrolpanel-open)
