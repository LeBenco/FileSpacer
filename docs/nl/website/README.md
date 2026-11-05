<a id="website-generation"></a>

# Website genereren

De site voegt `README.md`, handleiding, wijzigingen en derdenvermeldingen samen. Privacy en lokale gegevens hebben eigen secties. Het korte beleid blijft op GitHub; oude siteankers bereiken de handleiding. Engelse originelen zijn de bron voor site en repository. Documenten staan rechtstreeks in `docs/<language>/`; elke taal heeft eigen afbeeldingen in de submap `images/`. Engelse documenten staan in `docs/en/`; de Engelse README, wijzigingen en derdenvermeldingen in de hoofdmap blijven de bronnen voor de Engelse site. Bouw, tests en technische notities blijven op GitHub; externe documentlinks gaan naar `main`. Nilesoft heeft per taal een referentie; technische documenten bestaan ook in Pools, Nederlands en Oekraïens.

De generator ontdekt taalmappen met `locale.json`, vier bronnen, privacybeleid en Nilesoft-referentie. Hij maakt de taalkiezer boven navigatie met eigen taalnamen en Unicode-vlaggen. Het meegeleverde OFL-vlaglettertype Noto Color Emoji werkt ook op Windows zonder externe dienst.

`assets/favicon.ico` bevat het oorspronkelijke 16 × 16-beeld uit `src/res/FileSpacer.ico` zonder herschaling. Houd het bij regeneratie gelijk. `favicon_ico` gebruikt dit in elke taal.

Kopvolgorde en -hiërarchie moeten Engels volgen. De generator normaliseert niveaus, hergebruikt Engelse document-ID’s en herschrijft links. Taalwisselen behoudt de sectie. Ontbrekende documenten, koppen of lokale PNG/GIF’s doen de bouw falen. Het neutrale logo is gedeeld. Zie [vertaalonderhoud](translations/README.md).

Elke taal krijgt een eigen Just the Docs-bouw, navigatie, zoekindex, labels en media met hetzelfde vastgezette thema. De kleine layoutoverride vertaalt de skiplink en versioneert het themascript voor zoekupdates. Behoud MIT-vermelding en controleer bij thema-updates. De repository-README heeft geen taalkiezer of taallinks.

De kop linkt alleen naar de repository. Downloads en badges komen uit Markdown; de generator voegt geen tweede knop toe en leidt geen download uit wijzigingen af.

PNG/GIF’s zijn kleine gekoppelde miniaturen. Een lokaal script opent een native HTML-dialoog met vertaalde sluitknop, Escape, achtergrondsluiting en focusherstel. Zonder JavaScript/dialogen opent de link de afbeelding. Het thema heeft geen lightbox; geen extra bibliotheek of CDN. SVG-logo’s zijn geen screenshots. Inhoudshashes in URL’s vernieuwen miniatuur en vergroting na vervanging.

`lunr/custom-index.js` gebruikt Unicode-woordafkapping voor vertalingen zonder Engelse stemming/stopwoorden. Standaard ASCII zou Griekse woorden verliezen. Engels behoudt de oorspronkelijke pipeline. De testafhankelijkheid volgt Lunr 2.3.9 van het thema.

<a id="local-checks"></a>

## Lokale controles

Met Node.js 22 of nieuwer vanuit de hoofdmap:

```cmd
npm ci --prefix website --ignore-scripts
npm test --prefix website
npm run generate --prefix website
```

`build/site-source` is Jekyll-invoer, geen afgewerkte site. Engels staat aan de wortel, andere talen onder `_locales/<language-region>`. `locales.json` beschrijft builds/paden. Bouw elk met Jekyll en voeg samen volgens `path`: Engels aan de wortel, andere onder `/de/`, `/el/`, `/es/`, `/fr/`, `/it/`, `/nl/`, `/pl/`, `/pt/`, `/uk/`. Uitvoer is gegenereerd en door Git genegeerd. De applicatiebouw staat apart.

<a id="shared-website-and-offline-builds"></a>

## Gedeelde website- en offlinebouw

`.github/scripts/build-docs.mjs` beheert de hele pipeline: generator, Jekyll voor alle talen en behoud van vastgezet thema, navigatie, zoekindex en media. Bouw-Ruby is vastgelegd in `.github/scripts/Gemfile.lock`. Genereren vereist Node.js 22+, Ruby 3.3 en Bundler; lezen niet.

Installeer afhankelijkheden eenmaal vanuit CMD in de hoofdmap:

```cmd
npm ci --prefix website --ignore-scripts
cd .github\scripts
bundle install
cd ..\..
node .github\scripts\build-docs.mjs --target web
node .github\scripts\build-docs.mjs --target offline
```

Uitvoer is `build/site` en `build/offline-docs`. Offline maakt links/resources relatief en sluit zoekdata in het themascript in zodat `file://` zonder server werkt. Indexering/tokenisatie blijven gelijk. Een gewijzigde themalader doet generatie falen tot de offlineaanpassing is beoordeeld.

Beide workflows gebruiken `.github/actions/build-docs` voor afhankelijkheden en doel. Releases checken dezelfde tag voor app en documentatie uit; ze downloaden geen gepubliceerde site en wachten niet op Pages.

Offline bevat bronvingerafdruk, bestandshashes en talen. Verpakken controleert die vóór hergebruik. Teksthashes normaliseren CRLF/LF voor Linux-Windows-overdracht. Ontbrekende, verouderde of beschadigde uitvoer wordt herbouwd. Een gegenereerde NSIS-include installeert documentatie en verwijdert alleen eigen bestanden; andere helpbestanden blijven.

## GitHub Pages

Pages bouwt en controleert relevante PR’s, genereert bronnen eenmaal, leest `locales.json`, bouwt elke taal en deployt. Geen tweede taallijst in CI. Pushes naar `main` herbouwen bij wijzigingen aan README’s, `BUILD.md`, `docs/`, vertalingen, iconen, generator, gedeelde actie of Pages-workflow. PR’s gebruiken dezelfde filters en bouwen alleen. Handmatige start is mogelijk.

Pages start ook na een geslaagde release via `workflow_run`. Vóór het bouwen van de standaardbranch bij een push, handmatige start of geslaagde release werkt dezelfde workflow alleen de `href` van `last-release-url` in alle README’s bij. De HTML-parser behoudt vertalingen en opmaak en werkt gesynchroniseerde README-referenties bij. Hij commit echte wijzigingen en bouwt daarna de bijgewerkte branch. PR’s bouwen alleen en schrijven niets. Een aparte linkworkflow of recursieve dispatch is niet nodig.

Pages en release delen `build-docs` met doelen `web` en `offline`. Tags starten release; offlinehulp wacht op tests en gebruikt hun commit. Documentatiepushes maken geen applicatierelease; beide in één commit geven nog steeds één Pages-run per branchpush.

Engelse updates wachten niet op vertalingen. De generator bewaart tekst en vastgelegde ID’s en meldt veroudering met annotaties en een tabel. Zie de [vertaalgids](translations/README.md) voor bronregistratie.

Kies na push van de workflow **Settings > Pages > Source > GitHub Actions**. Het adres is `https://lebenco.github.io/FileSpacer/`; `_config.yml` stelt het projectpad voor assets en zoeken in.

**About > Website** kan het adres tonen. Andere opgenomen documenten vereisen wijziging van `sources` in `generate.mjs`. Pages-filters dekken alle `docs/` en README’s; voeg een filter toe voor nieuwe invoer elders.
