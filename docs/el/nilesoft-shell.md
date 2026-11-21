<a id="nilesoft-shell-configuration-reference"></a>

# Αναφορά ρυθμίσεων του Nilesoft Shell

Για μια επισκόπηση, δείτε το [Nilesoft Shell στον οδηγό χρήσης](user-guide.md#nilesoft-shell).

Αν χρησιμοποιείτε το [Nilesoft Shell](https://nilesoft.org/), μπορείτε να εφαρμόσετε τα θέματα και τις προσαρμογές των μενού του και στο FileSpacer μέσω του `shell.nss` και των αρχείων ρυθμίσεων που εισάγει. Για αυτή την προαιρετική ενσωμάτωση, συνιστάται προσαρμογή των ρυθμίσεων ώστε τα μενού να ταιριάζουν οπτικά με το FileSpacer. Τα παρακάτω παραδείγματα καλύπτουν την προσθήκη, την τροποποίηση και την αφαίρεση καταχωρίσεων, τις συνθήκες για το FileSpacer και τις προτεινόμενες ορθές γωνίες.

<a id="scope-system-menu-rules"></a>

## Περιορισμός των κανόνων για τα μενού του συστήματος

Για κανόνες που προορίζονται για την Εξερεύνηση αρχείων, την επιφάνεια εργασίας ή τη γραμμή εργασιών, προσθέστε `process.is_explorer &&` στην υπάρχουσα συνθήκη `where`. Για να εξακολουθούν να ισχύουν στις άλλες εφαρμογές αλλά να εξαιρείται το FileSpacer, χρησιμοποιήστε αντί γι’ αυτό `process.name != 'FileSpacer' &&`. Εφαρμόστε το φίλτρο στους γενικούς κανόνες `menu`, `item`, `modify` και `remove`, ακόμη και στα εισαγόμενα αρχεία.

<a id="apply-themes-to-filespacer"></a>

## Εφαρμογή θεμάτων στο FileSpacer

Επιτρέψτε το FileSpacer στο `settings.exclude.where` και διατηρήστε την εισαγωγή του θέματος. Για τη συνήθη συνθήκη εξαίρεσης:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Οι ιδιότητες του θέματος μπορούν επίσης να χρησιμοποιούν εκφράσεις ανά διεργασία. Για παράδειγμα, το `dark = if(process.name == 'FileSpacer', true, sys.dark)` μέσα στο `theme { ... }` επιλέγει σκοτεινή λειτουργία για το FileSpacer και την προτίμηση του συστήματος στις άλλες εφαρμογές.

Μετά την αποθήκευση ενός αρχείου NSS, το **Ctrl + δεξί κλικ** στην επιφάνεια εργασίας ή στη γραμμή εργασιών αναγκάζει το Nilesoft να φορτώσει ξανά τη ρύθμισή του στον Explorer, χωρίς επανεκκίνηση του Explorer ή κατάργηση εγγραφής της επέκτασης. Το Nilesoft φορτώνει επίσης ξανά όταν εντοπίζει πατημένο το αριστερό κουμπί κατά το άνοιγμα μενού· ένα κουμπί του FileSpacer μπορεί επομένως να εφαρμόσει αμέσως τις αλλαγές. Αυτό εξαρτάται από τη χειρονομία ανοίγματος και όχι από ταχύτερο μηχανισμό επαναφόρτωσης στο FileSpacer.

<a id="menu-opening-delay"></a>

## Καθυστέρηση ανοίγματος μενού

Για να καταργήσετε την [καθυστέρηση ανοίγματος των μενού](https://nilesoft.org/docs/configuration/settings) του Nilesoft Shell, ορίστε `showdelay = 0` στο υπάρχον μπλοκ `settings` του `shell.nss`. Για να ισχύει μόνο στο FileSpacer, χρησιμοποιήστε `showdelay = if(process.name == 'FileSpacer', 0, 200)`, αντικαθιστώντας το `200` με την καθυστέρηση που θα διατηρηθεί στις άλλες εφαρμογές, σε χιλιοστά του δευτερολέπτου.

<a id="target-a-specific-menu"></a>

## Στόχευση συγκεκριμένου μενού

Δηλώστε αυτές τις μεταβλητές πριν από τις εισαγωγές που τις χρησιμοποιούν:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Χρησιμοποιήστε στους κανόνες τις αντίστοιχες ιδιότητες `where` και `type`:

| Μενού | `where` | `type` |
| --- | --- | --- |
| Αναπτυσσόμενο μενού της γραμμής τίτλου | `filespacer_title` | `'*'` |
| Γραμμή εργαλείων **…** και **Προβολή** | `filespacer_toolbar` | `'*'` |
| Μόνο **Προβολή** | `filespacer_view` | `'*'` |
| Μόνο **…** της γραμμής εργαλείων | `filespacer_more` | `'*'` |
| Επιλεγμένα αρχεία ή φάκελοι | `filespacer_context` | `'file\|dir'` |
| Φόντο φακέλου | `filespacer_context` | `'back'` |

Χρησιμοποιήστε `mode='multiple'` για κανόνες που πρέπει να παραμένουν διαθέσιμοι ανεξάρτητα από την κατάσταση επιλογής. Το `process.name` δεν περιλαμβάνει το `.exe`. Το `window.parent.name` προσδιορίζει μια κλάση παραθύρου.

Το `filespacer_toolbar` εξακολουθεί να καλύπτει και τα δύο μενού της γραμμής εργαλείων. Χρησιμοποιήστε το `filespacer_view` μόνο για την Προβολή ή το `filespacer_more` μόνο για το **…**. Ο εσωτερικός δείκτης `window.parent.title` είναι σταθερός και δεν μεταφράζεται. Για να διατηρήσετε το Terminal εκτός Προβολής, προσθέστε `!filespacer_view &&` στην υπάρχουσα συνθήκη `where`, βάζοντας τη συνθήκη σε παρενθέσεις:

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

<a id="target-an-existing-menu-entry"></a>

## Στόχευση υπάρχουσας καταχώρισης μενού

Ο γενικός μηχανισμός `modify(find=...)` του Nilesoft εντοπίζει μια καταχώριση από το κείμενό της. Συνδυάστε τον με το `where` για να περιορίσετε την αλλαγή σε ένα μενού. Το `|` χωρίζει εναλλακτικά κείμενα και το `*` είναι χαρακτήρας μπαλαντέρ:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Αντιγραφή διαδρομής' title='Αντιγραφή διαδρομής φακέλου')
```

Δείτε την [αναφορά ρυθμίσεων του Nilesoft](https://nilesoft.org/docs/configuration/).

<a id="recommended-menu-appearance"></a>

## Προτεινόμενη εμφάνιση μενού

Συνιστώνται ορθές γωνίες για τα τρία μενού: το αναπτυσσόμενο μενού της γραμμής τίτλου, το **…** και την **Προβολή**, ώστε να ταιριάζουν με τα κουμπιά του FileSpacer. Προσθέστε αυτή τη ρύθμιση στο υπάρχον μπλοκ `theme { ... }`:

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

<a id="use-filespacer-actions"></a>

## Χρήση των ενεργειών του FileSpacer

Χρησιμοποιήστε το [`window.command`](https://nilesoft.org/docs/functions/window) με ένα από τα παρακάτω αναγνωριστικά για να προσθέσετε μια ενέργεια του FileSpacer, ακόμη και αν δεν περιλαμβάνεται στη δική του σύνθεση μενού. Περιορίστε τον κανόνα στο μενού της γραμμής τίτλου με τη συνθήκη `filespacer_title` που ορίζεται παραπάνω. Ισχύουν οι συνήθεις προϋποθέσεις της ενέργειας: για παράδειγμα, η επαναφορά μιας κλειδωμένης διάταξης απαιτεί να υπάρχει ήδη κλείδωμα.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Επαναφορά της κλειδωμένης διάταξης' cmd=window.command(1123))
```

| Ενέργεια | Αναγνωριστικό |
| --- | ---: |
| Επαναφορά της κλειδωμένης διάταξης | 1123 |
| Αντικατάσταση της διάταξης αναφοράς | 1124 |
| Κλείσιμο παραθύρων υποφακέλων | 1126 |
| Κλείσιμο των άλλων παραθύρων | 1127 |
| Προσαρμογή παραθύρου στα περιεχόμενα | 1128 |
| Αποθήκευση / άνοιγμα ομάδας φακέλων | 1129 |
