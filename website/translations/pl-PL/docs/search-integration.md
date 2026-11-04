<a id="search-history-and-everything"></a>

# Wyszukiwanie, historia i Everything

Punkty wejścia: `SearchBox`, `FolderWindow::applyNameFilter`, `ShellViewFilter` i `Everything`. Szczegóły interakcji: [instrukcja użytkownika](user-guide.md#search-and-history).

<a id="local-filtering-and-suggestions"></a>

## Filtrowanie lokalne i sugestie

SearchBox oddziela jawne zatwierdzenie od 150-ms opóźnienia pisania. Filtr lokalny jest stosowany przez powłokę; sugestie używają nieaktywującego okna podręcznego Win32 i natywnej listy rysowanej przez właściciela. Pole zachowuje fokus. Jeden magazyn historii i jeden konstruktor wpisów obsługują Everything oraz ostatnie zapytania.

Sugestia jest zaznaczana dopiero po nawigacji klawiaturą lub naciśnięciu myszy. Samo wskazanie nie wybiera Everything; edycja usuwa poprzedni wybór. Enter bez sugestii pozostaje lokalny. Escape zamyka listę bez kasowania tekstu. Tab, utrata fokusu, ruch/dezaktywacja i zniszczenie właściciela zamykają okno. Zachowaj dostępność tekstów i granice w obszarze roboczym monitora.

Tylko udane jawne zatwierdzenie lokalne, wybór ostatniego zapytania lub uruchomienie zewnętrzne zapisuje niepuste zapytanie. Samo pisanie nie może zapełniać historii.

<a id="launching-everything"></a>

## Uruchamianie Everything

`EverythingEnabled` domyślnie wynosi false. Puste `EverythingPath` oznacza automatyczne wykrywanie; `EverythingScope` wybiera bieżący folder systemu plików i podfoldery (0) lub wszędzie (1). Wykrywanie sprawdza oba widoki rejestru, App Paths, wpisy instalacji voidtools i standardowe lokalizacje programów.

Przekazuj jeden argument `-search`. Zakres folderu dodaje ścieżkę w cudzysłowie kończącą się `\` i grupuje zapytanie przez `<…>`, zachowując priorytet OR. Zapytanie pozostaje w składni Everything. Wewnętrzne cudzysłowy używają konwencji trzech cudzysłowów Everything, odrębnej od cytowania nazwy pliku wykonywalnego przez Windows CRT. `CreateProcessW` otrzymuje osobną bezwzględną ścieżkę programu i zapisywalny bufor polecenia Unicode, bez interpretera poleceń.

Utworzenie procesu potwierdza tylko uruchomienie, nie indeksowanie ani użyteczne wyniki. Po udanym uruchomieniu wyczyść filtr lokalny; przy błędzie zachowaj zapytanie i filtr oraz zgłoś błąd.

<a id="history-persistence"></a>

## Trwałe przechowywanie historii

`SearchHistoryLimit` domyślnie wynosi 5 i przyjmuje 0–20. Tabela SQLite `search_history(sequence,key,query)` jest odrębna od stanu widoku i historii zamkniętych okien. Numery sekwencyjne zapewniają kolejność także w tej samej sekundzie. Łączenie zapisów stanu opóźnia zapis; normalne zamknięcie go kończy.

Zmiana limitu transakcyjnie skraca historię każdego folderu, także zamkniętego. Powiadomienia niosą zastosowany limit i przestrzeń zwykłą/testową, więc opóźnione komunikaty nie odtwarzają wyczyszczonej pamięci podręcznej ani nie zmieniają danych produkcyjnych z okna testowego.

Historia podąża za zmianą tożsamości ścieżki. Wyzwalacz usunięcia `folder_state` usuwa ją po potwierdzonym usunięciu, uprawnionym wygaśnięciu, resecie lub przekroczeniu limitu. Zapis wymaga istniejącego stanu folderu i nie może odtworzyć wygasłego folderu. Sam błąd dostępu nie jest usunięciem; obowiązuje polityka przechowywania.

<a id="everything-icon-loading"></a>

## Ładowanie ikony Everything

Sugestia używa głównej ikony skonfigurowanego programu, nie dołączonego logo. Zadanie puli wątków Windows ustala ścieżkę, odczytuje metadane i mapuje zasoby przez `LoadLibraryExW` z flagami danych/obrazów zasobów. Nie wykonuje importów modułu. `LoadIconWithScaleDown` dobiera obraz do DPI; natychmiast zwolnij moduł zasobów i zachowaj tylko posiadany `HICON`.

Każde kolejne otwarcie sprawdza w tle ścieżkę, czas utworzenia/modyfikacji i rozmiar. Zmiany ustawień/DPI unieważniają pamięć podręczną pola. Aktualizacja zachowująca wszystkie te wartości nie jest wykrywana. Rysowanie używa nieblokującej blokady pamięci podręcznej; brak ikony nie blokuje działania. Zamknięcie pola anuluje publikację bez oczekiwania na zadanie; współdzielony stan i uchwyty pozostają ważne do jego końca.

<a id="tests-and-references"></a>

## Testy i źródła

Grupa `25-recherche-integration` rozdziela testy magazynu/argumentów, natywnego widoku i rzeczywistej aplikacji Everything. Przed testem zewnętrznym zastosuj [wymagania i izolację](../tests/regression/README.md#external-applications). Porównuj tożsamość wyświetlonych zapytań i wyników, w tym podfoldery i dopasowanie poza zakresem; utworzenie procesu ani zgodna liczba wyników nie wystarcza.

- [Opcje wiersza polecenia Everything](https://www.voidtools.com/support/everything/command_line_options/)
- [Składnia wyszukiwania Everything](https://www.voidtools.com/support/everything/searching/)
- [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
