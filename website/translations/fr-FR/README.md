<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

FileSpacer est un gestionnaire de fichiers pour Windows fondé sur le [paradigme spatial](https://en.wikipedia.org/wiki/Spatial_file_manager) : chaque dossier possède une seule fenêtre, qui mémorise sa position, sa taille et ses réglages d’affichage. Cela crée des [repères visuels stables](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf) et permet à la [mémoire spatiale](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) d’aider à reconnaître les dossiers familiers d’un coup d’œil. Dans un gestionnaire de fichiers par navigation, une même fenêtre affiche successivement différents dossiers : sa position et ses dimensions ne suffisent donc pas à identifier le dossier ouvert. Il faut regarder son contenu pour savoir où l’on se trouve. En conservant des repères distincts pour chaque dossier, l’approche spatiale peut alléger l’effort nécessaire pour se retrouver parmi ses fichiers.

Pour limiter l’accumulation des fenêtres, un inconvénient connu du paradigme spatial, FileSpacer ferme par défaut la fenêtre source à l’ouverture d’un dossier. Maintenir Ctrl pendant l’ouverture permet de la conserver pour travailler avec plusieurs dossiers à la fois.

<img src="docs/images/spatial-windows.png" alt="Trois fenêtres de dossiers sans chevauchement, de tailles, modes d’affichage et niveaux de zoom différents, dont une disposition libre des icônes." width="560">

FileSpacer conserve une interface délibérément sobre, contrairement à certaines interfaces surchargées qui laissent juste assez de place entre les boutons pour entrevoir un fichier.

Les onglets sont incompatibles avec le modèle spatial et ne seront jamais ajoutés.

Windows 10 ou version ultérieure, x64. Disponible en anglais, français, allemand, italien, espagnol, portugais (Portugal) et grec.

Aucune télémétrie, aucun suivi d’usage ni publicité. Les vérifications de mises à jour contactent GitHub ; les vérifications hebdomadaires sont facultatives et désactivées par défaut. Téléchargement et installation demandent confirmation. [Confidentialité](docs/user-guide.md#confidentialité).

[Télécharger la dernière version](https://github.com/LeBenco/FileSpacer/releases/latest)
· [Guide d’utilisation](docs/user-guide.md)
· [Historique des versions](CHANGELOG.md)

## Fonctionnalités

- **Une fenêtre unique par dossier**, réactivée si le dossier est déjà ouvert. Sa position, sa taille, ses réglages d’affichage et la disposition des icônes sont conservés d’une ouverture à l’autre. [Navigation spatiale](docs/user-guide.md#window-navigation).
- **Disposer les icônes librement**, avec une disposition enregistrée et restaurée pour chaque dossier. [Disposition des icônes](docs/user-guide.md#icon-layout).
- **Déplacer et redimensionner les fenêtres** depuis la zone des fichiers en conservant la sélection. Les raccourcis souris sont configurables ; par défaut, Alt + glisser gauche et Alt + glisser droit. [Gestes de fenêtre](docs/user-guide.md#moving-and-resizing).
- **Passer d’un dossier ouvert à l’autre** depuis le menu de la barre de titre, fermer les fenêtres parentes ou rouvrir la dernière fenêtre fermée avec Ctrl+Maj+T. [Navigation entre les fenêtres](docs/user-guide.md#window-navigation).
- **Verrouiller l’état enregistré d’une fenêtre**, tout en permettant des changements temporaires de position, de taille et d’affichage. [Verrouillage de l’état des fenêtres](docs/user-guide.md#locking-a-saved-layout).
- **Filtrer le dossier courant** et réutiliser ses recherches récentes. [Recherche et historique](docs/user-guide.md#search-and-history).
- **Conserver la sélection à l’activation d’une fenêtre** en cliquant sur son arrière-plan. [Sélection et activation](docs/user-guide.md#selection-and-activation).
- **Réduire le chevauchement des fenêtres** lors de l’ouverture de dossiers sans position enregistrée. [Placement des fenêtres](docs/user-guide.md#window-placement).

## Intégration

FileSpacer peut être défini comme gestionnaire de fichiers par défaut à l’installation ou ensuite dans les paramètres. Les fichiers s’ouvrent avec leur application Windows habituelle ; les dossiers réseau suivent le fonctionnement de Windows. [Intégration Windows](docs/user-guide.md#windows-integration).

Avec **Everything**, on peut lancer des recherches avancées depuis le champ de FileSpacer, dans le dossier courant et ses sous-dossiers ou dans tous les fichiers indexés. Les résultats s’ouvrent dans Everything. [Intégration et utilisation](docs/user-guide.md#everything).

Avec **Nilesoft Shell**, les menus de FileSpacer peuvent reprendre ses thèmes et ses commandes personnalisées. Une adaptation de sa configuration est recommandée pour une bonne intégration visuelle. [Intégration et configuration](docs/user-guide.md#nilesoft-shell).

## Compilation

La compilation utilise MSVC/NMAKE, Visual Studio 2022 Build Tools avec ATL et un SDK Windows, Inkscape et ImageMagick. NSIS est nécessaire pour l’installeur ; SQLite est inclus. Consultez le [guide de compilation](docs/build.md).

Les tests de non-régression associent des scénarios PowerShell à des outils C++ et C# pour les vérifications internes et les tests réels de l’interface Windows et du Shell. Exécutez-les avec `tests.cmd`, en local ou dans une VM Hyper-V configurée. Consultez le [guide des tests](tests/regression/README.md).

---

FileSpacer est un dérivé de [ChromaFiler](https://github.com/vanjac/chromafiler), créé par J. van’t Hoog (vanjac). Un grand merci à vanjac pour cette base !

Distribué sous [GNU GPL v3.0](LICENSE). Copyright © 2026 Benco pour les modifications de FileSpacer ; code ChromaFiler d’origine © 2023 J. van’t Hoog. Consultez les [mentions des composants tiers](THIRD_PARTY_NOTICES.md) et la [politique de confidentialité](docs/privacy-policy.md).
