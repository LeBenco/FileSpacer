<a id="changelog"></a>

# Historia zmian

<a id="150"></a>

## 1.5.0

- Zachować dowolne pozycje ikon przy przejściu do widoku Zawartość i z powrotem, również po ponownym otwarciu folderu.
- Dodać trzy wzajemnie wykluczające się tryby rozmieszczenia ikon: Automatyczne, Na siatce i Dowolne, z podglądem miejsca docelowego podczas przeciągania w trybach Automatyczne i Na siatce.
- Szybciej otwierać menu Widok z zachowaniem dostosowań Nilesoft.
- Zachować natywny układ w wierszach w widokach Kafelki i Zawartość, bez dowolnego rozmieszczania ikon, ponownego wyrównywania ani Snap.
- Dodać osiem widoków Eksploratora plików: Bardzo duże ikony, Duże ikony, Średnie ikony, Małe ikony, Lista, Szczegóły, Kafelki i Zawartość, ze skrótami Ctrl+Shift+1–8.
- Dodać polecenie **Wyrównaj ikony ponownie**, które jednorazowo wyrównuje bieżący układ, oraz **Snap** podczas przeciągania: przytrzymanie **S** wyrównuje do siatki Windows tylko przeciągane ikony, bez przesuwania pozostałych, z półprzezroczystą ramką miejsca docelowego o rozmiarze ikony.
- Odbudować bazę danych w razie uszkodzenia.
- Udostępnić polecenie „Otwórz w FileSpacer” w menu kontekstowych folderów we wszystkich programach.
- Naprawić problemy z widokiem i wprowadzaniem danych: utratę dowolnych pozycji w widoku Małe ikony, kroki Ctrl+kółko inne niż w Eksploratorze plików, automatyczne powtarzanie Backspace, nieprawidłowe wartości liczbowe w ustawieniach, punkt początkowy animacji otwierania przy różnych DPI, pozycję przewijania po ponownym otwarciu w Windows 11 oraz długie zawieszenie i rozrzucone ikony po ponownym otwarciu dużego folderu przy skali ekranu powyżej 100%.
- Zachowywać proporcje kolumn widoku Szczegóły wypełniających okno po zamknięciu i ponownym otwarciu folderu.
- Dodać opcję wyłączenia pobierania symboli Microsoft przyspieszających duże foldery i wyświetlać powiadomienie o ich pobieraniu.
- Zakończyć codzienne próby przygotowania przyspieszenia dużych folderów po trzech takich samych błędach i usuwać jego dane dla zastąpionych wersji Windows.
- Przywrócić przyspieszenie zaznaczania w dużych folderach w bieżących wersjach Windows 11, w których wyłączyła je aktualizacja Windows.
- Sprawić, by Ctrl+Backspace odwracał ustawienie okna źródłowego, tak jak Ctrl przy pozostałych sposobach otwierania folderu.
- Pokazywać pasek postępu na pasku adresu i przycisku na pasku zadań podczas wczytywania dużego lub wolno działającego folderu.

<a id="142--2026-11-12"></a>

## 1.4.2 — 2026-11-12

- Dodać natywne menu kontekstowe otwierane prawym przyciskiem myszy dla folderów w Szybkim dostępie.
- Poprawić ikonę przycisku Szybki dostęp w Windows 11.
- Dostosować kolory ikon przycisków nawigacji do motywu Windows.
- Rozszerzyć na Windows 11 znaczne przyspieszenie polecenia Zaznacz wszystko i usuwania zaznaczenia tysięcy plików w klasycznych widokach Shell.
- Naprawić zawieszanie się wszystkich okien folderów podczas długiego kopiowania metodą przeciągnij i upuść między oknami FileSpacer.
- Wyświetlać w menu kontekstowych folderów tylko polecenie „Otwórz w…” drugiego menedżera plików.
- Dodać płynną animację kropkowanego obrysu przy otwieraniu folderów, z globalnym włączaniem i wyłączaniem oraz regulacją czasu trwania w ustawieniach ogólnych.

<a id="141--2026-11-09"></a>

## 1.4.1 — 2026-11-09

- Objaśniać błędy bazy danych stanu folderów za pomocą informacji diagnostycznych, które można skopiować, dostępu do folderu danych i identyfikacji procesów, gdy jest dostępna. Gdy bazy nie można wczytać ani odzyskać, proponować resetowanie po potwierdzeniu, z pełną kopią zapasową.
- Unikać zbędnych zapisów historii wyszukiwania podczas stosowania niezmienionych preferencji, aby niedostępna baza danych stanu folderów nie blokowała tego kroku przed automatyczną aktualizacją.

- Naprawić blokowanie zmiany rozmiaru paska narzędzi po otwarciu jego menu lub Ustawień, także w widoku Szczegóły z odroczoną zmianą szerokości kolumn.

<a id="140--2026-11-09"></a>

## 1.4.0 — 2026-11-09

- Zachowywanie zapisanych swobodnych pozycji ikon, gdy Shell zgłasza zakończenie pustego wyliczania przed wczytaniem zawartości folderu.
- Zachować dostępność menu Widok i swobodnego rozmieszczania ikon, gdy zapisany stan folderu jest tymczasowo niedostępny do odczytu.
- Ładowanie ikony przycisku Szybki dostęp bezpośrednio z powłoki Windows.
- Dodać automatyczne dostosowywanie szerokości kolumn w widoku Szczegóły, z opcjonalną aktualizacją na bieżąco.
- Dodać skróty Eksploratora plików i ich dostosowania do trybu przestrzennego (Ctrl+N i Ctrl+Backspace).
- Znacznie przyspieszyć ograniczanie dużych zaznaczeń przez kliknięcie elementu lub tła w klasycznych widokach powłoki, z automatycznym i cichym powrotem do ścieżki publicznej, gdy prywatna funkcja Windows jest niedostępna.
- Przyspieszyć odczyt pełnych i dużych częściowych zaznaczeń w widokach ikon ze swobodnym rozmieszczeniem za pomocą publicznych interfejsów API systemu Windows.
- Włączać lub wyłączać miniatury globalnie w systemie Windows z menu Widok.
- Przywracać pozycję przewijania folderu po ponownym otwarciu jego okna.
- Rozmieszczanie nowych okien na podstawie normalnego rozmiaru i położenia istniejących okien FileSpacer, również zmaksymalizowanych, zminimalizowanych lub przyciągniętych do krawędzi.
- Ogromne przyspieszenie operacji „Zaznacz wszystko” i odznaczania tysięcy plików w klasycznych widokach powłoki. Optymalizacja korzysta z prywatnych funkcji systemu Windows, których dostępność i działanie nie są gwarantowane. Jeśli tych funkcji brakuje lub są niezgodne, FileSpacer automatycznie i bez komunikatów wraca do funkcji publicznych, bez korzyści z przyspieszenia.

<a id="132--2026-11-04"></a>

## 1.3.2 — 2026-11-04

- Dodać do aplikacji języki polski, niderlandzki i ukraiński.

<a id="131--2026-11-03"></a>

## 1.3.1 — 2026-11-03

- Naprawiono otwieranie przez domyślnego menedżera, które wybierało inną aplikację zamiast FileSpacer.

<a id="130--2026-11-02"></a>

## 1.3.0 — 2026-11-02

- Naprawiono równoczesny dostęp do stanu folderów, zachowano zmiany przy opóźnionym odtwarzaniu i ograniczono przerysowania dużych swobodnych układów ikon.
- Naprawiono przypadkowe ruchy okna i niezamierzone menu kontekstowe po przerwanych gestach.

- Dodano opcjonalny podgląd PowerToys Peek i pierwszeństwo Grab And Move dla konfliktowych gestów.
- Dodano dostosowanie menu paska tytułu w siedmiu językach, z przeciąganiem, ustawieniami domyślnymi i wyglądem natywnym lub Nilesoft Shell.
- Dodano działania zapisanego układu, dopasowanie do zawartości, zamykanie okien podrzędnych lub pozostałych, nazwane grupy i stabilne polecenia Nilesoft.
- Poprawić łączenia między zielonymi ścianami ikony aplikacji.

<a id="123--2026-10-30"></a>

## 1.2.3 — 2026-10-30

- Po aktualizacji przywracać okna folderów w tej samej instalacji FileSpacer.
- Naprawiono tożsamości sortowania mieszanego, nakładki i powiadomienia. Zachowano swobodne położenia ikon i widok natywny przy niedostępnym lub niezgodnym dostawcy.
- Naprawiono unikalność ustawień między procesami, walidację skrótów, ścieżki Everything, klawisze dostępu i warianty regionalne.
- Naprawiono zamykanie folderów, kliknięcia aktywacji i przywracanie podglądu menu Widok.
- Poprawić odświeżanie list szybkiego dostępu.
- Dodać wielojęzyczną pomoc offline dostępną przez F1 i okno Informacje.

<a id="122--2026-10-28"></a>

## 1.2.2 — 2026-10-28

- Naprawiono użycie własnego dostawcy folderu w swobodnym układzie ikon przy sortowaniu mieszanym. Swobodne rozmieszczanie używa dostawcy natywnego, a zatwierdzenie sortowania przełącza dostawcę, zachowując położenia ikon, zaznaczenie i fokus.

<a id="121--2026-10-28"></a>

## 1.2.1 — 2026-10-28

- Ograniczono chwilowe ikony ogólne przy otwieraniu folderów z sortowaniem mieszanym. Widoczne ikony są przygotowywane w pamięci Windows przed pokazaniem widoku, z ograniczonym oczekiwaniem na wolne procedury obsługi.

<a id="120--2026-10-28"></a>

## 1.2.0 — 2026-10-28

- Dodano opcjonalne wspólne sortowanie plików i folderów systemu plików w Wyświetlanie → Pliki, zachowując natywny widok Windows i stan folderów. Domyślnie wyłączone.
- Dodano niemiecki, włoski, hiszpański, portugalski (Portugalia) i grecki obok angielskiego i francuskiego.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

<a id="1110--2026-10-27"></a>

## 1.1.10 — 2026-10-27

- Naprawiono awarie zamykania wywołane wczesną inicjalizacją danych lokalnych wątku na późnych wątkach roboczych powłoki.
- Menu folderów oferuje otwarcie w Eksploratorze plików Windows, gdy FileSpacer jest domyślny, a inaczej w FileSpacer.
- Naprawiono skróty serwerów i udziałów sieciowych otwierające Eksplorator mimo domyślnego FileSpacer.
- Powiększono ikonę w Informacjach do 48 pikseli przy skali 100%, z tekstem obok.
- Zachowano pełną widoczność tytułów zmaksymalizowanych okien i rozciągnięto pasek narzędzi na całą szerokość.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

<a id="119--2026-10-27"></a>

## 1.1.9 — 2026-10-27

- Dodano podgląd trybów po wskazaniu w menu Widok, stopniowe przywracanie po anulowaniu i domyślnie włączoną opcję w Wyświetlaniu.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

<a id="118--2026-10-27"></a>

## 1.1.8 — 2026-10-27

- Zastąpić ikonę aplikacji projekcją tesseraktu, zachowując zoptymalizowane małe rozmiary.
- Uściślono angielskie i francuskie etykiety ustawień i zgrupowano opcje blokowania.
- Dodano ukrywanie kłódki zablokowanych folderów bez zmiany blokady zapisanego stanu.
- Strony ustawień i kontrolki skrótów używają natywnego tła motywu Windows.
- Rozmieszczanie okien i grupowanie paska zadań wyjaśniają standardowe etykietki narzędzi Windows obok opcji.
- Powiązano ustawienia z oknem folderu, które je otworzyło. Jego zamknięcie odrzuca oczekujące zmiany. Potwierdzona konserwacja zamyka ustawienia bez stosowania zmian.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

<a id="117--2026-10-26"></a>

## 1.1.7 — 2026-10-26

- Ograniczono nakładanie nowych okien dokładną geometrią i wyszukiwaniem wolnego miejsca. Zmieniono nazwę opcji na „Minimalizuj nakładanie okien”.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

<a id="116--2026-10-26"></a>

## 1.1.6 — 2026-10-26

- Naprawiono pozostawanie okien folderów po usunięciu lub przeniesieniu folderu nadrzędnego do Kosza.
- Naprawiono listy szybkiego dostępu proponujące niedostępne foldery.
- Ograniczono nakładanie folderów bez zapisanego położenia, ignorując zastępowane okno źródłowe. Włączono domyślnie w Ogólne > Nawigacja po folderach.
- Przeniesiono konserwację stanu folderów z Ogólnych do Informacji.
- Nowe okna mają proporcje 4:3 (580 × 435 przy 96 DPI), z dotychczasową wysokością i poszanowaniem ustawionych i zapisanych rozmiarów.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

<a id="115--2026-10-26"></a>

## 1.1.5 — 2026-10-26

- Naprawiono niezamierzony skrót po Alt + dwukrotnym kliknięciu i ruchu myszy. Zachowano natywne Właściwości, zaznaczenie i fokus bez odtwarzania przytrzymanego naciśnięcia.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

<a id="114--2026-10-25"></a>

## 1.1.4 — 2026-10-25

To wydanie bazuje na 1.1.3, które już zapewnia konfigurowalne gesty klawiatury i myszy do przenoszenia i zmiany rozmiaru okien.

- Dodano opcjonalne wyszukiwanie zewnętrzne Everything z automatycznym wykrywaniem, ręczną ścieżką, zakresem folderu/podfolderów lub wszędzie i jawnymi błędami uruchomienia.
- Everything i ostatnie wyszukiwania pojawiają się automatycznie po wejściu w istniejące pole, bez dodatkowego przycisku. Zachowano natywne wyszukiwanie, natychmiastowy filtr i historię lokalną.
- Domyślnie zapamiętywanych jest pięć wyszukiwań na folder między sesjami, od 0 do 20 niezależnie od Everything. Zachowano dane i konserwatywną politykę czyszczenia historii.
- Ikona programu Everything jest wyświetlana we właściwym rozmiarze, pobierana w tle i buforowana z odświeżaniem po zmianie programu.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

<a id="113--2026-10-25"></a>

## 1.1.3 — 2026-10-25

- Dodano kartę Skróty do ustawiania lub wyłączania gestów przez modyfikatory i lewy, środkowy lub prawy przycisk, z bezpośrednim przechwyceniem i kontrolą konfliktów.
- Obsługa Właściwości przez Alt + dwukrotne kliknięcie pozostaje w natywnym widoku Eksploratora.
- Przywrócono pełne etykiety na nieprzezroczystym tle dla jednego zaznaczonego elementu w widoku ikon. Wielokrotny wybór zachowuje renderowanie Windows. Opcja wyjaśnia zakres.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

<a id="112--2026-10-25"></a>

## 1.1.2 — 2026-10-25

- Dodano opcjonalną blokadę zapisanej geometrii i widoku z menu tytułu. Zmiany tymczasowe pozostają możliwe, a pozycje ikon nadal są zapisywane.
- Dodano kłódkę Small & Flat na zablokowanych folderach z natywnym ICO wielu rozdzielczości, skalowaniem DPI i wysokim kontrastem.
- Globalne wyłączenie blokad wymaga potwierdzenia, usuwa blokady otwartych i zamkniętych folderów i przywraca zapis między procesami.
- Ponowne otwarcie okna przez Ctrl+Shift+T lub listę szybkiego dostępu przewija Szczegóły i Listę do wiersza widocznego u góry przy zamknięciu.
- Diagnostyka nadal zapisuje minizrzut i raport lokalnie. Usunięto przypisywanie awarii staremu renderowaniu nazw, automatyczną zmianę opcji i restart.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

<a id="111--2026-10-24"></a>

## 1.1.1 — 2026-10-24

- Przywrócono kliknięcie aktywujące bez utraty zaznaczenia: pierwszy klik pustego tła nieaktywnego folderu tylko go aktywuje (opcja ponownie włączona).
- Naprawiono awarię przy zamknięciu folderu podczas odświeżania widoku powłoki (Odśwież, filtr nazw, polecenia Odśwież/Nowy).

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

<a id="110--2026-10-24"></a>

## 1.1.0 — 2026-10-24

- Przebudowano widoki folderów na implementacje powłoki Windows przez publiczne API COM zamiast dostosowywania wewnętrznych kontrolek.
- Zachowano powiązanie stanu przestrzennego, zaznaczenia/fokusu, ikon i filtrowania z produkcyjnym SQLite i publicznymi zdarzeniami powłoki.
- Zachowano natywne polecenia plików i gesty Alt przenoszenia, rozmiaru i Właściwości FileSpacer.
- Panel sterowania otwiera się w natywnym oknie Windows z zachowaniem folderu źródłowego i zaznaczenia.
- Usunięto automatyczne zaznaczanie opuszczonego folderu przy przejściu wyżej: kolejne zaznaczenia Ctrl nie mogą przypadkowo otwierać obu folderów.
- W tym wydaniu zaakceptowano brak pełnych własnych etykiet na nieprzezroczystym tle, zachowania zaznaczenia przy aktywacji i dokładnego przewinięcia po otwarciu. Wyłączono dwa nieaktywne ustawienia, zachowując wartości.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

<a id="108--2026-10-22"></a>

## 1.0.8 — 2026-10-22

- Dodano przenoszenie Alt + lewy i zmianę rozmiaru Alt + prawy z obszaru plików z zachowaniem zaznaczenia.
- Klasyczny widok powłoki pozostaje włączony niezależnie od wyświetlania pełnej nazwy zaznaczonego elementu.
- Zastosowano motyw Nilesoft Shell do menu trybów widoku.
- Usunięto długie opóźnienia zamykania przez pasywne bufory ikon powłoki, zachowując referencje czasu życia jawnie wywołanych poleceń.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

<a id="107--2026-10-20"></a>

## 1.0.7 — 2026-10-20

- Przy otwieraniu przodka zaznaczany jest folder źródłowy, z zachowaniem wyboru w już otwartych oknach.
- Naprawiono filtr nazw po zatwierdzeniu, zmianie lub wyczyszczeniu zapytania.
- Przywrócono natywną ikonę Szybkiego dostępu i wyśrodkowano przyciski dostępu oraz konfiguracji.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

<a id="106--2026-10-11"></a>

## 1.0.6 — 2026-10-11

- Otwarte foldery są automatycznie odtwarzane po aktualizacji w aplikacji, z zapisanym położeniem, rozmiarem i widokiem.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

<a id="105--2026-10-11"></a>

## 1.0.5 — 2026-10-11

- Dodano asynchroniczne sprawdzanie aktualizacji w Informacjach z dostosowywanym przyciskiem instalacji i stanem.
- Aktualizacje są pobierane w osobnym anulowalnym oknie postępu, bez blokowania folderów.
- Sprawdzać rozmiar i SHA-256 aktualizacji przed zamknięciem FileSpacer w celu jej zastosowania.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

<a id="104--2026-10-11"></a>

## 1.0.4 — 2026-10-11

- Dodano polecenie paska tytułu zamykające otwarte okna nadrzędne.
- Zamknięte foldery otwiera menu tytułu, Ctrl+Shift+T lub lista szybkiego dostępu, przywracając widok, zaznaczenie i przewinięcie.
- Dodano foldery Szybkiego dostępu Eksploratora z natywnymi ikonami do list paska zadań.
- Polecenie ponownego otwarcia pojawia się tylko, gdy ostatni zamknięty folder nie jest otwarty, z jego nazwą i ikoną.
- Usunięto bieżące okno z listy przełączania menu tytułu.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

<a id="103--2026-10-11"></a>

## 1.0.3 — 2026-10-11

- Przywrócono ręczne i opcjonalne cotygodniowe sprawdzanie wydań GitHub FileSpacer, domyślnie wyłączone.


- Zachowano natywną obsługę Alt + dwukrotne kliknięcie i Alt+Enter na folderach.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

<a id="102--2026-10-11"></a>

## 1.0.2 — 2026-10-11

- Motywy Nilesoft działają w menu folderu paska tytułu niezależnie od zaznaczenia, z natywnymi poleceniami powłoki.
- Umożliwiono osobne dostosowanie menu tytułu i paska narzędzi przez Nilesoft.
- Wyśrodkowano strzałkę rozwijania paska tytułu.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

<a id="101--2026-10-09"></a>

## 1.0.1 — 2026-10-09

- Włączono natywne minimalizowanie i maksymalizowanie oraz naprawiono przyciski tytułu zmaksymalizowanych okien.
- Zapisywać i przywracać stan zmaksymalizowania oddzielnie od zwykłej geometrii okna.
- Zastosowano natywne animacje paska zadań i właściwości ponownego uruchamiania każdego folderu.
- Grupowanie paska zadań dotyczy nowo utworzonych okien. Wyjaśniono to w ustawieniach.
- Umożliwiono usuwanie otwartych folderów i ich zapisanego stanu.
- Aplikacja i pomocnik odtwarzania nie blokują już folderów swoim katalogiem roboczym.
- Zachowywać bezwzględne ścieżki folderów podczas odzyskiwania po awarii.
- Normalne etykiety wracają natychmiast po usunięciu zaznaczenia, gdy pełne nazwy są włączone.
- Zwalniane są buforowane interfejsy powłoki po zamknięciu folderu i poprawiono powiadomienia.

[Pełna lista zmian](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

<a id="100--2026-10-09"></a>

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

[Historia wydań](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
