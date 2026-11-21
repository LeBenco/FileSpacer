<a id="nilesoft-shell-configuration-reference"></a>

# Opis konfiguracji Nilesoft Shell

Przegląd w [instrukcji użytkownika Nilesoft Shell](user-guide.md#nilesoft-shell).

[Nilesoft Shell](https://nilesoft.org/) może stosować motywy i dostosowanie menu także do FileSpacer przez `shell.nss` i importowane pliki konfiguracji. Dla tej opcjonalnej integracji zaleca się dostosowanie konfiguracji do wyglądu FileSpacer. Poniższe przykłady obejmują dodawanie, zmienianie i usuwanie pozycji menu, warunki specyficzne dla FileSpacer i zalecane prostokątne narożniki.

<a id="scope-system-menu-rules"></a>

## Ograniczanie reguł menu systemowych

Do reguł dla Eksploratora Windows, pulpitu lub paska zadań dodaj `process.is_explorer &&` do istniejącego warunku `where`. Aby zachować ich działanie w innych aplikacjach, ale wykluczyć FileSpacer, użyj `process.name != 'FileSpacer' &&`. Zastosuj filtr do ogólnych reguł `menu`, `item`, `modify` i `remove`, także w importowanych plikach.

<a id="apply-themes-to-filespacer"></a>

## Stosowanie motywów do FileSpacer

Dopuść FileSpacer w `settings.exclude.where` i zachowaj import motywu. Dla zwykłego warunku wykluczenia:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Właściwości motywu mogą też używać wyrażeń zależnych od procesu: np. `dark = if(process.name == 'FileSpacer', true, sys.dark)` w `theme { ... }` wybiera ciemny tryb dla FileSpacer, a ustawienie systemowe gdzie indziej.

Po zapisaniu pliku NSS **Ctrl + kliknięcie prawym przyciskiem** na pulpicie lub pasku zadań wymusza ponowne wczytanie konfiguracji Nilesoft w Eksploratorze, bez jego restartowania ani wyrejestrowywania rozszerzenia. Nilesoft wczytuje ją również, gdy wykryje wciśnięty lewy przycisk podczas otwierania menu; przycisk FileSpacer może więc od razu zastosować zmiany. Zależy to od gestu otwierania, a nie od szybszego mechanizmu odświeżania w FileSpacer.

<a id="menu-opening-delay"></a>

## Opóźnienie otwierania menu

Aby usunąć [opóźnienie otwierania menu](https://nilesoft.org/docs/configuration/settings) w Nilesoft Shell, należy ustawić `showdelay = 0` w istniejącym bloku `settings` pliku `shell.nss`. Aby zmiana dotyczyła tylko FileSpacer, użyć `showdelay = if(process.name == 'FileSpacer', 0, 200)`, zastępując `200` opóźnieniem, które ma pozostać w innych aplikacjach, w milisekundach.

<a id="target-a-specific-menu"></a>

## Wybór konkretnego menu

Zadeklaruj zmienne przed importami, które ich używają:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Użyj odpowiednich właściwości `where` i `type` w regułach:

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu paska tytułu | `filespacer_title` | `'*'` |
| Pasek narzędzi **…** i **Widok** | `filespacer_toolbar` | `'*'` |
| Tylko **Widok** | `filespacer_view` | `'*'` |
| Tylko **…** na pasku narzędzi | `filespacer_more` | `'*'` |
| Zaznaczone pliki lub foldery | `filespacer_context` | `'file\|dir'` |
| Tło folderu | `filespacer_context` | `'back'` |

Użyj `mode='multiple'` dla reguł dostępnych niezależnie od zaznaczenia. `process.name` pomija `.exe`. `window.parent.name` identyfikuje klasę okna.

`filespacer_toolbar` nadal obejmuje oba menu paska narzędzi. Użyj `filespacer_view` tylko dla Widoku albo `filespacer_more` tylko dla **…**. Wewnętrzny znacznik `window.parent.title` jest stały i nie jest tłumaczony. Aby zachować Terminal poza Widokiem, dodaj `!filespacer_view &&` do istniejącego warunku `where`, ujmując dotychczasowy warunek w nawiasy:

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

<a id="target-an-existing-menu-entry"></a>

## Wybór istniejącej pozycji menu

Ogólny mechanizm `modify(find=...)` Nilesoft dopasowuje pozycję według etykiety. Połącz go z `where`, aby ograniczyć zmianę do menu. `|` oddziela alternatywne etykiety, a `*` jest symbolem wieloznacznym:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin|Kopiuj ścieżkę' title='Kopiuj ścieżkę folderu')
```

Zobacz [opis konfiguracji](https://nilesoft.org/docs/configuration/) Nilesoft.

<a id="recommended-menu-appearance"></a>

## Zalecany wygląd menu

Dla trzech menu: menu paska tytułu, **…** i **Widok**, zalecane są prostokątne narożniki, pasujące do przycisków FileSpacer. Dodaj to ustawienie do istniejącego bloku `theme { ... }`:

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

Jeśli używasz własnego promienia, zastąp `default` jego dotychczasową wartością.

<a id="use-filespacer-actions"></a>

## Korzystanie z działań FileSpacer

Użyj [`window.command`](https://nilesoft.org/docs/functions/window) z poniższym ID, aby dodać działanie FileSpacer, nawet nieobecne w jego własnym menu. Ogranicz regułę do menu paska tytułu warunkiem `filespacer_title`. Zwykłe wymagania działania nadal obowiązują: np. przywracanie zablokowanego układu wymaga istniejącej blokady.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Przywróć zablokowany układ' cmd=window.command(1123))
```

| Działanie | ID |
| --- | ---: |
| Przywróć zablokowany układ | 1123 |
| Zastąp układ odniesienia | 1124 |
| Zamknij okna podrzędne | 1126 |
| Zamknij pozostałe okna | 1127 |
| Dopasuj okno do zawartości | 1128 |
| Zapisz / przywołaj grupę folderów | 1129 |
