# Σημειώσεις για στοιχεία τρίτων

Το FileSpacer βασίζεται στο [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, το οποίο διανέμεται με την άδεια GNU General Public License v3.0. Τροποποιήσεις του FileSpacer: Copyright (c) 2026 Benco. Η πλήρης άδεια βρίσκεται στο [LICENSE](LICENSE) στον πηγαίο κώδικα ή στο `LICENSE.txt` στον φάκελο εγκατάστασης. Το FileSpacer παρέχεται χωρίς εγγύηση· η αναδιανομή και η τροποποίηση επιτρέπονται σύμφωνα με αυτή την άδεια.

## SQLite

Το SQLite 3.53.4 συνδέεται στατικά με την εφαρμογή και διατίθεται ως κοινό κτήμα. Ο αμετάβλητος πηγαίος κώδικας και οι πληροφορίες προέλευσής του βρίσκονται στο `third_party/sqlite`. Δείτε <https://sqlite.org/copyright.html>.

## Lucide και Feather

Τα κουμπιά Επάνω και Ανανέωση χρησιμοποιούν προσαρμοσμένες διαδρομές σχεδίασης από τα εικονίδια `arrow-up` και `rotate-cw` του Lucide, σχεδιασμένες με το Windows GDI+. Το βέλος καλύπτεται επίσης από τη γνωστοποίηση MIT του Feather που περιλαμβάνεται στην πλήρη άδεια του Lucide. Η γεωμετρία, το πάχος γραμμής και ο προσανατολισμός του εικονιδίου ανανέωσης έχουν προσαρμοστεί για το FileSpacer.

Δείτε το [licenses/Lucide-LICENSE.txt](licenses/Lucide-LICENSE.txt) για τις πλήρεις γνωστοποιήσεις ISC και MIT στον πηγαίο κώδικα ή το `Lucide-LICENSE.txt` στον φάκελο εγκατάστασης. Δείτε <https://lucide.dev/license>.

## Γραμματοσειρές και στοιχεία των Windows

Ορισμένα εικονίδια της διεπαφής χρησιμοποιούν τη γραμματοσειρά Microsoft Segoe MDL2 Assets που εγκαθίσταται με τα Windows. Το FileSpacer δεν αναδιανέμει αυτή τη γραμματοσειρά. Το Windows Shell, τα κοινά στοιχεία ελέγχου, το GDI+ και τα υπόλοιπα στοιχεία συστήματος παρέχονται από τα Windows.

## Πρόγραμμα εγκατάστασης

Το πρόγραμμα εγκατάστασης NSIS χρησιμοποιεί το πρόσθετο LockedList, Copyright (c) 2013 Afrow Soft Ltd, με την άδεια zlib. Η πλήρης γνωστοποίηση βρίσκεται στο [installer/plugins/LockedList.txt](installer/plugins/LockedList.txt) και περιλαμβάνεται ως `LockedList-LICENSE.txt` στον φάκελο εγκατάστασης. Δείτε <https://nsis.sourceforge.io/LockedList_plug-in>.

Το AppUserModelID των συντομεύσεων ορίζεται από το `ShortcutAppID.nsh` του FileSpacer, χρησιμοποιώντας τις μακροεντολές Windows COM του NSIS. Το παλιό submodule `nsis-shortcut-properties` δεν χρησιμοποιείται. Δεν χρησιμοποιούνται το Material Design Icons ούτε το υποσύνολο της γραμματοσειράς Segoe που περιλαμβανόταν παλαιότερα.

## Εικονίδιο λουκέτου Small & Flat

Η ένδειξη κλειδώματος κατάστασης παραθύρου (`src/res/window-state-lock.ico`) είναι το αμετάβλητο εικονίδιο **lock** από τη συλλογή Small & Flat του paomedia, που έχει διατεθεί ως κοινό κτήμα με [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

Αρχική συλλογή και άδεια: https://github.com/paomedia/small-n-flat

Διανομή ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
