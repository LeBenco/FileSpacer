<a id="website-translations"></a>

# Tłumaczenia witryny

Główny README i dokumentacja pozostają angielskimi źródłami. Pełne warianty Markdown zasilają witrynę i lokalne odnośniki GitHub; nie dodają wyboru ani linków językowych do README repozytorium.

Każdy katalog `docs/<language>/` zawiera `locale.json` z lokalizacją język-region oraz `README.md`, `user-guide.md`, `CHANGELOG.md` i `THIRD_PARTY_NOTICES.md`. Prywatność i konserwacja danych lokalnych mają osobne sekcje instrukcji; `privacy-policy.md` pozostaje krótką polityką GitHub. `nilesoft-shell.md` jest pełnym lokalnym opisem, do którego prowadzi instrukcja, osobno w GitHub. Brak opisu przerywa generowanie zamiast odsyłać po cichu do angielskiego. Generator wykrywa katalogi; źródła angielskie pozostają oryginalne, z etykietami `website/locale.json`.

Angielska witryna używa głównych plików README, CHANGELOG.md i THIRD_PARTY_NOTICES.md oraz `docs/en/user-guide.md`. Dokumenty techniczne są w katalogu danej wersji językowej. Pliki o zbieżnych nazwach zachowują podkatalogi: indeks techniczny to `technical/README.md`, a krótki przewodnik kompilacji to `building/BUILD.md`. Przewodniki testów i witryny zachowują swoje podkatalogi. Nazwy plików dokumentów i obrazów pozostają bez zmian.

Przy aktualizacji zachowaj kolejność i hierarchię nagłówków źródła. `source-state.json` zapisuje angielskie skróty treści i ID ostatniej synchronizacji. Angielskie zmiany są publikowane od razu. Tłumaczenia zachowują tekst i ID; nieaktualność daje ostrzeżenia i tabelę workflow zamiast blokady. Wykrywane są też zmiany samego tekstu.

Po przetłumaczeniu zmian zapisz angielskie źródło w CMD w korzeniu (podstaw język):

```cmd
node website\generate.mjs --record-translation-sources fr-FR
```

Pominięcie języka zapisuje wszystkie. Zatwierdź wygenerowane pliki stanu z Markdown. Rejestracja waliduje nagłówki i linki; nie tłumaczy ani nie ocenia aktualności znaczenia. Nie zapisuj angielskiej zmiany przed ukończeniem tłumaczenia. Brak dokumentów, zepsute linki/media i niezgodność struktury z zapisanym źródłem nadal są błędami.

Zapisane angielskie ID zachowują działanie linków po zmianie nagłówków innego języka. Linki mogą używać tłumaczonego lub oryginalnego slugu. Dokumentacja użytkownika wskazuje plik GitHub wybranego języka. Polski, niderlandzki i ukraiński mają też dokumentację techniczną poza witryną; ich linki wskazują te pliki. Inne języki zachowują źródła angielskie. Zachowuj jawne ID kotwic HTML.

Stosuj jawne odnośniki względne do własnego katalogu `images/`, zachowując istniejące nazwy i przyrostki, np. `spatial-windows-de.png`. Generator kopiuje wskazane pliki bez dobierania innej nazwy. Neutralne logo jest wspólne. Brak zrzutu lub animacji jest błędem. Nie tłumacz tekstu wewnątrz wyeksportowanych obrazów.

Każdy język jest budowany osobno z tym samym przypiętym Just the Docs, własną nawigacją, indeksem, etykietami i mediami. Workflow wykrywa macierz z `locales.json` generatora.

<a id="terminology-and-style"></a>

## Terminologia i styl

Dla pojęć systemowych stosuj terminologię lokalnej dokumentacji Windows. Dokładne ścieżki ustawień muszą zachowywać rzeczywiste etykiety FileSpacer. Dla pojęć bez nazwy Windows używaj ustalonego słownictwa oprogramowania i naturalnych zwrotów. Zachowuj sens, przykłady, linki i dyskretny humor zamiast dosłowności. Licencje, polecenia, ścieżki rejestru, identyfikatory i nazwy programów pozostają niezmienione.

Preferuj krótkie opisy zachowania i użyteczne subtelności zamiast instrukcji oczywistych kontrolek. Francuski używa neutralnych opisów lub bezokoliczników bez ciągłego zwracania się do czytelnika. To wybór redakcyjny, nie powszechna norma: Microsoft i GNOME zwracają się bezpośrednio, Everything używa krótkich poleceń angielskich, a niemiecki zwykle formy grzecznościowej. Zachowuj idiom języka, nie francuską składnię. Zrzuty umieszczaj po opisie, z opisowym tekstem alternatywnym zamiast redundantnego podpisu.

Zrzut Everything używa dostępnej wersji brazylijskiej obok FileSpacer i fikcyjnych nazw portugalskich europejskich. Everything nie oferuje portugalskiego europejskiego na oficjalnej [liście języków](https://www.voidtools.com/support/everything/supported_languages/).

| Język | Eksplorator plików | Folder | Pasek zadań | Skróty klawiaturowe |
| --- | --- | --- | --- | --- |
| Français | Explorateur de fichiers | dossier | barre des tâches | raccourcis clavier |
| Deutsch | Datei-Explorer | Ordner | Taskleiste | Tastenkombinationen |
| Italiano | Esplora file | cartella | barra delle applicazioni | scelte rapide da tastiera |
| Español | Explorador de archivos | carpeta | barra de tareas | métodos abreviados de teclado |
| Português (Portugal) | Explorador de Ficheiros | pasta | barra de tarefas | atalhos de teclado |
| Ελληνικά | Εξερεύνηση αρχείων | φάκελος | γραμμή εργασιών | συντομεύσεις πληκτρολογίου |
| Polski | Eksplorator plików | folder | pasek zadań | skróty klawiaturowe |
| Nederlands | Bestandenverkenner | map | taakbalk | sneltoetsen |
| Українська | Файловий провідник | папка | панель завдань | сполучення клавіш |

Źródła stylu: [Eksplorator plików Microsoft](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [Everything](https://www.voidtools.com/support/everything/searching/), GNOME Files ([FR](https://help.gnome.org/gnome-help/files-browse.html.fr), [DE](https://help.gnome.org/gnome-help/files-browse.html.de)). Odpowiednio: duże oprogramowanie komercyjne, małe narzędzie i dokumentacja otwarta.

Źródła terminologii:

- Francuski: [Eksplorator plików](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/fr-fr/accessibility/windows/keyboard-shortcuts-in-windows).
- Niemiecki: [Eksplorator plików](https://support.microsoft.com/de-de/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/de-de/accessibility/windows/keyboard-shortcuts-in-windows).
- Włoski: [Eksplorator plików](https://support.microsoft.com/it-it/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/it-it/accessibility/windows/keyboard-shortcuts-in-windows).
- Hiszpański: [Eksplorator plików](https://support.microsoft.com/es-es/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/es-es/accessibility/windows/keyboard-shortcuts-in-windows).
- Portugalski (Portugalia): [Eksplorator plików](https://support.microsoft.com/pt-pt/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/pt-pt/accessibility/windows/keyboard-shortcuts-in-windows).
- Grecki: [Eksplorator plików](https://support.microsoft.com/el-gr/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/el-gr/accessibility/windows/keyboard-shortcuts-in-windows).
- Polski: [Eksplorator plików](https://support.microsoft.com/pl-pl/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/pl-pl/accessibility/windows/keyboard-shortcuts-in-windows).
- Niderlandzki: [Eksplorator plików](https://support.microsoft.com/nl-nl/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/nl-nl/accessibility/windows/keyboard-shortcuts-in-windows).
- Ukraiński: [Eksplorator plików](https://support.microsoft.com/uk-ua/windows/experience/fileexplorer/file-explorer-in-windows), [skróty klawiaturowe](https://support.microsoft.com/uk-ua/accessibility/windows/keyboard-shortcuts-in-windows).
- [Poradniki stylu lokalizacji Microsoft](https://learn.microsoft.com/en-us/globalization/reference/microsoft-style-guides).
- [Programy obsługi ikon Windows po portugalsku](https://learn.microsoft.com/pt-pt/windows/win32/shell/how-to-create-icon-handlers).
- [CC0 public-domain terminology in Greek](https://creativecommons.org/publicdomain/zero/1.0/deed.el).
