# Αναφορά ρυθμίσεων του Nilesoft Shell

Για μια επισκόπηση, δείτε το [Nilesoft Shell στον οδηγό χρήσης](user-guide.md#nilesoft-shell).

Αν χρησιμοποιείτε το [Nilesoft Shell](https://nilesoft.org/), μπορείτε να εφαρμόσετε τα θέματα και τις προσαρμογές των μενού του και στο FileSpacer μέσω του `shell.nss` και των αρχείων ρυθμίσεων που εισάγει. Για αυτή την προαιρετική ενσωμάτωση, συνιστάται προσαρμογή των ρυθμίσεων ώστε τα μενού να ταιριάζουν οπτικά με το FileSpacer. Τα παρακάτω παραδείγματα καλύπτουν την προσθήκη, την τροποποίηση και την αφαίρεση καταχωρίσεων, τις συνθήκες για το FileSpacer και τις προτεινόμενες ορθές γωνίες.

## Περιορισμός των κανόνων για τα μενού του συστήματος

Για κανόνες που προορίζονται για την Εξερεύνηση αρχείων, την επιφάνεια εργασίας ή τη γραμμή εργασιών, προσθέστε `process.is_explorer &&` στην υπάρχουσα συνθήκη `where`. Για να εξακολουθούν να ισχύουν στις άλλες εφαρμογές αλλά να εξαιρείται το FileSpacer, χρησιμοποιήστε αντί γι’ αυτό `process.name != 'FileSpacer' &&`. Εφαρμόστε το φίλτρο στους γενικούς κανόνες `menu`, `item`, `modify` και `remove`, ακόμη και στα εισαγόμενα αρχεία.

## Εφαρμογή θεμάτων στο FileSpacer

Επιτρέψτε το FileSpacer στο `settings.exclude.where` και διατηρήστε την εισαγωγή του θέματος. Για τη συνήθη συνθήκη εξαίρεσης:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Οι ιδιότητες του θέματος μπορούν επίσης να χρησιμοποιούν εκφράσεις ανά διεργασία. Για παράδειγμα, το `dark = if(process.name == 'FileSpacer', true, sys.dark)` μέσα στο `theme { ... }` επιλέγει σκοτεινή λειτουργία για το FileSpacer και την προτίμηση του συστήματος στις άλλες εφαρμογές.

Στο FileSpacer, οι αλλαγές του θέματος ισχύουν για τα νέα μενού αφού καταργήσετε την καταχώριση του Nilesoft Shell και το καταχωρίσετε ξανά, χωρίς επανεκκίνηση της Εξερεύνησης αρχείων.

## Στόχευση συγκεκριμένου μενού

Δηλώστε αυτές τις μεταβλητές πριν από τις εισαγωγές που τις χρησιμοποιούν:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Χρησιμοποιήστε στους κανόνες τις αντίστοιχες ιδιότητες `where` και `type`:

| Μενού | `where` | `type` |
| --- | --- | --- |
| Αναπτυσσόμενο μενού της γραμμής τίτλου | `filespacer_title` | `'*'` |
| Γραμμή εργαλείων **…** | `filespacer_toolbar` | `'*'` |
| Επιλεγμένα αρχεία ή φάκελοι | `filespacer_context` | `'file\|dir'` |
| Φόντο φακέλου | `filespacer_context` | `'back'` |

Χρησιμοποιήστε `mode='multiple'` για κανόνες που πρέπει να παραμένουν διαθέσιμοι ανεξάρτητα από την κατάσταση επιλογής. Το `process.name` δεν περιλαμβάνει το `.exe`· το `window.parent.name` προσδιορίζει μια κλάση παραθύρου.

## Στόχευση υπάρχουσας καταχώρισης μενού

Ο γενικός μηχανισμός `modify(find=...)` του Nilesoft εντοπίζει μια καταχώριση από το κείμενό της. Συνδυάστε τον με το `where` για να περιορίσετε την αλλαγή σε ένα μενού· το `|` χωρίζει εναλλακτικά κείμενα και το `*` είναι χαρακτήρας μπαλαντέρ:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Αντιγραφή διαδρομής' title='Αντιγραφή διαδρομής φακέλου')
```

Δείτε την [αναφορά ρυθμίσεων του Nilesoft](https://nilesoft.org/docs/configuration/).

## Προτεινόμενη εμφάνιση μενού

Για τα μενού της γραμμής τίτλου και της γραμμής εργαλείων συνιστώνται ορθές γωνίες, ώστε να ταιριάζουν με τα κουμπιά του FileSpacer. Προσθέστε αυτή τη ρύθμιση στο υπάρχον μπλοκ `theme { ... }`:

```nss
border.radius = if(
    process.name == 'FileSpacer' &&
    (
        window.parent.name == 'FileSpacer Item Window' ||
        window.parent.name == 'FileSpacer Toolbar Host'
    ),
    0,
    default
)
```

Αν χρησιμοποιείτε ήδη προσαρμοσμένη ακτίνα, αντικαταστήστε το `default` με την προηγούμενη τιμή της.
