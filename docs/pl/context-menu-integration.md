<a id="default-manager-and-context-menu-integration"></a>

# Domyślny menedżer i menu kontekstowe

`Settings.cpp` zarządza skojarzeniami folderów FileSpacer i pozycjami alternatywnego menedżera. Windows nadal wybiera i wywołuje działanie domyślne. Użycie: [integracja z Windows](user-guide.md#windows-integration).

<a id="associations-and-menu-visibility"></a>

## Skojarzenia i widoczność menu

Odczytuj efektywne skojarzenie `HKEY_CLASSES_ROOT` osobno dla `Directory`, `Drive`, `CompressedFolder`, `NetServer` i `NetShare`. Domyślny czasownik może być uporządkowaną listą rozdzielaną spacjami lub przecinkami: użyj pierwszego, zamiast porównywać cały ciąg. Korzeń serwera, np. `\\server`, to `NetServer`, a nie `Directory`; pominięcie klas sieciowych pozostawia te cele w innym menedżerze.

`setDefaultBrowser()` zmienia każde skojarzenie osobno i zgłasza błędy dla każdej klasy. `synchronizeContextMenus()` uzgadnia tylko rejestracje menu należące do FileSpacer w `Software\Classes` bieżącego użytkownika; nigdy nie zmienia domyślnego skojarzenia ani czasowników Windows. Zapewnia też nadpisania użytkownika dla polecenia zainstalowanego dla wszystkich.

Gdy FileSpacer jest domyślny, menu elementów zachowują **Otwórz w FileSpacer** i dodają **Otwórz w Eksploratorze plików Windows**. W przeciwnym razie oferują **Otwórz w FileSpacer**. Tło katalogu używa skojarzenia katalogu i pokazuje tylko alternatywny menedżer.

To rozróżnienie ma znaczenie: `CMF_DEFAULTONLY` pomija czasowniki oznaczone `ProgrammaticAccessOnly`, nawet gdy skojarzenie je wskazuje. Zachowaj widoczność czasownika **elementu** FileSpacer, aby domyślne wywołanie powłoki go wybierało. Stosuj `ProgrammaticAccessOnly` tylko do czasownika **tła**, nieużywanego do otwierania elementu. Zachowaj zarejestrowane wartości polecenia i `DelegateExecute`.

Osobny czasownik `filespacer.explorer` wywołuje `"%SystemRoot%\explorer.exe" "%1"` (`%V` dla tła). Przywrócenie domyślnego menedżera usuwa ten czasownik i nadpisanie widoczności FileSpacer. Nie przepisuj niezmienionych wartości; po rzeczywistej zmianie menu wyślij `SHCNE_ASSOCCHANGED`.

<a id="startup-settings-and-installer-boundary"></a>

## Granice uruchamiania, ustawień i instalatora

Zwykłe uruchomienie synchronizuje istniejące rejestracje; uruchomienie przenośne nic nie instaluje. Debug `/test` pomija synchronizację produkcyjną. Ustawienia integracji synchronizują po zmianach skojarzeń i zachowują osobne błędy klas. Brak zainstalowanych czasowników sieciowych wymaga aktualizacji instalacji, a nie ogłoszenia pełnego powodzenia ustawienia domyślnego.

Instalator wywołuje `/sync-context-menus` bez otwierania folderu i diagnostyki awarii, sprawdzając kod wyjścia. Usunięcie składnika/deinstalacja usuwa nadpisania FileSpacer w gałęziach użytkowników. Ani uruchomienie, ani synchronizacja nie zmieniają po cichu wyboru domyślnego menedżera.

<a id="testing-the-boundary"></a>

## Testowanie granic

Grupa `29-menu-gestionnaire` używa izolowanych uchwytów rejestru i rzeczywistego `SHCreateDefaultContextMenu`. Sprawdza domyślne wartości klas, alternatywne czasowniki, zachowanie poleceń i delegatów COM, propagację błędów i natywny wybór domyślny. Obie ścieżki `ShellExecuteEx` uruchamiają rzeczywisty podproces testowy. Rozwiązywanie skrótów sieciowych sprawdza skojarzenie celu w powłoce, nie tylko klasę `.lnk`.

Te kontrole nie stanowią akceptacji fizycznego dwukrotnego kliknięcia w Eksploratorze ani zainstalowanej aktualizacji/deinstalacji. Grupa 19 osobno otwiera i anuluje prawdziwe menu elementów i tła. Uruchamianie i interpretacja: [przewodnik regresji](tests/regression/README.md); wyniki przebiegów przechowuj w artefaktach.

Źródła: [Czasowniki powłoki](https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers),
[Tworzenie menu domyślnego](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-querycontextmenu),
[Wstępnie zdefiniowane klasy powłoki](https://learn.microsoft.com/en-us/windows/win32/shell/reg-shell-exts),
[Powiadomienie o zmianie skojarzeń](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify).
