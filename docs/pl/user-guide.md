<a id="using-filespacer"></a>

# Korzystanie z FileSpacer

FileSpacer zapamiętuje położenie i wygląd każdego okna między kolejnymi otwarciami. Foldery wracają na swoje miejsca i łatwo je rozpoznać, podobnie jak przedmioty ułożone na biurku. Program oferuje też konfigurowalne skróty, szybkie działania i przejrzysty interfejs, który można dostosować.

<div id="integracja-z-windows"></div>
<div id="windows-integration"></div>

FileSpacer można ustawić jako domyślny menedżer plików podczas instalacji lub w **Ustawienia > Integracja**, wybierając **Ustaw jako domyślny menedżer**. Menu kontekstowe folderów zawiera **Otwórz w FileSpacer**, a gdy FileSpacer jest domyślny, **Otwórz w Eksploratorze plików Windows**.

<a id="window-navigation-and-layout"></a>

## Nawigacja i rozmieszczenie okien

<div id="nawigacja-między-oknami"></div>
<div id="window-navigation"></div>
<div id="rozmieszczanie-okien"></div>
<div id="window-placement"></div>
<div id="zarządzanie-oknami"></div>
<div id="managing-windows"></div>

**Każdy folder ma własne okno.** Jeśli jest już otwarty, FileSpacer przenosi jego okno na pierwszy plan. Położenie, rozmiar, widok, układ ikon i pozycja przewijania są zapamiętywane nawet po przeniesieniu folderu lub zmianie jego nazwy. Okna bez zapisanego położenia są rozmieszczane tak, aby ograniczyć nakładanie się (opcja w **Ustawienia > Ogólne**).

<div id="blokowanie-zapisanego-układu"></div>
<div id="locking-a-saved-layout"></div>

**Można zablokować bieżące położenie, rozmiar i widok folderu, aby przywracać je przy każdym otwarciu.** Późniejsze zmiany są tymczasowe. Pozycje ikon nadal są zapisywane. Można **przywrócić zablokowany układ** bez zamykania okna.

**Odblokuj stan okna** zachowuje bieżący układ i ponownie umożliwia zapisywanie zmian.

Blokowanie włącza się w **Ustawienia > Ogólne**. Można tam również dostosować wyświetlanie kłódki po lewej stronie paska tytułu i włączyć blokowanie lub odblokowywanie stanu okna kliknięciem tej ikony. Polecenie można też [dodać do menu paska tytułu](#customize-the-title-bar-menu). Wyłączenie funkcji usuwa wszystkie blokady i ukrywa ikonę.

<img src="images/locked-title.png" alt="Zbliżenie tytułu folderu z oznaczeniem kłódki." width="280">

Otwarcie folderu lub folderu nadrzędnego domyślnie zamyka okno źródłowe. **Przytrzymanie Ctrl podczas otwierania** pozostawia je otwarte. Zachowanie można ustawić w **Ustawienia > Ogólne**. Otwarcie kilku zaznaczonych folderów pozostawia okno źródłowe otwarte.

Menu obok tytułu pozwala przełączać otwarte foldery i zamykać okna nadrzędne. **Ctrl+Shift+T** otwiera ostatnio zamknięty folder z jego układem i zaznaczeniem. Lista szybkiego dostępu na pasku zadań zawiera też ostatnio zamknięte foldery i foldery przypięte do Szybkiego dostępu.

<div id="grupy-folderów"></div>
<div id="folder-groups"></div>

**Zapisz / przywołaj grupę folderów** w menu tytułu pozwala otworzyć kilka folderów razem z ich zapisanymi układami.

<a id="folder-contents-and-display"></a>

## Wyświetlanie zawartości folderów

<div id="widok-i-układ"></div>
<div id="view-and-layout"></div>
<div id="układ-ikon"></div>
<div id="icon-layout"></div>

FileSpacer pozwala rozmieszczać elementy folderu w siatce, jak w Eksploratorze plików, lub dowolnie, jak na pulpicie systemu Windows. Dowolne rozmieszczanie jest dostępne przy każdym rozmiarze ikon oraz w widoku **Kafelki**. Widoki **Lista** i **Szczegóły** nie obsługują dowolnego rozmieszczania. Aby wybrać pozycje, wyłączyć opcję **Autorozmieszczanie ikon**. Wyrównywanie do siatki pozostaje dostępne. Menu **Widok** w każdym oknie zapewnia szybki dostęp do ustawień ikon, w tym opcji dostępnych w FileSpacer.

<div id="details-columns"></div>

W widoku **Szczegóły** kolumny mogą automatycznie wypełniać całą szerokość okna. Można na to zezwolić w **Ustawienia > Wyświetlanie**, a następnie włączać lub wyłączać tę opcję osobno dla każdego folderu w menu **Widok**.

![Wypełnienie szerokości okna kolumnami widoku Szczegóły, a następnie poszerzenie kolumny Nazwa.](images/details-columns.gif)

**Odświeżanie na bieżąco**, dostępne także w ustawieniach, może zmniejszyć płynność zmiany rozmiaru. Dla ostrożności nie zaleca się włączania go przy więcej niż kilkudziesięciu plikach.

<div id="opcje-widoku"></div>
<div id="view-options"></div>

Domyślnie foldery są zgrupowane przed plikami, jak w Eksploratorze plików. **Sortuj foldery i pliki razem** w **Ustawienia > Wyświetlanie** sortuje je razem, jak w [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) i [Linuksie](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Dotyczy to widoków Szczegóły, Lista i automatycznie rozmieszczanych ikon. Foldery wirtualne i archiwa zachowują kolejność systemu Windows.

![Pliki i foldery we wspólnej kolejności alfabetycznej.](images/mixed-sorting.png)

W tych samych ustawieniach **Pokaż pełną nazwę zaznaczonego pliku** pozwala odczytać całą nazwę w widoku ikon, nawet bardzo długą. Dotyczy to pojedynczego zaznaczonego pliku.

![Długa nazwa zaznaczonego pliku wyświetlona w całości w widoku ikon.](images/selected-name.png)

<div id="zaznaczenie-i-aktywacja"></div>
<div id="selection-and-activation"></div>

Domyślnie kliknięcie tła nieaktywnego okna usuwa jego zaznaczenie, jak w Eksploratorze plików. **Zachowaj zaznaczenie po kliknięciu nieaktywnego okna** w **Ustawienia > Ogólne** pozwala je aktywować bez utraty zaznaczenia.

<a id="customization"></a>

## Dostosowywanie

<a id="changing-the-interface"></a>

### Zmiana interfejsu

<div id="dostosowanie-menu-paska-tytułu"></div>
<div id="customize-the-title-bar-menu"></div>

**Menu paska tytułu** można dostosować w **Ustawienia > Wyświetlanie > Elementy interfejsu**. Polecenia dodaje się i przestawia przez przeciąganie. Przeciągnięcie wpisu poza menu usuwa go.

![Dodawanie działania do menu paska tytułu i usuwanie innego.](images/customize-title-menu.gif)

![Dostosowane menu paska tytułu z dodanym przywracaniem zablokowanego układu.](images/customized-title-menu.png)

Wpisy obsługiwane przez Nilesoft Shell zmienia się w jego konfiguracji, ponieważ jego reguły mają pierwszeństwo przed regułami FileSpacer ([zobacz integrację poniżej](#nilesoft-shell)).

<div id="przenoszenie-i-zmiana-rozmiaru"></div>
<div id="moving-and-resizing"></div>

Można skonfigurować **gesty myszy do przenoszenia i zmiany rozmiaru okna** bezpośrednio z jego zawartości, również przeciągając po pliku. Domyślnie:

| Działanie | Gest |
| --- | --- |
| Przenoszenie | Alt + przeciągnięcie lewym przyciskiem |
| Zmiana rozmiaru | Alt + przeciągnięcie prawym przyciskiem |

![Przenoszenie okna z Alt i przeciągnięciem lewym przyciskiem.](images/move-window.gif)

![Zmiana rozmiaru okna z Alt i przeciągnięciem prawym przyciskiem.](images/resize-window.gif)

Jeśli zaznaczono pliki, zaznaczenie zostaje zachowane. Esc anuluje działanie. Skróty ustawia się w **Ustawienia > Skróty**. Lewy i prawy przycisk wymagają Alt. Środkowego można używać samodzielnie.

<a id="integration"></a>

### Integracja

<div id="wyszukiwanie-i-historia"></div>
<div id="search-and-history"></div>
<div id="everything"></div>

[Everything](https://www.voidtools.com/) to narzędzie do szybkiego wyszukiwania plików i folderów po nazwie. FileSpacer pozwala uruchamiać je z pola wyszukiwania folderu. Integrację włącza się w **Ustawienia > Integracja**, z automatycznym wykrywaniem lub wyborem wersji przenośnej.

**Wyszukaj w Everything** uruchamia [wyszukiwanie](https://www.voidtools.com/support/everything/searching/) w folderze i podfolderach lub we wszystkich [zindeksowanych plikach](https://www.voidtools.com/support/everything/indexes/) po wybraniu **Wszędzie**. Wyniki otwierają się w Everything. Dla folderów wirtualnych wybrać Wszędzie. Udane uruchomienie czyści pole i lokalny filtr FileSpacer.

![Zapytanie w FileSpacer i odpowiadające mu wyniki Everything obok.](images/everything-search.png)

<div id="nilesoft-shell"></div>

[Nilesoft Shell](https://nilesoft.org/) to rozszerzenie systemu Windows do zmiany wyglądu i poleceń menu kontekstowych. Jeśli jest używane, może też obsługiwać menu FileSpacer.

FileSpacer tworzy menu paska tytułu, a następnie Nilesoft Shell stosuje własne reguły, które mają pierwszeństwo. [Dokumentacja konfiguracji](nilesoft-shell.md) opisuje wygląd i dostęp do działań FileSpacer.

Dla menu paska tytułu i paska narzędzi zalecane są [prostokątne narożniki](nilesoft-shell.md#recommended-menu-appearance).

**Menu nawigacji**

| Domyślne narożniki | Zalecane prostokątne narożniki |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu nawigacji z domyślnymi zaokrąglonymi narożnikami." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="To samo menu z prostokątnymi narożnikami." width="320"> |

**Menu główne**

| Domyślne narożniki | Zalecane prostokątne narożniki |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu główne z domyślnymi zaokrąglonymi narożnikami." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="To samo menu z prostokątnymi narożnikami." width="320"> |

[PowerToys](https://learn.microsoft.com/pl-pl/windows/powertoys/) to zestaw narzędzi Microsoft do rozszerzania i dostosowywania systemu Windows.

<div id="powertoys-peek"></div>

**Peek** wyświetla podgląd plików. W FileSpacer opcja **Podgląd klawiszem Spacja** w **Ustawienia > Integracja** otwiera go klawiszem Spacja. Peek musi być włączony w PowerToys. Minimalna wersja: 0.96 lub 0.102 dla wielu zaznaczonych elementów.

<div id="powertoys-grab-and-move"></div>

**Grab And Move** przenosi okna i zmienia ich rozmiar z obszaru zawartości. Po jego włączeniu odpowiednie gesty FileSpacer są wstrzymywane, nawet gdy PowerToys jest zamknięty. Aby je zachować, wykluczyć `FileSpacer.exe` w Grab And Move lub wyłączyć narzędzie.

<a id="updates-and-troubleshooting"></a>

## Aktualizacje i rozwiązywanie problemów

<div id="ustawienia-i-aktualizacje"></div>
<div id="settings-and-updates"></div>

**Ustawienia > Informacje** pozwala sprawdzać aktualizacje i włączyć cotygodniowe sprawdzanie, domyślnie wyłączone. Pobieranie i instalacja wymagają potwierdzenia.

<div id="dane-lokalne"></div>
<div id="local-data"></div>

Na tej samej stronie **Konserwacja > Zresetuj stan wszystkich folderów** zamyka okna i usuwa ich układy, historię oraz blokady. Pliki i ustawienia ogólne pozostają zachowane.

Resetowanie ustawień i zaawansowane rozwiązywanie problemów opisano w [notatce technicznej](../en/README.md#local-data) po angielsku.

<a id="privacy"></a>

## Prywatność

FileSpacer nie zawiera telemetrii, śledzenia użycia ani reklam. Raporty awarii nigdy nie są wysyłane automatycznie. Ich udostępnienie jest dobrowolne i może ujawnić ścieżki plików lub fragmenty pamięci.

Aktualizacje łączą się z GitHubem, który otrzymuje adres IP i wersję aplikacji, ale nie ścieżki plików ani stan folderów.

Poświadczeniami dysków sieciowych zarządza bezpośrednio Windows. FileSpacer ich nie odczytuje ani nie zapisuje.

Wyszukiwanie Everything przekazuje zapytanie i, jeśli jest ograniczone do folderu, jego ścieżkę do lokalnej aplikacji Everything.
