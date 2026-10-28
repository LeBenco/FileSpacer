# Cronologia delle versioni

## Non ancora pubblicato

## 1.2.2 — 2026-10-28

- Correggere l’uso del provider di cartelle personalizzato con l’ordinamento misto e le icone disposte liberamente. Usare il provider nativo per il posizionamento libero e cambiare provider quando si conferma una disposizione ordinata, conservando posizioni salvate delle icone, selezione e stato attivo.

## 1.2.1 — 2026-10-28

- Ridurre la comparsa transitoria di icone generiche all’apertura delle cartelle con ordinamento misto. Preparare le icone visibili nella cache di Windows prima di mostrare la visualizzazione nativa, con un’attesa limitata per i gestori di icone lenti.

## 1.2.0 — 2026-10-28

- Aggiungere l’ordinamento misto facoltativo di cartelle e file del file system in Visualizzazione → File, conservando la visualizzazione Windows nativa e lo stato salvato delle cartelle. L’opzione è disattivata per impostazione predefinita.
- Aggiungere le traduzioni dell’interfaccia in tedesco, italiano, spagnolo, portoghese (Portogallo) e greco, oltre a inglese e francese.
- Visualizzare titolo e sottotitolo del programma di installazione come testo Windows più grande, adattato ai DPI e con smussatura ClearType.
- Ampliare la guida online con schermate inglesi e francesi, dimostrazioni dei gesti per le finestre e confronti dei menu Nilesoft Shell.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

## 1.1.10 — 2026-10-27

- Richiedere la conferma di Windows soltanto per installare o disinstallare per tutti gli utenti, applicando le scelte personali di integrazione all’utente che ha avviato l’installazione.
- Mostrare la cronologia completa delle versioni in un pannello Novità scorrevole accanto alle opzioni di installazione nella prima schermata.
- Correggere la reimpostazione delle scelte dei componenti durante gli aggiornamenti; ripristinare le selezioni salvate e conservare il gestore di file predefinito durante gli aggiornamenti non interattivi.
- Usare la stessa barra laterale FileSpacer, il logo grande e la combinazione grigio chiaro e bianco in tutte le schermate di installazione e disinstallazione. Assegnare al programma di installazione un’icona distinta, composta dalle bitmap native dell’applicazione e da una freccia di download ottimizzata separatamente per ogni dimensione, mantenendo un sottile bordo giallo a destra e in basso della cartella.
- Correggere gli arresti anomali alla chiusura causati dall’inizializzazione anticipata di dati delle cartelle locali al thread nei thread di lavoro tardivi della shell.
- Proporre Apri in Esplora file di Windows nei menu di scelta rapida delle cartelle quando FileSpacer è predefinito, altrimenti Apri in FileSpacer; aggiornare i menu al cambio della preferenza, dopo l’installazione e all’avvio.
- Correggere l’apertura dei collegamenti a server e condivisioni di rete in Esplora file quando FileSpacer è predefinito. Aggiornare il componente del menu di scelta rapida delle cartelle e impostare di nuovo FileSpacer come predefinito per abilitare l’integrazione di rete.
- Portare a 48 pixel, con scala al 100%, l’icona dell’applicazione nella scheda Informazioni, disponendo il testo accanto.
- Mantenere interamente visibili i titoli delle finestre ingrandite ed estendere la barra degli strumenti per tutta la larghezza disponibile.
- Consentire l’esecuzione dei test di regressione in un client Hyper-V configurato, compilando l’applicazione e gli strumenti di test sull’host.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

## 1.1.9 — 2026-10-27

- Mostrare l’anteprima delle modalità al passaggio del puntatore sul menu Visualizza, con ripristino progressivo in caso di annullamento e un’opzione attiva per impostazione predefinita in Visualizzazione.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

## 1.1.8 — 2026-10-27

- Sostituire l’icona dell’applicazione e il logo del README con un tesseratto proiettato e un rapporto 11/20 tra gli spigoli viola e verdi del cubo, mantenendo ottimizzate le dimensioni più piccole.
- Chiarire le etichette inglesi e francesi delle impostazioni e raggruppare le opzioni di blocco delle finestre.
- Aggiungere un’opzione per nascondere il lucchetto nelle finestre bloccate senza modificarne il blocco dello stato.
- Usare gli sfondi nativi del tema Windows nelle pagine delle impostazioni e nei controlli delle scelte rapide.
- Spiegare il posizionamento delle finestre e il raggruppamento nella barra delle applicazioni con le normali descrizioni comandi di Windows accanto alle opzioni.
- Collegare le impostazioni alla finestra di cartella da cui sono state aperte e scartare le modifiche in sospeso quando viene chiusa. Le operazioni di manutenzione confermate chiudono le impostazioni senza applicare le modifiche in sospeso.
- Riorganizzare il README attorno alla navigazione spaziale e alle funzionalità, con riferimenti di ricerca, un collegamento all’ultima versione, guide dedicate alla compilazione e a Nilesoft Shell e documentazione utente ampliata.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

## 1.1.7 — 2026-10-26

- Ridurre al minimo la sovrapposizione delle nuove finestre usando la geometria esatta e cercando prima lo spazio libero. Rinominare l’opzione generale in «Riduci al minimo la sovrapposizione delle finestre».
- Proporre la scelta facoltativa del gestore di file predefinito nei programmi di installazione inglese e francese, con istruzioni per attivarla o annullarla in seguito dalle impostazioni di integrazione di FileSpacer.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

## 1.1.6 — 2026-10-26

- Correggere le finestre di cartelle che restano aperte quando una cartella superiore viene eliminata o spostata nel Cestino.
- Evitare che le Jump List propongano di riaprire cartelle non più disponibili.
- Ridurre la sovrapposizione delle finestre senza posizione salvata, ignorando la finestra di origine che viene sostituita durante la navigazione spaziale. Attivare il comportamento per impostazione predefinita con un’opzione in Generale > Navigazione delle cartelle.
- Spostare la manutenzione dello stato delle cartelle da Generale a Informazioni.
- Aprire le nuove finestre con proporzioni bilanciate di 4:3 (580 × 435 a 96 DPI), conservando l’altezza esistente e rispettando le dimensioni configurate e quelle salvate per ciascuna cartella.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

## 1.1.5 — 2026-10-26

- Correggere la creazione involontaria di un collegamento dopo Alt + doppio clic seguito da un movimento del mouse. Conservare proprietà native, selezione e stato attivo senza riprodurre la pressione mantenuta di un pulsante.
- Correggere i test sui tempi nativi del doppio clic e sui bersagli fisici del mouse. Confrontare proprietà, selezione, elemento attivo, ancoraggio della selezione, geometria e contenuti delle cartelle con e senza gesti per le finestre, comprese le combinazioni Ctrl/Maiusc.
- Isolare l’eseguibile reale di test di Everything e il suo file INI di avvio, affinché l’impostazione amministratore installata dall’utente non possa reindirizzare le ricerche di test o richiedere privilegi elevati. Verificare che le impostazioni installate restino invariate.
- Mantenere attivo il desktop e sospendere temporaneamente il salvaschermo durante i test dell’interfaccia, ripristinando lo stato iniziale durante la pulizia, anche dopo un errore nei test.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

## 1.1.4 — 2026-10-25

Questa versione si basa sulla 1.1.3, che offre già gesti configurabili da tastiera e mouse per spostare e ridimensionare le finestre.

- Aggiungere la ricerca esterna facoltativa con Everything: rilevamento automatico, percorso manuale dell’eseguibile, ambito limitato alla cartella e alle sottocartelle oppure ovunque ed errori espliciti di avvio.
- Mostrare automaticamente Everything e le ricerche recenti all’attivazione della casella esistente, senza un pulsante aggiuntivo. Conservare ricerca nativa, filtro immediato e riutilizzo delle query locali.
- Ricordare per impostazione predefinita cinque ricerche recenti per cartella tra le sessioni, con un limite da zero a venti indipendente da Everything. Conservare i dati esistenti e pulire la cronologia secondo i criteri prudenti già usati per lo stato delle cartelle.
- Mostrare l’icona dell’eseguibile Everything alla dimensione appropriata, con estrazione in background e cache in memoria aggiornata quando cambia l’eseguibile.
- Aggiungere test di regressione per archiviazione isolata, migrazione e integrazione reale Shell/Everything; rendere robusto il lettore di test della shell quando filtri successivi cambiano la visualizzazione.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

## 1.1.3 — 2026-10-25

- Aggiungere la scheda Scelte rapide per configurare o disattivare i gesti di spostamento e ridimensionamento con tasti modificatori e pulsanti sinistro, centrale o destro, acquisizione diretta e verifica dei conflitti.
- Lasciare alla visualizzazione nativa di Esplora file la gestione di Alt + doppio clic per le proprietà.
- Ripristinare i nomi completi su sfondo opaco per un singolo elemento selezionato nella visualizzazione Icone. Le selezioni multiple mantengono il rendering nativo di Windows e il testo dell’opzione ne precisa ora l’ambito.
- Aggiungere test esaustivi delle scelte rapide e dei gesti reali del mouse, con risorse delle impostazioni francesi e inglesi.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

## 1.1.2 — 2026-10-25

- Aggiungere il blocco facoltativo della geometria e della visualizzazione salvate di una finestra dal menu del titolo. Restano possibili le modifiche temporanee; le posizioni delle singole icone continuano a essere salvate normalmente.
- Mostrare un lucchetto Small & Flat sulle icone delle cartelle bloccate, con ICO nativi multirisoluzione, adattamento ai DPI e colori a contrasto elevato.
- Confermare la disattivazione globale del blocco, rimuovere quelli salvati per cartelle aperte e chiuse e riprendere il normale salvataggio tra i processi.
- Estendere i test di regressione a transizioni di blocco, errori di salvataggio, ripristino della shell e rendering nativo del lucchetto.
- Riaprire una cartella chiusa con Ctrl+Maiusc+T o le Jump List ripristina nelle visualizzazioni Dettagli ed Elenco la riga che era in cima alla chiusura.
- La diagnostica degli arresti anomali continua a scrivere un minidump e un rapporto nei dati locali dell’applicazione. Rimuovere l’attribuzione al precedente rendering dei nomi completi, la modifica automatica della relativa preferenza e il riavvio.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

## 1.1.1 — 2026-10-24

- Ripristinare l’attivazione che conserva la selezione: il primo clic sullo sfondo vuoto di una cartella inattiva ne attiva la finestra senza cancellarla. La preferenza viene riattivata.
- Correggere un arresto anomalo alla chiusura di una finestra di cartella durante l’aggiornamento della visualizzazione della shell, con Aggiorna, filtro per nome o comandi della shell Aggiorna/Nuovo.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

## 1.1.0 — 2026-10-24

- Rifattorizzare le visualizzazioni delle cartelle usando le implementazioni di Windows Shell tramite API COM pubbliche invece di adattarne i controlli interni.
- Mantenere stato spaziale delle finestre, selezione e stato attivo, posizioni delle icone e filtri collegati al database SQLite di produzione e agli eventi pubblici della shell.
- Conservare i comandi nativi dei file e i gesti Alt di FileSpacer per spostamento, ridimensionamento e proprietà.
- Aprire il Pannello di controllo nella propria finestra Windows, conservando cartella di origine e selezione.
- Rimuovere la selezione automatica della cartella di origine quando si passa a una cartella superiore per ragioni ergonomiche: ulteriori selezioni con Ctrl non devono aprire accidentalmente entrambe le cartelle.
- Accettare tre differenze in questa versione: non sono disponibili nomi selezionati completi personalizzati su sfondo opaco, conservazione della selezione all’attivazione e ripristino esatto dello scorrimento. Disattivare i due controlli delle impostazioni inattivi, mantenendo le preferenze salvate.
- Semplificare i test di regressione attorno alla configurazione nativa di produzione della shell; ritirare confronti esplorativi tra profili, verifiche di incorporamento e rapporti riservati alla migrazione.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

## 1.0.8 — 2026-10-22

- Spostare le finestre con Alt + trascinamento sinistro e ridimensionarle con Alt + trascinamento destro dall’area principale dei file, conservando la selezione.
- Mantenere attiva la visualizzazione classica della shell indipendentemente dalla preferenza per i nomi completi degli elementi selezionati.
- Applicare il tema Nilesoft Shell al menu delle modalità di visualizzazione.
- Evitare lunghi ritardi alla chiusura dovuti alle cache passive delle icone della shell, mantenendo i riferimenti al ciclo di vita acquisiti dai comandi della shell invocati esplicitamente.
- Isolare l’hosting della visualizzazione della shell ed estendere i test a gesti Alt, rientranza dei menu e ciclo di vita Shell/STA.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Selezionare la cartella figlia di origine all’apertura di una cartella superiore, conservando la selezione delle finestre già aperte.
- Correggere il filtro dei nomi dei file quando si invia, modifica o cancella una query.
- Ripristinare l’icona Windows nativa di Accesso rapido e centrare i pulsanti Accesso rapido e Configurazione.
- Rimuovere la diagnostica temporanea e usare Benco nei riconoscimenti del progetto.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Ripristinare automaticamente le finestre delle cartelle aperte dopo un aggiornamento dall’applicazione, mantenendo posizioni, dimensioni e impostazioni di visualizzazione salvate.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Cercare aggiornamenti in modo asincrono dalla pagina Informazioni, con pulsante di installazione e testo di stato adeguati.
- Scaricare gli aggiornamenti in una finestra di avanzamento separata e annullabile, lasciando utilizzabili le finestre delle cartelle.
- Verificare dimensioni e SHA-256 del programma di installazione prima di chiudere ordinatamente FileSpacer e avviare l’installazione.
- Pubblicare i metadati di download del programma di installazione insieme a ogni versione.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Aggiungere un comando nel menu del titolo per chiudere le finestre delle cartelle superiori.
- Riaprire le cartelle chiuse dal menu del titolo, dalle Jump List o con Ctrl+Maiusc+T, ripristinando visualizzazione, selezione e posizione di scorrimento.
- Aggiungere alle Jump List della barra delle applicazioni le cartelle fissate in Accesso rapido di Esplora file.
- Mostrare il comando di riapertura soltanto quando l’ultima cartella chiusa non è già aperta, con il nome e l’icona nativa.
- Escludere la finestra corrente dall’elenco delle finestre nel menu del titolo.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Ripristinare i controlli manuali e i controlli settimanali facoltativi degli aggiornamenti tramite le versioni GitHub di FileSpacer, disattivati per impostazione predefinita.

- Pubblicare il manifesto degli aggiornamenti insieme a ogni programma di installazione.

- Conservare la gestione nativa della shell di Alt + doppio clic e Alt + Invio sulle proprietà delle cartelle.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Applicare i temi Nilesoft Shell al menu delle cartelle nella barra del titolo indipendentemente dalla selezione dei file, mantenendo i comandi nativi della shell.
- Consentire personalizzazioni Nilesoft separate per i menu della barra del titolo e della barra degli strumenti.
- Centrare la freccia del menu a discesa nella barra del titolo.
- Documentare configurazione di Nilesoft Shell, applicazione del tema e filtri dei menu.
- Citare Benco nelle informazioni di copyright.
- Consentire al workflow di pubblicazione di sostituire il programma di installazione di una versione esistente.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Attivare i controlli nativi Riduci a icona e Ingrandisci e correggere i pulsanti del titolo nelle finestre ingrandite.
- Salvare e ripristinare separatamente lo stato ingrandito e la geometria normale della finestra, con migrazione automatica del database.
- Usare le animazioni native della barra delle applicazioni e le proprietà di riavvio specifiche di ogni cartella.
- Applicare le preferenze di raggruppamento della barra delle applicazioni alle nuove finestre e chiarire il comportamento nelle impostazioni.
- Consentire l’eliminazione delle cartelle aperte e rimuoverne lo stato salvato.
- Impedire all’applicazione e al suo assistente di ripristino di bloccare cartelle tramite la directory di lavoro.
- Conservare percorsi assoluti delle cartelle e modalità di test di debug durante il ripristino dopo un arresto anomalo.
- Ripristinare subito le etichette normali dei nomi dopo la deselezione quando è attiva la visualizzazione del nome completo.
- Rilasciare le interfacce della shell in cache alla chiusura delle finestre di cartelle e correggere la gestione delle notifiche della shell.
- Consentire la pubblicazione manuale delle versioni e includere il numero di versione nei nomi dei programmi di installazione.

[Tutte le modifiche](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

## 1.0.0 — 2026-10-09

- Prima versione di FileSpacer per Windows 10 o successivo, x64, basata su ChromaFiler.
- Offrire navigazione spaziale con una finestra per cartella e stato persistente di finestre, visualizzazioni, icone e filtri conservato in SQLite.
- Seguire spostamenti e rinomine delle cartelle e attivare la finestra esistente quando si riapre una cartella già aperta.
- Chiudere per impostazione predefinita la finestra di origine all’apertura di una cartella o di quella superiore, con comportamento configurabile e Ctrl per invertirlo.
- Aggiungere percorso di navigazione, barra del percorso modificabile e filtro dei nomi con filtro facoltativo durante la digitazione.
- Offrire elementi configurabili della barra degli strumenti, barra di stato inferiore e preferenze di raggruppamento nella barra delle applicazioni.
- Aggiungere la visualizzazione sperimentale facoltativa dei nomi completi degli elementi selezionati.
- Offrire interfacce inglese e francese, con l’inglese come lingua di ripiego.
- Aggiungere il ripristino dopo gli arresti anomali e i comandi di manutenzione per reimpostare preferenze o stato salvato delle cartelle.
- Rimuovere la navigazione a colonne Miller, l’editor di testo integrato, le finestre di anteprima e il pannello mobile.
- Fornire script di compilazione MSVC/NMAKE, programma di installazione NSIS, pubblicazioni GitHub automatizzate e documentazione del progetto.

[Cronologia della versione](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
