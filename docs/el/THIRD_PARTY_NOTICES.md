<a id="third-party-notices"></a>

# Σημειώσεις για στοιχεία τρίτων

Το FileSpacer βασίζεται στο [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, το οποίο διανέμεται με την άδεια [GNU General Public License v3.0](../../LICENSE).

<a id="installer"></a>

## NSIS

Το πρόγραμμα εγκατάστασης NSIS χρησιμοποιεί το πρόσθετο LockedList, Copyright (c) 2013 Afrow Soft Ltd, με την άδεια zlib. Η πλήρης γνωστοποίηση βρίσκεται στο [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) και περιλαμβάνεται ως `LockedList-LICENSE.txt` στον φάκελο εγκατάστασης. Δείτε <https://nsis.sourceforge.io/LockedList_plug-in>.

Το AppUserModelID των συντομεύσεων ορίζεται από το `ShortcutAppID.nsh` του FileSpacer, χρησιμοποιώντας τις μακροεντολές Windows COM του NSIS. Το παλιό submodule `nsis-shortcut-properties` δεν χρησιμοποιείται. Δεν χρησιμοποιούνται το Material Design Icons ούτε το υποσύνολο της γραμματοσειράς Segoe που περιλαμβανόταν παλαιότερα.

<a id="sqlite"></a>

## SQLite

Το SQLite 3.53.4 συνδέεται στατικά με την εφαρμογή και διατίθεται ως κοινό κτήμα. Ο αμετάβλητος πηγαίος κώδικας και οι πληροφορίες προέλευσής του βρίσκονται στο `third_party/sqlite`. Δείτε <https://sqlite.org/copyright.html>.

<a id="icons"></a>

## Εικονίδια

<div id="up-and-refresh-button-icons-lucide-and-feather"></div>

Στη γραμμή εργαλείων, το **βέλος προς τα επάνω** ανοίγει τον **γονικό φάκελο**, ενώ το **κυκλικό βέλος** **ανανεώνει τα περιεχόμενα του φακέλου**. Τα δύο εικονίδια είναι προσαρμογές των εικονιδίων `arrow-up` και `rotate-cw` του **Lucide**. Το εικονίδιο με το βέλος προς τα επάνω προέρχεται αρχικά από το **Feather**.

Δείτε το [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) για τις πλήρεις γνωστοποιήσεις ISC και MIT στον πηγαίο κώδικα ή το `Lucide-LICENSE.txt` στον φάκελο εγκατάστασης. Δείτε <https://lucide.dev/license>.

<div id="small--flat-lock-icon"></div>

Τα εικονίδια κλειστού και ανοιχτού λουκέτου (`src/res/window-state-lock.ico` και `src/res/window-state-unlock.ico`) προέρχονται από το εικονίδιο **lock** της συλλογής Small & Flat του paomedia, που έχει διατεθεί ως κοινό κτήμα με [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/). Το ανοιχτό λουκέτο είναι τροποποιημένη έκδοση του αρχικού.

Αρχική συλλογή και άδεια: https://github.com/paomedia/small-n-flat

Διανομή ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
