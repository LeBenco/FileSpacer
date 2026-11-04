<a id="regression-tests"></a>

# Regressietests

De referentielauncher is `tests.cmd` vanuit CMD in de hoofdmap. Hij bouwt Debug met MSVC/NMAKE en selecteert onafhankelijke tests uit deze checkout. Bronnen, programma’s en helpers moeten van dezelfde revisie komen. Afhankelijkheden: [bouwhandleiding](../../build.md).

<a id="run-locally"></a>

## Lokaal uitvoeren

Sluit FileSpacer. UI-tests vereisen een ontgrendeld interactief Windows-x64-bureaublad, PowerShell 5.1 en tijdelijke NTFS-map. Laat muis en toetsenbord vrij. Sluit Configuratiescherm vóór zijn externe-venstertest. Internal-tests vereisen geen fysieke invoer, maar gebruiken soms echte Windows-/COM-objecten.

```cmd
tests.cmd -Category Internal
tests.cmd -Pattern "12-navigation-spatiale\*"
tests.cmd -Category UI -Pattern "16-redimensionnement-alt\03-*"
tests.cmd
```

Voer nieuwe en betrokken tests uit tijdens ontwikkeling. `tests.cmd` zonder filters draait de hele toepassingssuite; GitHub ondersteunt dit ook. Herhaal geen native test die al in de gekozen suite zit.

| Optie | Betekenis |
| --- | --- |
| `-Category Internal\|UI\|All` | Filter op gedeclareerde categorie; standaard `All`. |
| `-Pattern "group\case-pattern"` | PowerShell-jokerteken voor relatief pad; standaard `*`. Combineer met `-Category`. |
| `-Scope Application\|All` | Beide kiezen de toepassingssuite; standaard `All`. |
| `-TimeoutSeconds N` | Deadline per test 30–3600 seconden; standaard 300. Time-out is falen. |
| `-RunDirectory "path"` | Rapporten opslaan/kopiëren naar nieuwe map; nooit overschrijven. |
| `-VM "name"` | Gekozen tests in geconfigureerde Hyper-V-client. |

Roep voor selectie **zonder bouwen of testen** de PowerShell-runner direct aan. `tests.cmd` bouwt vóór het doorgeven van argumenten, ook `-ListOnly`.

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\regression\Run-Regression.ps1 -Category UI -Pattern "30-tri-melange\*" -ListOnly
```

JSON noemt paden, categorieën en bereiken; gebruik dit in plaats van een vast testaantal. Sommige compatibiliteitstests halen fixtures uit Git-tags; houd de benodigde geschiedenis beschikbaar.

De UI-runner houdt systeem/scherm tijdelijk wakker en schort de schermbeveiliging op, met herstel ook na fouten. Hij wijzigt geen energieplan en ontgrendelt Windows niet. `desktop-activity.json` registreert herstel.

<a id="run-in-hyper-v"></a>

## Uitvoeren in Hyper-V

```cmd
tests.cmd -VM AgentWin10 -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

De host bouwt toepassing en gekozen native/C#-helpers vóór overdracht. De client controleert hashes en draait dezelfde scripts/asserties, zonder gastcompilatie of terugval naar het hostbureaublad. Selectie gaat vooraf aan verpakking. Zie [Hyper-V](../client/README.md) voor configuratie, bureaublad en diagnose.

<a id="run-on-github"></a>

## Uitvoeren op GitHub

GitHub Actions gebruikt de [regressieworkflow](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/regression.yml) voor dezelfde toepassingstests. Deze kan op verzoek worden gestart; gewone branchpushes en pull requests activeren hem niet.

Kies onder **Actions > Test FileSpacer regressions > Run workflow** `ref` (branch/tag/commit), `category` en `pattern`. `All` / `*` draait alles. Handmatige start vereist de workflow op de standaardbranch. De gekozen revisie moet workflow en runner bevatten; tests komen niet uit een andere checkout.

De workflow bepaalt de revisie eenmaal, bouwt Debug op `windows-2022` en draait voorbereide tests op het interactieve bureaublad. `-CI` werkt alleen op GitHub-hosted runners, niet met `-VM`/`-Prepared`. Dezelfde interactieve gebruiker krijgt een beperkt medium-integriteitstoken; voorbereiding controleert `Shell.Windows()` en token. COM-registratie is per gebruiker. Dit vervangt geen Windows 10- of andere vereiste configuratietests.

De workflow publiceert geen installatieprogramma of offlinehulp. De [releaseworkflow](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/release.yml) hergebruikt hem als eerste taak binnen dezelfde releaserun en bouwt vervolgens documentatie en NSIS uit de geteste SHA. Geslaagde verpakking is geen echte installatie- of upgradetest.

Actions toont SHA, totalen per categorie, afsluitcodes en eerste fouten. Logboeken, JSON, beelden en `regression-summary.md` blijven 14 dagen in `regression-<run>-<attempt>`, ook na fouten. Worker-/opruimfouten blijven ongeldig maken ondanks geslaagde cases.

<a id="find-the-relevant-cases"></a>

## Relevante tests vinden

Mappen onder `cases/` groeperen gedrag. Namen en paden zijn stabiele selectoren; vertalen is niet nodig.

| Gebied | Groepen |
| --- | --- |
| Harnas en voorbereide artefacten | `00-banc` |
| SQLite, bewaring, quota, weergave en geschiedenis | `01`–`02`, `06`–`09` |
| Menu/levensduur, STA en afmetingen | `03`–`05` |
| Updatemanifesten en sessieherstel | `10`–`11` |
| Ruimtelijke navigatie, voorkeuren en indelingen | `12`–`14` |
| Gebaren en sluiten tijdens interactie | `15`–`17`, `24-raccourcis` |
| Native opslag en Shell-opdrachten | `18-vue-shell-native`, `19-commandes-shell` |
| Belasting, meldingen en Configuratiescherm | `20-validation-elargie` |
| Activering, vergrendeling en namen | `21`–`23` |
| Zoeken, geschiedenis en Everything | `25-recherche-integration` |
| Plaatsing en Beeld-voorbeelden | `26-placement-fenetres`, `28-apercu-affichage` |
| Standaardbeheerkoppelingen | `29-menu-gestionnaire` |
| Lokale hulp en taalkeuze | `30-documentation`, `31-langues` |
| Gemengd sorteren en identiteiten | `30-tri-melange` |
| Titelmenu en Peek | `33-menu-titre`, `35-apercu-peek` |

Dit zijn dekkingsingangen, geen uitvoeringsverslag. Inspecteer voorbereiding en asserties vóór u beslist dat een wijziging gedekt is.

<a id="native-view-persistence"></a>

### Opslag van native weergavestatus

`18-vue-shell-native/02-Persistance.ps1` bouwt productiehost, instellingen, selectie, pictogrammen, filter en SQLite tegen een echte Shell-weergave. Controleert herstel, vernieuwen, filteren, serialisatie en vernietigen/hermaken. Gereedheid volgt publieke navigatie-/opsommingssignalen; een watchdog mag falen, nooit gereed verklaren.

Afzonderlijke diagnose van dit harnas:

```cmd
test-native-state.cmd --diagnostic
```

Initialiseert MSVC x64 en gebruikt `/Z7` zonder gedeelde PDB. Dit is een alternatieve aanroep, geen extra volledige controle.

<a id="external-applications"></a>

### Externe toepassingen

De echte Everything-test in groep 25 vereist een geïnstalleerd detecteerbaar exemplaar zonder open interface in de testsessie. De sessie-0-service mag blijven draaien. `FILESPACER_EVERYTHING_EXE` kiest een installatie, maar Detecteren moet die ook vinden. Het pad staat als UTF-16 in `everything-detected.txt`.

De test start een privékopie van het echte programma met eigen INI, fixtures en index. Everything leest `run_as_admin` naast het programma ook met `-config`; isoleer die bootstrap. Geïnstalleerde INI/verhogingskeuze blijven intact. Asserties lezen zichtbare zoekopdrachten en resultaatidentiteiten voor beide bereiken, niet alleen procesaanmaak. `FILESPACER_EXPECT_EVERYTHING_ABSENT=1` activeert de afwezigheidscontrole in case 02.

Peek-start/protocoltests en native menutests bewijzen geen echte Peek-/Nilesoft-weergave. Inspecteer fixtures en gebruik de externe toepassing voor visuele acceptatie. Ontbrekende vereisten valideren geen integratie.

<a id="mixed-sorting"></a>

### Gemengd sorteren

Groep `30-tri-melange` controleert volgorde, voorkeurwijzigingen, meldingen, opgeslagen identiteiten en providercontract. Hergebruikt gedeelde Shell-/navigatiescenario’s met provider aan. Houd gebaren/asserties in één implementatie. Filteren, vrije posities, hernoemen en verwijderen van de getoonde map zijn belangrijke grenzen; alleen volgorde dekt ze niet.

<a id="add-or-change-a-case"></a>

## Een test toevoegen of wijzigen

Plaats tests onder `cases/NN-topic/NN-Name.ps1` met exact `# Test-Category: Internal` of `# Test-Category: UI` op de eerste regel. Gebruik Windows PowerShell 5.1 en C# 5 / .NET Framework 4.8. Parseer scripts en compileer helpers vóór UI-uitvoering. Ken automatische PowerShell-variabelen niet toe.

Gebruik `common/` voor processen, native bouwen, fysieke invoer en resultaten. Houd afhankelijkheden expliciet: VM-voorbereiding inspecteert scripts, helperpaden en exacte argumentcombinaties. Nieuwe dynamische paden of bouwopdrachten kunnen aanpassing vereisen. Controleer directe én voorbereide uitvoering.

Observeer echte uitkomsten: identiteit, effectieve instelling, geometrie, selectie en focus. Alleen een aantal volstaat niet. Herhaal geen mislukte actie, forceer achteraf geen focus en wijzig geen assertie voor PASS. Geïnjecteerde fouten vullen echte Shell-tests aan en vervangen ze niet.

<a id="reports-isolation-and-diagnosis"></a>

## Rapporten, isolatie en diagnose

Lokale rapporten staan onder `build/regression/<date>-<id>/`. Bewaar `results.json`, `summary.json`, stdout/stderr, applicatieafsluitingen en bewijs samen. VM voegt deployment-/sessie-/herstelbewijs toe; zie [rapportindeling](../client/README.md#reports-and-diagnosis).

PASS geldt alleen voor voltooide tests. FAIL, BLOCKED, NOT_RUN, ontbrekende samenvatting, time-out, crash of opruimfout verhinderen volledig succes. Bewaar eerste fout en opruimfouten apart. Rapporteer bouw, functionele tests en statische analyse afzonderlijk.

De runner bewaart en herstelt `/test`-register en test-SQLite. Reset nooit productiegegevens. Tussen cases verwijdert hij alleen test-Jump List-identiteiten onder `Software\FileSpacer\Test\JumpLists`; binnen één case blijven ze bestaan. Anders kan `CommitList` / `FlushFileBuffers` werk tussen fixtures opstapelen. `case-fixture.json` registreert opruimen. Identificeer en sluit externe vensters afzonderlijk; beëindig nooit Verkenner voor Configuratiescherm-opruiming.

Aandachtspunten bij UI-diagnose:

- UI Automation toont Details-items als `DataItem` in `DataGrid`.
- Houd Ctrl fysiek ingedrukt tot het resultaat van Ctrl-slepen zichtbaar is.
- Controleer werkelijk doel en coördinaten; een bedoelde klik bewijst geen aflevering. Gebruik gedeelde helpers bij ontkoppelde VM-consoles.
- Eindig een uitgesloten formaatgebaar in de naameditor; rechts loslaten erbuiten kan terecht een menu openen.
- Dubbelklikken kan de bron sluiten; stel het bedoelde vensterbeleid in de fixture in.
- Houd paden kort bij Win32-fout 206. Native harnassen gebruiken `/Z7` zonder gedeelde PDB.

`set FS_SEARCH_TIMINGS=1` registreert echte leestijden in `search-observations.jsonl`; 20/01 schrijft ook `load-phases.jsonl`. Logboeken veranderen geen deadlines/asserties. Gelijktijdige toegankelijkheidsinspectie beïnvloedt timing sterk. Vergelijk met dezelfde observer, fixtures en omgeving; tijden zijn geen universele grenzen. Het harnas stopt externe inspectietools niet.
