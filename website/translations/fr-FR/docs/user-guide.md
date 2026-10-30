# Utiliser FileSpacer

## Navigation entre les fenêtres

Un dossier possède une seule fenêtre. Rouvrir un dossier déjà ouvert ramène sa fenêtre au premier plan ; position, taille, affichage et disposition des icônes sont mémorisés. Déplacer ou renommer un dossier conserve sa disposition.

- Ouvrir un dossier ou son parent ferme par défaut la fenêtre source. Ctrl inverse ce comportement une fois ; le réglage se change dans **Paramètres > Général**. Ouvrir plusieurs dossiers sélectionnés conserve la fenêtre source.
- Le menu à côté du titre permet de passer d’un dossier ouvert à l’autre. **Fermer les fenêtres parentes** ferme ses ancêtres ouverts, en conservant les autres dossiers.
- **Ctrl+Maj+T** rouvre le dernier dossier fermé sans fermer le dossier courant. La même commande existe dans le menu du titre et la liste de raccourcis de la barre des tâches ; cette dernière permet de remonter plus loin dans les fermetures. La disposition et la sélection sont restaurées, ainsi que la première ligne visible dans les modes Détails et Liste.
- La liste de raccourcis ouvre également les dossiers épinglés dans l’Accès rapide de l’Explorateur de fichiers.

Revenir au parent ne sélectionne pas le dossier qui vient d’être quitté. Un parent déjà ouvert conserve sa sélection. Le Panneau de configuration s’ouvre dans sa propre fenêtre Windows.

![Le menu du titre, avec les commandes de navigation et de disposition enregistrée.](images/window-menu.png)

## Affichage et disposition

### Disposition des icônes

Les fichiers et sous-dossiers peuvent être disposés librement en mode Icônes lorsque **Réorganiser automatiquement les icônes** est désactivé. Leurs positions sont enregistrées indépendamment des dimensions de la fenêtre, même lorsque son état enregistré est verrouillé.

### Options d’affichage

**Afficher le nom complet d’un fichier sélectionné**, dans **Paramètres > Affichage**, rend un nom sélectionné entièrement lisible en mode Icônes. L’option est activée par défaut et ne change ni les sélections multiples ni les autres modes.

![Un nom de fichier long, sélectionné et affiché en entier en mode Icônes.](images/selected-name.png)

**Prévisualiser les modes d’affichage dans le menu Affichage**, également activé par défaut, montre chaque mode au survol. Un clic le conserve ; Échap ou la sortie de l’aperçu restaure l’affichage initial. Un aperçu ne remplace jamais la disposition enregistrée.

**Mélanger les dossiers et les fichiers dans le tri** les trie ensemble, comme sous [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) et [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). L’option est désactivée par défaut dans **Paramètres > Affichage > Fichiers**. Elle concerne les modes Détails, Liste et Icônes réorganisées automatiquement ; les dispositions libres sont conservées. Les dossiers virtuels et les archives gardent l’ordre de Windows.

![Dossiers et fichiers mélangés dans le même ordre alphabétique.](images/mixed-sorting.png)

La visibilité de la barre d’outils et de la barre d’état se règle également dans les paramètres.

### Placement des fenêtres

Les dossiers sans position enregistrée s’ouvrent dans l’espace libre, ou avec un chevauchement réduit si la place manque. Les positions enregistrées restent prioritaires. **Paramètres > Général > Placer les nouvelles fenêtres dans l’espace libre** est activé par défaut ; sa désactivation utilise le placement de Windows sans déplacer les fenêtres existantes.

### Sélection et activation

**Conserver la sélection lors d’un clic sur une fenêtre inactive**, dans **Paramètres > Général**, permet d’activer un dossier par un clic sur son arrière-plan sans effacer sa sélection. Un second appui commence un rectangle de sélection. Les clics sur les éléments, les clics droits et les gestes de fenêtre gardent leur fonctionnement habituel.

## Gestion des fenêtres

### Déplacement et redimensionnement

Une fenêtre peut être déplacée ou redimensionnée en faisant glisser depuis la zone des fichiers, y compris sur un élément. Les raccourcis par défaut sont :

| Action | Raccourci |
| --- | --- |
| Déplacer la fenêtre | Alt + glisser avec le bouton gauche |
| Redimensionner depuis le coin inférieur droit | Alt + glisser avec le bouton droit |

![Déplacer une fenêtre avec Alt et un glisser du bouton gauche.](images/move-window.gif)

![Redimensionner une fenêtre avec Alt et un glisser du bouton droit.](images/resize-window.gif)

La sélection est conservée. Échap annule le geste. Champs de texte, en-têtes, barres de défilement, barres d’outils et menus gardent leur fonctionnement normal ; Alt + double-clic ouvre les propriétés d’un élément.

Chaque action possède son raccourci personnalisable dans **Paramètres > Raccourcis**. Les boutons gauche et droit nécessitent Alt ; le bouton central peut aussi s’utiliser sans Alt. Les modificateurs doivent correspondre exactement ; les combinaisons réservées ou en double sont refusées. Les changements s’appliquent aux fenêtres ouvertes ; Rétablir revient aux derniers raccourcis appliqués et Annuler abandonne les modifications en attente.

### Verrouillage de l’état enregistré

Le verrouillage conserve la position, la taille et l’affichage d’un dossier pour les retrouver à sa réouverture. La fonction est désactivée par défaut dans **Paramètres > Général > Activer le verrouillage de l’état des fenêtres**. Une fois activée, **Verrouiller l’état de la fenêtre** est disponible à côté du titre du dossier. Un badge de cadenas sur son icône signale un état verrouillé.

<img src="images/locked-title.png" alt="Gros plan du titre du dossier et de son badge de cadenas." width="280">

Les changements temporaires restent possibles ; rouvrir le dossier rétablit l’état verrouillé. Les positions individuelles des icônes continuent de s’enregistrer. **Déverrouiller l’état de la fenêtre** conserve la disposition actuelle. Masquer le badge ne supprime pas le verrouillage.

Désactiver la fonction demande confirmation et supprime tous les verrouillages, y compris ceux des dossiers fermés. La réactiver ne les restaure pas.

## Recherche et historique

Saisir tout ou partie du nom d’un fichier ou d’un dossier, puis appuyer sur Entrée ou cliquer sur la loupe pour filtrer le dossier courant. **Filtrer pendant la saisie**, dans **Paramètres > Général**, applique le filtre au fur et à mesure. Ce filtre local ne lance jamais Everything automatiquement.

Le champ propose les recherches récentes et, lorsque l’intégration est activée, **Rechercher dans Everything**. Haut/Bas et Entrée choisissent une suggestion ; Entrée sans suggestion sélectionnée applique le filtre local. Échap ferme la liste sans effacer le texte ; Ctrl+F active le champ.

L’historique conserve par défaut cinq recherches différentes validées par dossier. Sa limite se règle de 0 à 20 dans les paramètres généraux ; 0 l’efface et le désactive. La saisie seule n’ajoute pas d’entrée. [Everything](#everything) permet de chercher dans les sous-dossiers ou dans tous les fichiers indexés.

![Le champ de recherche avec Everything et les suggestions de recherches récentes.](images/search.png)

## Intégration

### Intégration Windows

FileSpacer peut être défini comme gestionnaire de fichiers par défaut à l’installation ou ensuite dans **Paramètres > Intégration**, avec **Définir comme explorateur par défaut**. **Rétablir l’explorateur par défaut** rend l’ouverture des dossiers à l’Explorateur de fichiers.

Le menu contextuel des dossiers propose **Ouvrir dans FileSpacer**, ou **Ouvrir dans l’Explorateur de fichiers Windows** lorsque FileSpacer est le gestionnaire par défaut. Il fonctionne aussi sur les lecteurs, dossiers ZIP, emplacements réseau et arrière-plans de dossier, sans imposer de changer l’application par défaut.

Les fichiers s’ouvrent avec leur application Windows habituelle. Dossiers réseau, aperçus et commandes ajoutées au menu contextuel suivent également le fonctionnement de Windows. **Options de l’Explorateur de fichiers** ouvre les préférences de dossiers communes, dont certaines s’appliquent aussi à FileSpacer.

### Everything

Une installation existante d’[Everything](https://www.voidtools.com/) peut être intégrée pour lancer des recherches avancées depuis le champ de FileSpacer. Les résultats s’ouvrent dans Everything.

L’option s’active dans **Paramètres > Intégration**. Everything peut être retrouvé automatiquement ou avec **Détecter** ; une version portable peut être choisie manuellement. La portée est le dossier courant et ses sous-dossiers, ou **Partout** ; les dossiers virtuels nécessitent Partout.

Choisir **Rechercher dans Everything** transmet la requête saisie, avec la syntaxe d’Everything. Un lancement réussi efface le champ et le filtre local de FileSpacer ; une erreur les conserve pour un nouvel essai.

![Une requête dans FileSpacer et les résultats correspondants dans Everything, à côté.](images/everything-search.png)

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) peut gérer l’apparence des menus de FileSpacer et personnaliser leurs commandes. Une adaptation de sa configuration est recommandée pour une bonne intégration visuelle, notamment des coins carrés pour les menus de la barre de titre et de la barre d’outils.

La [référence de configuration](nilesoft-shell.md) explique comment autoriser FileSpacer, conserver le thème et limiter à l’Explorateur les règles qui lui sont réservées. Après une modification, désenregistrer puis réenregistrer Nilesoft Shell l’applique aux menus nouvellement ouverts.

Le thème Modern ci-dessous compare ses coins par défaut aux coins carrés recommandés.

**Menu de navigation**

| Coins par défaut | Coins carrés recommandés |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu de navigation avec les coins arrondis par défaut." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Le même menu avec des coins carrés." width="320"> |

**Menu principal (…)**

| Coins par défaut | Coins carrés recommandés |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principal avec les coins arrondis par défaut." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Le même menu avec des coins carrés." width="320"> |

## Paramètres et mises à jour

Ctrl+, ouvre les paramètres, même si la barre d’outils est masquée. L’interface suit la langue d’affichage de Windows, avec l’anglais comme repli ; un changement de langue nécessite un redémarrage. F1 ouvre le guide installé, également disponible hors connexion.

Les mises à jour se recherchent dans **Paramètres > À propos**. Le téléchargement et l’installation nécessitent une confirmation ; l’installation n’est jamais automatique. Les dossiers précédemment ouverts retrouvent ensuite leur disposition enregistrée. Les vérifications hebdomadaires sont facultatives et désactivées par défaut ; elles s’activent dans À propos.

## Données locales

Dispositions des dossiers, verrouillages, recherches et historique des dossiers fermés sont conservés dans `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. Les préférences générales se trouvent dans le registre Windows, sous `HKEY_CURRENT_USER\Software\FileSpacer`. L’état enregistré peut contenir des chemins de dossiers, y compris réseau. Les fichiers temporaires de mise à jour et la liste des dossiers à rouvrir restent dans `%TEMP%\FileSpacerUpdate-{GUID}`.

**Paramètres > À propos > Maintenance > Réinitialiser l’état de tous les dossiers** ferme les fenêtres de dossiers et efface leurs dispositions, historiques et verrouillages. Les fichiers et les préférences générales sont conservés.

Pour le dépannage, `FileSpacer.exe /reset-options` réinitialise les préférences générales ; `/reset-all` efface aussi l’état des dossiers. Aucune de ces commandes ne supprime les fichiers, ne change les associations Windows ni n’efface les rapports de diagnostic existants.

## Confidentialité

FileSpacer ne contient ni télémétrie, ni suivi d’usage, ni publicité. Aucun rapport de plantage n’est envoyé automatiquement. Les éventuels rapports restent dans `%LOCALAPPDATA%\FileSpacer` ; leur partage est facultatif et peut révéler des chemins de fichiers ou des portions de mémoire d’un processus.

Les recherches de mises à jour et les téléchargements contactent GitHub en HTTPS. GitHub reçoit les informations habituelles de connexion, dont l’adresse IP et la version de l’application, mais aucun état de dossier, chemin de fichier ou rapport de diagnostic.

Les identifiants d’accès aux lecteurs réseau sont saisis et gérés par Windows, comme dans l’Explorateur de fichiers. FileSpacer ne les lit ni ne les enregistre. Choisir une recherche Everything transmet la requête et, si elle est limitée au dossier, son chemin à l’application Everything locale.
