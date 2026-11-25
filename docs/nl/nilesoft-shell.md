<a id="nilesoft-shell-configuration-reference"></a>

# Nilesoft Shell-configuratiereferentie

Zie [Nilesoft Shell in de gebruikershandleiding](user-guide.md#nilesoft-shell) voor een overzicht.

Met [Nilesoft Shell](https://nilesoft.org/) kunt u thema’s en menu-aanpassingen ook op FileSpacer toepassen via `shell.nss` en geïmporteerde configuratiebestanden. Voor deze optionele integratie worden aanpassingen aanbevolen zodat de menu’s bij FileSpacer passen. Hieronder staan regels voor toevoegen, wijzigen en verwijderen, FileSpacer-specifieke voorwaarden en aanbevolen rechte hoeken.

<a id="scope-system-menu-rules"></a>

## Systeemmenuregels afbakenen

Voeg voor regels voor Windows Verkenner, het bureaublad of de taakbalk `process.is_explorer &&` toe aan de bestaande `where`-voorwaarde. Gebruik `process.name != 'FileSpacer' &&` om hun bereik in andere toepassingen te behouden en FileSpacer uit te sluiten. Pas dit toe op algemene `menu`-, `item`-, `modify`- en `remove`-regels, ook in geïmporteerde bestanden.

<a id="apply-themes-to-filespacer"></a>

## Thema’s op FileSpacer toepassen

Sta FileSpacer toe in `settings.exclude.where` en behoud uw thema-import. Voor de gebruikelijke uitsluitingsvoorwaarde:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Thema-eigenschappen kunnen processpecifieke expressies gebruiken: `dark = if(process.name == 'FileSpacer', true, sys.dark)` in `theme { ... }` kiest bijvoorbeeld de donkere modus voor FileSpacer en elders de systeemvoorkeur.

Na het opslaan van een NSS-bestand dwingt **Ctrl + rechtsklik** op het bureaublad of de taakbalk Nilesoft om de configuratie in Verkenner opnieuw te laden, zonder Verkenner te herstarten of de registratie van de uitbreiding op te heffen. Nilesoft laadt ook opnieuw als het bij het openen van een menu een ingedrukte linkermuisknop detecteert; een FileSpacer-knop kan wijzigingen daardoor direct toepassen. Dit hangt af van het openingsgebaar, niet van een sneller herlaadmechanisme in FileSpacer.

<a id="menu-opening-delay"></a>

## Vertraging bij het openen van menu’s

Om de [vertraging bij het openen van menu’s](https://nilesoft.org/docs/configuration/settings) van Nilesoft Shell op te heffen, stelt u `showdelay = 0` in het bestaande `settings`-blok van `shell.nss` in. Gebruik `showdelay = if(process.name == 'FileSpacer', 0, 200)` om dit alleen op FileSpacer toe te passen. Vervang `200` door de vertraging die u voor andere toepassingen wilt behouden, in milliseconden.

<a id="target-a-specific-menu"></a>

## Een specifiek menu kiezen

Declareer deze variabelen vóór de imports die ze gebruiken:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Gebruik de bijbehorende eigenschappen `where` en `type` in uw regels:

| Menu | `where` | `type` |
| --- | --- | --- |
| Titelbalkmenu | `filespacer_title` | `'*'` |
| Werkbalk **…** en **Beeld** | `filespacer_toolbar` | `'*'` |
| Alleen **Beeld** | `filespacer_view` | `'*'` |
| Alleen werkbalk **…** | `filespacer_more` | `'*'` |
| Geselecteerde bestanden of mappen | `filespacer_context` | `'file\|dir'` |
| Mapachtergrond | `filespacer_context` | `'back'` |

Gebruik `mode='multiple'` voor regels die bij elke selectiestatus beschikbaar moeten blijven. `process.name` laat `.exe` weg. `window.parent.name` identificeert een vensterklasse.

`filespacer_toolbar` omvat nog steeds beide werkbalkmenu’s. Gebruik `filespacer_view` alleen voor Beeld of `filespacer_more` alleen voor **…**. De interne markering `window.parent.title` is stabiel en wordt niet vertaald. Om Terminal buiten Beeld te behouden, voegt u `!filespacer_view &&` toe aan de bestaande `where`-voorwaarde en zet u de oorspronkelijke voorwaarde tussen haakjes:

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

<a id="target-an-existing-menu-entry"></a>

## Een bestaand menu-item kiezen

Het algemene mechanisme `modify(find=...)` van Nilesoft zoekt op het label van een item. Combineer het met `where` om de wijziging tot een menu te beperken. `|` scheidt alternatieve labels en `*` is een jokerteken:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin|Pad kopiëren' title='Mappad kopiëren')
```

Zie de [configuratiereferentie](https://nilesoft.org/docs/configuration/) van Nilesoft.

De twee opdrachten voor pictogramafstand hebben vaste namen. Hun Win32-opdracht-ID’s zijn 1146 (horizontaal) en 1147 (verticaal). Gebruik de namen in het submenu om elke opdracht een eigen pictogram te geven.

```nss
modify(type='*' mode='multiple' where=filespacer_view in='Pictogramafstand' find='Horizontale pictogramafstand'
       image=image.svg('<svg viewBox="0 0 16 16"><path d="M2 3v10m12-10v10M4 8h8M6.5 5.5 4 8l2.5 2.5m3-5L12 8l-2.5 2.5" fill="none" stroke="@image.color1" stroke-linecap="round" stroke-linejoin="round"/></svg>'))
modify(type='*' mode='multiple' where=filespacer_view in='Pictogramafstand' find='Verticale pictogramafstand'
       image=image.svg('<svg viewBox="0 0 16 16"><path d="M3 2h10M3 14h10M8 4v8m-2.5-5.5L8 4l2.5 2.5m-5 3L8 12l2.5-2.5" fill="none" stroke="@image.color1" stroke-linecap="round" stroke-linejoin="round"/></svg>'))
```

<a id="recommended-menu-appearance"></a>

## Aanbevolen menu-uiterlijk

Rechte hoeken worden aanbevolen voor de drie menu’s: het titelbalkmenu, **…** en **Beeld**, passend bij de FileSpacer-knoppen. Voeg deze instelling toe aan uw bestaande `theme { ... }`-blok:

```nss
border.radius = if(
    process.name == 'FileSpacer' &&
    (
        window.parent.name == 'FileSpacer Item Window' ||
        window.parent.name == 'FileSpacer Toolbar Host'
    ),
    0,
    default
)
```

Vervang `default` door de vorige waarde als u al een aangepaste straal gebruikt.

<a id="use-filespacer-actions"></a>

## FileSpacer-acties gebruiken

Gebruik [`window.command`](https://nilesoft.org/docs/functions/window) met een onderstaande ID om een FileSpacer-actie toe te voegen, ook als die niet in FileSpacers eigen menu staat. Beperk de regel tot het titelbalkmenu met `filespacer_title`. De normale voorwaarden blijven gelden: een vergrendelde indeling herstellen vereist bijvoorbeeld een bestaande vergrendeling.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Vergrendelde indeling herstellen' cmd=window.command(1123))
```

| Actie | ID |
| --- | ---: |
| Vergrendelde indeling herstellen | 1123 |
| Referentie-indeling vervangen | 1124 |
| Onderliggende vensters sluiten | 1126 |
| Andere vensters sluiten | 1127 |
| Venster aan inhoud aanpassen | 1128 |
| Mapgroep opslaan / oproepen | 1129 |
