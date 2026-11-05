<a id="winget-distribution"></a>

# Dystrybucja WinGet

GitHub Actions zgłasza `Benco.FileSpacer` do publicznego [`microsoft/winget-pkgs`](https://github.com/microsoft/winget-pkgs) po udanym **Publish FileSpacer** (`workflow_run`), jeżeli WinGet został jawnie wybrany przez `release.sh X.Y.Z --distribute winget`. Domyślnie nie ma automatycznej dystrybucji. Powtarzaj `--distribute`, aby wybrać kilka kanałów; wybór jest zapisany w adnotacji tagu i artefakcie `release-publication`. Brak wyboru również oznacza brak automatycznego zgłoszenia. Ręczne uruchomienie dla opublikowanego stabilnego tagu jawnie wybiera WinGet, nawet jeżeli tag pierwotnie wskazywał `none`. Dystrybucja ma osobny wynik; jej błąd nie zmienia sukcesu wydania.

<a id="one-time-setup"></a>

## Konfiguracja jednorazowa

1. Utwórz GitHub **personal access token (classic)** z **public_repo** na koncie właściciela zgłoszeń i forka `microsoft/winget-pkgs`. WingetCreate nie obsługuje tokenów fine-grained. Zobacz [instrukcję Microsoft](https://github.com/microsoft/winget-create/blob/main/doc/token.md).
2. W repozytorium FileSpacer otwórz **Settings > Secrets and variables > Actions** i dodaj sekret **WINGET_TOKEN** z tokenem. Nie zapisuj go w workflow, commicie ani logu. Odnów po wygaśnięciu lub cofnięciu.
3. Zatwierdź i wypchnij narzędzia na gałąź domyślną przed następnym tagiem. GitHub ładuje odbiorcę `workflow_run` z tej gałęzi. Działa po wydaniu z `GITHUB_TOKEN`, które samo nie wywołałoby zdarzenia `release` innego workflow.

Zwykły `GITHUB_TOKEN` nadal wystarcza do publikacji FileSpacer. Dodatkowy PAT służy wyłącznie WingetCreate do zgłoszenia do zewnętrznego repozytorium. Nie potrzeba ręcznego pierwszego pakietu: manifesty obsługują pierwsze zgłoszenie i nowe wersje.

<a id="each-release"></a>

## Każde wydanie

Przy pierwszym wkładzie Microsoft może poprosić konto zgłaszające o zaakceptowanie [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/). Właściciel konta musi przeczytać umowę i, jeśli ją akceptuje, odpowiedzieć zgodnie z instrukcją bota Microsoft w PR zgłoszenia. Udany workflow WinGet potwierdza zgłoszenie; publikacja w katalogu czeka na akceptację CLA i przegląd Microsoft.

Stosuj [procedurę wydania](build.md#releases). Po sukcesie osobny workflow WinGet pobiera `release-publication` z dokładnie tego przebiegu. Artefakt zapisuje tag także przy ręcznym wydaniu. Następnie:

1. Sprawdza stabilny `vX.Y.Z` i pomija wersje już w `winget-pkgs` lub z otwartym PR zgłoszenia.
2. Pobiera wersjonowany publiczny instalator i `FileSpacer-update.txt`. Weryfikuje rozmiar i SHA-256 z manifestem i, jeśli dostępny, skrótem zasobu GitHub. Nie buduje drugiego instalatora.
3. Generuje manifesty YAML wersji, angielskich metadanych i instalatora, zachowując je w artefakcie. Obsługuje Windows x64, `/CurrentUser`, `/AllUsers`, ciche `/S` i końcowe NSIS `/D=<INSTALLPATH>`.
4. Instaluje .NET 9, pobiera WingetCreate 1.12.13.0, sprawdza przypięty SHA-256 i podpis Microsoft Authenticode, waliduje i zgłasza przez `wingetcreate submit`. PAT pochodzi z udokumentowanej zmiennej `WINGET_CREATE_GITHUB_TOKEN`.

Instalator zachowuje domyślne składniki i zapisane wybory aktualizacji. Manifest nie żąda zmiany domyślnego menedżera. Zgłoszenie nie oznacza natychmiastowej dostępności: kontrole i przegląd Microsoft muszą przyjąć PR. Obserwuj link PR w logu i odpowiadaj na uwagi.

Błąd zgłoszenia zachowuje wydanie GitHub i sukces jego workflow; tylko dystrybucja zawodzi. Po naprawie otwórz **Actions > Submit FileSpacer to WinGet > Run workflow**, wybierz gałąź domyślną i opublikowany tag. Powtarza tylko dystrybucję, bez kompilacji i publikacji. Brak sekretu jawnie kończy się błędem po zachowaniu manifestów. Tryb ręczny obsługuje stare i zewnętrznie opublikowane wydania. Nieudane lub anulowane wydania nic nie zgłaszają. Wersje już przyjęte są pomijane, ich instalatory i manifesty nie są zastępowane; opublikowane pliki zachowaj niezmienione.

Po przyjęciu pierwszego PR i odświeżeniu katalogu:

```powershell
winget install --id Benco.FileSpacer --exact --scope user
winget install --id Benco.FileSpacer --exact --scope machine
winget upgrade --id Benco.FileSpacer --exact
winget uninstall --id Benco.FileSpacer --exact
```

<a id="validation"></a>

## Walidacja

Testy dystrybucji w CMD:

```cmd
node --test .github/scripts/winget.test.mjs
```

Obejmują generowanie manifestów, kwalifikację wydania, integralność aktualizatora/plików, oba tryby instalacji i ponowienia z symulowanym HTTP. Nie testują NSIS ani prawdziwej powłoki. Polecenie zgłoszenia sprawdza schemat na runnerze Windows GitHub; rzeczywista instalacja, aktualizacja, deinstalacja i akceptacja podlegają też przeglądowi upstream. Przed pierwszym zgłoszeniem sprawdź oba tryby w izolowanej VM z rzeczywistym wydanym instalatorem. Manifesty są dostępne jako artefakt także po błędzie zgłoszenia.
