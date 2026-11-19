<a id="window-gestures"></a>

# Gesty okna

Konfigurację opisuje [instrukcja użytkownika](user-guide.md#moving-and-resizing). Implementacja zaczyna się w `WindowShortcuts.h`, `ShortcutSettingsPage.cpp`, `Settings.cpp`, `FolderWindow` i `WindowResizeGesture`.

<a id="binding-and-input-rules"></a>

## Przypisania i reguły wejścia

Domyślne gesty to Alt + przeciągnięcie lewym przyciskiem do przenoszenia i prawym do zmiany rozmiaru. Każde działanie można wyłączyć osobno. Dwa włączone działania nie mogą mieć tego samego przypisania.

| Modyfikatory | Lewy | Środkowy | Prawy |
| --- | --- | --- | --- |
| Brak, Ctrl, Shift, Ctrl + Shift | Zarezerwowane | Dozwolone | Tylko tło |
| Alt, Ctrl + Alt, Alt + Shift, Ctrl + Alt + Shift | Dozwolone | Dozwolone | Dozwolone |

Bez Alt przypisanie z prawym przyciskiem zaczyna się tylko na tle widoku; przeciąganie zastępuje tam natywne zaznaczanie przeciąganiem prawym przyciskiem. Zwolnienie bez przeciągania wraca do widoku jako kliknięcie: powłoka sama czyści zaznaczenie i otwiera menu tła. Na elemencie naciśnięcie pozostaje natywne: menu kontekstowe i przeciąganie prawym przyciskiem. Strona ustawień informuje o tym po wybraniu takiego przypisania.

Dopasuj modyfikatory dokładnie w chwili naciśnięcia przycisku. Zwolnienie modyfikatora podczas gestu nie zmienia działania. AltGr to zgłaszane przez Windows Ctrl + Alt; środkowy oznacza naciśnięcie kółka, nie przewijanie.

Pozostałe gesty działają na tle widoku i elementach bez zmiany zaznaczenia. Zanim zostanie ustalony naciśnięty element, naciśnięcie na klasycznej liście jest przekazywane do jej widocznej natywnej etykietki przez `TTM_RELAYEVENT`, tak jak przy natywnym naciśnięciu: pozostawiona etykietka zasłaniałaby element następnego kliknięcia. Edytory, pola zmiany nazwy, nagłówki, paski przewijania, narzędzi i menu zachowują natywne wejście. Escape, utrata przechwycenia i anulowanie przerywają gest. Przenoszenie lewym przyciskiem używa pętli przenoszenia Windows; pozostałe przenoszenie i cała zmiana rozmiaru — wspólnego silnika przechwycenia z natywnymi limitami i zapisem geometrii.

Dwukrotne kliknięcie i Właściwości nadal należą do powłoki. Jeśli gest zużył pierwszy klik późniejszego natywnego dwukrotnego kliknięcia, dostarcz opóźniony klik do widoku przed dwukrotnym kliknięciem. Nie zastępuj zaznaczania ani Właściwości Eksploratora ręcznym wywołaniem.

<a id="preferences-and-powertoys-precedence"></a>

## Ustawienia i pierwszeństwo PowerToys

Strona ustawień przechowuje projekt do Zastosuj/OK i odrzuca zarezerwowane lub konfliktowe przypisania. Domyślne ustawienia działania zmieniają jeden projekt; Cofnij przywraca ostatnią zastosowaną parę. Oba przypisania zapisują się atomowo jako wersjonowany DWORD `WindowShortcuts`. Nieprawidłowe dane przywracają wartości domyślne. Okna odczytują bieżącą parę na początku gestu, także między procesami.

FileSpacer nigdy nie wstrzymuje własnych gestów z powodu PowerToys Grab And Move. To narzędzie przechwytuje naciśnięcia modyfikatora z przyciskiem w niskopoziomowym haku, zanim otrzyma je aplikacja: przy tym samym przypisaniu działa samo, przy różnych przypisaniach oba pozostają aktywne. Strona Skróty pokazuje jedynie informację, gdy zapisane ustawienia PowerToys włączają Grab And Move i nie wykluczają FileSpacer (`powerToysOverridesWindowGestures()` w `Settings.cpp`); ustawienia te są odczytywane przy wyświetleniu lub aktywacji strony, nigdy przy wejściu myszy. Grab And Move ignoruje wejście wstrzykiwane, więc testy automatyczne nie mogą go sprawdzić.

<a id="tests"></a>

## Testy

```cmd
tests.cmd -Pattern "24-*"
```

Grupa 24 rozdziela walidację strony/rejestru, geometrię i anulowanie silnika przechwycenia, rzeczywiste gesty powłoki między procesami i porównanie natywnych Właściwości. Macierz sprawdza poprawne i konfliktowe pary oraz uszkodzone dane; testy samego HWND nie zastępują testów wejścia powłoki. Grupy 15–17 obejmują dalsze granice interakcji. Używaj izolowanych ustawień i bazy programu uruchamiającego, zachowując rzeczywisty wynik i kody wyjścia.
