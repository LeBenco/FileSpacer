# Informativa sulla privacy

FileSpacer non contiene telemetria, analisi dell’utilizzo o pubblicità. Non invia automaticamente allo sviluppatore informazioni sull’utilizzo né segnalazioni di arresto anomalo.

## Verifica degli aggiornamenti

I controlli manuali e quelli settimanali automatici, se attivati, richiedono a GitHub tramite HTTPS il manifesto dell’ultima versione di FileSpacer. I controlli automatici sono disattivati per impostazione predefinita. GitHub e la sua infrastruttura di download ricevono le normali informazioni di connessione, tra cui l’indirizzo IP e uno user agent con la versione di FileSpacer. La richiesta non contiene percorsi di file, stato delle cartelle o rapporti diagnostici.

Quando è disponibile un aggiornamento installabile, FileSpacer chiede conferma prima di scaricare il programma di installazione da GitHub tramite HTTPS. Prima di avviarlo, verifica dimensioni e SHA-256 del download rispetto al manifesto della versione. Il programma di installazione richiede il tuo intervento; gli aggiornamenti non vengono mai installati automaticamente. Le versioni senza metadati di installazione aprono la propria pagina nel browser predefinito.

## Dati locali

Le preferenze globali sono conservate nel Registro di sistema di Windows dell’utente corrente, sotto `HKEY_CURRENT_USER\Software\FileSpacer`. Posizioni e dimensioni delle finestre delle cartelle, impostazioni di visualizzazione, posizioni delle icone, blocchi dello stato, ricerche recenti e cronologia delle cartelle chiuse vengono salvati in un database SQLite in `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. SQLite può creare file di journal associati nella stessa cartella. Lo stato salvato può contenere identità e percorsi di cartelle, anche di rete.

Un aggiornamento conserva temporaneamente il programma di installazione, una copia dell’utilità che lo avvia e un elenco SQLite dei percorsi delle cartelle aperte in `%TEMP%\FileSpacerUpdate-{GUID}`. Questo elenco locale serve a riaprire le cartelle dopo l’installazione o l’annullamento; non viene caricato in rete.

La protezione dagli arresti anomali può salvare rapporti diagnostici e minidump in `%LOCALAPPDATA%\FileSpacer`. Restano sul computer a meno che l’utente non decida di condividerli. La diagnostica può contenere percorsi di file, dettagli sui processi e parti della memoria dei processi. La stessa cartella contiene un file di blocco delle impostazioni.

Le build di debug possono usare preferenze e dati locali di test separati con `/test`.

## Windows e applicazioni esterne

L’apertura delle cartelle di rete, la risoluzione degli elementi della shell, la visualizzazione di anteprime o miniature fornite da Windows e l’uso delle estensioni dei menu di scelta rapida fanno ricorso a Windows e ai componenti della shell installati. Questi componenti e i server remoti possono accedere alla rete secondo il proprio funzionamento e le proprie informative.

I file si aprono con l’applicazione associata o con il selettore di applicazioni di Windows. Il modo in cui tali applicazioni trattano i file non è controllato da FileSpacer.

Se l’integrazione Everything è attiva, scegliere la sua azione di ricerca trasmette la query visibile all’applicazione Everything installata separatamente. Le ricerche limitate alla cartella trasmettono anche il percorso corrente. Il filtro locale non avvia Everything; sono l’indice e le impostazioni dell’applicazione esterna a determinare i risultati.

## Reimpostazione delle preferenze

`FileSpacer.exe /reset-options` reimposta le preferenze globali. `FileSpacer.exe /reset-all` cancella anche lo stato salvato delle cartelle. Nessuno dei due comandi elimina i file dell’utente o modifica le associazioni di file di Windows. I rapporti diagnostici già salvati non vengono rimossi.
