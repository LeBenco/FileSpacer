<a id="building-filespacer-on-windows-x64"></a>

# Kompilacja FileSpacer w Windows x64

FileSpacer wymaga Windows 10 lub nowszego, x64. Używa MSVC/NMAKE i NSIS, nie CMake. Kompilację i testy uruchamiaj w CMD w katalogu głównym. Mapa kodu: [przegląd deweloperski](technical/README.md).

<a id="dependencies"></a>

## Zależności

- Visual Studio 2022 Build Tools z MSVC, ATL i Windows SDK. Program uruchamiający oczekuje `%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools`.
- Inkscape, zwykle `C:\Program Files\Inkscape\bin\inkscape.com`.
- Python z Pillow dostarczany z Inkscape dla Windows; kompilacja używa sąsiedniego `python.exe`. `ICON_PYTHON` wybiera inną instalację.
- ImageMagick z `magick` w PATH.
- NSIS, zwykle `%ProgramFiles(x86)%\NSIS\makensis.exe`, do pakowania.
- Node.js 22+, Ruby 3.3 i Bundler do pomocy offline; zobacz [witrynę i kompilacje offline](website/README.md#shared-website-and-offline-builds).

<a id="build-and-clean"></a>

## Kompilacja i czyszczenie

Zamknij FileSpacer przed kompilacją, następnie uruchom:

```cmd
build.cmd debug
build.cmd release
build.cmd installer
```

`build.cmd` inicjalizuje MSVC x64 przez `vcvars64.bat` i uruchamia NMAKE. Debug używa `FILESPACER_DEBUG`, `/MTd` i podsystemu konsoli. Release używa `/O2`, `/MT` i podsystemu Windows. C++ używa `/W4` i C++14. Nowe jednostki dodawaj do właściwej listy `OBJECTS` w `Makefile`.

| Wynik | Zastosowanie |
| --- | --- |
| `build\debug\FileSpacer.exe` | Rozwój i testy regresji. |
| `build\release\FileSpacer.exe` | Aplikacja wydania. |
| `FileSpacerFolder.dll` | Dostawca natywnego widoku sortowania mieszanego obok programu; zachowaj razem. |
| `build\FileSpacer-install.exe` | Instalator do dystrybucji. Pakowanie go nie uruchamia. |

Aby wskazać niestandardowe lokalizacje narzędzi:

```cmd
set "INKSCAPE=D:\Tools\Inkscape\bin\inkscape.com"
set "MAKENSIS=D:\Tools\NSIS\makensis.exe"
build.cmd installer
```

Niezmieniona kompilacja powinna pozostawić aktualny program. Cel instalatora zawsze pakuje ponownie artefakty Release.

```cmd
build.cmd clean
```

Clean usuwa wyniki Debug/Release, generowane ICO i instalator, zachowując źródła i dane użytkownika. Następna kompilacja odtwarza ikonę.

<a id="generated-artwork"></a>

## Generowana grafika

`generate-icon.py` tworzy dziesięć obrazów `src\res\FileSpacer.ico` z SVG i wzorców małych rozmiarów. Przy 16–48 px folder jest rysowany na siatce pikseli, wielokąty tesseraktu nadpróbkowane i zmniejszane filtrem Box. Inkscape renderuje 64–256 px. ImageMagick pakuje obrazy; generator sprawdza zdekodowane RGBA przed atomowym zastąpieniem ICO. NumPy nie jest wymagany.

Wielokąty 2D SVG są wiążące; generator nie przelicza rzutu z metadanych 4D/kamery. Zmiana sylwetki lub układu 212 × 212 wymaga też aktualizacji `FOLDER_MASTERS` / `draw_folder`. Zachowaj zgodność [favikony 16 px](website/README.md).

Pakowanie uruchamia `generate-installer-assets.py` dla strzałki pobierania, paneli bocznych, DLL zasobów i historii UTF-16. Wyniki trafiają do `build\installer-assets`, poza Git. `installer\wizard-ui.rc` definiuje wspólny układ. x86 `WizardBitmap.dll` skaluje panele przez GDI+, z mechanizmem NSIS jako rezerwą. Jest wypakowywany tylko do tymczasowego katalogu wtyczek NSIS. Sprawdzenie Debug w wierszu deweloperskim x64:

```cmd
nmake /f Makefile CFG=Debug installer-bitmap
```

## SQLite

Niezmieniony kod zbiorczy w `third_party\sqlite` jest kompilowany osobno jako C i łączony statycznie. Nie potrzeba DLL SQLite, CLI ani pobierania przy kompilacji. Zobacz [pochodzenie i opcje](https://github.com/LeBenco/FileSpacer/blob/main/third_party/sqlite/README.txt).

SQLite używa `/W3`, `_CRT_SECURE_NO_WARNINGS`, `SQLITE_OMIT_LOAD_EXTENSION` i `SQLITE_DQS=0`, zachowując domyślne bezpieczeństwo wątków i statyczną bibliotekę uruchomieniową. Release łączy z `/OPT:REF /OPT:ICF`. W wierszu deweloperskim:

```cmd
dumpbin /dependents build\release\FileSpacer.exe
```

Nie powinno być zależności `sqlite3.dll`; systemowe DLL Windows pozostają wymagane.

<a id="offline-help-and-installer-contracts"></a>

## Pomoc offline i kontrakty instalatora

Instalator pakuje generowany katalog `help`. Sprawdza odcisk źródeł i skróty plików, odtwarzając brakującą, nieaktualną lub uszkodzoną kopię. Node.js musi być w PATH (`NODE` może go zastąpić); Ruby/Bundler są potrzebne tylko do generowania. Workflow może przenieść zweryfikowaną pomoc z Linuksa do pakowania Windows z tego samego testowanego commitu. Czytanie wymaga tylko przeglądarki, bez serwera i sieci.

Instalator startuje jako `asInvoker`. Instalacja dla wszystkich uruchamia podwyższonego pomocnika przez `runas` dopiero na początku instalacji. Proces inicjujący zachowuje katalog, język i składniki, czeka na kod wyjścia i stosuje osobiste skojarzenia do użytkownika inicjującego. Pomocnik nie może zmieniać osobistego menedżera administratora. Deinstalacja dla wszystkich też wymaga podwyższenia. Anulowanie zachowuje kontrakt kodów aktualizatora.

`install-info.ini` zapisuje tryb, język i składniki. W `[Components]` wartości `StartMenu`, `ContextMenu` i `DefaultFileManager` wynoszą `0` lub `1`. Aktualizacje odtwarzają wybory; ciche zachowują efektywne skojarzenie, nawet zmienione po instalacji. Domyślny menedżer wymaga składnika menu kontekstowego.

Przekaźnik aktualizacji ustala tryb z metadanych obok działającego programu. Rejestr rezerwowy musi wskazywać dokładnie ten katalog w jednym trybie. Po sukcesie wszystkich sekcji instalator zapisuje UTF-16 `/UpdateResult=` z `[Installation]`, `Mode`, `Executable` i `Complete=1`. Przekaźnik waliduje program i tryb przed odtworzeniem okien. Anulowanie/błąd przywraca oryginał; nieprawidłowy wynik sukcesu zachowuje sesję i zgłasza błąd. Sprzątanie starych przekaźników nie usuwa oczekujących sesji. Granicę obejmuje `11-session-maj`.

Dzienniki to `%TEMP%\FileSpacer-install.log` i `%TEMP%\FileSpacer-install-machine.log` dla pomocnika. `/Log="C:\path\setup.log"` zmienia główny dziennik. Każdy przebieg zastępuje swój plik. Kod operacji plikowych 5 to kategoria instalatora, nie numer błędu Windows. Zachowuj błąd początkowy, wyjątek/stos czyszczenia i kody poleceń; logowanie nie może zmieniać wyniku operacji.

Kompilacja NSIS sprawdza pakowanie, nie rzeczywistą instalację, UAC, aktualizację ani deinstalację. Po zmianie sprawdzaj odpowiednie ścieżki zainstalowane.

<a id="releases"></a>

## Wydania

[Przepływ pracy wydania](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/release.yml) działa dla tagu `vX.Y.Z` lub ręcznie dla istniejącego tagu. Najpierw używa [przepływu pracy regresji aplikacji](https://github.com/LeBenco/FileSpacer/blob/main/.github/workflows/regression.yml) i wymaga jego powodzenia, potem buduje pomoc offline i NSIS z dokładnego testowanego SHA. Sprawdza wersję programu z tagiem i publikuje `FileSpacer-X.Y.Z-install.exe` oraz `FileSpacer-update.txt` w GitHub Releases. Zwykły push gałęzi nie publikuje instalatora. Używa automatycznego tokenu GitHub, bez własnego sekretu wydania.

Po publikacji osobny proces WinGet zgłasza instalator do katalogu społeczności. Wymaga jednorazowego sekretu `WINGET_TOKEN`; niepowodzenie nie cofa wydania GitHub. Konfiguracja, przegląd i ponowienia bez kompilacji: [dystrybucja WinGet](winget.md).

Przed publikacją zaktualizuj obie definicje wersji w `src/resource.h`, dodaj sekcję `CHANGELOG.md` i zatwierdź zamierzone zmiany. W Git Bash podstaw wersję:

```bash
bash release.sh X.Y.Z
```

Skrypt wymaga czystego drzewa, sprawdza zdalną gałąź/tag, pokazuje historię i prosi o potwierdzenie przed wypchnięciem `main` i tagu z adnotacją. Potem sprawdź workflow i oba artefakty. Podsumowania błędów i logi pozostają artefaktami workflow.

<a id="tests"></a>

## Testy

[Przewodnik regresji](tests/regression/README.md) opisuje ten sam zestaw testów aplikacji na lokalnym pulpicie Windows, w Hyper-V i w GitHub Actions, wraz z wyborem przypadków, wymaganiami i interpretacją raportów. Przykład:

```cmd
tests.cmd -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Uruchamiaj odpowiednie przypadki podczas pracy. Testy używają Debug i izolowanych danych; nigdy nie usuwaj produkcyjnej bazy dla testów. Przed zmianą odtwarzania lub czasu życia przeczytaj [architekturę powłoki](native-shell-view.md).
