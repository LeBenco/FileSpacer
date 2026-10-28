# Utiliser FileSpacer

## Navigation entre les fenêtres

Chaque dossier possède une fenêtre qui mémorise sa position, sa taille et ses réglages d’affichage. Rouvrir un dossier déjà ouvert active sa fenêtre. Déplacer ou renommer un dossier conserve sa disposition enregistrée.

- Ouvrir un dossier ou son parent ferme par défaut la fenêtre source. Maintenez Ctrl pour inverser ce comportement une fois, ou changez le réglage dans **Paramètres > Général**. Ouvrir plusieurs dossiers sélectionnés conserve la fenêtre source.
- Le menu à côté du titre du dossier permet de passer d’un dossier ouvert à l’autre. **Fermer les fenêtres parentes** ferme ses ancêtres ouverts tout en conservant les autres dossiers.
- **Ctrl+Maj+T**, ou **Rouvrir la dernière fenêtre fermée** dans le menu de la barre de titre ou la liste de raccourcis de la barre des tâches, rouvre un dossier fermé sans fermer le dossier courant. Sa disposition et sa sélection sont restaurées ; les modes Détails et Liste restaurent aussi la première ligne visible. La commande de la barre des tâches permet de remonter les fermetures précédentes.
- La liste de raccourcis de la barre des tâches ouvre également les dossiers épinglés dans l’Accès rapide de l’Explorateur de fichiers.

Revenir au parent ne sélectionne pas le dossier dont vous venez. Un parent déjà ouvert conserve sa sélection. Le Panneau de configuration s’ouvre dans sa propre fenêtre Windows.

![Le menu du titre du dossier avec la navigation entre les fenêtres et le verrouillage de l’état enregistré.](images/window-menu.png)

*Le menu du titre, illustré ici avec l’état de la fenêtre verrouillé.*

## Affichage et disposition

### Disposition des icônes

Désactivez **Réorganiser automatiquement les icônes** dans un mode à icônes pour disposer librement les fichiers et sous-dossiers. Leurs positions sont enregistrées indépendamment de la taille et de la position de la fenêtre, y compris lorsque son état enregistré est verrouillé.

### Options d’affichage

La visibilité de la barre d’outils et de la barre d’état se règle dans **Paramètres**.

Dans **Paramètres > Affichage**, deux options sont activées par défaut :

- **Afficher le nom complet d’un fichier sélectionné** garde un nom sélectionné lisible en mode Icônes. Cela ne change ni les sélections multiples ni les autres modes d’affichage.
- **Prévisualiser les modes d’affichage dans le menu Affichage** montre un aperçu du mode survolé. Cliquez pour le conserver ; Échap ou la sortie de l’aperçu restaure l’affichage initial. L’aperçu ne remplace pas la disposition enregistrée.

**Mélanger les dossiers et les fichiers dans le tri**, dans **Paramètres > Affichage > Fichiers**, est désactivé par défaut. Activez cette option pour trier ensemble dossiers et fichiers, comme sous [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) et [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Elle s’applique aux modes Détails, Liste et Icônes réorganisées automatiquement. Les dispositions libres sont conservées. Les dossiers virtuels et les archives conservent l’ordre de Windows.

![Une fenêtre contenant des dossiers et fichiers fictifs mélangés dans l’ordre alphabétique.](images/mixed-sorting.png)

*Dossiers et fichiers partagent le même ordre alphabétique.*

### Placement des fenêtres

**Paramètres > Général > Placer les nouvelles fenêtres dans l’espace libre** est activé par défaut. Les dossiers sans position enregistrée s’ouvrent dans l’espace disponible, avec un chevauchement réduit lorsqu’il n’y a plus d’espace libre. Les positions enregistrées restent prioritaires. Désactiver cette option utilise le placement par défaut de Windows sans déplacer les fenêtres existantes.

### Sélection et activation

Lorsque **Paramètres > Général > Conserver la sélection lors d’un clic sur une fenêtre inactive** est activé, cliquer sur l’arrière-plan d’un dossier inactif l’active sans effacer la sélection. Un second appui permet de commencer un rectangle de sélection. Les clics sur les éléments, les clics droits et les gestes de fenêtre conservent leur comportement habituel.

## Gestion des fenêtres

### Déplacement et redimensionnement

Faites glisser dans la zone des fichiers, sur son arrière-plan ou sur un élément, avec les raccourcis par défaut :

| Action | Raccourci |
| --- | --- |
| Déplacer la fenêtre | Alt + glisser avec le bouton gauche |
| Redimensionner depuis le coin inférieur droit | Alt + glisser avec le bouton droit |

**Déplacer la fenêtre — Alt + glisser gauche**

![Déplacement d’une fenêtre de dossier en maintenant Alt et en faisant glisser avec le bouton gauche dans la zone des fichiers.](images/move-window.gif)

**Redimensionner la fenêtre — Alt + glisser droit**

![Redimensionnement d’une fenêtre de dossier en maintenant Alt et en faisant glisser avec le bouton droit dans la zone des fichiers.](images/resize-window.gif)

La sélection est conservée. Échap annule le geste et restaure les dimensions et la position initiales. Les champs de texte, en-têtes, barres de défilement, barres d’outils et menus gardent leur comportement habituel. Alt + double-clic sur un élément ouvre ses propriétés.

Dans **Paramètres > Raccourcis**, activez ou désactivez chaque action et développez-la pour modifier son raccourci. La zone de saisie directe enregistre une combinaison de touches et de bouton. Les boutons gauche et droit nécessitent Alt ; le bouton central accepte aussi les combinaisons sans Alt. Les modificateurs doivent correspondre exactement. Les raccourcis réservés ou en double ne peuvent pas être appliqués.

Le bouton de valeur par défaut réinitialise une action. **Rétablir** restaure les derniers raccourcis appliqués ; **Annuler** abandonne les modifications en attente. **Appliquer** ou **OK** rend les modifications disponibles dans les fenêtres FileSpacer ouvertes.

### Verrouillage de l’état enregistré

Activez **Paramètres > Général > Activer le verrouillage de l’état des fenêtres** (désactivé par défaut), puis choisissez **Verrouiller l’état de la fenêtre** dans le menu à côté du titre du dossier. Le verrouillage enregistre sa position, sa taille et ses réglages d’affichage. Un **badge de cadenas sur l’icône du dossier dans la barre de titre**, à côté de son nom, identifie un état verrouillé.

<img src="images/locked-title.png" alt="Gros plan de la barre de titre : un badge de cadenas recouvre l’icône du dossier à côté de Projet studio." width="420">

*Le badge de cadenas indique que l’état enregistré est verrouillé.*

Vous pouvez toujours déplacer et redimensionner temporairement la fenêtre, ou changer son affichage. Rouvrir le dossier restaure l’état verrouillé. Les positions individuelles des icônes continuent de s’enregistrer normalement. **Déverrouiller l’état de la fenêtre** conserve la disposition actuelle. Le badge peut être masqué dans les paramètres généraux sans supprimer le verrouillage.

Désactiver la fonction demande confirmation et supprime tous les verrouillages enregistrés, y compris ceux des dossiers fermés. La réactiver ne les restaure pas.

## Recherche et historique

Saisissez du texte dans le champ de recherche, puis appuyez sur Entrée ou cliquez sur la loupe pour filtrer le dossier courant. **Paramètres > Général > Filtrer pendant la saisie** applique le filtre au fur et à mesure. Ce filtre local ne lance jamais Everything automatiquement.

Activer le champ affiche les recherches récentes et, lorsque l’intégration est activée, **Rechercher dans Everything**. Utilisez Haut/Bas et Entrée pour choisir une entrée, ou cliquez dessus. Entrée sans suggestion sélectionnée applique le filtre local. Échap ferme la liste sans effacer le texte ; Ctrl+F active à nouveau le champ.

L’historique conserve par défaut cinq recherches différentes validées par dossier. Modifiez cette limite de 0 à 20 dans les paramètres généraux ; 0 efface et désactive l’historique. La saisie seule n’ajoute pas d’entrée. Consultez [Everything](#everything) pour rechercher dans les sous-dossiers ou dans tous les fichiers indexés.

![Suggestions de recherche avec Everything et des requêtes récentes sur des fichiers de projet fictifs.](images/search.png)

*Everything est une action distincte ; les recherches récentes filtrent le dossier courant.*

## Intégration

### Intégration Windows

L’installeur propose **Définir FileSpacer comme gestionnaire de fichiers par défaut**, décoché par défaut. Il installe aussi le composant de menu contextuel des dossiers. Vous pouvez ensuite modifier ce choix dans **Paramètres > Intégration** avec **Définir comme navigateur par défaut** ; **Réinitialiser le navigateur par défaut** rend l’ouverture des dossiers à l’Explorateur de fichiers Windows.

Le composant de menu contextuel propose **Ouvrir dans l’Explorateur de fichiers Windows** lorsque FileSpacer est le gestionnaire par défaut, et **Ouvrir dans FileSpacer** dans le cas contraire. Il s’applique aux dossiers, lecteurs, dossiers compressés, emplacements réseau et arrière-plans de dossiers. Installer ce composant seul ne change pas l’application par défaut.

**Options de l’Explorateur de fichiers**, sur la même page de paramètres, ouvre les préférences de dossiers Windows, dont certaines affectent aussi l’affichage des fichiers dans FileSpacer.

### Everything

Installez [Everything](https://www.voidtools.com/) séparément, puis activez-le dans **Paramètres > Intégration**. Laissez le chemin de l’exécutable vide pour la détection automatique, utilisez **Détecter**, ou choisissez manuellement son fichier `.exe` pour une installation portable.

Choisissez la portée de la recherche : le dossier courant et ses sous-dossiers, ou **Partout**. Les dossiers virtuels n’ont pas de portée sur le système de fichiers ; utilisez Partout pour eux.

Saisissez une requête dans le champ de recherche de FileSpacer et choisissez **Rechercher dans Everything** dans les suggestions. Les résultats s’ouvrent dans Everything, avec son index et sa syntaxe de recherche. Un lancement réussi efface le champ et le filtre local de FileSpacer ; en cas d’erreur, ils sont conservés pour vous permettre de réessayer.

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) permet de modifier l’apparence des menus de FileSpacer et de personnaliser leurs entrées. Il est facultatif et s’installe séparément.

Autorisez FileSpacer dans la règle d’exclusion de votre configuration Nilesoft et conservez l’import de votre thème. Limitez à l’Explorateur les règles qui lui sont destinées afin qu’elles ne modifient pas aussi les menus de FileSpacer.

Consultez la [référence de configuration et les exemples](nilesoft-shell.md) pour les réglages exacts, les règles propres à chaque menu et l’apparence recommandée. Après une modification, désenregistrez et réenregistrez Nilesoft Shell pour l’appliquer aux menus nouvellement ouverts.

Le thème Modern ci-dessous utilise ses coins arrondis par défaut à gauche et les coins carrés recommandés à droite.

**Menu de navigation**

| Coins par défaut | Coins carrés recommandés |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menu de navigation de la barre de titre avec les coins arrondis par défaut du thème Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="Le même menu de navigation avec les coins carrés recommandés." width="320"> |

**Menu principal (…)**

| Coins par défaut | Coins carrés recommandés |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menu principal avec les coins arrondis par défaut du thème Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="Le même menu principal avec les coins carrés recommandés." width="320"> |

## Paramètres et mises à jour

**Préférences.** **Ctrl+,** ouvre les paramètres, même lorsque la barre d’outils est masquée. L’interface suit la langue d’affichage de Windows, avec l’anglais comme langue de repli ; un changement de langue nécessite un redémarrage.

**Mises à jour.** Recherchez une nouvelle version dans **Paramètres > À propos**, puis choisissez **Installer FileSpacer vX.Y.Z** pour mettre à jour. L’installation nécessite une confirmation ; les mises à jour ne s’installent jamais automatiquement. Après l’installation, FileSpacer rouvre les dossiers précédemment ouverts avec leur disposition enregistrée. Les vérifications hebdomadaires facultatives sont désactivées par défaut et peuvent être activées dans À propos.

**Maintenance.** **Paramètres > À propos > Maintenance > Réinitialiser l’état de tous les dossiers** ferme les fenêtres de dossiers ouvertes et efface leurs dispositions enregistrées, leur historique et les verrouillages d’état. Vos fichiers et vos préférences générales sont conservés.

**Documentation.** Choisissez **Documentation** au-dessus de **Paramètres de FileSpacer** dans le menu principal, appuyez sur **F1** ou utilisez le bouton dans **Paramètres > À propos**. Le guide installé s’ouvre dans votre navigateur dans la langue d’affichage de Windows, avec l’anglais comme langue de repli. Les pages, images et recherches fonctionnent hors connexion ; le sélecteur de langue permet de changer de traduction.
