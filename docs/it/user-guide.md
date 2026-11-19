<a id="using-filespacer"></a>

# Usare FileSpacer

FileSpacer ricorda la posizione e l’aspetto di ogni finestra tra un’apertura e l’altra. Le cartelle ritrovano il loro posto e diventano riconoscibili, come oggetti disposti su una scrivania. Offre anche scorciatoie configurabili, azioni rapide e un’interfaccia essenziale e personalizzabile.

<div id="integrazione-con-windows"></div>
<div id="windows-integration"></div>

FileSpacer si può impostare come gestore di file predefinito durante l’installazione oppure in **Impostazioni > Integrazione**, con **Imposta come gestore di file predefinito**. Il menu contestuale delle cartelle propone **Apri in FileSpacer**, oppure **Apri in Esplora file di Windows** quando FileSpacer è il gestore predefinito.

<a id="window-navigation-and-layout"></a>

## Navigazione e disposizione delle finestre

<div id="navigazione-tra-finestre"></div>
<div id="window-navigation"></div>
<div id="posizionamento-delle-finestre"></div>
<div id="window-placement"></div>
<div id="gestione-delle-finestre"></div>
<div id="managing-windows"></div>

**Ogni cartella ha la propria finestra.** Se è già aperta, FileSpacer la porta in primo piano. Posizione, dimensioni, visualizzazione, disposizione delle icone e posizione di scorrimento vengono memorizzate anche dopo lo spostamento o la ridenominazione della cartella. Le finestre senza posizione salvata vengono collocate limitando la sovrapposizione (opzione in **Impostazioni > Generale**).

<div id="blocco-dello-stato-salvato"></div>
<div id="locking-a-saved-layout"></div>

**Si possono bloccare la posizione, le dimensioni e la visualizzazione attuali di una cartella per ritrovarle a ogni riapertura.** Le modifiche successive restano temporanee. Le posizioni delle icone continuano a essere salvate. Si può poi **ripristinare la disposizione bloccata** senza chiudere la finestra.

**Sblocca lo stato della finestra** conserva la disposizione attuale e permette di tornare a salvare le modifiche.

Il blocco si attiva in **Impostazioni > Generale**. Qui si può anche personalizzare l’aspetto del lucchetto a sinistra della barra del titolo e abilitarne il clic per bloccare o sbloccare lo stato della finestra. Il comando si può anche [aggiungere al menu della barra del titolo](#customize-the-title-bar-menu). Disattivare la funzione elimina tutti i blocchi e nasconde l’icona.

<img src="images/locked-title.png" alt="Dettaglio del titolo della cartella con il lucchetto." width="280">

Aprire una cartella o la cartella superiore chiude per impostazione predefinita la finestra di origine. **Tenere premuto Ctrl durante l’apertura** fa il contrario, qualunque sia il modo di aprire. Il comportamento si imposta in **Impostazioni > Generale**. L’apertura di più cartelle selezionate mantiene aperta la finestra di origine.

Il menu accanto al titolo permette di passare tra le cartelle aperte e chiudere le finestre delle cartelle superiori. **Ctrl+Maiusc+T** riapre l’ultima cartella chiusa con la sua disposizione e selezione. La Jump List della barra delle applicazioni include anche le cartelle chiuse di recente e quelle aggiunte ad Accesso rapido.

<div id="gruppi-di-cartelle"></div>

| Scelta rapida | Azione |
| --- | --- |
| Ctrl+N | Aprire la cartella iniziale scelta nelle impostazioni o attivare la sua finestra esistente, lasciando aperta quella di origine. |
| Ctrl+W | Chiudere la finestra, anche dai campi dell’indirizzo o della ricerca. |
| Backspace | Aprire la cartella superiore rispettando l’impostazione per mantenere aperta la finestra di origine. |
| Ctrl+Backspace | Aprire o attivare la cartella superiore invertendo l’impostazione che lascia aperta la finestra di origine. |
| F4, Ctrl+L o Alt+D | Selezionare l’indirizzo. |
| F3, Ctrl+E o Ctrl+F | Spostare il focus sulla ricerca. |
| F5 o Ctrl+R | Aggiornare la cartella, anche durante la digitazione nell’indirizzo o nella ricerca. |

Nei campi dell’indirizzo, della ricerca e di ridenominazione, Backspace e Ctrl+Backspace mantengono la loro funzione di modifica del testo. I file di testo possono ancora essere creati dal menu contestuale **Nuovo**.

<div id="folder-groups"></div>

**Salva / apri un gruppo di cartelle**, nel menu del titolo, permette di ritrovare più cartelle insieme con le loro disposizioni salvate.

<a id="folder-contents-and-display"></a>

## Visualizzazione del contenuto delle cartelle

<div id="visualizzazione-e-disposizione"></div>

**Ctrl+rotellina del mouse** passa tra dimensioni delle icone e modalità di visualizzazione. Il gesto di zoom del touchpad usa la stessa azione quando Windows lo inoltra. **Ctrl+Maiusc+1–8** seleziona nell’ordine icone molto grandi, grandi, medie e piccole, elenco, dettagli, riquadri e contenuto. La modalità viene ricordata per la cartella; il blocco dello stato salvato conserva il suo normale comportamento.

<div id="view-and-layout"></div>
<div id="disposizione-delle-icone"></div>
<div id="icon-layout"></div>

In **Visualizza > Disposizione**, scegliere come disporre le icone:

- **Automatica** dispone le icone senza spazi vuoti.
- **Sulla griglia** consente di scegliere le posizioni sulla griglia, lasciando anche spazi vuoti.
- **Libera** consente di posizionarle ovunque, come sul desktop di Windows.

Queste opzioni sono disponibili con tutte le dimensioni delle icone. **Elenco**, **Dettagli** e **Contenuto** mantengono la disposizione ordinata.

**Riallinea le icone** allinea una sola volta la disposizione attuale alla griglia di Windows, senza cambiare modalità né riempire gli spazi vuoti.

In modalità **Libera**, tenere premuto **S** durante il trascinamento e fino al rilascio del pulsante del mouse per allineare alla griglia solo le icone trascinate. Le altre restano al loro posto. Rilasciando **S** prima di deporle, il posizionamento resta libero. Nella vista Icone, un riquadro grigio indica la posizione di destinazione.

<div id="details-columns"></div>

Nella visualizzazione **Dettagli**, le colonne possono occupare automaticamente tutta la larghezza della finestra. La funzione si abilita in **Impostazioni > Visualizzazione** e si attiva o disattiva per ogni cartella dal relativo menu **Visualizza**.

![Colonne della visualizzazione Dettagli estese a tutta la finestra, poi allargamento della colonna Nome.](images/details-columns.gif)

L’**aggiornamento continuo**, disponibile nelle stesse impostazioni, può rendere il ridimensionamento meno fluido. Per prudenza, è sconsigliato attivarlo con più di qualche decina di file.

<div id="opzioni-di-visualizzazione"></div>
<div id="view-options"></div>

Per impostazione predefinita, le cartelle sono raggruppate prima dei file, come in Esplora file. **Mescola cartelle e file nell’ordinamento**, in **Impostazioni > Visualizzazione**, li ordina insieme, come in [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) e [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). L’opzione riguarda Dettagli, Elenco e Icone disposte automaticamente. Cartelle virtuali e archivi mantengono l’ordine di Windows.

![Cartelle e file nello stesso ordine alfabetico.](images/mixed-sorting.png)

Nelle stesse impostazioni, **Mostra il nome completo di un file selezionato** permette di leggere per intero il nome nella vista Icone, anche se è molto lungo. Si applica quando è selezionato un solo file.

![Un nome di file lungo, selezionato e visibile per intero nella vista a icone.](images/selected-name.png)

<div id="selezione-e-attivazione"></div>
<div id="selection-and-activation"></div>

Per impostazione predefinita, un clic sullo sfondo di una finestra inattiva ne cancella la selezione, come in Esplora file. **Mantieni la selezione facendo clic su una finestra inattiva**, in **Impostazioni > Generale**, permette di attivarla senza perdere la selezione.

<a id="customization"></a>

## Personalizzazione

<a id="changing-the-interface"></a>

### Modificare l’interfaccia

<div id="personalizzare-il-menu-della-barra-del-titolo"></div>
<div id="customize-the-title-bar-menu"></div>

Il **menu della barra del titolo** si personalizza in **Impostazioni > Visualizzazione > Elementi dell’interfaccia**. I comandi si aggiungono e riordinano trascinandoli. Per rimuovere una voce, trascinarla fuori dal menu.

![Aggiunta di un’azione al menu della barra del titolo e rimozione di un’altra.](images/customize-title-menu.gif)

![Il menu della barra del titolo con l’azione Ripristina la disposizione bloccata.](images/customized-title-menu.png)

Le voci gestite da Nilesoft Shell si modificano nella sua configurazione, perché le sue regole hanno priorità su quelle di FileSpacer ([vedere l’integrazione più sotto](#nilesoft-shell)).

<div id="spostamento-e-ridimensionamento"></div>
<div id="moving-and-resizing"></div>

Si possono definire **gesti del mouse per spostare o ridimensionare una finestra** direttamente dal suo contenuto, anche trascinando su un file. Impostazioni predefinite:

| Azione | Gesto |
| --- | --- |
| Spostare | Alt + trascinamento con il pulsante sinistro |
| Ridimensionare | Alt + trascinamento con il pulsante destro |

![Spostamento di una finestra con Alt e il pulsante sinistro.](images/move-window.gif)

![Ridimensionamento con Alt e il pulsante destro.](images/resize-window.gif)

Se ci sono file selezionati, la selezione viene mantenuta. Esc annulla l’azione. Le scorciatoie si personalizzano in **Impostazioni > Scelte rapide**. Il pulsante sinistro richiede Alt. Usato da solo, il pulsante destro agisce soltanto trascinando dallo sfondo della cartella; un clic destro senza trascinare apre ancora il menu. Il pulsante centrale si può usare da solo.

<a id="integration"></a>

### Integrazione

<div id="ricerca-e-cronologia"></div>
<div id="search-and-history"></div>
<div id="everything"></div>

[Everything](https://www.voidtools.com/) è uno strumento di ricerca che trova rapidamente file e cartelle per nome. FileSpacer permette di avviarlo dal campo di ricerca di una cartella. L’integrazione si attiva in **Impostazioni > Integrazione**, con rilevamento automatico o scelta di una versione portabile.

**Cerca in Everything** avvia [la ricerca](https://www.voidtools.com/support/everything/searching/) nella cartella e nelle sottocartelle, oppure in tutti i [file indicizzati](https://www.voidtools.com/support/everything/indexes/) con **Ovunque**. I risultati si aprono in Everything. Per le cartelle virtuali, scegliere Ovunque. Un avvio riuscito svuota il campo e rimuove il filtro locale di FileSpacer.

![Una ricerca in FileSpacer con i risultati corrispondenti in Everything a fianco.](images/everything-search.png)

<div id="nilesoft-shell"></div>

[Nilesoft Shell](https://nilesoft.org/) è un’estensione di Windows che permette di personalizzare l’aspetto e i comandi dei menu contestuali. Se utilizzata, può gestire anche i menu di FileSpacer.

FileSpacer compone il menu della barra del titolo, poi Nilesoft Shell applica le sue regole, che hanno priorità. La [guida di configurazione](nilesoft-shell.md) descrive le impostazioni visive e l’accesso alle azioni di FileSpacer.

Se i menu si aprono con un ritardo, l’impostazione `showdelay` di Nilesoft Shell permette di eliminarlo, per tutte le applicazioni o solo per FileSpacer ([configurazione](nilesoft-shell.md#menu-opening-delay)).

Per i menu della barra del titolo e della barra degli strumenti sono consigliati [angoli retti](nilesoft-shell.md#recommended-menu-appearance).

**Menu di navigazione**

| Angoli predefiniti | Angoli squadrati consigliati |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu di navigazione della barra del titolo con gli angoli arrotondati predefiniti del tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Lo stesso menu di navigazione con gli angoli squadrati consigliati." width="320"> |

**Menu principale**

| Angoli predefiniti | Angoli squadrati consigliati |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principale con gli angoli arrotondati predefiniti del tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Lo stesso menu principale con gli angoli squadrati consigliati." width="320"> |

[PowerToys](https://learn.microsoft.com/it-it/windows/powertoys/) raccoglie utilità Microsoft per ampliare e personalizzare Windows.

<div id="powertoys-peek"></div>

**Peek** mostra un’anteprima dei file. In FileSpacer, **Anteprima con Spazio**, in **Impostazioni > Integrazione**, permette di aprirla con Spazio. Peek deve essere attivo in PowerToys. Versione minima: 0.96, oppure 0.102 per la selezione multipla.

<div id="powertoys-grab-and-move"></div>

**Grab And Move** permette di spostare e ridimensionare le finestre dal loro contenuto. Con le stesse scorciatoie, ha la priorità sui gesti equivalenti di FileSpacer. Per mantenerli, escludere `FileSpacer.exe` in Grab And Move o disattivare lo strumento.

<a id="updates-and-troubleshooting"></a>

## Aggiornamenti e risoluzione dei problemi

<div id="impostazioni-e-aggiornamenti"></div>
<div id="settings-and-updates"></div>

**Impostazioni > Informazioni** permette di cercare aggiornamenti e attivare i controlli settimanali, disattivati per impostazione predefinita. Download e installazione richiedono conferma.

<div id="dati-locali"></div>
<div id="local-data"></div>

Nella stessa pagina, **Manutenzione > Reimposta lo stato di tutte le cartelle** chiude le finestre e cancella disposizioni, cronologie e blocchi. File e preferenze generali vengono mantenuti.

Il ripristino delle preferenze e la risoluzione avanzata dei problemi sono descritti nella [nota tecnica](../en/README.md#local-data), in inglese.

<a id="privacy"></a>

## Privacy

FileSpacer non include telemetria, monitoraggio dell’uso o pubblicità. Nessuna segnalazione di arresto anomalo viene inviata automaticamente. La condivisione è facoltativa e può rivelare percorsi di file o porzioni di memoria.

Gli aggiornamenti contattano GitHub, che riceve l’indirizzo IP e la versione dell’applicazione, ma nessun percorso di file o stato delle cartelle.

Per velocizzare la gestione delle cartelle grandi (da un migliaio di elementi), FileSpacer scarica file di simboli da Microsoft, che riceve l’indirizzo IP e la versione dei componenti di Windows interessati, ma nessun percorso di file o stato delle cartelle. Una notifica segnala ogni scaricamento, che si può disattivare in **Impostazioni > Integrazione**.

Le credenziali delle unità di rete sono gestite direttamente da Windows. FileSpacer non le legge né le salva.

Una ricerca di Everything invia la query e, se limitata a una cartella, il relativo percorso all’applicazione Everything locale.
