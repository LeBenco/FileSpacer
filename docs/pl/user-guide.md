# Korzystanie z FileSpacer

## Nawigacja między oknami

FileSpacer oferuje płynny, prosty i konfigurowalny interfejs oparty na modelu przestrzennym:

- **Jeden folder, jedno okno.** Ponowne otwarcie już otwartego folderu przenosi jego okno na pierwszy plan. Położenie, rozmiar, widok i układ ikon są zapamiętywane również po przeniesieniu folderu lub zmianie jego nazwy.
- Otwarcie folderu lub folderu nadrzędnego domyślnie zamyka okno źródłowe. Przytrzymanie klawisza Ctrl podczas otwierania folderu pozostawia okno źródłowe otwarte. Domyślne zachowanie można zmienić w **Ustawienia > Ogólne**. Otwarcie kilku zaznaczonych folderów zachowuje okno źródłowe.
- Menu obok tytułu pozwala przełączać się między otwartymi folderami. **Zamknij okna nadrzędne** zamyka otwarte okna folderów nadrzędnych.
- **Ctrl+Shift+T** otwiera ostatnio zamknięty folder z jego układem i zaznaczeniem. Polecenie jest też w menu tytułu i na liście szybkiego dostępu na pasku zadań. Ta ostatnia pozwala sięgnąć do wcześniej zamkniętych folderów.
- Lista szybkiego dostępu na pasku zadań otwiera też foldery przypięte w Szybkim dostępie Eksploratora plików.

Przejście do folderu nadrzędnego nie zaznacza właśnie opuszczonego folderu. Już otwarty folder nadrzędny zachowuje zaznaczenie.

![Menu tytułu folderu z poleceniami nawigacji i zapisanego układu.](images/window-menu-pl.png)

## Widok i układ

### Układ ikon

Widoki ikon pozwalają dowolnie rozmieszczać pliki i podfoldery, gdy **Autorozmieszczanie ikon** jest wyłączone. Położenia ikon są zapisywane niezależnie od geometrii okna, również przy zablokowanym zapisanym układzie.

### Opcje widoku

Opcja **Pokaż pełną nazwę zaznaczonego pliku** w **Ustawienia > Wyświetlanie** pozwala odczytać pełną nazwę pojedynczego zaznaczonego pliku w widoku ikon.

![Długa nazwa zaznaczonego pliku wyświetlona w całości w widoku ikon.](images/selected-name-pl.png)

Opcja **Podgląd trybów wyświetlania w menu Widok** pokazuje każdy tryb po wskazaniu go myszą. Kliknięcie go zatwierdza. Escape lub opuszczenie podglądu przywraca pierwotny widok. Podgląd jest tymczasowy i nie zastępuje zapisanego układu.

Opcja **Sortuj foldery i pliki razem** stosuje wspólną kolejność, jak w [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) i [Linuksie](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Znajduje się w **Ustawienia > Wyświetlanie > Pliki** i dotyczy widoków Szczegóły, Lista oraz automatycznie rozmieszczanych ikon. Foldery wirtualne i archiwa zachowują kolejność Windows.

![Pliki i foldery we wspólnej kolejności alfabetycznej.](images/mixed-sorting-pl.png)

### Rozmieszczanie okien

Foldery bez zapisanego położenia otwierają się w wolnym miejscu lub z ograniczonym nakładaniem, gdy miejsca brakuje. Odpowiada za to opcja **Ustawienia > Ogólne > Umieszczaj nowe okna w wolnym miejscu**.

### Zaznaczenie i aktywacja

**Zachowaj zaznaczenie po kliknięciu nieaktywnego okna** w **Ustawienia > Ogólne** pozwala kliknięciem tła aktywować folder bez utraty zaznaczenia. Gdy okno jest już aktywne, kliknięcie i przeciągnięcie po jego tle pozwala narysować prostokąt zaznaczenia.

## Zarządzanie oknami

### Przenoszenie i zmiana rozmiaru

Okno można przenieść lub zmienić jego rozmiar, przeciągając w dowolnym miejscu obszaru plików, również nad elementem. Domyślne gesty:

| Działanie | Skrót |
| --- | --- |
| Przenieś okno | Alt + przeciągnięcie lewym przyciskiem |
| Zmień rozmiar od prawego dolnego rogu | Alt + przeciągnięcie prawym przyciskiem |

![Przenoszenie okna z Alt i przeciągnięciem lewym przyciskiem.](images/move-window-pl.gif)

![Zmiana rozmiaru okna z Alt i przeciągnięciem prawym przyciskiem.](images/resize-window-pl.gif)

Zaznaczenie pozostaje bez zmian. Escape anuluje gest.

Każde działanie ma osobny skrót w **Ustawienia > Skróty**. Lewy i prawy przycisk wymagają Alt. Środkowy działa też bez niego.

### Blokowanie zapisanego układu

Blokada zachowuje położenie, rozmiar i ustawienia widoku folderu do przywrócenia przy ponownym otwarciu. Funkcja jest domyślnie wyłączona w **Ustawienia > Ogólne > Włącz blokowanie stanu okien**. Po włączeniu polecenie **Zablokuj stan okna** pojawia się obok tytułu folderu. Kłódka na ikonie folderu oznacza zablokowany układ.

<img src="images/locked-title-pl.png" alt="Zbliżenie tytułu folderu z oznaczeniem kłódki." width="280">

Tymczasowe zmiany pozostają możliwe. Ponowne otwarcie przywraca zablokowany układ. Położenia poszczególnych ikon nadal zapisują się normalnie. **Odblokuj stan okna** zachowuje bieżący układ.

Wyłączenie funkcji wymaga potwierdzenia i usuwa wszystkie blokady, także dla zamkniętych folderów. Ponowne włączenie ich nie przywraca.

### Dostosowanie menu paska tytułu

Menu można dostosować w **Ustawienia > Wyświetlanie > Elementy interfejsu > Dostosuj menu paska tytułu**. Przeciągnij działanie z prawej strony do menu, przesuń pozycję, aby zmienić kolejność, lub wyciągnij ją poza menu, aby usunąć.

Pozycji zarządzanych przez Nilesoft Shell nie można zmieniać tutaj, ponieważ jego reguły mają pierwszeństwo. Zobacz [integrację Nilesoft Shell](#nilesoft-shell).

![Dodawanie działania do menu paska tytułu i usuwanie innego.](images/customize-title-menu-pl.gif)

![Dostosowane menu paska tytułu z dodanym przywracaniem zablokowanego układu.](images/customized-title-menu-pl.png)

- **Przywróć zablokowany układ** przywraca oknu układ odniesienia. **Zastąp układ odniesienia** po potwierdzeniu zapisuje bieżący układ jako nowy zablokowany układ odniesienia.

- **Zapomnij układ tego folderu** usuwa zapisany układ i blokadę, zachowując historię wyszukiwania. Zapisywanie pozostaje wstrzymane w bieżącym oknie do jego zamknięcia. Kolejne otwarcie używa ustawień domyślnych. Zablokowanie nowego układu wznawia zapisywanie.

### Grupy folderów

**Zapisz / przywołaj grupę folderów** zapisuje otwarte foldery pod wybraną nazwą. Przywołanie grupy otwiera je ponownie z zapisanymi układami.

## Wyszukiwanie i historia

Wpisz całą nazwę pliku lub folderu albo jej część i naciśnij Enter, aby przefiltrować bieżący folder. Opcja **Filtruj podczas pisania** w **Ustawienia > Ogólne** stosuje filtr w trakcie wpisywania.

Pole proponuje ostatnie wyszukiwania oraz, przy włączonej integracji, **Wyszukaj w Everything** do wyszukiwania zewnętrznego. Enter bez wybranej sugestii stosuje filtr lokalny. Ctrl+F aktywuje pole.

Każdy folder ma własną historię. Liczbę zachowywanych zatwierdzonych wyszukiwań ustawia się w ustawieniach ogólnych. 0 czyści i wyłącza historię. [Everything](#everything) pozwala szukać w podfolderach lub we wszystkich zindeksowanych plikach.

![Pole wyszukiwania z Everything i sugestiami ostatnich wyszukiwań.](images/search-pl.png)

## Integracja

### Integracja z Windows

FileSpacer można ustawić jako domyślny menedżer plików podczas instalacji lub w **Ustawienia > Integracja**, wybierając **Ustaw jako domyślny menedżer**. **Przywróć domyślny menedżer** przywraca otwieranie folderów w Eksploratorze plików.

Menu kontekstowe folderów udostępnia **Otwórz w FileSpacer** lub **Otwórz w Eksploratorze plików Windows**, gdy FileSpacer jest domyślny. Działa to także dla dysków, folderów ZIP, lokalizacji sieciowych i tła folderów bez konieczności zmiany aplikacji domyślnej.

**Opcje Eksploratora plików** otwierają wspólne ustawienia folderów, z których część dotyczy też FileSpacer.

### PowerToys Grab And Move

Gdy Grab And Move jest włączony w ustawieniach PowerToys, gesty przenoszenia i zmiany rozmiaru FileSpacer są wstrzymane dla objętych nim okien, nawet jeśli PowerToys nie działa.

Aby nadal używać gestów FileSpacer, dodaj `FileSpacer.exe` do wykluczonych aplikacji Grab And Move lub wyłącz tę funkcję w PowerToys.

### PowerToys Peek

Opcja **Podgląd klawiszem Spacja** w **Ustawienia > Integracja** otwiera podgląd Peek zaznaczonych plików lub folderów po naciśnięciu samego klawisza **Spacji**. Domyślnie jest wyłączona.

Peek musi być też włączony w PowerToys. Integracja wymaga PowerToys 0.96 lub nowszego, a przy wielu zaznaczonych elementach — 0.102 lub nowszego.

### Everything

Istniejącą instalację [Everything](https://www.voidtools.com/) można zintegrować, aby wykonywać zaawansowane wyszukiwania z pola FileSpacer. Wyniki otwierają się w Everything.

Opcję włącza się w **Ustawienia > Integracja**, z automatycznym wykrywaniem Everything lub ręcznym wskazaniem wersji przenośnej. Wyszukiwanie obejmuje bieżący folder i podfoldery, a po wybraniu **Wszędzie** wszystkie zindeksowane pliki. Dla folderów wirtualnych trzeba wybrać Wszędzie.

**Wyszukaj w Everything** uruchamia wyszukiwanie zewnętrzne ze składnią Everything. Udane uruchomienie czyści pole i lokalny filtr FileSpacer.

![Zapytanie w FileSpacer i odpowiadające mu wyniki Everything obok.](images/everything-search-pl.png)

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) zmienia wygląd menu FileSpacer i dostosowuje polecenia. Zaleca się dostosowanie konfiguracji dla spójnego wyglądu, zwłaszcza prostokątnych narożników menu tytułu i paska narzędzi.

FileSpacer najpierw składa menu tytułu. Następnie Nilesoft Shell stosuje swoje reguły, które mogą zmieniać nazwy, przenosić, ukrywać i dodawać polecenia, więc mają pierwszeństwo. Szare pozycje edytora zmienia się w plikach konfiguracji Nilesoft Shell.

Działania FileSpacer można stosować w konfiguracji Nilesoft Shell. [Opis konfiguracji](nilesoft-shell.md) zawiera ustawienia i identyfikatory działań. Przykład z opisanym tam warunkiem `filespacer_title`:

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Przywróć zablokowany układ' cmd=window.command(1123))
```

Poniższe zrzuty ekranu pokazują motyw Modern z domyślnymi narożnikami oraz z zalecanymi prostokątnymi.

**Menu nawigacji**

| Domyślne narożniki | Zalecane prostokątne narożniki |
| --- | --- |
| <img src="images/nilesoft-navigation-default-pl.png" alt="Menu nawigacji z domyślnymi zaokrąglonymi narożnikami." width="320"> | <img src="images/nilesoft-navigation-adapted-pl.png" alt="To samo menu z prostokątnymi narożnikami." width="320"> |

**Menu główne (…)**

| Domyślne narożniki | Zalecane prostokątne narożniki |
| --- | --- |
| <img src="images/nilesoft-main-menu-default-pl.png" alt="Menu główne z domyślnymi zaokrąglonymi narożnikami." width="320"> | <img src="images/nilesoft-main-menu-adapted-pl.png" alt="To samo menu z prostokątnymi narożnikami." width="320"> |

## Ustawienia i aktualizacje

Ctrl+, otwiera Ustawienia. F1 otwiera przewodnik offline. Interfejs używa języka wyświetlania Windows. Zmiana języka wymaga ponownego uruchomienia aplikacji.

Aktualizacje sprawdza się w **Ustawienia > Informacje**, gdzie można też włączyć opcjonalne cotygodniowe sprawdzanie, domyślnie wyłączone. Pobieranie i instalacja wymagają potwierdzenia. Następnie wcześniej otwarte foldery wracają z zapisanymi układami.

## Dane lokalne

Układy, blokady, historie i preferencje są przechowywane lokalnie. Lokalizacje zapisu opisuje [techniczna notatka dotycząca rozwiązywania problemów](../en/README.md#local-data) (po angielsku).

**Ustawienia > Informacje > Konserwacja > Zresetuj stan wszystkich folderów** zamyka okna folderów i usuwa ich zapisane układy, historię i blokady. Pliki i ustawienia ogólne pozostają zachowane.

Do rozwiązywania problemów `FileSpacer.exe /reset-options` resetuje ustawienia ogólne. `/reset-all` usuwa też zapisany stan folderów. Te polecenia zachowują pliki.

## Prywatność

FileSpacer nie stosuje telemetrii, śledzenia użycia ani reklam. Raporty awarii nie są wysyłane automatycznie. Raporty diagnostyczne pozostają w `%LOCALAPPDATA%\FileSpacer`. Ich udostępnianie jest opcjonalne i może ujawniać ścieżki plików lub fragmenty pamięci procesu.

Sprawdzanie aktualizacji i pobieranie łączą się z GitHub przez HTTPS. GitHub otrzymuje zwykłe informacje o połączeniu, w tym adres IP i wersję aplikacji, ale nie stan folderów, ścieżki plików ani raporty diagnostyczne.

Poświadczenia dysków sieciowych wprowadza i obsługuje Windows, tak jak w Eksploratorze plików. FileSpacer ich nie odczytuje ani nie przechowuje. Wybranie wyszukiwania Everything przekazuje zapytanie i, przy wyszukiwaniu w folderze, jego ścieżkę do lokalnej aplikacji Everything.
