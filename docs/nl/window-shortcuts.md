<a id="window-gestures"></a>

# Venstergebaren

Configuratie staat in de [gebruikershandleiding](user-guide.md#moving-and-resizing). Begin voor de implementatie in `WindowShortcuts.h`, `ShortcutSettingsPage.cpp`, `Settings.cpp`, `FolderWindow` en `WindowResizeGesture`.

<a id="binding-and-input-rules"></a>

## Toewijzing en invoerregels

Standaard verplaatst Alt + slepen met links en wijzigt Alt + slepen met rechts het formaat. Elke actie kan afzonderlijk uitgeschakeld worden. Twee ingeschakelde acties mogen niet dezelfde toewijzing hebben.

| Wijzigingstoetsen | Links | Midden | Rechts |
| --- | --- | --- | --- |
| Geen, Ctrl, Shift, Ctrl + Shift | Gereserveerd | Toegestaan | Alleen achtergrond |
| Alt, Ctrl + Alt, Alt + Shift, Ctrl + Alt + Shift | Toegestaan | Toegestaan | Toegestaan |

Zonder Alt start een toewijzing met de rechterknop alleen op de achtergrond van de weergave; slepen vervangt daar de native selectie door slepen met rechts. Loslaten zonder slepen gaat als klik terug naar de weergave: de Shell heft zelf de selectie op en opent het achtergrondmenu. Op een item blijft het indrukken native: contextmenu en slepen met rechts. De instellingenpagina vermeldt dit wanneer zo’n toewijzing wordt gekozen.

Vergelijk wijzigingstoetsen exact bij het indrukken van de muisknop. Loslaten tijdens het gebaar verandert de actie niet. AltGr is de door Windows gemelde combinatie Ctrl + Alt; midden betekent het wiel indrukken, niet scrollen.

Andere gebaren werken op achtergrond en items zonder selectie te veranderen. Editors, naamwijzigingsvelden, kopteksten, schuifbalken, werkbalken en menu’s behouden native invoer. Escape, verlies van muisvastlegging en annulering breken het gebaar af. Verplaatsen met links gebruikt de Windows-verplaatslus; ander verplaatsen en alle formaatwijzigingen gebruiken de gedeelde vastleggingsengine met native limieten en geometrieopslag.

Dubbelklikken en Eigenschappen blijven van de Shell. Heeft een gebaar de eerste klik van een latere native dubbelklik verbruikt, lever die uitgestelde klik dan eerst aan de weergave. Vervang de selectie of Eigenschappen van Verkenner niet door handmatige aanroepen.

<a id="preferences-and-powertoys-precedence"></a>

## Voorkeuren en voorrang van PowerToys

De instellingenpagina bewaart concepten tot Toepassen/OK en weigert gereserveerde of conflicterende toewijzingen. Standaardwaarden per actie wijzigen één concept; Herstellen herstelt het laatst toegepaste paar. Beide worden atomair vastgelegd als de versiegebonden DWORD `WindowShortcuts`. Ongeldige opgeslagen gegevens vallen terug op standaardwaarden. Vensters lezen het huidige paar bij aanvang van een gebaar, ook tussen processen.

FileSpacer onderbreekt zijn eigen gebaren nooit voor PowerToys Grab And Move. Dat hulpmiddel onderschept zijn combinaties van wijzigingstoets en muisknop in een low-level hook voordat de toepassing ze ontvangt: met dezelfde toewijzing handelt het alleen, met verschillende toewijzingen blijven beide actief. De pagina Sneltoetsen toont alleen een melding zolang de opgeslagen PowerToys-instellingen Grab And Move inschakelen en FileSpacer niet uitsluiten (`powerToysOverridesWindowGestures()` in `Settings.cpp`); die instellingen worden gelezen bij het tonen of activeren van de pagina, nooit bij muisinvoer. Grab And Move negeert geïnjecteerde invoer; geautomatiseerde tests kunnen het dus niet aansturen.

<a id="tests"></a>

## Tests

```cmd
tests.cmd -Pattern "24-*"
```

Groep 24 scheidt pagina-/registervalidatie, geometrie en annulering van de vastleggingsengine, echte Shell-gebaren tussen processen en vergelijking van native Eigenschappen. De matrix controleert geldige/conflicterende paren en beschadigde waarden; HWND-tests vervangen geen Shell-invoeracceptatie. Groepen 15–17 dekken verdere interactiegrenzen. Gebruik de geïsoleerde voorkeuren/database van de runner en behoud echte resultaten en afsluitcodes.
