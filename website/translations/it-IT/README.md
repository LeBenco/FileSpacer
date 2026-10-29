<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

FileSpacer è un gestore di file per Windows basato sul [paradigma spaziale](https://en.wikipedia.org/wiki/Spatial_file_manager): ogni cartella ha un’unica finestra che ricorda posizione, dimensioni e impostazioni di visualizzazione. Si creano così [riferimenti visivi stabili](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), che consentono alla [memoria spaziale](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) di aiutarti a riconoscere a colpo d’occhio le cartelle familiari. In un gestore di file basato sulla navigazione, la stessa finestra mostra cartelle diverse una dopo l’altra: posizione e dimensioni non bastano quindi a identificare la cartella aperta, e per orientarti devi guardarne il contenuto. Conservando riferimenti distinti per ogni cartella, l’approccio spaziale può ridurre lo sforzo necessario per ritrovarti tra i tuoi file.

Per limitare l’accumulo di finestre, un noto inconveniente del paradigma spaziale, FileSpacer chiude per impostazione predefinita la finestra di origine quando apri una cartella. Tieni premuto Ctrl durante l’apertura per mantenerla aperta quando vuoi lavorare con più cartelle contemporaneamente.

FileSpacer mantiene un’interfaccia volutamente sobria, a differenza di certe interfacce sovraccariche che lasciano appena lo spazio tra i pulsanti per intravedere un file.

Le schede sono incompatibili con il modello spaziale e non verranno mai aggiunte.

Windows 10 o versioni successive, x64. Disponibile in inglese, francese, tedesco, italiano, spagnolo, portoghese (Portogallo) e greco.

[Scarica l’ultima versione](https://github.com/LeBenco/FileSpacer/releases/latest)
· [Guida all’uso](docs/user-guide.md)
· [Cronologia delle versioni](CHANGELOG.md)

![Una finestra di FileSpacer con file e cartelle dimostrativi disposti liberamente.](docs/images/overview.png)

*Ogni cartella conserva la propria finestra e la disposizione delle icone.*

## Funzionalità

- **Disponi liberamente le icone**: le posizioni vengono salvate e ripristinate per ogni cartella. [Disposizione delle icone](docs/user-guide.md#icon-layout).
- **Sposta e ridimensiona le finestre** da qualsiasi punto dell’area dei file, senza perdere la selezione. I gesti del mouse sono configurabili; quelli predefiniti sono Alt + trascinamento con il pulsante sinistro e Alt + trascinamento con il pulsante destro. [Gesti per le finestre](docs/user-guide.md#moving-and-resizing).
- **Passa da una cartella aperta all’altra** dal menu della barra del titolo, chiudi le finestre delle cartelle superiori o riapri l’ultima finestra chiusa con Ctrl+Maiusc+T. [Navigazione tra finestre](docs/user-guide.md#window-navigation).
- **Blocca lo stato salvato di una finestra**, mantenendo la possibilità di modificarne temporaneamente posizione, dimensioni e visualizzazione. [Blocco dello stato](docs/user-guide.md#locking-a-saved-layout).
- **Filtra la cartella corrente** e riutilizza le ricerche recenti. [Ricerca e cronologia](docs/user-guide.md#search-and-history).
- **Mantieni la selezione quando attivi una finestra** facendo clic sullo sfondo. [Selezione e attivazione](docs/user-guide.md#selection-and-activation).
- **Riduci la sovrapposizione delle finestre** quando apri cartelle senza una posizione salvata. [Posizionamento delle finestre](docs/user-guide.md#window-placement).

## Integrazione

Se usi **Nilesoft Shell**, puoi affidargli anche l’aspetto dei menu di FileSpacer e personalizzarne i comandi, anche con regole per singoli menu. È consigliabile adattare la configurazione per una buona integrazione visiva. [Integrazione e configurazione](docs/user-guide.md#nilesoft-shell).

**Everything** può cercare nella cartella corrente e nelle sottocartelle, oppure ovunque, usando la query inserita in FileSpacer. I risultati si aprono in Everything, che va installato separatamente. [Installazione e utilizzo](docs/user-guide.md#everything).

Puoi anche impostare FileSpacer come applicazione predefinita per l’apertura delle cartelle. [Integrazione con Windows](docs/user-guide.md#windows-integration).

## Compilazione

La compilazione usa MSVC/NMAKE, Visual Studio 2022 Build Tools con ATL e un Windows SDK, Inkscape e ImageMagick. Per il programma di installazione è necessario NSIS; SQLite è incluso. Consulta la [guida alla compilazione](docs/build.md).

I test di regressione combinano scenari PowerShell con strumenti C++ e C# per le verifiche interne e i test reali dell’interfaccia Windows e della shell. Eseguili con `tests.cmd`, in locale o in una VM Hyper-V configurata. Consulta la [guida ai test](tests/regression/README.md).

---

FileSpacer è un fork di [ChromaFiler](https://github.com/vanjac/chromafiler), creato da J. van’t Hoog. Un grande grazie a vanjac per questa base!

Distribuito con licenza [GNU GPL v3.0](LICENSE). Copyright © 2026 Benco per le modifiche di FileSpacer; codice ChromaFiler originale © 2023 J. van’t Hoog. Consulta le [note sui componenti di terze parti](THIRD_PARTY_NOTICES.md) e l’[informativa sulla privacy](docs/privacy-policy.md).
