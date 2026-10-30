# Usare FileSpacer

## Navigazione tra finestre

Ogni cartella ha una finestra che ricorda posizione, dimensioni e impostazioni di visualizzazione. Aprire di nuovo una cartella già aperta ne attiva la finestra. Spostare o rinominare una cartella conserva la disposizione salvata.

- Aprire una cartella o la cartella superiore chiude per impostazione predefinita la finestra di origine. Tieni premuto Ctrl per invertire questo comportamento una volta, oppure cambia l’impostazione in **Impostazioni > Generale**. Aprire più cartelle selezionate mantiene aperta la finestra di origine.
- Il menu accanto al titolo della cartella permette di passare tra le cartelle aperte. **Chiudi le finestre delle cartelle superiori** chiude le finestre aperte delle cartelle che la contengono, lasciando aperte le altre.
- **Ctrl+Maiusc+T**, oppure **Riapri l’ultima finestra chiusa** nel menu della barra del titolo o nella Jump List della barra delle applicazioni, riapre una cartella chiusa senza chiudere quella corrente. Vengono ripristinate disposizione e selezione; nelle visualizzazioni Dettagli ed Elenco viene ripristinata anche la prima riga visibile. Il comando della barra delle applicazioni può risalire alle chiusure precedenti.
- La Jump List della barra delle applicazioni apre anche le cartelle aggiunte ad Accesso rapido in Esplora file.

Passare alla cartella superiore non seleziona quella da cui provieni. Una cartella superiore già aperta conserva la propria selezione. Il Pannello di controllo si apre in una finestra Windows separata.

![Il menu del titolo della cartella con la navigazione tra finestre e il blocco dello stato salvato.](images/window-menu.png)

## Visualizzazione e disposizione

### Disposizione delle icone

Nelle visualizzazioni a icone puoi posizionare liberamente file e sottocartelle quando **Disponi icone automaticamente** è disattivato. Le loro posizioni vengono salvate indipendentemente dalle dimensioni e dalla posizione della finestra, anche quando il suo stato salvato è bloccato.

### Opzioni di visualizzazione

**Mostra il nome completo di un file selezionato**, in **Impostazioni > Visualizzazione**, rende interamente leggibile un singolo nome selezionato nella vista a icone. È attivo per impostazione predefinita e non modifica le selezioni multiple né le altre viste.

![Un nome di file lungo, selezionato e visibile per intero nella vista a icone.](images/selected-name.png)

**Mostra l’anteprima delle modalità nel menu Visualizza**, anch’esso attivo per impostazione predefinita, mostra ogni modalità al passaggio del puntatore. Un clic la conferma; Esc o l’uscita dall’anteprima ripristina la vista iniziale. L’anteprima non sostituisce mai la disposizione salvata.

**Mescola cartelle e file nell’ordinamento** li ordina insieme, come su [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) e [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). L’opzione è disattivata per impostazione predefinita in **Impostazioni > Visualizzazione > File**. Si applica a Dettagli, Elenco e alle icone disposte automaticamente; le disposizioni libere restano invariate. Cartelle virtuali e archivi mantengono l’ordinamento di Windows.

![Cartelle e file nello stesso ordine alfabetico.](images/mixed-sorting.png)

Anche la visibilità della barra degli strumenti e della barra di stato si cambia nelle impostazioni.

### Posizionamento delle finestre

**Impostazioni > Generale > Disponi le nuove finestre nello spazio libero** è attivo per impostazione predefinita. Le cartelle senza posizione salvata si aprono nello spazio disponibile; quando non ne rimane, la sovrapposizione viene ridotta. Le posizioni salvate hanno la precedenza. Disattivando l’opzione si usa il posizionamento predefinito di Windows, senza spostare le finestre esistenti.

### Selezione e attivazione

Con **Impostazioni > Generale > Mantieni la selezione facendo clic su una finestra inattiva** attivo, un clic sullo sfondo di una cartella inattiva ne attiva la finestra senza cancellare la selezione. Premi una seconda volta per iniziare un rettangolo di selezione. I clic sugli elementi, i clic con il pulsante destro e i gesti per le finestre conservano il loro comportamento abituale.

## Gestione delle finestre

### Spostamento e ridimensionamento

Una finestra può essere spostata o ridimensionata trascinando nell’area dei file, anche sopra un elemento. Le combinazioni predefinite sono:

| Azione | Scelta rapida |
| --- | --- |
| Spostare la finestra | Alt + trascinamento con il pulsante sinistro |
| Ridimensionare dall’angolo inferiore destro | Alt + trascinamento con il pulsante destro |

![Spostamento di una finestra con Alt e il pulsante sinistro.](images/move-window.gif)

![Ridimensionamento con Alt e il pulsante destro.](images/resize-window.gif)

La selezione rimane intatta. Esc annulla il gesto. Campi di testo, intestazioni, barre di scorrimento, barre degli strumenti e menu mantengono il comportamento normale; Alt + doppio clic apre le proprietà di un elemento.

Ogni azione ha una combinazione personalizzabile in **Impostazioni > Scelte rapide**. I pulsanti sinistro e destro richiedono Alt; quello centrale funziona anche senza Alt. I modificatori devono corrispondere esattamente; combinazioni riservate o duplicate non sono accettate. Le modifiche si applicano alle finestre aperte. Ripristina torna alle ultime combinazioni applicate; Annulla scarta le modifiche in sospeso.

### Blocco dello stato salvato

Il blocco conserva posizione, dimensioni e vista di una cartella per la successiva apertura. La funzione è disattivata per impostazione predefinita in **Impostazioni > Generale > Abilita il blocco dello stato della finestra**. Una volta attivata, **Blocca lo stato della finestra** è disponibile accanto al titolo. Un lucchetto sull’icona della cartella segnala il blocco.

<img src="images/locked-title.png" alt="Dettaglio del titolo della cartella con il lucchetto." width="280">

Sono comunque possibili modifiche temporanee; riaprendo la cartella si ripristina lo stato bloccato. Le posizioni delle singole icone continuano a essere salvate. **Sblocca lo stato della finestra** mantiene la disposizione attuale. Nascondere il lucchetto non rimuove il blocco.

Disattivare la funzione richiede conferma e rimuove tutti i blocchi, anche dalle cartelle chiuse. Riattivarla non li ripristina.

## Ricerca e cronologia

Digita tutto o parte del nome di un file o di una cartella nel campo di ricerca, quindi premi Invio o fai clic sulla lente per filtrare la cartella corrente. **Impostazioni > Generale > Filtra durante la digitazione** applica il filtro mentre scrivi. Questo filtro locale non avvia mai Everything automaticamente.

Attivare la casella mostra le ricerche recenti e, quando l’integrazione è attiva, **Cerca in Everything**. Usa le frecce Su/Giù e Invio oppure fai clic per scegliere una voce. Invio senza un suggerimento selezionato applica il filtro locale. Esc chiude l’elenco senza cancellare il testo; Ctrl+F riattiva la casella.

La cronologia conserva per impostazione predefinita cinque ricerche distinte confermate per cartella. Puoi impostare un limite da 0 a 20 nelle impostazioni generali; 0 svuota e disattiva la cronologia. La sola digitazione non aggiunge voci. Consulta [Everything](#everything) per cercare nelle sottocartelle o in tutti i file indicizzati.

![Suggerimenti di ricerca con Everything e query recenti per file di progetto fittizi.](images/search.png)

## Integrazione

### Integrazione con Windows

FileSpacer può essere impostato come gestore di file predefinito durante l’installazione o in seguito con **Impostazioni > Integrazione > Imposta come gestore di file predefinito**. **Ripristina Esplora file come predefinito** restituisce l’apertura delle cartelle a Esplora file.

Il menu di scelta rapida delle cartelle offre **Apri in FileSpacer**, oppure **Apri in Esplora file di Windows** quando FileSpacer è predefinito. Vale anche per unità, cartelle ZIP, percorsi di rete e sfondi delle cartelle, senza obbligare a cambiare l’applicazione predefinita.

I file si aprono con la consueta applicazione Windows. Anche cartelle di rete, anteprime e comandi aggiunti al menu di scelta rapida seguono il normale funzionamento di Windows. **Opzioni di Esplora file** apre le preferenze comuni delle cartelle, alcune delle quali valgono anche per FileSpacer.

### Everything

Un’installazione esistente di [Everything](https://www.voidtools.com/) può essere integrata per avviare ricerche avanzate dal campo di FileSpacer. I risultati si aprono in Everything.

L’opzione si attiva in **Impostazioni > Integrazione**. Everything viene individuato automaticamente o con **Rileva**; una copia portatile può essere scelta manualmente. La ricerca comprende la cartella corrente e le sottocartelle, oppure **Ovunque**. Le cartelle virtuali richiedono Ovunque.

**Cerca in Everything** invia la query digitata, usando la sintassi di Everything. Un avvio riuscito svuota il campo e il filtro locale di FileSpacer; in caso di errore rimangono disponibili per riprovare.

![Una ricerca in FileSpacer con i risultati corrispondenti in Everything a fianco.](images/everything-search.png)

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) può gestire l’aspetto dei menu di FileSpacer e personalizzarne i comandi. Si consiglia di adattarne la configurazione per un aspetto coerente, in particolare con angoli squadrati nei menu della barra del titolo e della barra degli strumenti.

Il [riferimento di configurazione](nilesoft-shell.md) spiega come consentire FileSpacer, mantenere il tema e limitare a Esplora file le regole che lo riguardano. Dopo una modifica, annullare e ripetere la registrazione di Nilesoft Shell la applica ai menu aperti successivamente.

Il tema Modern qui sotto confronta gli angoli predefiniti con quelli squadrati consigliati.

**Menu di navigazione**

| Angoli predefiniti | Angoli squadrati consigliati |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu di navigazione della barra del titolo con gli angoli arrotondati predefiniti del tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Lo stesso menu di navigazione con gli angoli squadrati consigliati." width="320"> |

**Menu principale (…)**

| Angoli predefiniti | Angoli squadrati consigliati |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principale con gli angoli arrotondati predefiniti del tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Lo stesso menu principale con gli angoli squadrati consigliati." width="320"> |

## Impostazioni e aggiornamenti

Ctrl+, apre le impostazioni anche con la barra degli strumenti nascosta. L’interfaccia segue la lingua di visualizzazione di Windows, con l’inglese come alternativa; un cambio di lingua richiede il riavvio. F1 apre la guida installata, disponibile anche offline.

Gli aggiornamenti si cercano in **Impostazioni > Informazioni**. Scaricamento e installazione richiedono conferma; l’installazione non è mai automatica. Al termine vengono riaperte le cartelle precedenti con le disposizioni salvate. I controlli settimanali sono facoltativi e disattivati per impostazione predefinita; si attivano in Informazioni.

## Dati locali

Disposizioni delle cartelle, blocchi, ricerche e cronologia delle cartelle chiuse sono conservati localmente in `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. Le preferenze generali si trovano nel Registro di sistema di Windows, in `HKEY_CURRENT_USER\Software\FileSpacer`. Lo stato salvato può contenere percorsi di cartelle, anche di rete. I file temporanei di aggiornamento e l’elenco delle cartelle da riaprire rimangono in `%TEMP%\FileSpacerUpdate-{GUID}`.

**Impostazioni > Informazioni > Manutenzione > Reimposta lo stato di tutte le cartelle** chiude le finestre delle cartelle e ne cancella disposizioni, cronologia e blocchi salvati. File e preferenze generali vengono conservati.

Per la risoluzione dei problemi, `FileSpacer.exe /reset-options` reimposta le preferenze generali; `/reset-all` cancella anche lo stato salvato delle cartelle. Nessuno dei due comandi elimina file, cambia le associazioni di Windows o rimuove rapporti diagnostici esistenti.

## Privacy

FileSpacer non contiene telemetria, monitoraggio dell’utilizzo o pubblicità. I rapporti sugli arresti anomali non vengono inviati automaticamente. Gli eventuali rapporti diagnostici rimangono in `%LOCALAPPDATA%\FileSpacer`; condividerli è facoltativo e può rivelare percorsi di file o parti della memoria del processo.

I controlli degli aggiornamenti e i download contattano GitHub tramite HTTPS. GitHub riceve le normali informazioni di connessione, compresi indirizzo IP e versione dell’applicazione, ma nessuno stato di cartella, percorso di file o rapporto diagnostico.

Le credenziali per le unità di rete vengono immesse e gestite da Windows, come in Esplora file. FileSpacer non le legge né le memorizza. Una ricerca Everything passa la query e, se limitata alla cartella, il relativo percorso all’applicazione Everything locale.
