<a id="winget-distribution"></a>

# WinGet-distributie

GitHub Actions dient `Benco.FileSpacer` in bij [`microsoft/winget-pkgs`](https://github.com/microsoft/winget-pkgs) nadat **Publish FileSpacer** slaagt (`workflow_run`) en WinGet expliciet is geselecteerd met `release.sh X.Y.Z --distribute winget`. Standaard vindt geen automatische distributie plaats. Herhaal `--distribute` om meerdere kanalen te kiezen; de keuze staat in de tagannotatie en het artefact `release-publication`. Ontbrekende selectie betekent ook geen automatische inzending. Handmatig starten voor een bestaande gepubliceerde stabiele tag selecteert WinGet wel, ook als de tag oorspronkelijk `none` koos. Distributie heeft een apart resultaat; falen verandert de geslaagde release niet.

<a id="one-time-setup"></a>

## Eenmalige configuratie

1. Maak een GitHub **personal access token (classic)** met **public_repo** voor het account dat inzendingen en de fork van `microsoft/winget-pkgs` beheert. WingetCreate ondersteunt geen fine-grained PAT’s. Zie [Microsofts instructies](https://github.com/microsoft/winget-create/blob/main/doc/token.md).
2. Voeg onder **Settings > Secrets and variables > Actions** van FileSpacer het repositorygeheim **WINGET_TOKEN** toe. Zet het nooit in workflow, commit of log. Vernieuw bij verloop/intrekking.
3. Commit en push distributietools naar de standaardbranch vóór de volgende tag. GitHub laadt `workflow_run` daarvandaan. Dit werkt na publicatie met `GITHUB_TOKEN`, die zelf geen `release`-event voor een andere workflow zou starten.

Gewoon `GITHUB_TOKEN` volstaat voor FileSpacer-publicatie. De extra PAT dient alleen WingetCreate voor de externe communityrepository. Handmatige eerste-pakketaanmaak is niet nodig: manifesten ondersteunen eerste en volgende versies.

<a id="each-release"></a>

## Elke release

Bij een eerste bijdrage kan Microsoft het indienende account vragen de [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/) te accepteren. De accounteigenaar moet deze lezen en, bij akkoord, antwoorden zoals de Microsoft-beleidsbot in de indienings-PR vraagt. Een geslaagde WinGet-workflow bevestigt de indiening; publicatie in de catalogus wacht op acceptatie van de CLA en de beoordeling door Microsoft.

Volg de [releaseprocedure](build.md#releases). Na succes haalt WinGet `release-publication` uit exact die run. Dit artefact bewaart de tag ook voor handmatige releases. Daarna:

1. Controleert stabiel `vX.Y.Z` en slaat bestaande versies of open inzendings-PR’s over.
2. Downloadt de versiegebonden publieke installer en `FileSpacer-update.txt`, vergelijkt grootte/SHA-256 met manifest en beschikbare GitHub-digest. Bouwt geen tweede installer.
3. Genereert en bewaart YAML voor versie, Engelse metadata en installer. Omvat Windows x64, `/CurrentUser`, `/AllUsers`, stil `/S` en NSIS’ laatste argument `/D=<INSTALLPATH>`.
4. Installeert .NET 9, downloadt WingetCreate 1.12.13.0, controleert vastgezette SHA-256 en Microsoft Authenticode en valideert/dient in met `wingetcreate submit`. De PAT komt via de gedocumenteerde variabele `WINGET_CREATE_GITHUB_TOKEN`.

De installer behoudt componentstandaarden en opgeslagen upgradekeuzes. Het manifest vraagt geen andere standaardbeheerder. Inzenden maakt niet direct beschikbaar: Microsoft-controles en review moeten de PR accepteren. Volg de PR-link in het log en reageer op verzoeken.

Inzendfalen behoudt GitHub-release en geslaagd releaseresultaat; alleen distributie faalt. Herstel de oorzaak en kies **Actions > Distribute FileSpacer (channel=winget) > Run workflow**, standaardbranch en gepubliceerde tag. Dit herhaalt alleen distributie zonder bouwen/publiceren. Een ontbrekend geheim faalt expliciet na bewaren van manifesten. Handmatig ondersteunt ook oudere en extern gepubliceerde releases. Mislukte/geannuleerde releases dienen niets in. Geaccepteerde versies worden overgeslagen en niet vervangen; houd publieke versie-assets ongewijzigd.

Na acceptatie van de eerste PR en catalogusvernieuwing:

```powershell
winget install --id Benco.FileSpacer --exact --scope user
winget install --id Benco.FileSpacer --exact --scope machine
winget upgrade --id Benco.FileSpacer --exact
winget uninstall --id Benco.FileSpacer --exact
```

<a id="validation"></a>

## Validatie

Distributietests vanuit CMD:

```cmd
node --test .github/scripts/winget.test.mjs
```

Ze dekken manifestgeneratie, releasegeschiktheid, integriteit, beide installatiebereiken en herhalen met gesimuleerd HTTP. Geen NSIS of echte Shell. De inzendopdracht valideert het schema op GitHub Windows; echte installatie, upgrade, verwijdering en acceptatie worden ook upstream beoordeeld. Controleer beide bereiken vóór eerste inzending in een geïsoleerde VM met de echte release-installer. Manifesten blijven als artefact beschikbaar bij inzendfalen.
