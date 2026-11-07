# Historia zmian

## Nieopublikowane

- Rozmieszczanie nowych okien na podstawie normalnego rozmiaru i położenia istniejących okien FileSpacer, również zmaksymalizowanych, zminimalizowanych lub przyciągniętych do krawędzi.
- Ogromne przyspieszenie operacji „Zaznacz wszystko” i odznaczania tysięcy plików w klasycznych widokach powłoki. Optymalizacja korzysta z prywatnych funkcji systemu Windows, których dostępność i działanie nie są gwarantowane. Jeśli tych funkcji brakuje lub są niezgodne, FileSpacer automatycznie i bez komunikatów wraca do funkcji publicznych, bez korzyści z przyspieszenia.

## 1.3.2 — 2026-11-04

- Dodano język polski, niderlandzki i ukraiński do aplikacji, instalatora, witryny oraz dokumentacji użytkownika i technicznej.

## 1.3.1 — 2026-11-03

- Naprawiono otwieranie przez domyślnego menedżera, które wybierało inną aplikację zamiast FileSpacer.

## 1.3.0 — 2026-11-02

- Dodano trwałe dzienniki diagnostyczne szczegółowych operacji instalatora, błędów poleceń i pełnych wyjątków czyszczenia dokumentacji.
- Naprawiono równoczesny dostęp do stanu folderów, zachowano zmiany przy opóźnionym odtwarzaniu i ograniczono przerysowania dużych swobodnych układów ikon.
- Naprawiono przypadkowe ruchy okna i niezamierzone menu kontekstowe po przerwanych gestach.

- Dodano opcjonalny podgląd PowerToys Peek i pierwszeństwo Grab And Move dla konfliktowych gestów.
- Dodano dostosowanie menu paska tytułu w siedmiu językach, z przeciąganiem, ustawieniami domyślnymi i wyglądem natywnym lub Nilesoft Shell.
- Dodano działania zapisanego układu, dopasowanie do zawartości, zamykanie okien podrzędnych lub pozostałych, nazwane grupy i stabilne polecenia Nilesoft.
- Naprawiono błędy czyszczenia pomocy blokujące deinstalację i błędne rozpoznawanie kilku procesów Eksploratora jako osobnych sesji w testach VM.
- Naprawiono szczeliny między zielonymi ścianami ikony i odświeżono wielojęzyczne przewodniki oraz demonstracje menu.

## 1.2.3 — 2026-10-30

- Wygładzono logo panelu bocznego instalatora i deinstalatora przez wysokiej jakości skalowanie do bieżącego rozmiaru.
- Zachowano cytowane argumenty podwyższenia uprawnień, aby aktualizacja dla wszystkich uruchamiała cichego pomocnika zamiast ponownie otwierać instalator.
- Domyślnie aktualizowana jest instalacja źródłowa, a okna przywracane dokładnym programem zgłoszonym przez instalator, także przy współistnieniu instalacji osobistej i dla wszystkich.
- Naprawiono tożsamości sortowania mieszanego, nakładki i powiadomienia. Zachowano swobodne położenia ikon i widok natywny przy niedostępnym lub niezgodnym dostawcy.
- Naprawiono unikalność ustawień między procesami, walidację skrótów, ścieżki Everything, klawisze dostępu i warianty regionalne.
- Naprawiono zamykanie folderów, kliknięcia aktywacji i przywracanie podglądu menu Widok.
- Naprawiono odświeżanie list szybkiego dostępu i diagnostykę instalatora. Zachowano pliki pomocy użytkownika przy aktualizacji i deinstalacji.
- Dołączono wielojęzyczną pomoc offline przez F1 i Informacje oraz odświeżono lokalne przewodniki instalacji i Nilesoft Shell.

## 1.2.2 — 2026-10-28

- Naprawiono użycie własnego dostawcy folderu w swobodnym układzie ikon przy sortowaniu mieszanym. Swobodne rozmieszczanie używa dostawcy natywnego, a zatwierdzenie sortowania przełącza dostawcę, zachowując położenia ikon, zaznaczenie i fokus.

## 1.2.1 — 2026-10-28

- Ograniczono chwilowe ikony ogólne przy otwieraniu folderów z sortowaniem mieszanym. Widoczne ikony są przygotowywane w pamięci Windows przed pokazaniem widoku, z ograniczonym oczekiwaniem na wolne procedury obsługi.

## 1.2.0 — 2026-10-28

- Dodano opcjonalne wspólne sortowanie plików i folderów systemu plików w Wyświetlanie → Pliki, zachowując natywny widok Windows i stan folderów. Domyślnie wyłączone.
- Dodano niemiecki, włoski, hiszpański, portugalski (Portugalia) i grecki obok angielskiego i francuskiego.
- Tytuł i podtytuł instalatora używają większego tekstu Windows uwzględniającego DPI i wygładzania ClearType.
- Rozszerzono przewodnik online o angielskie i francuskie zrzuty, gesty okna i porównania menu Nilesoft Shell.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

## 1.1.10 — 2026-10-27

- Potwierdzenie Windows jest wymagane tylko przy instalacji i deinstalacji dla wszystkich użytkowników. Osobiste ustawienia dotyczą użytkownika uruchamiającego instalator.
- Pełna historia wersji jest wyświetlana w przewijanym panelu Nowości obok opcji na pierwszym ekranie instalacji.
- Naprawiono reset wyboru składników przy aktualizacji. Przywracane są zapisane wybory, a ciche aktualizacje zachowują domyślnego menedżera.
- Ujednolicono panel FileSpacer, duże logo i jasnoszaro-białą paletę wszystkich ekranów instalacji i deinstalacji. Osobna ikona instalatora łączy natywne bitmapy programu i zoptymalizowaną strzałkę pobierania dla każdego rozmiaru, zachowując cienki żółty brzeg po prawej i na dole folderu.
- Naprawiono awarie zamykania wywołane wczesną inicjalizacją danych lokalnych wątku na późnych wątkach roboczych powłoki.
- Menu folderów oferuje otwarcie w Eksploratorze plików Windows, gdy FileSpacer jest domyślny, a inaczej w FileSpacer. Synchronizacja następuje po zmianie domyślnego programu, instalacji i uruchomieniu.
- Naprawiono skróty serwerów i udziałów sieciowych otwierające Eksplorator mimo domyślnego FileSpacer. Zaktualizuj składnik menu kontekstowego folderów i ponownie wybierz FileSpacer jako domyślny.
- Powiększono ikonę w Informacjach do 48 pikseli przy skali 100%, z tekstem obok.
- Zachowano pełną widoczność tytułów zmaksymalizowanych okien i rozciągnięto pasek narzędzi na całą szerokość.
- Umożliwiono testy regresji w skonfigurowanym kliencie Hyper-V, z kompilacją aplikacji i pomocników na hoście.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

## 1.1.9 — 2026-10-27

- Dodano podgląd trybów po wskazaniu w menu Widok, stopniowe przywracanie po anulowaniu i domyślnie włączoną opcję w Wyświetlaniu.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

## 1.1.8 — 2026-10-27

- Zastąpiono ikonę i logo README rzutem tesseraktu o stosunku fioletowej do zielonej krawędzi 11/20, zachowując optymalizację małych rozmiarów.
- Uściślono angielskie i francuskie etykiety ustawień i zgrupowano opcje blokowania.
- Dodano ukrywanie kłódki zablokowanych folderów bez zmiany blokady zapisanego stanu.
- Strony ustawień i kontrolki skrótów używają natywnego tła motywu Windows.
- Rozmieszczanie okien i grupowanie paska zadań wyjaśniają standardowe etykietki narzędzi Windows obok opcji.
- Powiązano ustawienia z oknem folderu, które je otworzyło. Jego zamknięcie odrzuca oczekujące zmiany. Potwierdzona konserwacja zamyka ustawienia bez stosowania zmian.
- Uporządkowano README wokół nawigacji przestrzennej i funkcji, z badaniami, odnośnikiem do wydania, osobnymi przewodnikami kompilacji i Nilesoft oraz rozbudowaną dokumentacją.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

## 1.1.7 — 2026-10-26

- Ograniczono nakładanie nowych okien dokładną geometrią i wyszukiwaniem wolnego miejsca. Zmieniono nazwę opcji na „Minimalizuj nakładanie okien”.
- Dodano opcjonalny wybór domyślnego menedżera w angielskim i francuskim instalatorze z instrukcją późniejszej zmiany w Integracji.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

## 1.1.6 — 2026-10-26

- Naprawiono pozostawanie okien folderów po usunięciu lub przeniesieniu folderu nadrzędnego do Kosza.
- Naprawiono listy szybkiego dostępu proponujące niedostępne foldery.
- Ograniczono nakładanie folderów bez zapisanego położenia, ignorując zastępowane okno źródłowe. Włączono domyślnie w Ogólne > Nawigacja po folderach.
- Przeniesiono konserwację stanu folderów z Ogólnych do Informacji.
- Nowe okna mają proporcje 4:3 (580 × 435 przy 96 DPI), z dotychczasową wysokością i poszanowaniem ustawionych i zapisanych rozmiarów.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

## 1.1.5 — 2026-10-26

- Naprawiono niezamierzony skrót po Alt + dwukrotnym kliknięciu i ruchu myszy. Zachowano natywne Właściwości, zaznaczenie i fokus bez odtwarzania przytrzymanego naciśnięcia.
- Poprawiono testy natywnego czasu dwukrotnego kliknięcia i fizycznych celów myszy oraz porównania Właściwości, zaznaczenia, fokusu, kotwicy, geometrii i zawartości przy gestach wyłączonych i włączonych, także z Ctrl/Shift.
- Izolowano rzeczywisty program testowy Everything i jego początkowy INI, aby ustawienie administratora nie przekierowywało wyszukiwań i nie podnosiło uprawnień. Sprawdzono niezmienność ustawień instalacji.
- Podczas testów UI utrzymywany jest aktywny pulpit i czasowo wyłączany wygaszacz, z przywróceniem stanu przy sprzątaniu także po błędach.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

## 1.1.4 — 2026-10-25

To wydanie bazuje na 1.1.3, które już zapewnia konfigurowalne gesty klawiatury i myszy do przenoszenia i zmiany rozmiaru okien.

- Dodano opcjonalne wyszukiwanie zewnętrzne Everything z automatycznym wykrywaniem, ręczną ścieżką, zakresem folderu/podfolderów lub wszędzie i jawnymi błędami uruchomienia.
- Everything i ostatnie wyszukiwania pojawiają się automatycznie po wejściu w istniejące pole, bez dodatkowego przycisku. Zachowano natywne wyszukiwanie, natychmiastowy filtr i historię lokalną.
- Domyślnie zapamiętywanych jest pięć wyszukiwań na folder między sesjami, od 0 do 20 niezależnie od Everything. Zachowano dane i konserwatywną politykę czyszczenia historii.
- Ikona programu Everything jest wyświetlana we właściwym rozmiarze, pobierana w tle i buforowana z odświeżaniem po zmianie programu.
- Dodano izolowane testy magazynu, migracji i prawdziwej powłoki/Everything. Uodporniono czytnik testowy na kolejne zmiany filtrów.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

## 1.1.3 — 2026-10-25

- Dodano kartę Skróty do ustawiania lub wyłączania gestów przez modyfikatory i lewy, środkowy lub prawy przycisk, z bezpośrednim przechwyceniem i kontrolą konfliktów.
- Obsługa Właściwości przez Alt + dwukrotne kliknięcie pozostaje w natywnym widoku Eksploratora.
- Przywrócono pełne etykiety na nieprzezroczystym tle dla jednego zaznaczonego elementu w widoku ikon. Wielokrotny wybór zachowuje renderowanie Windows. Opcja wyjaśnia zakres.
- Dodano wyczerpujące testy konfiguracji skrótów i rzeczywistych gestów z francuskimi i angielskimi zasobami.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

## 1.1.2 — 2026-10-25

- Dodano opcjonalną blokadę zapisanej geometrii i widoku z menu tytułu. Zmiany tymczasowe pozostają możliwe, a pozycje ikon nadal są zapisywane.
- Dodano kłódkę Small & Flat na zablokowanych folderach z natywnym ICO wielu rozdzielczości, skalowaniem DPI i wysokim kontrastem.
- Globalne wyłączenie blokad wymaga potwierdzenia, usuwa blokady otwartych i zamkniętych folderów i przywraca zapis między procesami.
- Rozszerzono testy zmian blokad, błędów zapisu, odtwarzania powłoki i natywnej kłódki.
- Ponowne otwarcie okna przez Ctrl+Shift+T lub listę szybkiego dostępu przewija Szczegóły i Listę do wiersza widocznego u góry przy zamknięciu.
- Diagnostyka nadal zapisuje minizrzut i raport lokalnie. Usunięto przypisywanie awarii staremu renderowaniu nazw, automatyczną zmianę opcji i restart.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

## 1.1.1 — 2026-10-24

- Przywrócono kliknięcie aktywujące bez utraty zaznaczenia: pierwszy klik pustego tła nieaktywnego folderu tylko go aktywuje (opcja ponownie włączona).
- Naprawiono awarię przy zamknięciu folderu podczas odświeżania widoku powłoki (Odśwież, filtr nazw, polecenia Odśwież/Nowy).

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

## 1.1.0 — 2026-10-24

- Przebudowano widoki folderów na implementacje powłoki Windows przez publiczne API COM zamiast dostosowywania wewnętrznych kontrolek.
- Zachowano powiązanie stanu przestrzennego, zaznaczenia/fokusu, ikon i filtrowania z produkcyjnym SQLite i publicznymi zdarzeniami powłoki.
- Zachowano natywne polecenia plików i gesty Alt przenoszenia, rozmiaru i Właściwości FileSpacer.
- Panel sterowania otwiera się w natywnym oknie Windows z zachowaniem folderu źródłowego i zaznaczenia.
- Usunięto automatyczne zaznaczanie opuszczonego folderu przy przejściu wyżej: kolejne zaznaczenia Ctrl nie mogą przypadkowo otwierać obu folderów.
- W tym wydaniu zaakceptowano brak pełnych własnych etykiet na nieprzezroczystym tle, zachowania zaznaczenia przy aktywacji i dokładnego przewinięcia po otwarciu. Wyłączono dwa nieaktywne ustawienia, zachowując wartości.
- Uproszczono testy wokół produkcyjnej konfiguracji natywnej powłoki. Wycofano porównania profili, sondy osadzania i raporty migracji.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

## 1.0.8 — 2026-10-22

- Dodano przenoszenie Alt + lewy i zmianę rozmiaru Alt + prawy z obszaru plików z zachowaniem zaznaczenia.
- Klasyczny widok powłoki pozostaje włączony niezależnie od wyświetlania pełnej nazwy zaznaczonego elementu.
- Zastosowano motyw Nilesoft Shell do menu trybów widoku.
- Usunięto długie opóźnienia zamykania przez pasywne bufory ikon powłoki, zachowując referencje czasu życia jawnie wywołanych poleceń.
- Izolowano hostowanie widoku i rozszerzono testy Alt, ponownego wejścia menu i czasu życia powłoki/STA.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Przy otwieraniu przodka zaznaczany jest folder źródłowy, z zachowaniem wyboru w już otwartych oknach.
- Naprawiono filtr nazw po zatwierdzeniu, zmianie lub wyczyszczeniu zapytania.
- Przywrócono natywną ikonę Szybkiego dostępu i wyśrodkowano przyciski dostępu oraz konfiguracji.
- Usunięto diagnostykę tymczasową i użyto Benco w podziękowaniach.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Otwarte foldery są automatycznie odtwarzane po aktualizacji w aplikacji, z zapisanym położeniem, rozmiarem i widokiem.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Dodano asynchroniczne sprawdzanie aktualizacji w Informacjach z dostosowywanym przyciskiem instalacji i stanem.
- Aktualizacje są pobierane w osobnym anulowalnym oknie postępu, bez blokowania folderów.
- Weryfikowany jest rozmiar i SHA-256 instalatora przed łagodnym zamknięciem FileSpacer i instalacją.
- Przy każdym wydaniu publikowane są metadane pobierania instalatora.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Dodano polecenie paska tytułu zamykające otwarte okna nadrzędne.
- Zamknięte foldery otwiera menu tytułu, Ctrl+Shift+T lub lista szybkiego dostępu, przywracając widok, zaznaczenie i przewinięcie.
- Dodano foldery Szybkiego dostępu Eksploratora z natywnymi ikonami do list paska zadań.
- Polecenie ponownego otwarcia pojawia się tylko, gdy ostatni zamknięty folder nie jest otwarty, z jego nazwą i ikoną.
- Usunięto bieżące okno z listy przełączania menu tytułu.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Przywrócono ręczne i opcjonalne cotygodniowe sprawdzanie wydań GitHub FileSpacer, domyślnie wyłączone.

- Manifest aktualizacji jest publikowany obok instalatora każdego wydania.

- Zachowano natywną obsługę Alt + dwukrotne kliknięcie i Alt+Enter na folderach.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Motywy Nilesoft działają w menu folderu paska tytułu niezależnie od zaznaczenia, z natywnymi poleceniami powłoki.
- Umożliwiono osobne dostosowanie menu tytułu i paska narzędzi przez Nilesoft.
- Wyśrodkowano strzałkę rozwijania paska tytułu.
- Udokumentowano konfigurację Nilesoft, stosowanie motywów i filtrowanie menu.
- Dodano Benco do informacji o prawach autorskich.
- Proces publikacji może zastąpić instalator istniejącego wydania.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Włączono natywne minimalizowanie i maksymalizowanie oraz naprawiono przyciski tytułu zmaksymalizowanych okien.
- Stan maksymalizacji jest zapisywany osobno od zwykłej geometrii, z automatyczną migracją bazy.
- Zastosowano natywne animacje paska zadań i właściwości ponownego uruchamiania każdego folderu.
- Grupowanie paska zadań dotyczy nowo utworzonych okien. Wyjaśniono to w ustawieniach.
- Umożliwiono usuwanie otwartych folderów i ich zapisanego stanu.
- Aplikacja i pomocnik odtwarzania nie blokują już folderów swoim katalogiem roboczym.
- Zachowano bezwzględne ścieżki i tryb testowy Debug podczas odtwarzania po awarii.
- Normalne etykiety wracają natychmiast po usunięciu zaznaczenia, gdy pełne nazwy są włączone.
- Zwalniane są buforowane interfejsy powłoki po zamknięciu folderu i poprawiono powiadomienia.
- Dodano ręczną publikację wydań i numer wersji w nazwach instalatorów.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

## 1.0.0 — 2026-10-09

- Pierwsze wydanie FileSpacer dla Windows 10 lub nowszego, x64, na bazie ChromaFiler.
- Nawigacja przestrzenna z jednym oknem na folder oraz trwałym położeniem, rozmiarem, widokiem i pozycjami ikon w SQLite.
- Śledzenie przeniesień i zmiany nazw oraz aktywacja istniejącego okna przy ponownym otwarciu folderu.
- Domyślne zamykanie okna źródłowego przy otwieraniu folderu lub przodka, konfigurowalne i odwracane przez Ctrl.
- Dodano nawigację okruszkową i edytowalną ścieżkę oraz filtr nazw z opcjonalnym filtrowaniem podczas pisania.
- Konfigurowalne elementy paska narzędzi, dolny pasek stanu i grupowanie paska zadań.
- Dodano opcjonalne eksperymentalne pełne nazwy zaznaczonych elementów.
- Dodano angielski i francuski interfejs z angielskim jako zastępczym.
- Dodano odtwarzanie po awarii i polecenia resetowania ustawień lub stanu folderów.
- Usunięto kolumny Millera, wbudowany edytor tekstu, okna podglądu i pływającą tacę.
- Dodano skrypty MSVC/NMAKE, instalator NSIS, automatyczne wydania GitHub i dokumentację projektu.

[Historia wydań](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
