<a id="third-party-notices"></a>

# Note sui componenti di terze parti

FileSpacer è basato su [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, distribuito con licenza [GNU General Public License v3.0](../../LICENSE).

<a id="installer"></a>

## NSIS

Il programma di installazione NSIS usa il plug-in LockedList, Copyright (c) 2013 Afrow Soft Ltd, con licenza zlib. La nota completa si trova in [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) ed è inclusa come `LockedList-LICENSE.txt` nella cartella di installazione. Vedi <https://nsis.sourceforge.io/LockedList_plug-in>.

L’AppUserModelID dei collegamenti viene impostato da `ShortcutAppID.nsh` di FileSpacer usando le macro Windows COM di NSIS. Il precedente sottomodulo `nsis-shortcut-properties` non viene usato. Non vengono usati Material Design Icons né il sottoinsieme del tipo di carattere Segoe incluso in passato.

<a id="sqlite"></a>

## SQLite

SQLite 3.53.4 è collegato staticamente all’applicazione ed è di pubblico dominio. I sorgenti non modificati e le informazioni sulla loro provenienza si trovano in `third_party/sqlite`. Vedi <https://sqlite.org/copyright.html>.

<a id="icons"></a>

## Icone

<div id="up-and-refresh-button-icons-lucide-and-feather"></div>

Nella barra degli strumenti, la **freccia verso l’alto** apre la **cartella superiore** e la **freccia circolare** **aggiorna il contenuto della cartella**. Le due icone sono adattate da **Lucide** (`arrow-up` e `rotate-cw`). L’icona della freccia verso l’alto proviene originariamente da **Feather**.

Vedi [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) per le note ISC e MIT complete nei sorgenti, oppure `Lucide-LICENSE.txt` nella cartella di installazione. Vedi <https://lucide.dev/license>.

<div id="small--flat-lock-icon"></div>

Le icone del lucchetto chiuso e aperto (`src/res/window-state-lock.ico` e `src/res/window-state-unlock.ico`) derivano dall’icona **lock** della raccolta Small & Flat di paomedia, dedicata al pubblico dominio con [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/). Il lucchetto aperto è una versione derivata dell’originale.

Raccolta originale e licenza: https://github.com/paomedia/small-n-flat

Distribuzione ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
