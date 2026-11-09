<a id="website-translations"></a>

# Websitevertalingen

Hoofd-README en documentatie blijven Engelse bronnen. Volledige Markdown-varianten leveren site en vertaalde GitHub-referenties; ze voegen geen taalkiezer of links aan de repository-README toe.

Elke map `docs/<language>/` bevat `locale.json` met de taal-regiocode, `README.md`, `user-guide.md`, `CHANGELOG.md` en `THIRD_PARTY_NOTICES.md`. Privacy/lokaal onderhoud hebben aparte handleidingsecties; `privacy-policy.md` blijft kort beleid op GitHub. `nilesoft-shell.md` is de volledige vertaalde referentie uit de handleiding, apart op GitHub. Ontbreken doet generatie falen in plaats van stil naar Engels te linken. De generator ontdekt mappen; Engels gebruikt de README, het wijzigingsoverzicht en de derdenvermeldingen uit de hoofdmap, `docs/en/user-guide.md` en `website/locale.json`.

Technische documenten staan in de taalmap. Documenten met dezelfde bestandsnaam behouden een submap: de technische index staat in `technical/README.md` en de korte bouwinstructies in `building/BUILD.md`. Test- en websitegidsen behouden hun eigen submappen. De bestaande bestandsnamen van documenten en afbeeldingen blijven behouden.

Behoud bij updates Engelse kopvolgorde/hiërarchie. `source-state.json` bewaart Engelse inhoudshashes en sectie-ID’s van de laatste synchronisatie. Engels wordt direct gepubliceerd. Vertalingen behouden tekst en ID’s; veroudering geeft waarschuwingen en een tabel in plaats van blokkering. Ook tekstwijzigingen worden ontdekt.

Registreer na vertalen de Engelse referentie vanuit CMD in de hoofdmap (vervang de locale):

```cmd
node website\generate.mjs --record-translation-sources fr-FR
```

Zonder locale worden alle vastgelegd. Commit bronstatus met Markdown. Registratie valideert koppen/links, vertaalt niet en beoordeelt geen inhoudelijke actualiteit. Registreer Engelse wijzigingen pas na vertalen. Ontbrekende documenten, gebroken links/media en structuurverschillen met de vastgelegde bron blijven fouten.

Elke kop heeft direct ervoor een expliciet Engels anker, bijvoorbeeld `<a id="recommended-menu-appearance"></a>`. Gebruik in elke taal dezelfde ID’s en verwijs ernaar in alle links. Vertaalde koppen kunnen veranderen zonder hun ankers te wijzigen. Oude vertaalde ankers blijven bruikbaar voor bestaande links. Gebruikersdocumentatie verwijst naar het GitHub-bestand in de gekozen taal. Technische verwijzingen gebruiken een vertaling als die beschikbaar is, anders de Engelse bron.

Gebruik in elke taalmap dezelfde bestandsnamen zonder taalachtervoegsel, bijvoorbeeld `images/spatial-windows.png`. De map `docs/<language>/` geeft de taal aan. Ook de relatieve afbeeldings-URL’s op de site zijn gelijk. De generator kopieert de echte vertaalde bestanden en meldt een fout als een opname ontbreekt. Het toepassingslogo wordt gedeeld.

Elke taal wordt apart gebouwd met hetzelfde vastgezette Just the Docs, eigen navigatie, index, labels en media. De workflow ontdekt de matrix uit `locales.json`.

<a id="terminology-and-style"></a>

## Terminologie en stijl

Gebruik lokale Windows-termen voor systeemconcepten. Exacte instellingenpaden behouden echte FileSpacer-labels. Gebruik voor andere concepten gevestigde softwaretermen en natuurlijke formuleringen. Behoud betekenis, voorbeelden, links en ingetogen humor in plaats van woordelijke vertaling. Licenties, opdrachten, registerpaden, ID’s en programmanamen blijven gelijk.

Verkies korte gedragsbeschrijvingen en nuttige nuances boven uitleg van vanzelfsprekende knoppen. Frans gebruikt neutrale beschrijvingen/infinitieven zonder herhaald aanspreken. Dit is redactioneel, geen algemene norm: Microsoft/GNOME spreken direct aan, Everything gebruikt korte Engelse opdrachten en Duits vaak formele aanspreking. Volg het taalidioom, niet Franse zinsbouw. Plaats opnamen na de uitleg met beschrijvende alt-tekst in plaats van dubbele bijschriften.

De Everything-opname gebruikt Braziliaans Portugees naast FileSpacer en Europese fictieve namen. Everything biedt geen Europees Portugees in de officiële [taallijst](https://www.voidtools.com/support/everything/supported_languages/).

| Taal | Bestandenverkenner | Map | Taakbalk | Sneltoetsen |
| --- | --- | --- | --- | --- |
| Français | Explorateur de fichiers | dossier | barre des tâches | raccourcis clavier |
| Deutsch | Datei-Explorer | Ordner | Taskleiste | Tastenkombinationen |
| Italiano | Esplora file | cartella | barra delle applicazioni | scelte rapide da tastiera |
| Español | Explorador de archivos | carpeta | barra de tareas | métodos abreviados de teclado |
| Português (Portugal) | Explorador de Ficheiros | pasta | barra de tarefas | atalhos de teclado |
| Ελληνικά | Εξερεύνηση αρχείων | φάκελος | γραμμή εργασιών | συντομεύσεις πληκτρολογίου |
| Polski | Eksplorator plików | folder | pasek zadań | skróty klawiaturowe |
| Nederlands | Bestandenverkenner | map | taakbalk | sneltoetsen |
| Українська | Файловий провідник | папка | панель завдань | сполучення клавіш |

Stijlbronnen: [Microsoft Bestandenverkenner](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [Everything](https://www.voidtools.com/support/everything/searching/), GNOME Files ([FR](https://help.gnome.org/gnome-help/files-browse.html.fr), [DE](https://help.gnome.org/gnome-help/files-browse.html.de)). Respectievelijk grote commerciële software, een kleiner hulpmiddel en opensourcedocumentatie.

Terminologiebronnen:

- Frans: [Bestandenverkenner](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/fr-fr/accessibility/windows/keyboard-shortcuts-in-windows).
- Duits: [Bestandenverkenner](https://support.microsoft.com/de-de/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/de-de/accessibility/windows/keyboard-shortcuts-in-windows).
- Italiaans: [Bestandenverkenner](https://support.microsoft.com/it-it/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/it-it/accessibility/windows/keyboard-shortcuts-in-windows).
- Spaans: [Bestandenverkenner](https://support.microsoft.com/es-es/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/es-es/accessibility/windows/keyboard-shortcuts-in-windows).
- Portugees (Portugal): [Bestandenverkenner](https://support.microsoft.com/pt-pt/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/pt-pt/accessibility/windows/keyboard-shortcuts-in-windows).
- Grieks: [Bestandenverkenner](https://support.microsoft.com/el-gr/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/el-gr/accessibility/windows/keyboard-shortcuts-in-windows).
- Pools: [Bestandenverkenner](https://support.microsoft.com/pl-pl/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/pl-pl/accessibility/windows/keyboard-shortcuts-in-windows).
- Nederlands: [Bestandenverkenner](https://support.microsoft.com/nl-nl/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/nl-nl/accessibility/windows/keyboard-shortcuts-in-windows).
- Oekraïens: [Bestandenverkenner](https://support.microsoft.com/uk-ua/windows/experience/fileexplorer/file-explorer-in-windows), [sneltoetsen](https://support.microsoft.com/uk-ua/accessibility/windows/keyboard-shortcuts-in-windows).
- [Microsoft-stijlgidsen voor lokalisatie](https://learn.microsoft.com/en-us/globalization/reference/microsoft-style-guides).
- [Windows-pictogramhandlers in het Portugees](https://learn.microsoft.com/pt-pt/windows/win32/shell/how-to-create-icon-handlers).
- [CC0 public-domain terminology in Greek](https://creativecommons.org/publicdomain/zero/1.0/deed.el).
