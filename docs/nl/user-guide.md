# FileSpacer gebruiken

## Navigeren tussen vensters

FileSpacer biedt een soepele, sobere en aanpasbare interface op basis van het ruimtelijke model:

- **Eén map, één venster.** Een al geopende map opnieuw openen brengt haar venster naar voren. Positie, afmetingen, weergave en pictogramindeling blijven bewaard, ook na het verplaatsen of hernoemen van de map.
- Een map of bovenliggende map openen sluit standaard het bronvenster. Houd Ctrl ingedrukt bij het openen van een map om het bronvenster open te houden. De standaard is instelbaar onder **Instellingen > Algemeen**. Meerdere geselecteerde mappen openen houdt het bronvenster open.
- Het menu naast de maptitel schakelt tussen geopende mappen. **Bovenliggende vensters sluiten** sluit de geopende bovenliggende mapvensters.
- **Ctrl+Shift+T** opent de laatst gesloten map met haar indeling en selectie. Dezelfde opdracht staat in het titelmenu. Via de Jump List op de taakbalk kunt u deze map of eerder gesloten mappen opnieuw openen.
- De Jump List op de taakbalk opent ook mappen die zijn vastgemaakt aan Snelle toegang in Bestandenverkenner.

Naar een bovenliggende map gaan selecteert de verlaten map niet. Een al geopende bovenliggende map behoudt haar selectie.

![Het maptitelmenu met opdrachten voor navigatie en opgeslagen indelingen.](images/window-menu-nl.png)

## Weergave en indeling

### Pictogramindeling

In pictogramweergaven kunnen bestanden en submappen vrij worden geplaatst wanneer **Pictogrammen automatisch schikken** uitstaat. Pictogramposities worden onafhankelijk van de venstergeometrie opgeslagen, ook als de opgeslagen vensterindeling is vergrendeld.

### Weergaveopties

**Volledige naam van geselecteerd bestand tonen**, onder **Instellingen > Weergave**, maakt de naam van één geselecteerd bestand volledig leesbaar in de pictogramweergave.

![Een lange geselecteerde bestandsnaam volledig zichtbaar in de pictogramweergave.](images/selected-name-nl.png)

**Weergavemodi vooraf bekijken in het menu Beeld** toont elke modus wanneer de aanwijzer erboven staat. Klikken bevestigt de modus. Escape of het verlaten van het voorbeeld herstelt de oorspronkelijke weergave. Het voorbeeld is tijdelijk en vervangt de opgeslagen indeling niet.

**Mappen en bestanden samen sorteren** sorteert beide samen, zoals in [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) en [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). De optie staat onder **Instellingen > Weergave > Bestanden** en geldt voor Details, Lijst en automatisch gerangschikte pictogrammen. Virtuele mappen en archieven behouden de Windows-volgorde.

![Bestanden en mappen in dezelfde alfabetische volgorde.](images/mixed-sorting-nl.png)

### Vensterplaatsing

Mappen zonder opgeslagen positie openen in vrije ruimte, of met minder overlap als de ruimte opraakt. Dit is instelbaar onder **Instellingen > Algemeen > Nieuwe vensters in vrije ruimte plaatsen**.

### Selectie en activering

**Selectie behouden bij klikken op een inactief venster**, onder **Instellingen > Algemeen**, laat een klik op de achtergrond een map activeren zonder de selectie te wissen. Zodra het venster actief is, kunt u op de achtergrond klikken en slepen om een selectierechthoek te tekenen.

## Vensters beheren

### Verplaatsen en formaat wijzigen

U kunt een venster verplaatsen of het formaat wijzigen door ergens in het bestandsgebied te slepen, ook boven een item. De standaardgebaren zijn:

| Actie | Sneltoets |
| --- | --- |
| Venster verplaatsen | Alt + slepen met links |
| Formaat wijzigen vanaf de rechteronderhoek | Alt + slepen met rechts |

![Een venster verplaatsen met Alt en slepen met de linkermuisknop.](images/move-window-nl.gif)

![Het vensterformaat wijzigen met Alt en slepen met de rechtermuisknop.](images/resize-window-nl.gif)

De selectie blijft intact. Escape annuleert het gebaar.

Elke actie heeft een eigen instelbare sneltoets onder **Instellingen > Sneltoetsen**. De linker- en rechtermuisknop vereisen Alt. De middelste knop kan ook zonder.

### Een opgeslagen indeling vergrendelen

Vergrendelen bewaart de positie, afmetingen en weergave-instellingen als de indeling die bij opnieuw openen wordt hersteld. De functie staat standaard uit onder **Instellingen > Algemeen > Vensterstatusvergrendeling inschakelen**. Na inschakeling is **Vensterstatus vergrendelen** beschikbaar naast de maptitel. Een slotje op het mappictogram markeert een vergrendelde indeling.

<img src="images/locked-title-nl.png" alt="Detail van de maptitel met het slotje." width="280">

Tijdelijke wijzigingen blijven mogelijk. Opnieuw openen herstelt de vergrendelde indeling. Individuele pictogramposities worden normaal opgeslagen. **Vensterstatus ontgrendelen** behoudt de huidige indeling.

De functie uitschakelen vraagt om bevestiging en verwijdert alle vergrendelingen, ook voor gesloten mappen. Opnieuw inschakelen herstelt ze niet.

### Het titelbalkmenu aanpassen

Het menu is aanpasbaar onder **Instellingen > Weergave > Interface-elementen > Titelbalkmenu aanpassen**. Sleep een actie van rechts naar het menu, verplaats een item om de volgorde te wijzigen of sleep het eruit om het te verwijderen.

Items die door Nilesoft Shell worden beheerd, kunnen hier niet worden gewijzigd omdat zijn regels voorrang hebben. Zie [Nilesoft Shell-integratie](#nilesoft-shell).

![Een actie aan het titelbalkmenu toevoegen en een andere verwijderen.](images/customize-title-menu-nl.gif)

![Het aangepaste titelbalkmenu met Vergrendelde indeling herstellen.](images/customized-title-menu-nl.png)

- **Vergrendelde indeling herstellen** zet het venster terug naar de referentie-indeling. **Referentie-indeling vervangen** slaat na bevestiging de huidige indeling op als nieuwe vergrendelde referentie.

- **Indeling van deze map vergeten** wist de opgeslagen indeling en vergrendeling, maar behoudt de zoekgeschiedenis. Opslaan blijft in het huidige venster onderbroken totdat het sluit. Bij opnieuw openen gelden de standaardinstellingen. Een nieuwe indeling vergrendelen hervat het opslaan.

### Mapgroepen

**Mapgroep opslaan / oproepen** bewaart de geopende mappen onder een naam. De groep oproepen opent ze opnieuw met hun opgeslagen indelingen.

## Zoeken en geschiedenis

Voer een volledige of gedeeltelijke bestands- of mapnaam in en druk op Enter om de huidige map te filteren. **Filteren tijdens het typen**, onder **Instellingen > Algemeen**, past het filter tijdens het typen toe.

Het veld biedt recente zoekopdrachten en, als de integratie is ingeschakeld, **Zoeken in Everything** voor een externe zoekopdracht. Enter zonder geselecteerde suggestie past het lokale filter toe. Ctrl+F activeert het veld.

Elke map heeft een eigen zoekgeschiedenis. Het aantal bewaarde bevestigde zoekopdrachten is instelbaar onder Algemeen. 0 wist en schakelt de geschiedenis uit. [Everything](#everything) kan submappen of alle geïndexeerde bestanden doorzoeken.

![Het zoekveld met Everything en suggesties uit recente zoekopdrachten.](images/search-nl.png)

## Integratie

### Windows-integratie

FileSpacer kan tijdens installatie of later onder **Instellingen > Integratie** met **Instellen als standaardbestandsbeheerder** worden ingesteld. **Standaardbestandsbeheerder herstellen** laat mappen weer in Bestandenverkenner openen.

Het mapsnelmenu biedt **Openen in FileSpacer**, of **Openen in Windows Bestandenverkenner** wanneer FileSpacer de standaard is. Dit werkt ook op stations, ZIP-mappen, netwerklocaties en mapachtergronden, zonder de standaardtoepassing te hoeven wijzigen.

**Opties voor Bestandenverkenner** opent gedeelde mapvoorkeuren, waarvan sommige ook voor FileSpacer gelden.

### PowerToys Grab And Move

Wanneer Grab And Move is ingeschakeld in PowerToys, worden de verplaats- en formaatwijzigingsgebaren van FileSpacer opgeschort voor de betrokken vensters, ook als PowerToys niet actief is.

Voeg `FileSpacer.exe` toe aan de uitgesloten apps van Grab And Move of schakel de functie uit in PowerToys om de FileSpacer-gebaren te blijven gebruiken.

### PowerToys Peek

**Voorbeeld met spatiebalk**, onder **Instellingen > Integratie**, opent een Peek-voorbeeld van geselecteerde bestanden of mappen met alleen de **spatiebalk**. De optie staat standaard uit.

Peek moet ook in PowerToys zijn ingeschakeld. De integratie vereist PowerToys 0.96 of nieuwer en 0.102 of nieuwer voor meervoudige selecties.

### Everything

Een bestaande installatie van [Everything](https://www.voidtools.com/) kan worden geïntegreerd voor geavanceerde zoekopdrachten vanuit het FileSpacer-zoekveld. Resultaten openen in Everything.

De optie staat onder **Instellingen > Integratie**, met automatische detectie van Everything of handmatige selectie van een draagbare kopie. De zoekopdracht omvat de huidige map en submappen, of alle geïndexeerde bestanden met **Overal**. Voor virtuele mappen moet Overal worden gekozen.

**Zoeken in Everything** start de externe zoekopdracht met de syntaxis van Everything. Na geslaagd starten worden het FileSpacer-veld en lokale filter gewist.

![Een zoekopdracht in FileSpacer met de bijbehorende Everything-resultaten ernaast.](images/everything-search-nl.png)

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) kan de menu’s van FileSpacer vormgeven en opdrachten aanpassen. Aanpassing van de configuratie wordt aanbevolen voor een consistent uiterlijk, met name rechte hoeken op titelbalk- en werkbalkmenu’s.

FileSpacer stelt eerst het titelbalkmenu samen. Daarna past Nilesoft Shell regels toe die opdrachten kunnen hernoemen, verplaatsen, verbergen of toevoegen en dus voorrang hebben. Grijze items in de editor worden gewijzigd in de configuratiebestanden van Nilesoft Shell.

FileSpacer-acties zijn bruikbaar in de configuratie van Nilesoft Shell. De [configuratiereferentie](nilesoft-shell.md) bevat de instellingen en actie-ID’s. Voorbeeld met de daar gedefinieerde voorwaarde `filespacer_title`:

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Vergrendelde indeling herstellen' cmd=window.command(1123))
```

De afbeeldingen hieronder tonen het Modern-thema met de standaardhoeken en met de aanbevolen rechte hoeken.

**Navigatiemenu**

| Standaardhoeken | Aanbevolen rechte hoeken |
| --- | --- |
| <img src="images/nilesoft-navigation-default-nl.png" alt="Navigatiemenu met standaard afgeronde hoeken." width="320"> | <img src="images/nilesoft-navigation-adapted-nl.png" alt="Hetzelfde menu met rechte hoeken." width="320"> |

**Hoofdmenu (…)**

| Standaardhoeken | Aanbevolen rechte hoeken |
| --- | --- |
| <img src="images/nilesoft-main-menu-default-nl.png" alt="Hoofdmenu met standaard afgeronde hoeken." width="320"> | <img src="images/nilesoft-main-menu-adapted-nl.png" alt="Hetzelfde menu met rechte hoeken." width="320"> |

## Instellingen en updates

Ctrl+, opent Instellingen. F1 opent de offlinehandleiding. De interface volgt de Windows-weergavetaal. Een taalwijziging vereist het opnieuw starten van de toepassing.

Updates worden gecontroleerd onder **Instellingen > Info**, waar ook optionele wekelijkse controles kunnen worden ingeschakeld. Deze staan standaard uit. Downloaden en installeren vereisen bevestiging. Daarna openen de eerdere mappen weer met hun opgeslagen indeling.

## Lokale gegevens

Indelingen, vergrendelingen, geschiedenis en voorkeuren worden lokaal opgeslagen. De opslaglocaties staan in de [technische notitie voor probleemoplossing](../en/README.md#local-data) (in het Engels).

**Instellingen > Info > Onderhoud > Alle mapstatussen opnieuw instellen** sluit mapvensters en wist hun opgeslagen indeling, geschiedenis en vergrendelingen. Bestanden en algemene voorkeuren blijven behouden.

Voor probleemoplossing stelt `FileSpacer.exe /reset-options` algemene voorkeuren opnieuw in. `/reset-all` wist ook opgeslagen mapstatussen. Deze opdrachten behouden de bestanden.

## Privacy

FileSpacer bevat geen telemetrie, gebruiksregistratie of reclame. Crashrapporten worden niet automatisch verstuurd. Diagnostische rapporten blijven in `%LOCALAPPDATA%\FileSpacer`. Delen is optioneel en kan bestandspaden of delen van het procesgeheugen onthullen.

Updatecontroles en downloads maken via HTTPS contact met GitHub. GitHub ontvangt de gebruikelijke verbindingsinformatie, waaronder het IP-adres en de toepassingsversie, maar geen mapstatussen, bestandspaden of diagnostische rapporten.

Referenties voor netwerkstations worden door Windows ingevoerd en verwerkt, zoals in Bestandenverkenner. FileSpacer leest of bewaart ze niet. Een Everything-zoekopdracht kiezen geeft de zoekopdracht en, bij mapgebonden zoeken, het pad door aan de lokale Everything-toepassing.
