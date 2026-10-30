# Guida alla configurazione di Nilesoft Shell

Per una panoramica, consulta [Nilesoft Shell nella guida utente](user-guide.md#nilesoft-shell).

Se usi [Nilesoft Shell](https://nilesoft.org/), puoi applicare i suoi temi e le personalizzazioni dei menu anche a FileSpacer tramite `shell.nss` e i file di configurazione importati. Per questa integrazione facoltativa, è consigliabile adattare la configurazione affinché i menu si integrino visivamente con FileSpacer. Gli esempi seguenti illustrano come aggiungere, modificare o rimuovere voci, impostare condizioni per FileSpacer e usare gli angoli squadrati consigliati.

## Limitare le regole dei menu di sistema

Per le regole destinate a Esplora file, al desktop o alla barra delle applicazioni, aggiungi `process.is_explorer &&` alla condizione `where` esistente. Per mantenerle attive nelle altre applicazioni escludendo FileSpacer, usa invece `process.name != 'FileSpacer' &&`. Applica il filtro alle regole generiche `menu`, `item`, `modify` e `remove`, anche nei file importati.

## Applicare i temi a FileSpacer

Consenti FileSpacer in `settings.exclude.where` e mantieni l’importazione del tema. Per la consueta condizione di esclusione:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Anche le proprietà del tema possono usare espressioni specifiche per il processo. Ad esempio, `dark = if(process.name == 'FileSpacer', true, sys.dark)` all’interno di `theme { ... }` sceglie la modalità scura per FileSpacer e la preferenza di sistema altrove.

Con FileSpacer, le modifiche al tema si applicano ai nuovi menu dopo aver annullato e ripetuto la registrazione di Nilesoft Shell, senza riavviare Esplora file.

## Individuare un menu specifico

Dichiara queste variabili prima delle importazioni che le utilizzano:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Usa nelle regole le proprietà `where` e `type` corrispondenti:

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu a discesa della barra del titolo | `filespacer_title` | `'*'` |
| Barra degli strumenti **…** | `filespacer_toolbar` | `'*'` |
| File o cartelle selezionati | `filespacer_context` | `'file\|dir'` |
| Sfondo della cartella | `filespacer_context` | `'back'` |

Usa `mode='multiple'` per le regole che devono rimanere disponibili indipendentemente dallo stato della selezione. `process.name` non comprende `.exe`; `window.parent.name` identifica una classe di finestra.

## Individuare una voce di menu esistente

Il meccanismo generale `modify(find=...)` di Nilesoft trova una voce in base alla sua etichetta. Combinalo con `where` per limitare la modifica a un menu; `|` separa le etichette alternative e `*` è un carattere jolly:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copia percorso' title='Copia percorso della cartella')
```

Consulta la [documentazione della configurazione di Nilesoft](https://nilesoft.org/docs/configuration/).

## Aspetto consigliato per i menu

Per i menu della barra del titolo e della barra degli strumenti sono consigliati angoli squadrati, coerenti con i pulsanti di FileSpacer. Aggiungi questa impostazione al blocco `theme { ... }` esistente:

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

Se usi già un raggio personalizzato, sostituisci `default` con il suo valore precedente.
