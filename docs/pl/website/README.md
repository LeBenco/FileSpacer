<a id="website-generation"></a>

# Generowanie witryny

Witryna łączy `README.md`, instrukcję, historię i informacje o składnikach. Prywatność i dane lokalne mają osobne sekcje instrukcji. Krótka polityka pozostaje w GitHub; dawne kotwice witryny prowadzą do instrukcji. Angielskie oryginały są źródłem witryny i dokumentacji repozytorium. Dokumenty znajdują się bezpośrednio w `docs/<language>/`, a obrazy każdej wersji językowej w jej podkatalogu `images/`. Dokumenty angielskie są w `docs/en/`; angielskie README, historia zmian i informacje o składnikach w korzeniu repozytorium pozostają źródłami angielskiej witryny. Kompilacja, testy i notatki techniczne pozostają w GitHub; linki do plików spoza witryny prowadzą do `main`. Opis Nilesoft ma wersję dla każdego języka; dokumenty techniczne są dostępne także po polsku, niderlandzku i ukraińsku.

Generator wykrywa katalogi języków z `locale.json`, czterema źródłami, polityką i opisem Nilesoft. Tworzy wybór języka nad nawigacją, używając własnych nazw i flag Unicode. Dołączona czcionka flag Noto Color Emoji na licencji OFL działa również w Windows bez usługi zewnętrznej.

`assets/favicon.ico` zawiera oryginalny obraz 16 × 16 z `src/res/FileSpacer.ico`, bez zmiany próbkowania. Synchronizuj go po regeneracji ikony. Ustawienie motywu `favicon_ico` używa go we wszystkich językach.

Kolejność i hierarchia nagłówków muszą odpowiadać angielskim. Generator normalizuje poziomy, zachowuje angielskie ID dokumentów i przepisuje linki. Zmiana języka zachowuje czytaną sekcję. Brak dokumentów, nagłówków lub lokalnych PNG/GIF przerywa generowanie. Neutralne logo jest wspólne. Terminologia i nazwy: [utrzymanie tłumaczeń](translations/README.md).

Każdy język ma osobny build Just the Docs, nawigację, indeks wyszukiwania, teksty i media z tym samym przypiętym motywem. Mała nadpisana warstwa tłumaczy pomijanie do treści i wersjonuje skrypt motywu dla odświeżania wyszukiwania. Zachowaj informację MIT i przejrzyj przy zmianie motywu. README repozytorium nie ma wyboru języka ani linków językowych.

Nagłówek prowadzi tylko do repozytorium. Pobieranie i plakietki pochodzą ze źródeł Markdown; generator nie dodaje drugiego przycisku ani nie wyprowadza go z historii.

Zrzuty PNG/GIF to małe klikalne miniatury. Lokalny skrypt otwiera natywny dialog HTML z tłumaczonym zamknięciem, Escape, kliknięciem tła i przywróceniem fokusu. Bez JavaScript lub dialogu link otwiera obraz. Motyw nie ma wbudowanej przeglądarki obrazów; brak dodatkowej biblioteki i CDN. Logo SVG nie jest zrzutem. Skrót treści w URL odświeża miniatury i powiększenia po zmianie pliku.

`lunr/custom-index.js` używa przycinania słów Unicode dla tłumaczeń, bez angielskich rdzeni i słów pomijanych. Domyślne ASCII Lunr gubiłoby greckie słowa. Angielski zachowuje pierwotny pipeline. Zależność testowa odpowiada Lunr 2.3.9 motywu.

<a id="local-checks"></a>

## Kontrole lokalne

Z Node.js 22 lub nowszym, w katalogu głównym:

```cmd
npm ci --prefix website --ignore-scripts
npm test --prefix website
npm run generate --prefix website
```

`build/site-source` zawiera dane wejściowe Jekyll, nie gotową witrynę. Angielski jest w korzeniu, inne pod `_locales/<language-region>`. `locales.json` opisuje buildy i ścieżki. Zbuduj każdy przez Jekyll i połącz według `path`: angielski w korzeniu, pozostałe w `/de/`, `/el/`, `/es/`, `/fr/`, `/it/`, `/nl/`, `/pl/`, `/pt/` i `/uk/`. Wyniki są generowane i ignorowane przez Git. Kompilacja aplikacji jest niezależna.

<a id="shared-website-and-offline-builds"></a>

## Wspólne generowanie witryny i pomocy offline

`.github/scripts/build-docs.mjs` odpowiada za cały pipeline: generator, Jekyll dla wszystkich języków i zachowanie przypiętego motywu, nawigacji, wyszukiwania i mediów. Zależności Ruby tylko do kompilacji są zamrożone w `.github/scripts/Gemfile.lock`. Gotowe strony wymagają do generowania Node.js 22+, Ruby 3.3 i Bundler; odczyt nie wymaga ich.

W CMD w katalogu głównym zainstaluj zależności raz:

```cmd
npm ci --prefix website --ignore-scripts
cd .github\scripts
bundle install
cd ..\..
node .github\scripts\build-docs.mjs --target web
node .github\scripts\build-docs.mjs --target offline
```

Wyniki to `build/site` i `build/offline-docs`. Wariant offline rozwiązuje linki i zasoby względnie do HTML i osadza dane wyszukiwania w skrypcie motywu, więc działa z `file://` bez serwera. Indeks i tokenizacja pozostają te same. Zmiana loadera motywu przerywa generowanie do przeglądu adaptacji offline.

Oba workflow używają `.github/actions/build-docs` do instalacji zależności i odpowiedniego celu. Wydanie pobiera ten sam tag dla aplikacji i dokumentacji; nie pobiera opublikowanej witryny ani nie czeka na Pages.

Offline zawiera odcisk źródeł, skróty plików i języki. Pakowanie instalatora weryfikuje je przed ponownym użyciem. Skróty tekstu normalizują CRLF/LF dla transferu Linux–Windows. Brak, nieaktualność lub uszkodzenie powodują Jekyll rebuild. Generowany include NSIS instaluje dokumentację i usuwa tylko własne pliki; obce pliki pomocy pozostają.

## GitHub Pages

Pages buduje i sprawdza odpowiednie PR. Generuje źródła raz, czyta `locales.json`, buduje każdy język i wdraża. CI nie ma drugiej listy języków. Push na `main` przebudowuje przy zmianach dowolnego README, `BUILD.md`, `docs/`, tłumaczeń, ikon, generatora, wspólnej akcji lub workflow Pages. PR używają tych samych filtrów i tylko budują. Można uruchamiać ręcznie.

Pages i wydanie nie uruchamiają się wzajemnie: współdzielą `build-docs` z celami `web` i `offline`. Tagi uruchamiają tylko wydanie; pomoc offline czeka na testy i używa ich commitu. Push dokumentacji nie wydaje aplikacji; zmiany obu w jednym commicie nadal dają jeden Pages run dla push gałęzi.

Angielskie aktualizacje nie czekają na tłumaczenia. Generator zachowuje tekst i zapisane ID, zgłaszając nieaktualność adnotacjami i tabelą workflow. Rejestrowanie źródeł: [przewodnik tłumaczeń](translations/README.md).

Po wypchnięciu workflow wybierz **Settings > Pages > Source > GitHub Actions**. Adres to `https://lebenco.github.io/FileSpacer/`; `_config.yml` ustawia ścieżkę projektu dla zasobów i wyszukiwania.

Pole **About > Website** repozytorium może pokazywać ten adres. Zmiana włączonych dokumentów wymaga aktualizacji `sources` w `generate.mjs`. Filtry Pages obejmują całe `docs/` i README; dodaj filtr dla nowego wejścia spoza nich.
