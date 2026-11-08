<a id="third-party-notices"></a>

# Mentions des composants tiers

FileSpacer est fondé sur [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, distribué sous [GNU General Public License v3.0](../../LICENSE).

<a id="installer"></a>

## NSIS

L’installeur NSIS utilise le module LockedList, Copyright (c) 2013 Afrow Soft Ltd, sous licence zlib. La mention complète se trouve dans [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) et est incluse sous le nom `LockedList-LICENSE.txt` dans le dossier d’installation. Voir <https://nsis.sourceforge.io/LockedList_plug-in>.

L’AppUserModelID des raccourcis est défini par `ShortcutAppID.nsh` de FileSpacer au moyen des macros Windows COM de NSIS. L’ancien sous-module `nsis-shortcut-properties` n’est pas utilisé. Material Design Icons et l’ancien sous-ensemble de la police Segoe ne sont pas utilisés.

<a id="sqlite"></a>

## SQLite

SQLite 3.53.4 est lié statiquement à l’application. Il appartient au domaine public. Les sources non modifiées et leur provenance se trouvent dans `third_party/sqlite`. Consultez <https://sqlite.org/copyright.html>.

<a id="icons"></a>

## Icônes

<div id="up-and-refresh-button-icons-lucide-and-feather"></div>

Dans la barre d’outils, la **flèche vers le haut** permet de **remonter au dossier parent**. La **flèche circulaire** permet d’**actualiser le contenu du dossier**. Ces deux icônes sont adaptées de **Lucide** (`arrow-up` et `rotate-cw`). La flèche vers le haut provient à l’origine de **Feather**.

Consultez [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) pour les mentions ISC et MIT complètes dans les sources, ou `Lucide-LICENSE.txt` dans le dossier d’installation. Voir <https://lucide.dev/license>.

<div id="small--flat-lock-icon"></div>

Les icônes de cadenas fermé et ouvert (`src/res/window-state-lock.ico` et `src/res/window-state-unlock.ico`) proviennent de l’icône **lock** de la collection Small & Flat de paomedia, placée dans le domaine public sous [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/). Le cadenas ouvert en est une version dérivée.

Collection d’origine et licence : https://github.com/paomedia/small-n-flat

Distribution ICO : https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
