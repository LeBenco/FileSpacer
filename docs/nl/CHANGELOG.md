<a id="changelog"></a>

# Wijzigingsgeschiedenis

<a id="141--2026-11-09"></a>

## 1.4.1 — 2026-11-09

- Fouten in de database met opgeslagen mapgegevens toelichten met kopieerbare diagnostische informatie, toegang tot de gegevensmap en waar mogelijk informatie over betrokken processen. Als de database niet kan worden geladen of hersteld, opnieuw instellen na bevestiging en met een volledige back-up aanbieden.
- Onnodige schrijfacties naar de zoekgeschiedenis vermijden bij het toepassen van ongewijzigde instellingen, zodat een niet-beschikbare database met mapgegevens deze stap vóór een automatische update niet blokkeert.

- Het vastlopen van de grootteaanpassing van de werkbalk na het openen van het bijbehorende menu of Instellingen verhelpen, ook in de detailweergave met uitgestelde aanpassing van de kolombreedtes.

<a id="140--2026-11-09"></a>

## 1.4.0 — 2026-11-09

- Opgeslagen vrije pictogramposities behouden wanneer de Shell een lege opsomming als voltooid meldt voordat de mapinhoud is geladen.
- Het menu Beeld en de vrije plaatsing van pictogrammen beschikbaar houden wanneer de opgeslagen mapstatus tijdelijk niet kan worden gelezen.
- Het pictogram van de knop Snelle toegang rechtstreeks uit de Windows-shell laden.
- Automatische aanpassing van kolombreedtes in de detailweergave toevoegen, met optionele live-updates.
- Sneltoetsen van Verkenner en aanpassingen voor de ruimtelijke werkwijze toevoegen (Ctrl+N en Ctrl+Backspace).
- Het verkleinen van grote selecties door op een item of de achtergrond van klassieke Shell-weergaven te klikken sterk versnellen, met een automatische, stille terugval als de private Windows-functie ontbreekt.
- Volledige en grote gedeeltelijke selecties sneller uitlezen in pictogramweergaven met vrije plaatsing via openbare Windows-API’s.
- De algemene Windows-instelling voor miniaturen wijzigen via het menu Beeld.
- De schuifpositie van een map herstellen wanneer het venster opnieuw wordt geopend.
- Nieuwe vensters plaatsen op basis van de normale grootte en positie van bestaande FileSpacer-vensters, ook wanneer deze gemaximaliseerd, geminimaliseerd of vastgemaakt zijn.
- Enorme versnelling van ‘Alles selecteren’ en het deselecteren van duizenden bestanden in klassieke Shell-weergaven. Deze optimalisatie gebruikt private Windows-functies waarvan de beschikbaarheid en het gedrag niet gegarandeerd zijn. Als deze functies ontbreken of niet compatibel zijn, valt FileSpacer automatisch en zonder melding terug op publieke functies, zonder de snelheidswinst.

<a id="132--2026-11-04"></a>

## 1.3.2 — 2026-11-04

- Pools, Nederlands en Oekraïens toegevoegd aan de toepassing, het installatieprogramma, de website en de gebruikers- en ontwikkelaarsdocumentatie.

<a id="131--2026-11-03"></a>

## 1.3.1 — 2026-11-03

- Opgelost dat openen via de standaardbestandsbeheerder een andere toepassing dan FileSpacer koos.

<a id="130--2026-11-02"></a>

## 1.3.0 — 2026-11-02

- Gedetailleerde installatiebewerkingen, opdrachtfouten en volledige uitzonderingen bij documentatieopruiming vastgelegd in blijvende diagnostische logboeken.
- Gelijktijdige mapstatustoegang hersteld, wijzigingen tijdens vertraagd herstel behouden en hertekenen bij grote vrije pictogramindelingen verminderd.
- Onbedoelde vensterbewegingen en snelmenu’s na onderbroken venstergebaren verholpen.

- Optionele PowerToys Peek-voorbeelden toegevoegd en conflicterende venstergebaren aan Grab And Move overgelaten.
- Titelbalkmenu aanpasbaar gemaakt in zeven talen, met slepen, standaardwaarden en native of Nilesoft Shell-weergave.
- Acties voor opgeslagen indelingen, aanpassen aan inhoud, onderliggende/andere vensters sluiten, benoemde mapgroepen en stabiele Nilesoft-opdrachten toegevoegd.
- Help-opruimfouten die de-installatie blokkeerden en VM-tests die meerdere Verkenner-processen als aparte sessies zagen verholpen.
- Naden tussen groene pictogramvlakken verholpen en meertalige handleidingen en menudemonstraties vernieuwd.

<a id="123--2026-10-30"></a>

## 1.2.3 — 2026-10-30

- Zijbalklogo van installatie- en verwijderprogramma vloeiender gemaakt met hoogwaardige schaling naar de actuele schermgrootte.
- Geciteerde verhogingsargumenten behouden zodat updates voor alle gebruikers de stille worker starten in plaats van het installatieprogramma opnieuw te openen.
- Standaard de oorspronkelijke installatie bijgewerkt en vensters hersteld met exact het door setup gemelde programma, ook bij persoonlijke en algemene installaties naast elkaar.
- Identiteiten, overlays en mapmeldingen bij gemengd sorteren hersteld. Vrije pictogramposities behouden en native weergaven gebruikt bij ontbrekende of incompatibele provider.
- Uniciteit van Instellingen tussen processen, sneltoetsvalidatie, Everything-paden, toegangstoetsen en regionale vertalingen hersteld.
- Mapsluiting, activeringsklikken en herstel na menuvoorbeelden van Beeld verholpen.
- Vernieuwen van Jump Lists en installatiediagnostiek hersteld. Eigen helpbestanden behouden bij upgrades en de-installatie.
- Meertalige offlinehulp via F1 en Info meegeleverd en vertaalde installatiehandleidingen en Nilesoft-referenties vernieuwd.

<a id="122--2026-10-28"></a>

## 1.2.2 — 2026-10-28

- Gebruik van de aangepaste mapprovider bij vrije pictogramindelingen hersteld. Vrije plaatsing gebruikt de native provider. Vastleggen van een gesorteerde indeling schakelt providers met behoud van pictogramposities, selectie en focus.

<a id="121--2026-10-28"></a>

## 1.2.1 — 2026-10-28

- Tijdelijke algemene pictogrammen bij gemengd sorteren verminderd. Zichtbare pictogrammen worden vóór het tonen in de Windows-cache voorbereid, met begrensd wachten op trage pictogramhandlers.

<a id="120--2026-10-28"></a>

## 1.2.0 — 2026-10-28

- Optioneel gemengd sorteren van bestanden en bestandssysteemmappen toegevoegd onder Weergave → Bestanden, met behoud van native Windows-weergave en mapstatus. Standaard uitgeschakeld.
- Duitse, Italiaanse, Spaanse, Portugese (Portugal) en Griekse interfaces toegevoegd naast Engels en Frans.
- Installatietitel en -ondertitel als grotere DPI-bewuste Windows-tekst met ClearType weergegeven.
- Onlinehandleiding uitgebreid met Engelse en Franse schermafbeeldingen, venstergebaren en Nilesoft-menuvergelijkingen.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

<a id="1110--2026-10-27"></a>

## 1.1.10 — 2026-10-27

- Windows-bevestiging alleen bij installatie/verwijdering voor alle gebruikers gevraagd. Persoonlijke integratie toegepast op de gebruiker die setup startte.
- Volledige versiegeschiedenis in een schuifbaar Nieuw-paneel naast de keuzes op het eerste installatiescherm getoond.
- Reset van componentkeuzes bij upgrades verholpen. Opgeslagen keuzes hersteld en standaardbeheerder bij stille upgrades behouden.
- Op elk installatie-/verwijderingsscherm dezelfde FileSpacer-zijbalk, groot logo en lichtgrijs-wit palet gebruikt. Een apart installatiepictogram combineert native bitmaps met een per formaat geoptimaliseerde downloadpijl en behoudt een dunne gele rand rechts en onder aan de map.
- Afsluitcrashes door vroege initialisatie van threadlokale mapgegevens op late Shell-werkthreads verholpen.
- Mapsnelmenu’s bieden openen in Windows Bestandenverkenner als FileSpacer de standaard is, anders in FileSpacer. Bijgewerkt na standaardwijziging, installatie en opstarten.
- Netwerkserver-/sharesnelkoppelingen die ondanks standaard FileSpacer in Verkenner openden hersteld. Werk de mapsnelmenucomponent bij en kies FileSpacer opnieuw als standaard voor netwerkintegratie.
- Toepassingspictogram onder Info vergroot tot 48 pixels bij 100%, met tekst ernaast.
- Titels van gemaximaliseerde vensters volledig zichtbaar gehouden en werkbalk over de beschikbare breedte uitgerekt.
- Regressietests op een geconfigureerde Hyper-V-client mogelijk gemaakt, met compilatie van toepassing en helpers op de host.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

<a id="119--2026-10-27"></a>

## 1.1.9 — 2026-10-27

- Weergavemodi vooraf getoond bij aanwijzen in Beeld, met geleidelijk herstel bij annuleren en standaard ingeschakelde optie onder Weergave.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

<a id="118--2026-10-27"></a>

## 1.1.8 — 2026-10-27

- Pictogram en README-logo vervangen door een geprojecteerde tesseract met paars-groene kubusrandverhouding 11/20, met behoud van kleine-formaatoptimalisaties.
- Engelse en Franse instellingslabels verduidelijkt en vergrendelingsopties gegroepeerd.
- Optie toegevoegd om het slotje te verbergen zonder de opgeslagen statusvergrendeling te wijzigen.
- Native Windows-thema-achtergronden gebruikt op instellingenpagina’s en sneltoetsbesturingselementen.
- Vensterplaatsing en taakbalkgroepering toegelicht met standaard Windows-knopinfo naast de opties.
- Instellingen aan het aanroepende mapvenster gekoppeld. Sluiten verwerpt openstaande wijzigingen. Bevestigd mapstatusonderhoud sluit instellingen zonder die wijzigingen toe te passen.
- README rond ruimtelijke navigatie en functies georganiseerd, met onderzoeksbronnen, nieuwste-releasekoppeling, aparte bouw-/Nilesoft-gidsen en uitgebreidere documentatie.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

<a id="117--2026-10-26"></a>

## 1.1.7 — 2026-10-26

- Overlap van nieuwe vensters met exacte geometrie en eerst vrije ruimte zoeken beperkt. Algemene optie hernoemd naar ‘Vensteroverlap minimaliseren’.
- Optionele standaardbeheerderkeuze toegevoegd aan Engelse/Franse setup, met uitleg voor later inschakelen of terugdraaien onder Integratie.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

<a id="116--2026-10-26"></a>

## 1.1.6 — 2026-10-26

- Mapvensters die openbleven na verwijdering van een bovenliggende map of verplaatsing naar de Prullenbak verholpen.
- Jump Lists die niet meer beschikbare mappen aanboden verholpen.
- Overlap beperkt bij mappen zonder opgeslagen positie, met negeren van het vervangen bronvenster. Standaard ingeschakeld onder Algemeen > Mapnavigatie.
- Mapstatusonderhoud van Algemeen naar Info verplaatst.
- Nieuwe mapvensters standaard 4:3 gemaakt (580 × 435 bij 96 DPI), met bestaande hoogte en respect voor ingestelde en opgeslagen afmetingen.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

<a id="115--2026-10-26"></a>

## 1.1.5 — 2026-10-26

- Onbedoelde snelkoppeling na Alt + dubbelklik en muisbeweging verholpen. Native Eigenschappen, selectie en focus behouden zonder een ingedrukte knop opnieuw af te spelen.
- Regressiedekking voor native dubbelkliktiming en fysieke muisdoelen gecorrigeerd. Eigenschappen, selectie, focus, anker, geometrie en inhoud vergeleken met gebaren uit/aan, inclusief Ctrl/Shift.
- Echte Everything-testprogramma en bootstrap-INI geïsoleerd zodat de geïnstalleerde administratoroptie zoekopdrachten niet omleidt of verhoging start. Gecontroleerd dat geïnstalleerde instellingen ongewijzigd blijven.
- Bureaublad wakker gehouden en schermbeveiliging tijdelijk opgeschort tijdens UI-tests, met herstel bij opruimen, ook na fouten.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

<a id="114--2026-10-25"></a>

## 1.1.4 — 2026-10-25

Deze versie bouwt voort op 1.1.3, dat al instelbare toetsenbord-/muisgebaren voor verplaatsen en formaat wijzigen biedt.

- Optioneel extern Everything-zoeken toegevoegd met automatische detectie, handmatig programmapad, map/submap- of overalbereik en expliciete startfouten.
- Everything en recente zoekopdrachten automatisch bij betreden van het bestaande zoekveld getoond, zonder extra knop. Native zoeken, direct filteren en lokale herhaling behouden.
- Standaard vijf recente zoekopdrachten per map tussen sessies onthouden, instelbaar 0–20 los van Everything. Bestaande gegevens en voorzichtig bewaarbeleid voor geschiedenis behouden.
- Everything-programmapictogram op passende grootte getoond, met extractie op de achtergrond en cachevernieuwing bij programmawijziging.
- Geïsoleerde opslag-, migratie- en echte Shell/Everything-tests toegevoegd. De Shell-testlezer bestand gemaakt tegen opeenvolgende filterwijzigingen.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

<a id="113--2026-10-25"></a>

## 1.1.3 — 2026-10-25

- Tab Sneltoetsen toegevoegd om gebaren in te stellen of uit te schakelen met wijzigingstoetsen en links/midden/rechts, directe invoervastlegging en conflictcontrole.
- Afhandeling van Eigenschappen via Alt + dubbelklik aan de native Verkenner-weergave overgelaten.
- Volledige labels op ondoorzichtige achtergrond voor één geselecteerd item in pictogramweergave hersteld. Meervoudige selectie blijft native. Voorkeur beschrijft dit bereik.
- Uitgebreide sneltoetsconfiguratie- en echte muisgebarentests toegevoegd, met Franse en Engelse instellingenresources.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

<a id="112--2026-10-25"></a>

## 1.1.2 — 2026-10-25

- Optionele vergrendeling van opgeslagen geometrie/weergave vanuit het titelmenu toegevoegd. Tijdelijke wijzigingen blijven mogelijk en pictogramposities worden normaal opgeslagen.
- Small & Flat-slotje op vergrendelde mappen getoond met native ICO in meerdere resoluties, DPI-schaling en hoogcontrastkleuren.
- Globaal uitschakelen van vergrendeling bevestigd, sloten voor open/gesloten mappen gewist en normale opslag tussen processen hervat.
- Dekking voor vergrendelingsovergangen, opslagfouten, Shell-herstel en native slotweergave uitgebreid.
- Opnieuw openen via Ctrl+Shift+T of Jump List schuift Details/Lijst terug naar de rij die bij sluiten bovenaan stond.
- Crashdiagnostiek blijft lokaal minidump en rapport schrijven. Toeschrijving aan vroegere naamweergave, automatische voorkeurwijziging en herstart verwijderd.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

<a id="111--2026-10-24"></a>

## 1.1.1 — 2026-10-24

- Selectiebehoudende activeringsklik hersteld: eerste klik op lege achtergrond van inactieve map activeert zonder selectie te wissen (optie opnieuw ingeschakeld).
- Crash bij sluiten tijdens Shell-vernieuwing verholpen (Vernieuwen, naamfilter, verbs Vernieuwen/Nieuw).

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

<a id="110--2026-10-24"></a>

## 1.1.0 — 2026-10-24

- Mapweergaven omgebouwd naar Windows Shell-implementaties via publieke COM-API’s in plaats van interne besturingselementen aan te passen.
- Ruimtelijke vensterstatus, selectie/focus, pictogramposities en filters aan productie-SQLite en publieke Shell-gebeurtenissen gekoppeld gehouden.
- Native bestandsopdrachten en FileSpacer Alt-gebaren voor verplaatsen, formaat en Eigenschappen behouden.
- Configuratiescherm in het eigen Windows-venster geopend met behoud van bronmap en selectie.
- Automatische selectie van de bronmap bij omhoog navigeren verwijderd: volgende Ctrl-selecties mogen niet onbedoeld beide mappen openen.
- Drie verschillen geaccepteerd: eigen volledige geselecteerde namen met ondoorzichtige achtergrond, selectiebehoud bij activering en exacte heropende schuifposities ontbreken. Twee inactieve opties uitgeschakeld met behoud van voorkeuren.
- Regressiedekking op de native productieconfiguratie gericht. Verkennende profielvergelijkingen, inbeddingsproeven en migratierapportage verwijderd.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

<a id="108--2026-10-22"></a>

## 1.0.8 — 2026-10-22

- Mapvensters vanuit het bestandsgebied verplaatst met Alt + links slepen en formaat gewijzigd met Alt + rechts, met selectiebehoud.
- Klassieke Shell-weergave ingeschakeld gehouden los van volledige naamweergave.
- Nilesoft Shell-thema op het weergavemodusmenu toegepast.
- Lange afsluitvertragingen door passieve Shell-pictogramcaches vermeden, met behoud van levensduurverwijzingen van expliciet gestarte opdrachten.
- Shell-hosting geïsoleerd en tests voor Alt-gebaren, menuherintreding en Shell/STA-levensduur uitgebreid.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

<a id="107--2026-10-20"></a>

## 1.0.7 — 2026-10-20

- Bron-submap geselecteerd bij openen van bovenliggende mappen, met selectiebehoud in reeds geopende vensters.
- Naamfilter bij indienen, wijzigen of wissen van een zoekopdracht hersteld.
- Native Snelle toegang-pictogram hersteld en toegang-/instellingenknoppen gecentreerd.
- Tijdelijke diagnostiek verwijderd en Benco in vermeldingen gebruikt.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

<a id="106--2026-10-11"></a>

## 1.0.6 — 2026-10-11

- Geopende mappen na een update vanuit de app automatisch hersteld met opgeslagen posities, afmetingen en weergave.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

<a id="105--2026-10-11"></a>

## 1.0.5 — 2026-10-11

- Asynchroon controleren op updates onder Info toegevoegd, met aangepaste installatieknop en statustekst.
- Updates in een apart annuleerbaar voortgangsvenster gedownload terwijl mapvensters bruikbaar blijven.
- Installatiegrootte en SHA-256 gecontroleerd vóór ordelijk sluiten van FileSpacer en starten van setup.
- Downloadmetadata van het installatieprogramma bij elke release gepubliceerd.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

<a id="104--2026-10-11"></a>

## 1.0.4 — 2026-10-11

- Titelbalkopdracht toegevoegd om geopende bovenliggende vensters te sluiten.
- Gesloten mappen heropend via titelmenu, Ctrl+Shift+T of Jump Lists, met herstel van weergave, selectie en schuifpositie.
- Snelle toegang-mappen met native Shell-pictogrammen aan taakbalk-Jump Lists toegevoegd.
- Heropenopdracht alleen getoond als de laatst gesloten map niet al open is, met haar naam en native pictogram.
- Huidig venster uit de vensterwissellijst van het titelmenu weggelaten.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

<a id="103--2026-10-11"></a>

## 1.0.3 — 2026-10-11

- Handmatige en optionele wekelijkse updatecontroles via FileSpacer GitHub Releases hersteld, standaard uitgeschakeld.

- Updatemanifest naast elk release-installatieprogramma gepubliceerd.

- Native Shell-afhandeling van Alt + dubbelklik en Alt+Enter op mappen behouden.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

<a id="102--2026-10-11"></a>

## 1.0.2 — 2026-10-11

- Nilesoft-thema’s op titelbalkmapmenu toegepast onafhankelijk van selectie, met native Shell-opdrachten.
- Aparte Nilesoft-aanpassing van titelbalk- en werkbalkmenu’s mogelijk gemaakt.
- Uitklappijl van de titelbalk gecentreerd.
- Nilesoft-configuratie, thema’s en menufiltering gedocumenteerd.
- Benco in auteursrechtvermeldingen opgenomen.
- Releaseworkflow toegestaan het installatieprogramma van een bestaande release te vervangen.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

<a id="101--2026-10-09"></a>

## 1.0.1 — 2026-10-09

- Native minimaliseren/maximaliseren ingeschakeld en titelknoppen in gemaximaliseerde vensters hersteld.
- Gemaximaliseerde status los van normale geometrie opgeslagen en hersteld, met automatische databasemigratie.
- Native taakbalkanimaties en herstarteigenschappen per map gebruikt.
- Taakbalkgroepering op nieuwe vensters toegepast en dit in Instellingen verduidelijkt.
- Verwijderen van geopende mappen en hun opgeslagen status mogelijk gemaakt.
- Voorkomen dat toepassing en herstelhelper mappen via hun werkmap vergrendelen.
- Absolute mappaden en Debug-testmodus tijdens crashherstel behouden.
- Normale naamlabels direct na deselectie hersteld bij ingeschakelde volledige namen.
- Gecachete Shell-interfaces bij mapsluiting vrijgegeven en meldingsafhandeling gecorrigeerd.
- Handmatige releasepublicatie ondersteund en versie in installatienamen opgenomen.

[Alle wijzigingen](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

<a id="100--2026-10-09"></a>

## 1.0.0 — 2026-10-09

- Eerste FileSpacer-release voor Windows 10 of nieuwer, x64, gebaseerd op ChromaFiler.
- Ruimtelijke navigatie met één venster per map en blijvende positie, afmetingen, weergave en pictogramposities in SQLite.
- Mapverplaatsingen en naamswijzigingen gevolgd. Bestaand venster geactiveerd bij opnieuw openen.
- Bronvenster standaard gesloten bij openen van een map of ouder, instelbaar en eenmalig omkeerbaar met Ctrl.
- Broodkruimel- en bewerkbare padbalk plus naamfilter met optioneel filteren tijdens typen toegevoegd.
- Instelbare werkbalkelementen, onderste statusbalk en taakbalkgroepering geboden.
- Optionele experimentele volledige naamweergave voor geselecteerde items toegevoegd.
- Engelse en Franse interfaces geboden met Engels als terugvaltaal.
- Crashherstel en onderhoudsopdrachten voor reset van voorkeuren of mapstatus toegevoegd.
- Miller-kolommen, ingebouwde teksteditor, voorbeeldvensters en zwevende lade verwijderd.
- MSVC/NMAKE-scripts, NSIS-installatieprogramma, automatische GitHub-releases en projectdocumentatie geleverd.

[Releasegeschiedenis](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
