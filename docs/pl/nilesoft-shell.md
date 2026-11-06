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

Przy używaniu FileSpacer zamiast Eksploratora plików zmiany motywu działają w nowo otwieranych menu po wyrejestrowaniu i ponownym zarejestrowaniu Nilesoft Shell, bez restartu Eksploratora.

<a id="target-a-specific-menu"></a>

## Wybór konkretnego menu

Zadeklaruj zmienne przed importami, które ich używają:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Użyj odpowiednich właściwości `where` i `type` w regułach:

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu paska tytułu | `filespacer_title` | `'*'` |
| Pasek narzędzi **…** | `filespacer_toolbar` | `'*'` |
| Zaznaczone pliki lub foldery | `filespacer_context` | `'file\|dir'` |
| Tło folderu | `filespacer_context` | `'back'` |

Użyj `mode='multiple'` dla reguł dostępnych niezależnie od zaznaczenia. `process.name` pomija `.exe`. `window.parent.name` identyfikuje klasę okna.

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

Prostokątne narożniki menu tytułu i paska narzędzi pasują do przycisków FileSpacer. Korzystając z powyższych zmiennych, dodaj to ustawienie do istniejącego bloku `theme { ... }`:

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
| Zapomnij układ tego folderu | 1125 |
| Zamknij okna podrzędne | 1126 |
| Zamknij pozostałe okna | 1127 |
| Dopasuj okno do zawartości | 1128 |
| Zapisz / przywołaj grupę folderów | 1129 |
