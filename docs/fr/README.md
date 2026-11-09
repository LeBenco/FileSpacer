<p align="center">
    <img height="200" alt="FileSpacer" src="../../src/res/FileSpacer.svg" />
</p>


<a id="filespacer"></a>

# FileSpacer

[![Télécharger dernière version](https://img.shields.io/badge/T%C3%A9l%C3%A9charger-derni%C3%A8re%20version-blue)](https://github.com/LeBenco/FileSpacer/releases/latest) &emsp;
[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![télémétrie: aucune](https://img.shields.io/badge/t%C3%A9l%C3%A9m%C3%A9trie-aucune-2ea44f)](#privacy)
[![Licence: GPL v3](https://img.shields.io/badge/Licence-GPLv3-blue.svg)](#license-and-credits)

FileSpacer est un gestionnaire de fichiers pour Windows fondé sur le [paradigme spatial](https://en.wikipedia.org/wiki/Spatial_file_manager) : chaque dossier possède une seule fenêtre, qui mémorise sa position, sa taille et ses réglages d’affichage. Cela crée des [repères visuels stables](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf) et permet à la [mémoire spatiale](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) d’aider à reconnaître les dossiers familiers d’un coup d’œil. Dans un gestionnaire de fichiers par navigation, une même fenêtre affiche successivement différents dossiers : sa position et ses dimensions ne suffisent donc pas à identifier le dossier ouvert. Il faut regarder son contenu pour savoir où l’on se trouve. En conservant des repères distincts pour chaque dossier, l’approche spatiale peut alléger l’effort nécessaire pour se retrouver parmi ses fichiers. Les onglets sont incompatibles avec le modèle spatial et ne seront jamais ajoutés.

<img src="images/spatial-windows.png" alt="Trois fenêtres de dossiers sans chevauchement, de tailles, modes d’affichage et niveaux de zoom différents, dont une disposition libre des icônes." width="560">

FileSpacer privilégie une expérience légère et épurée. Pour limiter l’accumulation des fenêtres, un inconvénient connu du paradigme spatial, FileSpacer ferme par défaut la fenêtre source à l’ouverture d’un dossier. Maintenir Ctrl pendant l’ouverture permet de la conserver pour travailler avec plusieurs dossiers à la fois. FileSpacer conserve une interface délibérément sobre, contrairement à certaines interfaces surchargées qui laissent juste assez de place entre les boutons pour entrevoir un fichier.

<a id="system--languages"></a>**Système et langues :** Windows 10 ou version ultérieure, x64. Disponible en dix langues : anglais, français, allemand, italien, espagnol, portugais (Portugal), grec, polonais, néerlandais et ukrainien.

<a id="privacy"></a>**Confidentialité :** **Aucune télémétrie, aucun suivi d’usage ni publicité.** Les vérifications de mises à jour contactent GitHub. Les vérifications hebdomadaires sont facultatives et désactivées par défaut. Téléchargement et installation demandent confirmation. [Confidentialité](user-guide.md#privacy).

**Installation :**

- **Installation manuelle :** télécharger et exécuter <a href="https://github.com/LeBenco/FileSpacer/releases/download/v1.3.2/FileSpacer-1.3.2-install.exe" id="last-release-url">l’installateur de la dernière version</a>.
- **WinGet :** `winget install --id Benco.FileSpacer --exact`
- **Chocolatey** (terminal administrateur) : `choco install filespacer -y`

Les paquets WinGet sont en attente d’approbation dans leur catalogue.

<a id="features"></a>

## Fonctionnalités

- **Une fenêtre unique par dossier**. Position, taille, mode d’affichage et emplacement des icônes [sont mémorisés](user-guide.md#window-navigation). Les icônes peuvent être disposées librement ou alignées sur la [grille de positionnement classique](user-guide.md#icon-layout).
- **Organisation des fenêtres**. La fenêtre source se ferme par défaut à l’ouverture d’un autre dossier. Les fenêtres sans position mémorisée sont placées [en limitant le chevauchement](user-guide.md#window-placement).
- **Gérer ses fenêtres**. Déplacer ou redimensionner les fenêtres grâce à des [raccourcis souris](user-guide.md#moving-and-resizing). Enregistrer et rouvrir des [groupes de dossiers](user-guide.md#folder-groups). [Verrouiller la disposition des fenêtres](user-guide.md#locking-a-saved-layout).

**Documentation :**

[![Docs](https://img.shields.io/badge/Docs-Guide%20d%E2%80%99utilisation-blue)](user-guide.md)
[![Docs](https://img.shields.io/badge/Docs-Historique%20des%20versions-orange)](CHANGELOG.md)

<a id="integration"></a>

## Intégration

FileSpacer peut être défini comme gestionnaire de fichiers par défaut à l’installation ou ensuite dans les paramètres. Les fichiers s’ouvrent avec leur application Windows habituelle. Les dossiers réseau suivent le fonctionnement de Windows. [Intégration Windows](user-guide.md#windows-integration).

Avec **Everything**, on peut lancer des recherches avancées depuis le champ de FileSpacer, dans le dossier courant et ses sous-dossiers ou dans tous les fichiers indexés. Les résultats s’ouvrent dans Everything. [Intégration et utilisation](user-guide.md#everything).

Avec **Nilesoft Shell**, les menus de FileSpacer peuvent reprendre ses thèmes et ses commandes personnalisées. Une adaptation de sa configuration est recommandée pour une bonne intégration visuelle. [Intégration et configuration](user-guide.md#nilesoft-shell).

<a id="build"></a>

## Compilation

[![Publish FileSpacer](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

La compilation utilise MSVC/NMAKE, Visual Studio 2022 Build Tools avec ATL et un SDK Windows, Inkscape et ImageMagick. NSIS est nécessaire pour l’installeur. SQLite est inclus. Consultez le [guide de compilation](../en/build.md).

Les tests de non-régression associent des scénarios PowerShell à des outils C++ et C# pour les vérifications internes et les tests réels de l’interface Windows et du Shell. Le [workflow de non-régression GitHub Actions](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) exécute cette suite applicative à la demande et automatiquement avant une publication. Les mêmes cas peuvent être lancés avec `tests.cmd` sur un poste Windows local ou dans une VM Hyper-V configurée. Consultez le [guide des tests](../en/tests/regression/README.md) pour la sélection des cas, les prérequis et les résultats.

<a id="license-and-credits"></a>

## Licence et crédits

<a id="copyright-and-license"></a>

### Droits d’auteur et licence
FileSpacer est distribué sous la licence <img align="absbottom" src="https://img.shields.io/badge/Licence-GPLv3-blue.svg" alt="Licence: GPL v3">.<br>
Copyright © 2026 Benco pour les modifications de FileSpacer. La licence complète se trouve dans [LICENSE](../../LICENSE) dans les sources, ou `LICENSE.txt` dans le dossier d’installation. FileSpacer est fourni sans garantie. Sa redistribution et sa modification sont autorisées dans les conditions de cette licence.

<a id="fork-and-upstream-credits"></a>

### Origine du projet et crédits
FileSpacer est un dérivé de [ChromaFiler](https://github.com/vanjac/chromafiler), créé par J. van’t Hoog (vanjac). Un grand merci à vanjac pour cette base ! Code ChromaFiler d’origine © 2023 J. van’t Hoog.

<a id="third-party-notices"></a>

### Composants tiers
Voir les [mentions des composants tiers](THIRD_PARTY_NOTICES.md) et la [politique de confidentialité](privacy-policy.md).
