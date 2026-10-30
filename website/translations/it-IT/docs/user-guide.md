# Usare FileSpacer

## Navigazione tra finestre

Ogni cartella ha una finestra che ricorda posizione, dimensioni e impostazioni di visualizzazione. Aprire di nuovo una cartella già aperta ne attiva la finestra. Spostare o rinominare una cartella conserva la disposizione salvata.

- Aprire una cartella o la cartella superiore chiude per impostazione predefinita la finestra di origine. Tieni premuto Ctrl per invertire questo comportamento una volta, oppure cambia l’impostazione in **Impostazioni > Generale**. Aprire più cartelle selezionate mantiene aperta la finestra di origine.
- Il menu accanto al titolo della cartella permette di passare tra le cartelle aperte. **Chiudi le finestre delle cartelle superiori** chiude le finestre aperte delle cartelle che la contengono, lasciando aperte le altre.
- **Ctrl+Maiusc+T**, oppure **Riapri l’ultima finestra chiusa** nel menu della barra del titolo o nella Jump List della barra delle applicazioni, riapre una cartella chiusa senza chiudere quella corrente. Vengono ripristinate disposizione e selezione; nelle visualizzazioni Dettagli ed Elenco viene ripristinata anche la prima riga visibile. Il comando della barra delle applicazioni può risalire alle chiusure precedenti.
- La Jump List della barra delle applicazioni apre anche le cartelle aggiunte ad Accesso rapido in Esplora file.

Passare alla cartella superiore non seleziona quella da cui provieni. Una cartella superiore già aperta conserva la propria selezione. Il Pannello di controllo si apre in una finestra Windows separata.

![Il menu del titolo della cartella con la navigazione tra finestre e il blocco dello stato salvato.](images/window-menu.png)

*Il menu del titolo, qui con lo stato della finestra bloccato.*

## Visualizzazione e disposizione

### Disposizione delle icone

Nelle visualizzazioni a icone puoi posizionare liberamente file e sottocartelle quando **Disponi icone automaticamente** è disattivato. Le loro posizioni vengono salvate indipendentemente dalle dimensioni e dalla posizione della finestra, anche quando il suo stato salvato è bloccato.

### Opzioni di visualizzazione

La visibilità della barra degli strumenti e della barra di stato si modifica in **Impostazioni**.

In **Impostazioni > Visualizzazione**, due opzioni sono attive per impostazione predefinita:

- **Mostra il nome completo di un file selezionato** mantiene leggibile il nome di un singolo file selezionato nella visualizzazione Icone. Non modifica le selezioni multiple né le altre visualizzazioni.
- **Mostra l’anteprima delle modalità nel menu Visualizza** mostra un’anteprima della modalità su cui passi con il puntatore. Fai clic per mantenerla; Esc o l’uscita dall’anteprima ripristina la visualizzazione iniziale. L’anteprima non sostituisce la disposizione salvata.

**Mescola cartelle e file nell’ordinamento**, in **Impostazioni > Visualizzazione > File**, è disattivato per impostazione predefinita. Attivalo per ordinare insieme cartelle e file, come su [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) e [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Si applica alle visualizzazioni Dettagli, Elenco e Icone disposte automaticamente. Le disposizioni libere restano invariate. Cartelle virtuali e archivi mantengono l’ordinamento di Windows.

![Una finestra con cartelle e file dimostrativi mescolati in ordine alfabetico.](images/mixed-sorting.png)

*Cartelle e file condividono lo stesso ordine alfabetico.*

### Posizionamento delle finestre

**Impostazioni > Generale > Disponi le nuove finestre nello spazio libero** è attivo per impostazione predefinita. Le cartelle senza posizione salvata si aprono nello spazio disponibile; quando non ne rimane, la sovrapposizione viene ridotta. Le posizioni salvate hanno la precedenza. Disattivando l’opzione si usa il posizionamento predefinito di Windows, senza spostare le finestre esistenti.

### Selezione e attivazione

Con **Impostazioni > Generale > Mantieni la selezione facendo clic su una finestra inattiva** attivo, un clic sullo sfondo di una cartella inattiva ne attiva la finestra senza cancellare la selezione. Premi una seconda volta per iniziare un rettangolo di selezione. I clic sugli elementi, i clic con il pulsante destro e i gesti per le finestre conservano il loro comportamento abituale.

## Gestione delle finestre

### Spostamento e ridimensionamento

Trascina nell’area dei file, sullo sfondo o su un elemento, con questi gesti predefiniti:

| Azione | Scelta rapida |
| --- | --- |
| Spostare la finestra | Alt + trascinamento con il pulsante sinistro |
| Ridimensionare dall’angolo inferiore destro | Alt + trascinamento con il pulsante destro |

**Spostare la finestra — Alt + trascinamento con il pulsante sinistro**

![Spostamento di una finestra di cartella tenendo premuto Alt e trascinando con il pulsante sinistro nell’area dei file.](images/move-window.gif)

**Ridimensionare la finestra — Alt + trascinamento con il pulsante destro**

![Ridimensionamento di una finestra di cartella tenendo premuto Alt e trascinando con il pulsante destro nell’area dei file.](images/resize-window.gif)

La selezione viene mantenuta. Esc annulla il gesto e ripristina dimensioni e posizione iniziali. Campi di testo, intestazioni, barre di scorrimento, barre degli strumenti e menu conservano il comportamento normale. Alt + doppio clic su un elemento ne apre le proprietà.

In **Impostazioni > Scelte rapide**, attiva o disattiva ogni azione ed espandila per modificarne la combinazione. L’area di acquisizione diretta registra una combinazione di tasti e pulsante del mouse. I pulsanti sinistro e destro richiedono Alt; quello centrale supporta anche combinazioni senza Alt. I tasti modificatori devono corrispondere esattamente. Le combinazioni riservate o duplicate non possono essere applicate.

Il pulsante dell’impostazione predefinita reimposta una singola azione. **Ripristina** recupera le ultime scelte rapide applicate; **Annulla** scarta le modifiche in sospeso. **Applica** o **OK** rende le modifiche disponibili nelle finestre di FileSpacer aperte.

### Blocco dello stato salvato

Il blocco salva posizione, dimensioni e impostazioni di visualizzazione di una cartella per ripristinarle alla riapertura. Per usare questa funzione facoltativa, attiva **Impostazioni > Generale > Abilita il blocco dello stato della finestra** (disattivato per impostazione predefinita), poi scegli **Blocca lo stato della finestra** nel menu accanto al titolo della cartella. Un **lucchetto sovrapposto all’icona della cartella nella barra del titolo**, accanto al nome, identifica uno stato bloccato.

<img src="images/locked-title.png" alt="Dettaglio della barra del titolo: un lucchetto è sovrapposto all’icona della cartella accanto a Studio project." width="420">

*Il lucchetto indica che lo stato salvato è bloccato.*

Puoi comunque spostare e ridimensionare temporaneamente la finestra o cambiarne la visualizzazione. Riaprire la cartella ripristina lo stato bloccato. Le posizioni delle singole icone continuano a essere salvate normalmente. **Sblocca lo stato della finestra** mantiene la disposizione corrente. Puoi nascondere il lucchetto nelle impostazioni generali senza rimuovere il blocco.

Disattivare la funzione richiede conferma e rimuove tutti i blocchi salvati, anche quelli delle cartelle chiuse. Riattivarla non li ripristina.

## Ricerca e cronologia

Scrivi nella casella di ricerca e premi Invio o fai clic sulla lente per filtrare la cartella corrente. **Impostazioni > Generale > Filtra durante la digitazione** applica il filtro mentre scrivi. Questo filtro locale non avvia mai Everything automaticamente.

Attivare la casella mostra le ricerche recenti e, quando l’integrazione è attiva, **Cerca in Everything**. Usa le frecce Su/Giù e Invio oppure fai clic per scegliere una voce. Invio senza un suggerimento selezionato applica il filtro locale. Esc chiude l’elenco senza cancellare il testo; Ctrl+F riattiva la casella.

La cronologia conserva per impostazione predefinita cinque ricerche distinte confermate per cartella. Puoi impostare un limite da 0 a 20 nelle impostazioni generali; 0 svuota e disattiva la cronologia. La sola digitazione non aggiunge voci. Consulta [Everything](#everything) per cercare nelle sottocartelle o in tutti i file indicizzati.

![Suggerimenti di ricerca con Everything e query recenti per file di progetto fittizi.](images/search.png)

*Everything è un’azione separata; le ricerche recenti filtrano la cartella corrente.*

## Integrazione

### Integrazione con Windows

Il programma di installazione propone **Imposta FileSpacer come gestore di file predefinito**, non selezionato per impostazione predefinita. Installa anche il componente del menu di scelta rapida delle cartelle. Puoi modificare la scelta in seguito da **Impostazioni > Integrazione** con **Imposta come gestore di file predefinito**; **Ripristina Esplora file come predefinito** restituisce l’apertura delle cartelle a Esplora file di Windows.

Il componente del menu di scelta rapida propone **Apri in Esplora file di Windows** quando FileSpacer è predefinito, altrimenti **Apri in FileSpacer**. Si applica a cartelle, unità, cartelle compresse, percorsi di rete e sfondi delle cartelle. Installare soltanto questo componente non cambia l’applicazione predefinita.

**Opzioni di Esplora file**, nella stessa pagina di impostazioni, apre le opzioni cartella di Windows; alcune influiscono anche sulla visualizzazione dei file in FileSpacer.

### Everything

Se usi [Everything](https://www.voidtools.com/), puoi integrarlo con FileSpacer per avviare ricerche avanzate direttamente dalla sua casella di ricerca.

Per usare questa integrazione facoltativa, attivala in **Impostazioni > Integrazione**. Lascia vuoto il percorso dell’eseguibile per il rilevamento automatico, usa **Rileva** oppure scegli manualmente il file `.exe` per un’installazione portabile.

Scegli l’ambito della ricerca: cartella corrente e sottocartelle oppure **Ovunque**. Le cartelle virtuali non hanno un ambito nel file system: per esse usa Ovunque.

Inserisci una query nella casella di ricerca di FileSpacer e scegli **Cerca in Everything** nei suggerimenti. I risultati si aprono in Everything, usando il suo indice e la sua sintassi di ricerca. Se l’avvio riesce, FileSpacer svuota la casella e rimuove il filtro locale; in caso di errore conserva entrambi per permetterti di riprovare.

### Nilesoft Shell

Se usi [Nilesoft Shell](https://nilesoft.org/), puoi affidargli anche l’aspetto dei menu di FileSpacer e la personalizzazione delle loro voci.

Per questa integrazione facoltativa, è consigliabile adattare la configurazione di Nilesoft affinché i menu si integrino visivamente con FileSpacer, in particolare usando angoli squadrati per i menu della barra del titolo e della barra degli strumenti.

La [guida alla configurazione con esempi](nilesoft-shell.md) spiega come consentire FileSpacer, mantenere l’importazione del tema e limitare a Esplora file le regole riservate a quest’ultimo. Se modifichi la configurazione, annulla la registrazione di Nilesoft Shell e registralo di nuovo per applicare le modifiche ai menu aperti successivamente.

Il tema Modern qui sotto usa gli angoli arrotondati predefiniti a sinistra e gli angoli squadrati consigliati a destra.

**Menu di navigazione**

| Angoli predefiniti | Angoli squadrati consigliati |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu di navigazione della barra del titolo con gli angoli arrotondati predefiniti del tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Lo stesso menu di navigazione con gli angoli squadrati consigliati." width="320"> |

**Menu principale (…)**

| Angoli predefiniti | Angoli squadrati consigliati |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principale con gli angoli arrotondati predefiniti del tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Lo stesso menu principale con gli angoli squadrati consigliati." width="320"> |

## Impostazioni e aggiornamenti

**Preferenze.** **Ctrl+,** apre le impostazioni anche quando la barra degli strumenti è nascosta. L’interfaccia segue la lingua di visualizzazione di Windows, con l’inglese come lingua di ripiego; per cambiare lingua è necessario riavviare l’applicazione.

**Aggiornamenti.** Cerca una nuova versione in **Impostazioni > Informazioni**, poi scegli **Installa FileSpacer vX.Y.Z** per aggiornare. L’installazione richiede conferma; gli aggiornamenti non vengono mai installati automaticamente. Dopo l’installazione, FileSpacer riapre le cartelle precedentemente aperte con le disposizioni salvate. I controlli settimanali facoltativi sono disattivati per impostazione predefinita e si possono attivare in Informazioni.

**Manutenzione.** **Impostazioni > Informazioni > Manutenzione > Reimposta lo stato di tutte le cartelle** chiude le finestre di cartelle aperte e cancella disposizioni salvate, cronologia e blocchi dello stato. I tuoi file e le preferenze globali vengono conservati.

**Documentazione.** Scegliere **Documentazione** sopra **Impostazioni di FileSpacer** nel menu principale, premere **F1** oppure usare il pulsante in **Impostazioni > Informazioni**. La guida installata si apre nel browser nella lingua di visualizzazione di Windows, con l’inglese come alternativa. Pagine, immagini e ricerca funzionano senza connessione; il selettore della lingua permette di cambiare traduzione.
