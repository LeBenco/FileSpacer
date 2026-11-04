<a id="working-on-filespacer"></a>

# Praca nad FileSpacer

FileSpacer jest natywnym przestrzennym menedżerem C++/Win32. Windows dostarcza widok i operacje plikowe; FileSpacer — zasady okien, zapis stanu i własne kontrolki. Ta dokumentacja pomaga poruszać się po kodzie, nie jest opisem API. Użycie: [instrukcja użytkownika](user-guide.md).

<a id="start-here"></a>

## Od czego zacząć

1. [Skompiluj aplikację](build.md) przez MSVC/NMAKE.
2. [Uruchom odpowiednie testy regresji](../tests/regression/README.md) lokalnie, w [dedykowanym kliencie Hyper-V](../tests/client/README.md) lub przez [przepływ pracy GitHub Actions](../tests/regression/README.md#run-on-github).
3. Przeczytaj [architekturę powłoki](native-shell-view.md) przed zmianami czasu życia, odtwarzania i tożsamości elementów.

<a id="code-map"></a>

## Mapa kodu

Wszystkie źródła aplikacji są w `src/`.

| Obszar | Punkty wejścia |
| --- | --- |
| Uruchamianie, proces i odtwarzanie | `main.cpp`, `ProcessLifetime`, `CrashRecovery` |
| Okna folderów i nawigacja przestrzenna | `ItemWindow`, `FolderWindow`, `CreateItemWindow` |
| Natywny widok plików | `ShellViewHost`, `ShellViewSettings`, `ShellViewSelection`, `ShellViewIcons`, `ShellViewFilter` |
| Tożsamość i zapis stanu | `FolderIdentity`, `FolderStateStore`, `ClosedWindows` |
| Rozmieszczanie i gesty | `WindowPlacement`, `WindowResizeGesture`, `WindowShortcuts.h` |
| Ustawienia i tłumaczenia | `Settings`, `SettingsDialog`, `ShortcutSettingsPage`, `dialog.*.rc`, `resource.*.rc` |
| Wyszukiwanie i podgląd | `SearchBox`, `Everything`, `Peek` |
| Menu tytułu i grupy | `TitleMenu.h`, `TitleMenuDialog`, `TitleMenuRenderer`, `FolderGroups.cpp` |
| Sortowanie mieszane | `MixedFolder`, `MixedFolderProvider` |
| Aktualizacje | `Update*` |

Kompilacja, instalator, testy i witryna mają osobne punkty wejścia: odpowiednio `Makefile` / `build.cmd`, `installer/`, `tests/` i `website/`. Nowe jednostki C++ dodaj do odpowiedniej listy obiektów Makefile. Teksty UI przechowuj w zasobach językowych, zachowując kodowanie.

<a id="boundaries-to-preserve"></a>

## Granice do zachowania

- Folder ma jedno okno, również między procesami FileSpacer. Tożsamość jest uzyskiwana przed widokiem powłoki. `FolderIdentity` używa natywnej tożsamości lokalnego NTFS, znormalizowanych ścieżek UNC zdalnie, ścieżek dla innych lokalnych systemów plików i nazw parsowania powłoki dla folderów wirtualnych.
- Ustawienia ogólne należą do rejestru; stan folderu do SQLite. Geometria, widok i pozycje ikon są osobno obecnymi częściami migawki. Brak danych nie usuwa znanej części. Główne połączenie bazy należy do wątku UI; czytniki tła mają własne. Transakcje zapisu muszą być krótkie i poza wywołaniami powłoki/sieci.
- Widok filtrowany lub niegotowy nie jest pełnym układem ikon i nie wolno go tak zapisywać. Blokada chroni geometrię i widok, ale pozycje ikon nadal się zapisują.
- Błąd dostępu nie dowodzi usunięcia. Zachowaj stan, chyba że usunięcie potwierdzono lub działają reguły retencji/limitu.
- Wywołania powłoki i menu modalne mogą obsłużyć komunikaty i zamknąć właściciela przed powrotem. Referencja COM utrzymuje obiekt C++, nie okno ani widok. Ponownie sprawdzaj czas życia i generację obserwacji.

<a id="focused-notes"></a>

## Notatki tematyczne

- [Wyszukiwanie, historia i Everything](search-integration.md)
- [Gesty okna](window-shortcuts.md)
- [Menu tytułu i polecenia](title-menu.md)
- [Domyślny menedżer i menu kontekstowe](context-menu-integration.md)
- [Generowanie witryny i pomocy offline](../website/README.md)
- [Utrzymanie tłumaczeń](../website/translations/README.md)

Angielski tekst jest źródłem dokumentacji technicznej; dostępne są też tłumaczenia polskie, niderlandzkie i ukraińskie. Opisuj bieżące zachowanie, ograniczenia i użyteczne punkty diagnostyczne. Pomiary, dzienniki i werdykty poszczególnych przebiegów przechowuj z artefaktami testów, nie w tych przewodnikach.
