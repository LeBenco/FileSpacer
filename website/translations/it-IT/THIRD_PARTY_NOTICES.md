# Note sui componenti di terze parti

FileSpacer è basato su [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, distribuito con licenza GNU General Public License v3.0. Modifiche di FileSpacer: Copyright (c) 2026 Benco. La licenza completa si trova in [LICENSE](LICENSE) nei sorgenti oppure in `LICENSE.txt` nella cartella di installazione. FileSpacer viene fornito senza garanzia; la ridistribuzione e la modifica sono consentite alle condizioni della licenza.

## SQLite

SQLite 3.53.4 è collegato staticamente all’applicazione ed è di pubblico dominio. I sorgenti non modificati e le informazioni sulla loro provenienza si trovano in `third_party/sqlite`. Vedi <https://sqlite.org/copyright.html>.

## Lucide e Feather

I pulsanti Su e Aggiorna usano tracciati adattati dalle icone Lucide `arrow-up` e `rotate-cw`, disegnati con Windows GDI+. La freccia è coperta anche dalla nota MIT di Feather inclusa nella licenza completa di Lucide. Geometria, spessore del tratto e orientamento dell’icona di aggiornamento sono stati adattati per FileSpacer.

Vedi [licenses/Lucide-LICENSE.txt](licenses/Lucide-LICENSE.txt) per le note ISC e MIT complete nei sorgenti, oppure `Lucide-LICENSE.txt` nella cartella di installazione. Vedi <https://lucide.dev/license>.

## Tipi di carattere e componenti di Windows

Alcune icone dell’interfaccia usano il tipo di carattere Microsoft Segoe MDL2 Assets installato con Windows. FileSpacer non lo ridistribuisce. Windows Shell, i controlli comuni, GDI+ e gli altri componenti di sistema sono forniti da Windows.

## Programma di installazione

Il programma di installazione NSIS usa il plug-in LockedList, Copyright (c) 2013 Afrow Soft Ltd, con licenza zlib. La nota completa si trova in [installer/plugins/LockedList.txt](installer/plugins/LockedList.txt) ed è inclusa come `LockedList-LICENSE.txt` nella cartella di installazione. Vedi <https://nsis.sourceforge.io/LockedList_plug-in>.

L’AppUserModelID dei collegamenti viene impostato da `ShortcutAppID.nsh` di FileSpacer usando le macro Windows COM di NSIS. Il precedente sottomodulo `nsis-shortcut-properties` non viene usato. Non vengono usati Material Design Icons né il sottoinsieme del tipo di carattere Segoe incluso in passato.

## Icona del lucchetto Small & Flat

Il simbolo del blocco dello stato della finestra (`src/res/window-state-lock.ico`) è l’icona **lock** non modificata della raccolta Small & Flat di paomedia, dedicata al pubblico dominio con [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

Raccolta originale e licenza: https://github.com/paomedia/small-n-flat

Distribuzione ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
