<p align="center">
    <img height="200" alt="FileSpacer" src="../../src/res/FileSpacer.svg" />
</p>

# FileSpacer

[![Pobierz najnowsze wydanie](https://img.shields.io/badge/Pobierz-najnowsze%20wydanie-blue)](https://github.com/LeBenco/FileSpacer/releases/latest) &emsp;
[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![Bez telemetrii](https://img.shields.io/badge/telemetria-brak-2ea44f)](#privacy)
[![Licencja: GPL v3](https://img.shields.io/badge/Licencja-GPLv3-blue.svg)](#licencja-i-podziękowania)

FileSpacer to menedżer plików dla Windows oparty na [paradygmacie przestrzennym](https://en.wikipedia.org/wiki/Spatial_file_manager): każdy folder ma jedno okno zapamiętujące położenie, rozmiar i ustawienia widoku. Zapewnia to [stałe punkty orientacyjne](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), dzięki którym [pamięć przestrzenna](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) pomaga natychmiast rozpoznawać znane foldery. W nawigacyjnym menedżerze to samo okno kolejno wyświetla różne foldery: samo położenie i rozmiar nie identyfikują folderu, więc trzeba spojrzeć na zawartość. Stałe punkty odniesienia dla każdego folderu mogą ograniczyć obciążenie poznawcze podczas poruszania się po plikach. Karty są niezgodne z modelem przestrzennym i nigdy nie zostaną dodane.

<img src="images/spatial-windows-pl.png" alt="Trzy nienakładające się okna folderów o różnych rozmiarach, widokach i wielkościach ikon, w tym ze swobodnym układem." width="560">

FileSpacer stawia na lekkość i przejrzystość. Aby ograniczyć gromadzenie okien, znaną wadę modelu przestrzennego, domyślnie zamyka okno źródłowe przy otwieraniu folderu. Przytrzymaj Ctrl, aby je zachować i pracować z kilkoma folderami. Interfejs celowo pozostaje dyskretny — w odróżnieniu od tych, w których między przyciskami ledwo można dostrzec plik.

<a id="system--languages"></a>**System i języki:** Windows 10 lub nowszy, x64. Dziesięć języków: angielski, francuski, niemiecki, włoski, hiszpański, portugalski (Portugalia), grecki, polski, niderlandzki i ukraiński.

<a id="privacy"></a>**Prywatność:** **Bez telemetrii, śledzenia użycia i reklam**. Sprawdzanie aktualizacji łączy się z GitHub; cotygodniowe sprawdzanie jest opcjonalne i domyślnie wyłączone. Pobieranie i instalacja wymagają potwierdzenia. [Polityka prywatności](user-guide.md#prywatność).

**Instalacja:**

- [Instalacja ręczna](https://github.com/LeBenco/FileSpacer/releases/latest): pobierz i uruchom `FileSpacer-X.Y.Z-install.exe`.
- **WinGet:** `winget install --id Benco.FileSpacer --exact`
- **Chocolatey** (terminal uruchomiony jako administrator): `choco install filespacer -y`

Pakiety WinGet i Chocolatey oczekują na zatwierdzenie w odpowiednich katalogach.

**Dokumentacja:**

[![Dokumentacja](https://img.shields.io/badge/Docs-Instrukcja-blue)](user-guide.md)
[![Dokumentacja](https://img.shields.io/badge/Docs-Historia-orange)](CHANGELOG.md)

_FileSpacer jest forkiem [ChromaFiler](https://github.com/vanjac/chromafiler), pierwotnie utworzonego przez J. van't Hooga. Szczegóły w [podziękowaniach](#licencja-i-podziękowania)._

## Funkcje

- **Jedno okno na folder**, używane ponownie, gdy folder jest otwarty. Przywracane są położenie, rozmiar, ustawienia widoku i układ ikon. [Nawigacja przestrzenna](user-guide.md#nawigacja-między-oknami).
- **Dowolne rozmieszczanie ikon**, z zapisem i przywracaniem dla każdego folderu. [Układ ikon](user-guide.md#układ-ikon).
- **Przenoszenie i zmiana rozmiaru okien** z dowolnego miejsca obszaru plików, z zachowaniem zaznaczenia. Gesty myszy są konfigurowalne; domyślne to Alt + lewy i Alt + prawy przycisk z przeciągnięciem. [Gesty okna](user-guide.md#przenoszenie-i-zmiana-rozmiaru).
- **Przełączanie otwartych folderów** z menu tytułu, zamykanie okien nadrzędnych i ponowne otwieranie ostatniego zamkniętego okna przez Ctrl+Shift+T. [Nawigacja](user-guide.md#nawigacja-między-oknami).
- **Blokowanie zapisanego układu okna**, z możliwością tymczasowych zmian położenia, rozmiaru i widoku. [Blokowanie](user-guide.md#blokowanie-zapisanego-układu).
- **Filtrowanie bieżącego folderu** i ponowne użycie ostatnich wyszukiwań. [Wyszukiwanie i historia](user-guide.md#wyszukiwanie-i-historia).
- **Zachowanie zaznaczenia przy aktywacji okna** kliknięciem tła. [Zaznaczenie i aktywacja](user-guide.md#zaznaczenie-i-aktywacja).
- **Ograniczenie nakładania okien** folderów bez zapisanego położenia. [Rozmieszczanie](user-guide.md#rozmieszczanie-okien).
- **Dostosowanie menu paska tytułu** działaniami układu i okien oraz zapisywanie nazwanych grup folderów. [Menu](user-guide.md#dostosowanie-menu-paska-tytułu) · [Grupy](user-guide.md#grupy-folderów).

## Integracja

FileSpacer można ustawić jako domyślny menedżer plików podczas instalacji lub później w ustawieniach. Pliki otwierają się w zwykłych aplikacjach Windows; foldery sieciowe korzystają z obsługi Windows. [Integracja z Windows](user-guide.md#integracja-z-windows).

**Everything** umożliwia zaawansowane wyszukiwanie z pola FileSpacer w bieżącym folderze i podfolderach lub we wszystkich indeksowanych plikach. Wyniki otwierają się w Everything. [Integracja i użycie](user-guide.md#everything).

**Nilesoft Shell** nadaje menu FileSpacer motywy i własne polecenia. Zaleca się dostosowanie konfiguracji dla spójnego wyglądu. [Integracja i konfiguracja](user-guide.md#nilesoft-shell).

## Kompilacja

[![Publikacja FileSpacer](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

Kompilacja wymaga MSVC/NMAKE, Visual Studio 2022 Build Tools z ATL i Windows SDK, Inkscape oraz ImageMagick. Instalator wymaga NSIS; SQLite jest dołączony. Zobacz [przewodnik kompilacji](build.md).

Testy regresji łączą scenariusze PowerShell z pomocnikami C++ i C# do kontroli wewnętrznych oraz rzeczywistych testów interfejsu i powłoki Windows. [Przepływ pracy regresji w GitHub Actions](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) uruchamia ten zestaw testów aplikacji na żądanie i automatycznie przed wydaniem. Te same przypadki można uruchomić przez `tests.cmd` na lokalnym pulpicie Windows lub w skonfigurowanej maszynie wirtualnej Hyper-V. [Przewodnik testów](tests/regression/README.md) opisuje wybór przypadków, wymagania i wyniki.

## Licencja i podziękowania

### Prawa autorskie i licencja
FileSpacer jest udostępniany na licencji <img align="absbottom" src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="Licencja: GPL v3">.<br>
Copyright © 2026 Benco za modyfikacje FileSpacer. Pełny tekst licencji znajduje się w pliku [LICENSE](../../LICENSE) w źródłach lub `LICENSE.txt` w folderze instalacyjnym. FileSpacer jest dostarczany bez gwarancji; redystrybucja i modyfikacje są dozwolone na warunkach tej licencji.

### Fork i podziękowania dla oryginalnego projektu
FileSpacer jest forkiem [ChromaFiler](https://github.com/vanjac/chromafiler) (oryginalny kod ChromaFiler © 2023 J. van't Hoog), którego autorem jest J. van't Hoog (vanjac). Dziękujemy vanjac za tę podstawę!

### Informacje o składnikach innych firm
Zobacz [informacje o składnikach innych firm](THIRD_PARTY_NOTICES.md) oraz [politykę prywatności](privacy-policy.md).
