<a id="title-bar-menu-implementation"></a>

# Implementacja menu paska tytułu

Edycję i grupy opisuje [instrukcja użytkownika](user-guide.md#dostosowanie-menu-paska-tytułu). Zacznij od `TitleMenu.h`, `TitleMenuDialog.cpp`, `TitleMenuRenderer.cpp` i `FolderGroups.cpp`; polecenia obsługuje okno folderu.

<a id="composition-and-rendering"></a>

## Skład i renderowanie

Ustawienia przechowują stabilne wartości `TitleMenuEntry`, niezależne od tłumaczonych etykiet i dynamicznych ID otwartych okien. `validTitleMenu()` przyjmuje do 64 wpisów, dopuszcza powtarzane separatory i odrzuca powtórzone działania. Wpis otwartych okien to blok rozwijany z bieżącej listy przy otwarciu menu, nie polecenie stale wskazujące jedno okno.

Edytor zapisuje od razu po OK; Anuluj odrzuca projekt. Przywróć domyślne zmienia tylko projekt. Inne okna FileSpacer odczytują skład przy następnym otwarciu menu. Zachowaj kontrole kontekstu i dostępności działań przy przenoszeniu w menu i wywołaniu zewnętrznym.

FileSpacer składa menu, potem Nilesoft Shell stosuje reguły. Wpisy dodane lub pozycjonowane przez Nilesoft są tylko do odczytu. Zmieniona nazwa pozostaje edytowalna, jeśli Nilesoft nie kontroluje położenia; niejednoznaczne ciągi separatorów ostrożnie uznaje się za zarządzane zewnętrznie.

Podgląd używa rzeczywistego renderera Windows/Nilesoft w jednorazowym procesie FileSpacer na prywatnym pulpicie. Nigdy nie wywołuje pokazywanych działań. Podczas przeciągania zachowaj przechwycone piksele menu. Gdy nie można przechwycić warstwowego tła Nilesoft, użyj jawnego nieprzezroczystego wariantu zamiast symulowania akrylu kompozytora.

<a id="action-boundaries"></a>

## Granice działań

- Przywracanie zablokowanego układu stosuje geometrię i widok przez istniejący widok bez utraty zaznaczenia. Zastąpienie wzorca przechwytuje bieżący układ przy istniejącej blokadzie i wymaga potwierdzenia.
- Zapomnienie układu usuwa układ/blokadę, zachowując historię. Bieżące okno nie zapisuje go do zamknięcia, chyba że jawnie zablokowano nowy układ.
- Zamykanie okien podrzędnych/pozostałych używa zwykłego zamykania i zapisu, także między procesami. Dopasowanie używa natywnych pozycji, odstępów i widocznych szerokości kolumn, w granicach obszaru roboczego monitora.
- Grupy przechowują nazwane listy niezależnie od menu. Przywołanie otwiera/aktywuje foldery z ich układem, zachowując inne okna. Usunięcie grupy nie usuwa folderów; reset ustawień ogólnych nie czyści grup.

<a id="nilesoft-command-bridge"></a>

## Most poleceń Nilesoft

Poniższe identyfikatory `WM_COMMAND` są stabilne. Różnią się od zapisanych `TitleMenuEntry` i dynamicznych ID listy okien. Nilesoft może wywołać działanie nieobecne w menu przez [`window.command`](https://nilesoft.org/docs/functions/window). W regule ograniczonej do menu tytułu FileSpacer:

```nss
item(title='Dopasuj okno do zawartości' cmd=window.command(1128))
```

Właściciel menu przekazuje polecenie do rzeczywistego okna folderu; kontrole cyklu życia, kontekstu i blokady nadal obowiązują. Zakres i wygląd: [opis konfiguracji](nilesoft-shell.md).

| Działanie | ID polecenia |
| --- | ---: |
| Kopiuj ścieżkę | 1120 |
| Zmień nazwę | 1008 |
| Zablokuj stan okna | 1100 |
| Właściwości | 1121 |
| Zamknij okna nadrzędne | 1122 |
| Otwórz ostatnio zamknięte okno | 1021 |
| Ustawienia FileSpacer | 1009 |
| Przywróć zablokowany układ | 1123 |
| Zastąp układ odniesienia | 1124 |
| Zapomnij układ tego folderu | 1125 |
| Zamknij okna podrzędne | 1126 |
| Zamknij pozostałe okna | 1127 |
| Dopasuj okno do zawartości | 1128 |
| Zapisz / przywołaj grupę folderów | 1129 |

<a id="tests"></a>

## Testy

Grupa `33-menu-titre` sprawdza natywny skład i działania w Debug `/test`, z izolowanymi ustawieniami, grupami i stanem. Ten tryb wyklucza Nilesoft z menu/edytora. Jawny test współdziałania może go włączyć przez `FILESPACER_TEST_NILESOFT=1` we własnym środowisku procesu; Release to ignoruje. Testy natywne nie walidują renderowania Nilesoft.

Testy współdziałania używają istniejącej instalacji Nilesoft i odrębnej konfiguracji testowej. Zachowaj i przywróć dokładny stan rejestru i konfiguracji, także po przekroczeniu czasu. Żaden plik binarny Nilesoft nie jest rozpowszechniany z FileSpacer ani zwykłymi testami. Nie kopiuj zainstalowanych DLL do publikowanych artefaktów; zachowaj właściwe informacje licencyjne i dystrybucyjne osobnych zależności.
