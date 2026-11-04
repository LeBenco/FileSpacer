<a id="regression-tests"></a>

# Testy regresji

Punktem odniesienia jest `tests.cmd` uruchamiany w CMD w katalogu głównym. Kompiluje aplikację Debug przez MSVC/NMAKE i wybiera niezależne przypadki z tego drzewa. Źródła, programy i pomocniki muszą pochodzić z tej samej rewizji. Zależności: [kompilacja](../../build.md).

<a id="run-locally"></a>

## Uruchamianie lokalne

Zamknij FileSpacer. Testy UI wymagają odblokowanego interaktywnego pulpitu Windows x64, PowerShell 5.1 i katalogu tymczasowego NTFS. Udostępnij testowi mysz i klawiaturę. Zamknij Panel sterowania przed jego testem zewnętrznym. Testy Internal nie potrzebują fizycznego wejścia, choć niektóre używają prawdziwych obiektów Windows/COM.

```cmd
tests.cmd -Category Internal
tests.cmd -Pattern "12-navigation-spatiale\*"
tests.cmd -Category UI -Pattern "16-redimensionnement-alt\03-*"
tests.cmd
```

Podczas pracy uruchamiaj nowe i odpowiednie przypadki. `tests.cmd` bez filtrów uruchamia całą aplikacyjną regresję; workflow GitHub też ją obsługuje. Nie uruchamiaj drugi raz testu natywnego już objętego wybranym zestawem.

| Opcja | Znaczenie |
| --- | --- |
| `-Category Internal\|UI\|All` | Filtr kategorii zadeklarowanej w przypadku; domyślnie `All`. |
| `-Pattern "group\case-pattern"` | Wzorzec PowerShell dla względnej ścieżki; domyślnie `*`. Łączy się z `-Category`. |
| `-Scope Application\|All` | Obie wartości wybierają zestaw aplikacji; domyślnie `All`. |
| `-TimeoutSeconds N` | Limit przypadku 30–3600 s, domyślnie 300. Przekroczenie jest błędem. |
| `-RunDirectory "path"` | Zapis/kopia raportów do nowego katalogu; brak nadpisywania. |
| `-VM "name"` | Wybrane przypadki w skonfigurowanym Hyper-V. |

Aby zobaczyć wybór **bez kompilacji i testów**, wywołaj bezpośrednio runner PowerShell. `tests.cmd` kompiluje przed przekazaniem argumentów, także `-ListOnly`.

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\regression\Run-Regression.ps1 -Category UI -Pattern "30-tri-melange\*" -ListOnly
```

JSON podaje ścieżki, kategorie i zakresy; używaj go zamiast stałej liczby testów. Niektóre testy zgodności przygotowują dane z tagów Git, więc potrzebna jest odpowiednia historia.

Runner UI czasowo utrzymuje system i ekran aktywne i wyłącza wygaszacz, przywracając stan także po błędzie. Nie zmienia planu zasilania ani nie odblokowuje Windows. Przywrócenie zapisuje `desktop-activity.json`.

<a id="run-in-hyper-v"></a>

## Uruchamianie w Hyper-V

```cmd
tests.cmd -VM AgentWin10 -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Host kompiluje aplikację i wybrane pomocniki natywne/C# przed transferem. Klient weryfikuje skróty i uruchamia te same skrypty i asercje, bez kompilacji gościa i bez przejścia na pulpit hosta. Wybór poprzedza pakowanie. Przygotowanie, pulpit i diagnostyka: [Hyper-V](../client/README.md).

<a id="run-on-github"></a>

## Uruchamianie w GitHub

GitHub Actions używa [przepływu pracy regresji](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/regression.yml) do uruchamiania tego samego zestawu testów aplikacji. Można go uruchomić na żądanie; zwykłe wypchnięcia gałęzi i pull requesty go nie wyzwalają.

W **Actions > Test FileSpacer regressions > Run workflow** wybierz `ref` (gałąź/tag/commit), `category` i `pattern`. `All` / `*` uruchamia całość. Workflow musi istnieć na gałęzi domyślnej, a wybrana rewizja zawierać potrzebny workflow i runner; testy nie są brane z innego drzewa.

Workflow raz ustala rewizję, kompiluje Debug na `windows-2022` i uruchamia przygotowane przypadki na interaktywnym pulpicie. `-CI` działa tylko na hostowanych runnerach GitHub, bez `-VM`/`-Prepared`. Ten sam użytkownik działa z ograniczonym tokenem średniej integralności; przygotowanie sprawdza `Shell.Windows()` i token. COM jest rejestrowany dla użytkownika. Nie zastępuje to Windows 10 ani innych wymaganych ekranów/dostawców.

Workflow nie publikuje instalatora ani pomocy offline. [Przepływ pracy wydania](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/release.yml) używa go jako pierwszego zadania w tym samym przebiegu, a następnie buduje dokumentację i NSIS z testowanego SHA. Udane pakowanie nie jest testem instalacji lub aktualizacji.

Podsumowanie Actions podaje SHA, liczby kategorii, kod wyjścia i pierwszy błąd. Logi, JSON, zrzuty i `regression-summary.md` są przechowywane 14 dni w `regression-<run>-<attempt>`, także po błędach. Błędy pomocnika i sprzątania unieważniają wynik mimo udanych przypadków.

<a id="find-the-relevant-cases"></a>

## Znajdowanie odpowiednich przypadków

Katalogi `cases/` grupują zachowania. Nazwy i ścieżki są stabilnymi selektorami; nie trzeba ich tłumaczyć.

| Obszar | Grupy |
| --- | --- |
| Stend i przygotowane artefakty | `00-banc` |
| SQLite, retencja, limit, widok i historia | `01`–`02`, `06`–`09` |
| Menu, czas życia, STA i wymiary | `03`–`05` |
| Manifesty i sesje aktualizacji | `10`–`11` |
| Nawigacja przestrzenna, ustawienia i układ | `12`–`14` |
| Gesty, zamykanie podczas interakcji | `15`–`17`, `24-raccourcis` |
| Zapis widoku i polecenia powłoki | `18-vue-shell-native`, `19-commandes-shell` |
| Obciążenie, powiadomienia i Panel sterowania | `20-validation-elargie` |
| Aktywacja, blokady i etykiety | `21`–`23` |
| Wyszukiwanie, historia i Everything | `25-recherche-integration` |
| Rozmieszczanie i podgląd Widok | `26-placement-fenetres`, `28-apercu-affichage` |
| Skojarzenia domyślnego menedżera | `29-menu-gestionnaire` |
| Pomoc lokalna i wybór języka | `30-documentation`, `31-langues` |
| Sortowanie mieszane i tożsamości | `30-tri-melange` |
| Menu tytułu i Peek | `33-menu-titre`, `35-apercu-peek` |

To punkty wejścia pokrycia, nie wykaz wykonanych testów. Sprawdź przygotowanie i asercje każdego przypadku przed uznaniem go za pokrycie zmiany.

<a id="native-view-persistence"></a>

### Trwały stan natywnego widoku

`18-vue-shell-native/02-Persistance.ps1` buduje produkcyjny host, ustawienia, zaznaczenie, ikony, filtr i SQLite z prawdziwym widokiem powłoki. Sprawdza odtwarzanie, odświeżanie, filtrowanie, serializację i niszczenie/odtworzenie widoku. Gotowość wynika z publicznych sygnałów nawigacji/wyliczania; watchdog może oblać test, nie ogłosić gotowość.

Samodzielna diagnostyka tego testu:

```cmd
test-native-state.cmd --diagnostic
```

Inicjalizuje MSVC x64 i używa `/Z7`, unikając wspólnego PDB. To alternatywne wywołanie tego samego testu, nie dodatkowa pełna kontrola.

<a id="external-applications"></a>

### Aplikacje zewnętrzne

Test Everything w grupie 25 wymaga zainstalowanej, wykrywalnej kopii bez otwartego interfejsu w sesji testowej. Usługa sesji 0 może działać. `FILESPACER_EVERYTHING_EXE` wybiera instalację, ale Wykryj musi ją też znajdować. Ścieżka jest zapisywana jako UTF-16 w `everything-detected.txt`.

Przypadek uruchamia prywatną kopię prawdziwego programu z własnym INI, danymi i indeksem. Everything odczytuje `run_as_admin` obok programu także z `-config`; izoluj plik startowy. Zainstalowany INI i wybór uprawnień muszą pozostać niezmienione. Asercje czytają widoczne zapytania i tożsamość wyników obu zakresów, nie samo uruchomienie. `FILESPACER_EXPECT_EVERYTHING_ABSENT=1` włącza kontrolę braku programu w przypadku 02.

Testy protokołu/uruchamiania Peek i natywnego menu nie dowodzą prawdziwego renderowania Peek lub Nilesoft. Sprawdź dane testowe i użyj rzeczywistego programu do akceptacji wizualnej. Brak wymagań nie waliduje integracji.

<a id="mixed-sorting"></a>

### Sortowanie mieszane

Grupa `30-tri-melange` sprawdza kolejność, opcje, powiadomienia, zapis tożsamości i kontrakt dostawcy. Używa wspólnych scenariuszy poleceń/nawigacji z włączonym dostawcą. Zachowuj jedną implementację gestów i asercji. Filtr, swobodne ikony, zmiana nazwy i usuwanie wyświetlanego folderu są ważnymi granicami; sama kolejność ich nie pokrywa.

<a id="add-or-change-a-case"></a>

## Dodawanie i zmiana przypadku

Umieść przypadek w `cases/NN-topic/NN-Name.ps1`. Pierwsza linia musi dokładnie brzmieć `# Test-Category: Internal` lub `# Test-Category: UI`. Używaj Windows PowerShell 5.1 i C# 5 / .NET Framework 4.8. Sprawdź parser i skompiluj pomocniki przed UI. Nie przypisuj zmiennych automatycznych PowerShell.

Wykorzystuj `common/` dla procesów, kompilacji, wejścia i wyników. Zależności muszą być jawne: przygotowanie VM analizuje skrypty, ścieżki pomocników i dokładne kombinacje argumentów. Nowa ścieżka dynamiczna lub komenda kompilacji może wymagać zmiany przygotowania. Sprawdzaj wykonanie bezpośrednie i przygotowane.

Obserwuj rzeczywisty wynik: tożsamość, efektywne ustawienie, geometrię, zaznaczenie i fokus. Sama liczba nie wystarcza. Nie powtarzaj błędnego działania, nie wymuszaj po nim fokusu ani nie zmieniaj asercji dla PASS. Wymuszone błędy uzupełniają prawdziwą powłokę, nie zastępują jej.

<a id="reports-isolation-and-diagnosis"></a>

## Raporty, izolacja i diagnostyka

Raporty lokalne są w `build/regression/<date>-<id>/`. Zachowuj razem `results.json`, `summary.json`, stdout/stderr, wyjścia aplikacji i dowody. VM dodaje dowody wdrożenia/sesji/przywrócenia; zobacz [układ raportów](../client/README.md#reports-and-diagnosis).

PASS dotyczy tylko ukończonych przypadków. FAIL, BLOCKED, NOT_RUN, brak podsumowania, timeout, awaria lub błędne sprzątanie wykluczają pełny sukces. Zachowuj osobno pierwszy błąd i błędy sprzątania. Kompilację, funkcjonalność i analizę statyczną raportuj oddzielnie.

Runner kopiuje i przywraca rejestr `/test` i testową bazę SQLite. Nigdy nie resetuj danych produkcyjnych. Między przypadkami usuwa tylko testowe tożsamości list w `Software\FileSpacer\Test\JumpLists`; w obrębie przypadku pozostają do końca. Inaczej `CommitList` / `FlushFileBuffers` może kumulować pracę między danymi testowymi. `case-fixture.json` zapisuje czyszczenie. Okna zewnętrzne identyfikuj i zamykaj osobno; nigdy nie kończ Eksploratora dla sprzątania Panelu sterowania.

Pułapki przy diagnozowaniu UI:

- UI Automation pokazuje Szczegóły jako `DataItem` w `DataGrid`.
- Przytrzymuj Ctrl fizycznie do zaobserwowania wyniku przeciągnięcia.
- Sprawdzaj faktyczny cel i współrzędne; zamiar kliku nie dowodzi dostarczenia. Dla rozłączonej konsoli VM używaj wspólnych helperów.
- Wykluczony gest rozmiaru kończ w edytorze nazwy; zwolnienie prawego przycisku poza nim może prawidłowo otworzyć menu.
- Dwukrotne kliknięcie może zamknąć źródło; ustaw wymaganą politykę w danych testu.
- Przy błędzie Win32 206 skracaj ścieżki. `/Z7` unika współdzielonego PDB.

`set FS_SEARCH_TIMINGS=1` zapisuje rzeczywiste odczyty w `search-observations.jsonl`; 20/01 dodaje `load-phases.jsonl`. Logi nie zmieniają limitów ani asercji. Równoczesna inspekcja dostępności może znacząco zmienić czas. Porównuj z tym samym obserwatorem, danymi i środowiskiem; pomiary nie są uniwersalnymi progami. Runner nie zatrzymuje zewnętrznych narzędzi inspekcji.
