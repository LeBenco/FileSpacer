<a id="native-shell-view-architecture"></a>

# Architektura natywnego widoku powłoki

Przeczytaj przed zmianą hostowania widoku, odtwarzania stanu lub granicy między elementami powłoki i oknami przestrzennymi. Szersza mapa: [Praca nad FileSpacer](technical/README.md).

<a id="responsibilities"></a>

## Odpowiedzialności

`ShellViewHost` osadza `CLSID_ExplorerBrowser`. Windows odpowiada za wyliczanie, renderowanie, ikony, tryby i operacje plikowe. `FolderWindow` ustala politykę nawigacji przestrzennej i koordynuje zapis. Opcje produkcyjne to `FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING`, bez wymuszania `FVO_VISTALAYOUT`; foldery wirtualne mogą wybrać początkową prezentację. `EBO_NOPERSISTVIEWSTATE` pozostawia zapis folderów pod kontrolą FileSpacer.

| Składnik | Odpowiedzialność |
| --- | --- |
| `ShellViewHost` | Czas życia przeglądarki, nawigacja, dostęp i publiczne zdarzenia. |
| `ShellViewSettings` | Odczyt/stosowanie trybu, rozmiaru ikon, flag, kolumn, sortowania i grupowania. |
| `ShellViewSelection` | Zaznaczenie, fokus i jednorazowa migawka otwarcia/przewinięcia. |
| `ShellViewIcons` | Natywne tożsamości, swobodne pozycje ikon i strażnik przerysowania partii. |
| `ShellViewFilter` | Filtr nazw przez `IFolderFilter`. |
| `FolderStateStore` | Niezależne części stanu, transakcje, blokady i retencja. |

Operacje widoku używają publicznych interfejsów COM. Dokładnie dwa wąskie wyjątki dotykają okien należących do powłoki; oba są opcjonalnymi zależnościami implementacyjnymi, które wracają do zachowania natywnego, gdy brakuje warunku wstępnego:

- Tło zaznaczonej etykiety (`ShellViewLabels.h`, opisane niżej): podklasowuje tylko publiczne okno zwracane przez `IShellView::GetWindow`, odczytuje powiadomienia `NM_CUSTOMDRAW` i udokumentowany stan ListView. Nie wyszukuje kontrolki potomnej i niczego do niej nie wysyła.
- Strażnik przerysowania ikon (`IconRedrawGuard` w `ShellViewIcons.cpp`): wokół jednej partii `SelectAndPositionItems` z zapisanymi swobodnymi pozycjami wstrzymuje wewnętrzne wznowienia `WM_SETREDRAW(TRUE)` powłoki, aby `IFolderView2::SetRedraw(FALSE)` obowiązywało przez całą partię. Wyszukiwanie ogranicza się do jednej kontrolki potomnej `WC_LISTVIEW` okna `IShellView::GetWindow`, w tym samym wątku i procesie. Widok i kontrolka muszą być widoczne, przerysowanie nie może być już wyłączone (właściwość `SysSetRedraw`) i nie może być zainstalowany wcześniejszy strażnik; w przeciwnym razie partia działa wyłącznie pod `SetRedraw`. Strażnik nigdy sam nie zmienia stanu kontrolki: `SetRedraw(FALSE)` usuwa `WS_VISIBLE` (udokumentowany skutek `WM_SETREDRAW`), `SetRedraw(TRUE)` go przywraca, a dopiero potem strażnik żąda wstrzymanego przerysowania na publicznym oknie widoku (`RedrawWindow` z `RDW_ALLCHILDREN`, jak nakazuje kontrakt `WM_SETREDRAW`). Jeśli `SetRedraw(TRUE)` się nie powiedzie, podklasa jest usuwana bez wysyłania czegokolwiek do kontrolki. Zmierzony powód: odtworzenie 5 000 pozycji trwało około 3,7 s ze strażnikiem wobec około 162 s z samym interfejsem publicznym, którego zawieszenie powłoka wznawiała wewnątrz partii. Grupa 18 obejmuje to odtwarzanie.

Nie uogólniaj żadnego z tych wyjątków na inne operacje widoku, które pozostają wywołaniami COM. Testy mogą obserwować kontrolki podczas gestów; ta instrumentacja nie jest zależnością aplikacji.

<a id="readiness-restoration-and-lifetime"></a>

## Gotowość, odtwarzanie i czas życia

Zakończenie nawigacji, utworzenie widoku i zakończenie wyliczania są odrębnymi zdarzeniami. `OnViewCreated` nie oznacza gotowości zawartości. Obserwuj publiczne sygnały; czas ani liczba elementów nie dowodzą gotowości.

Powiadomienia niosą generację obserwacji widoku. Unieważnij ją przed odpięciem lub zniszczeniem i ignoruj starsze komunikaty. Jednorazowe odtwarzanie oznacz jako zużyte przed wywołaniami COM dopuszczającymi ponowne wejście. Jeśli ustawienia wywołują ponowne wyliczanie, poczekaj na kolejne zakończenie przed odtworzeniem ikon/zaznaczenia lub zapisem.

Wywołania powłoki, menu i odświeżanie mogą przetwarzać komunikaty. Utrzymuj referencje COM i ponownie sprawdzaj istnienie okna i widoku. Referencja chroni obiekt C++, nie `HWND` ani zasoby przeglądarki. Kieruj odświeżanie i czasowniki przez `FolderWindow::refreshView()` i `invokeViewCommand()`, aby strażnicy głębokości bezpiecznie odraczali zniszczenie. Nieaktualna kontynuacja nie może odtwarzać widoku ani planować zapisu.

Geometria, widok i pozycje ikon to osobne części migawki. Błąd przechwycenia pozostawia znaną część. Filtrowane pozycje zwracają `S_FALSE`; zapisuj wyłącznie przy `S_OK`. Pusta, filtrowana lub nie w pełni wyliczona zawartość nie jest pełnym układem. Tymczasowy podgląd menu Widok również nie może trafić do trwałego stanu.

Zaznaczenie i fokus elementu są odtwarzane osobno, raz, bez przejmowania fokusu klawiatury. Czyszczenie przez `SelectItem(nullptr, SVSI_DESELECTOTHERS)` nie może zawierać `SVSI_NOTAKEFOCUS`; flaga dotyczy wskazanego elementu. Przejście wyżej nie zaznacza automatycznie źródła ani nie zastępuje zaznaczenia otwartego przodka.

<a id="mixed-filefolder-sorting"></a>

## Wspólne sortowanie plików i folderów

Po włączeniu sortowania mieszanego `MixedFolder` udostępnia złącze powłoki w lokalnych danych FileSpacer z rejestracją COM użytkownika. `FileSpacerFolder.dll` implementuje `IShellFolder2` i `IPersistFolder2`, tworząc widok przez `SHCreateShellFolderView`. Hostem pozostaje `IExplorerBrowser`. Foldery wirtualne i archiwa zachowują dostawcę Windows. Aplikację i DLL należy wdrażać razem.

Tylko korzeń dostawcy ma własną tożsamość złącza. Potomkowie zachowują niezmienione względne PIDL folderu natywnego. Wyliczanie, metadane, ikony, menu, obiekty transferu i działania delegują do folderu bazowego. Dostawca porównuje kolumny bez pierwszeństwa folderów; porównania tożsamości pozostają natywne. Tryb swobodny utrzymuje dostawcę i pozycje ikon bez wymuszania autorozmieszczania.

Prywatny kontrakt `IMixedFolder::NativeItem` przekracza granicę DLL FileSpacer. Gdy operacja poza dostawcą potrzebuje prawdziwego elementu, użyj `nativeShellItem()` / `nativeViewSelection()`. SQLite przechowuje natywne identyfikatory potomków, zachowując zaznaczenie i pozycje przy zmianie opcji. Nie opakowuj ani nie reinterpretuj PIDL. Widok subskrybuje zmiany prawdziwego korzenia przez `SFVM_GETNOTIFY`, bez powielania powiadomień złącza.

Rejestracja i złącze są izolowane w `/test`. Testy usuwają tylko własną rejestrację i katalog. Zwykła deinstalacja usuwa rejestrację należącą do FileSpacer.

<a id="selected-name-background-and-scrolling"></a>

## Tło zaznaczonej nazwy i przewijanie

`ShellViewLabels.h` opcjonalnie wypełnia nieprzezroczyste tło jednej zaznaczonej etykiety w widoku ikon. Windows nadal rysuje i rozwija tekst. Wiele zaznaczeń, sam fokus i inne tryby zachowują natywne renderowanie.

Dostosowanie podklasowuje tylko publiczne okno z `IShellView::GetWindow`, na jego wątku. Odczytuje `NM_CUSTOMDRAW` ze zweryfikowanej ListView tego widoku i używa udokumentowanych odczytów. Sprawdzaj faktyczne zaznaczenie zamiast samego `CDIS_SELECTED`. Zachowaj `DefSubclassProc` i wynik; nie dodawaj `CDRF_SKIPDEFAULT`. Callback nie ma wskaźnika FileSpacer i usuwa się na `WM_NCDESTROY`.

Ta ścieżka jest adaptacją zależną od implementacji, nie ogólnym kontraktem powłoki dla rozwijania etykiet. Gdy warunki lub powiadomienia znikną, pozostaw standardowe rysowanie. Nie rozszerzaj jej na własne renderowanie wielu etykiet ani operacje widoku.

Ponowne otwarcie Szczegółów/Listy przywraca kotwicę pierwszego widocznego wiersza, nie dokładne piksele. `ShellViewSelection` znajduje ją przez `IFolderView::Items` i pozycje w obszarze klienta pod dostępnym nagłówkiem. Odtworzenie pokazuje ostatni element, potem kotwicę przy wyłączonym przerysowaniu i bez przejmowania fokusu. Widoki ikon używają nieprzewiniętych pozycji i otwierają się u góry. `IFolderView2::GetVisibleItem` nie jest zapytaniem o widoczny obszar; nie zakładaj przewinięcia w `GetViewStateStream`.

<a id="activation-and-external-folders"></a>

## Aktywacja i foldery zewnętrzne

Przy zachowywaniu zaznaczenia zwykły lewy klik tła nieaktywnego widoku tylko go aktywuje. Pętla używa `WM_ACTIVATE` / `WA_CLICKACTIVE` i rzeczywistego celu widoku przed odrzuceniem pierwszego naciśnięcia tła. Klik tytułu nie może uzbroić odrzucania późniejszego kliku klienta. Elementy, prawy przycisk, nagłówki, przewijanie i aktywne okna zachowują normalną obsługę. `WM_MOUSEACTIVATE` ramki nie jest niezawodnym punktem przez okna powłoki.

Panel sterowania otwiera się zewnętrznie przez `IOpenControlPanel::Open`. Porównuj tożsamości powłoki, nie tłumaczone nazwy ani zdekodowane PIDL. Zachowaj folder źródłowy i zaznaczenie.

<a id="tests-and-references"></a>

## Testy i źródła

[Przewodnik regresji](tests/regression/README.md) mapuje zachowania na grupy. Zacznij od 18 dla rzeczywistego Shell/SQLite, 19 dla poleceń, 21 dla aktywacji, 23 dla etykiet, 28 dla podglądu Widok i `30-tri-melange` dla dostawcy. Testy wymuszonych awarii/czasu życia obejmują ścieżki nieweryfikowane przez sam udany rzeczywisty widok.

- [IExplorerBrowser](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorerbrowser)
- [IFolderView2](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ifolderview2)
- [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/ne-shobjidl-folderviewoptions)
- [Natywne widoki folderów](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreateshellfolderview)
- [Złącza przestrzeni nazw](https://learn.microsoft.com/en-us/windows/win32/shell/nse-junction)
- [SFVM_GETNOTIFY](https://learn.microsoft.com/en-us/windows/win32/shell/sfvm-getnotify)
- [IOpenControlPanel::Open](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iopencontrolpanel-open)
