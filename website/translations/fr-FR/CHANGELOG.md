# Historique des versions

## À venir

## 1.2.3 — 2026-10-30

- Corriger les identités, les surimpressions d’icônes et les notifications de dossiers du tri mixte ; conserver les positions libres des icônes et utiliser les vues natives si le fournisseur est indisponible ou incompatible.
- Garantir une seule fenêtre Préférences entre processus et corriger la validation des raccourcis, les chemins Everything, les mnémoniques clavier et les traductions régionales.
- Corriger la fermeture des dossiers, les clics d’activation et la restauration après un aperçu du menu Affichage.
- Corriger les rafraîchissements des Jump Lists et les diagnostics de l’installeur ; conserver les fichiers d’aide utilisateur pendant les mises à niveau et la désinstallation.
- Intégrer l’aide multilingue hors ligne par F1 et À propos, et actualiser les guides de configuration traduits et les références Nilesoft Shell.

## 1.2.2 — 2026-10-28

- Corriger l’utilisation du fournisseur de dossiers personnalisé par le tri mixte en disposition libre des icônes. Utiliser le fournisseur natif pour le placement libre et changer de fournisseur à la validation d’une disposition triée, en conservant les positions enregistrées des icônes, la sélection et le focus.

## 1.2.1 — 2026-10-28

- Réduire l’apparition transitoire d’icônes génériques à l’ouverture des dossiers en tri mixte. Préparer les icônes visibles dans le cache Windows avant d’afficher la vue native, avec une attente limitée pour les gestionnaires d’icônes lents.

## 1.2.0 — 2026-10-28

- Ajouter un tri mixte facultatif des fichiers et dossiers du système de fichiers, sous Affichage → Fichiers, en conservant la vue Windows native et l’état enregistré des dossiers. L’option est désactivée par défaut.
- Ajouter les traductions de l’interface en allemand, italien, espagnol, portugais (Portugal) et grec, en plus de l’anglais et du français.
- Afficher le titre et le sous-titre de l’installeur avec du texte Windows plus grand, adapté au DPI et lissé avec ClearType.
- Enrichir le guide en ligne avec des captures françaises et anglaises, des démonstrations des gestes de fenêtre et des comparaisons des menus Nilesoft Shell.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

## 1.1.10 — 2026-10-27

- Demander la confirmation Windows uniquement pour une installation ou une désinstallation pour tous les utilisateurs, et appliquer les choix personnels d’intégration à l’utilisateur ayant démarré l’installation.
- Afficher l’historique complet des versions dans un panneau défilant Nouveautés à côté des choix d’installation sur le premier écran.
- Corriger la réinitialisation des choix de composants lors des mises à niveau ; restaurer les sélections enregistrées et conserver le gestionnaire de fichiers par défaut pendant les mises à niveau silencieuses.
- Utiliser la même barre latérale FileSpacer, le grand logo et la palette gris clair et blanc sur tous les écrans de l’installeur et du désinstalleur. Donner à l’installeur une icône distincte composée des bitmaps natifs de l’application et d’un badge de flèche de téléchargement optimisé séparément à chaque taille, en conservant un fin liseré jaune à droite et en bas du dossier.
- Corriger les plantages à l’arrêt dus à l’initialisation prématurée de données locales de thread sur des threads de travail tardifs du Shell.
- Proposer Ouvrir dans l’Explorateur de fichiers Windows dans les menus contextuels de dossiers lorsque FileSpacer est le gestionnaire par défaut, et Ouvrir dans FileSpacer dans le cas contraire ; actualiser les menus lors du changement de gestionnaire, après l’installation et au démarrage.
- Corriger l’ouverture des raccourcis de serveurs et partages réseau dans l’Explorateur de fichiers Windows lorsque FileSpacer est le gestionnaire par défaut. Mettre à jour le composant de menu contextuel des dossiers et sélectionner à nouveau FileSpacer par défaut pour activer l’intégration réseau.
- Agrandir l’icône de l’application de l’onglet À propos à 48 pixels à l’échelle 100 %, avec le texte à côté.
- Garder les titres des fenêtres maximisées entièrement visibles et étendre la barre d’outils sur toute la largeur disponible.
- Permettre l’exécution des tests de non-régression dans un client Hyper-V configuré, tout en compilant l’application et les outils de test sur l’hôte.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

## 1.1.9 — 2026-10-27

- Prévisualiser les modes d’affichage au survol du menu Affichage, avec restauration progressive en cas d’annulation et une option activée par défaut dans les paramètres d’affichage.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

## 1.1.8 — 2026-10-27

- Remplacer l’icône de l’application et le logo du README par une projection de tesseract, avec un rapport de 11/20 entre les arêtes mauves et vertes du cube, en conservant des petites tailles optimisées.
- Clarifier les libellés français et anglais des paramètres et regrouper les options de verrouillage des fenêtres.
- Ajouter une option pour masquer le badge de cadenas des fenêtres verrouillées sans modifier leur verrouillage d’état.
- Utiliser les arrière-plans natifs du thème Windows dans les pages de paramètres et les contrôles de raccourcis.
- Expliquer le placement des fenêtres et le regroupement dans la barre des tâches avec des infobulles Windows à côté des options.
- Lier les paramètres à la fenêtre de dossier qui les a ouverts et abandonner les modifications en attente à sa fermeture. Les opérations de maintenance confirmées ferment les paramètres sans appliquer les modifications en attente.
- Réorganiser le README autour de la navigation spatiale et des fonctionnalités, avec des références de recherche, un lien vers la dernière version, des guides de compilation et de Nilesoft Shell et une documentation utilisateur enrichie.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

## 1.1.7 — 2026-10-26

- Minimiser le chevauchement des nouvelles fenêtres avec une géométrie exacte, en recherchant d’abord l’espace libre. Renommer l’option générale en « Minimiser le chevauchement des fenêtres ».
- Proposer le choix facultatif du gestionnaire de fichiers par défaut dans l’installeur français et anglais, avec les instructions pour l’activer ou le rétablir plus tard dans les paramètres d’intégration de FileSpacer.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

## 1.1.6 — 2026-10-26

- Corriger la persistance des fenêtres de dossiers lorsqu’un ancêtre est supprimé ou placé dans la Corbeille.
- Corriger les listes de raccourcis proposant de rouvrir des dossiers devenus indisponibles.
- Réduire le chevauchement au placement des fenêtres sans position enregistrée, en ignorant la fenêtre source remplacée lors de la navigation spatiale. Activer ce comportement par défaut avec une option dans Général > Navigation entre les dossiers.
- Déplacer la maintenance de l’état des dossiers de Général vers À propos.
- Ouvrir les nouvelles fenêtres avec un rapport 4:3 équilibré (580 × 435 à 96 DPI), en conservant la hauteur existante et en respectant les dimensions configurées et la taille enregistrée de chaque dossier.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

## 1.1.5 — 2026-10-26

- Corriger la création involontaire d’un raccourci lorsqu’un Alt + double-clic est suivi d’un déplacement de souris. Préserver les propriétés natives, la sélection et le focus sans rejouer un appui maintenu.
- Corriger la couverture de non-régression du délai natif de double-clic et des cibles physiques de la souris ; comparer les propriétés, la sélection, le focus, l’ancre de sélection, la géométrie et le contenu des dossiers avec et sans gestes de fenêtre, y compris avec Ctrl/Maj.
- Isoler l’exécutable réel de test Everything et son fichier INI de démarrage afin que le réglage administrateur installé de l’utilisateur ne puisse pas rediriger les recherches de test ni déclencher une élévation. Vérifier que les paramètres installés restent inchangés.
- Maintenir le poste éveillé et suspendre temporairement l’économiseur d’écran pendant les tests d’interface, en restaurant leur état initial pendant le nettoyage, même après un échec.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

## 1.1.4 — 2026-10-25

Cette version s’appuie sur la 1.1.3, qui propose déjà des gestes clavier/souris configurables pour déplacer et redimensionner les fenêtres.

- Ajouter une recherche externe Everything facultative, avec détection automatique, chemin manuel de l’exécutable, portée limitée au dossier et à ses sous-dossiers ou étendue à tout l’index, et erreurs explicites de lancement.
- Afficher automatiquement Everything et les recherches récentes à l’activation du champ de recherche existant, sans bouton supplémentaire. Conserver la recherche native, le filtrage instantané et la réutilisation des requêtes locales.
- Mémoriser par défaut cinq recherches récentes par dossier entre les sessions, avec une limite de zéro à vingt indépendante d’Everything. Préserver les données existantes et nettoyer l’historique selon la politique prudente de conservation de l’état des dossiers.
- Afficher l’icône de l’exécutable Everything à la taille appropriée, avec extraction en arrière-plan et cache mémoire actualisé lorsque l’exécutable change.
- Ajouter des tests de stockage isolé, de migration et d’intégration réelle Shell/Everything ; rendre le lecteur de tests du Shell robuste lorsque des filtres successifs modifient la vue.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

## 1.1.3 — 2026-10-25

- Ajouter un onglet Raccourcis aux paramètres pour configurer ou désactiver les gestes de déplacement et de redimensionnement avec les touches modificatrices et les boutons gauche, central ou droit, avec saisie directe et validation des conflits.
- Laisser la gestion d’Alt + double-clic sur les propriétés à la vue native de l’Explorateur.
- Restaurer les noms complets sur fond opaque pour un seul élément sélectionné en mode Icônes. Les sélections multiples conservent le rendu natif Windows ; le libellé de la préférence précise maintenant cette portée.
- Ajouter des tests exhaustifs de configuration des raccourcis et de gestes réels de souris, avec les ressources de paramètres françaises et anglaises.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

## 1.1.2 — 2026-10-25

- Ajouter le verrouillage facultatif de la géométrie et de l’affichage enregistrés d’une fenêtre depuis son menu de titre. Les changements temporaires restent possibles et les positions individuelles des icônes continuent de s’enregistrer normalement.
- Afficher un badge de cadenas Small & Flat sur les icônes de dossiers verrouillés, avec des ICO natifs multirésolutions, l’adaptation au DPI et les couleurs à contraste élevé.
- Confirmer la désactivation globale du verrouillage, supprimer les verrouillages enregistrés des dossiers ouverts et fermés et reprendre l’enregistrement normal entre les processus.
- Étendre les tests de non-régression aux transitions de verrouillage, aux échecs d’enregistrement, à la restauration du Shell et au rendu natif du badge.
- Rouvrir un dossier fermé avec Ctrl+Maj+T ou les listes de raccourcis restaure en modes Détails et Liste la ligne qui se trouvait en haut à sa fermeture.
- Les diagnostics de plantage continuent d’écrire un minidump et un rapport dans les données locales de l’application ; l’attribution à l’ancien rendu des noms complets, le changement automatique de préférence et le redémarrage sont supprimés.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

## 1.1.1 — 2026-10-24

- Restaurer l’activation préservant la sélection : le premier clic sur l’arrière-plan vide d’un dossier inactif l’active sans effacer la sélection (préférence réactivée).
- Corriger un plantage à la fermeture d’une fenêtre de dossier pendant l’actualisation de sa vue Shell (commande Actualiser, filtre par nom, commandes Shell Actualiser/Nouveau).

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

## 1.1.0 — 2026-10-24

- Refondre les vues de dossiers pour utiliser les implémentations du Shell Windows via les API COM publiques, au lieu d’adapter les contrôles internes du Shell.
- Conserver l’état spatial des fenêtres, la sélection et le focus, les positions des icônes et le filtrage avec le stockage SQLite de production et les événements publics du Shell.
- Conserver les commandes natives de fichiers et les gestes FileSpacer Alt de déplacement et de redimensionnement ainsi que les propriétés.
- Ouvrir le Panneau de configuration dans sa propre fenêtre Windows en conservant le dossier source et sa sélection.
- Supprimer la sélection automatique du dossier d’origine lors de la navigation vers un parent ou un ancêtre pour des raisons ergonomiques : une sélection ultérieure avec Ctrl ne doit pas ouvrir accidentellement les deux dossiers.
- Accepter trois différences dans cette version : le rendu personnalisé des noms sélectionnés complets sur fond opaque, la conservation de la sélection à l’activation et la restauration exacte du défilement sont indisponibles. Désactiver les deux contrôles de paramètres inactifs tout en conservant leurs préférences enregistrées.
- Simplifier les tests de non-régression autour de la configuration native de production du Shell ; retirer les comparaisons exploratoires de profils, les sondes d’intégration et les rapports réservés à la migration.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

## 1.0.8 — 2026-10-22

- Déplacer les fenêtres avec Alt + glisser gauche et les redimensionner avec Alt + glisser droit depuis la zone principale des fichiers, en conservant la sélection.
- Maintenir la vue Shell classique active indépendamment du réglage d’affichage du nom complet de l’élément sélectionné.
- Appliquer le thème Nilesoft Shell au menu des modes d’affichage.
- Éviter les longs délais d’arrêt dus aux caches passifs d’icônes du Shell, tout en conservant les références de durée de vie acquises par les commandes Shell explicitement invoquées.
- Isoler l’hébergement de la vue Shell et étendre les tests des gestes Alt, de la réentrance des menus et de la durée de vie Shell/STA.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Sélectionner le dossier enfant d’origine à l’ouverture d’un parent ou d’un ancêtre, en préservant la sélection des fenêtres déjà ouvertes.
- Corriger le filtrage des noms de fichiers lors de l’application, de la modification et de l’effacement d’une requête.
- Restaurer l’icône native Windows de l’Accès rapide et centrer les boutons Accès rapide et Configuration.
- Supprimer les diagnostics temporaires et utiliser Benco dans les mentions du projet.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Restaurer automatiquement les fenêtres de dossiers ouvertes après une mise à jour dans l’application, en conservant leurs positions, tailles et réglages d’affichage enregistrés.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Rechercher les mises à jour de façon asynchrone depuis À propos, avec un bouton d’installation et un texte d’état adaptés.
- Télécharger les mises à jour dans une boîte de progression séparée et annulable, tout en gardant les fenêtres de dossiers utilisables.
- Vérifier la taille et le SHA-256 de l’installeur avant de fermer proprement FileSpacer et de démarrer l’installation.
- Publier les métadonnées de téléchargement de l’installeur avec chaque version.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Ajouter une commande au menu du titre pour fermer les fenêtres parentes.
- Rouvrir les dossiers fermés depuis le menu de la barre de titre, les listes de raccourcis ou Ctrl+Maj+T, en restaurant leur affichage, leur sélection et leur défilement.
- Ajouter les dossiers de l’Accès rapide de l’Explorateur aux listes de raccourcis de la barre des tâches.
- Afficher la commande de réouverture uniquement lorsque le dernier dossier fermé n’est pas déjà ouvert, avec son nom et son icône native.
- Exclure la fenêtre courante de la liste des fenêtres du menu du titre.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Restaurer les vérifications manuelles et les vérifications hebdomadaires facultatives des mises à jour à partir des versions GitHub de FileSpacer, désactivées par défaut.

- Publier le manifeste de mise à jour avec chaque installeur de version.

- Conserver la gestion native du Shell pour Alt + double-clic et Alt + Entrée sur les dossiers.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Appliquer les thèmes Nilesoft Shell au menu des dossiers de la barre de titre indépendamment de la sélection, tout en conservant les commandes natives du Shell.
- Permettre une personnalisation Nilesoft distincte pour les menus de la barre de titre et de la barre d’outils.
- Centrer la flèche du menu déroulant de la barre de titre.
- Documenter la configuration de Nilesoft Shell, l’application du thème et le filtrage des menus.
- Mentionner Benco dans les copyrights.
- Permettre au workflow de publication de remplacer l’installeur d’une version existante.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Activer les contrôles natifs de réduction et d’agrandissement, et corriger les boutons de titre des fenêtres maximisées.
- Enregistrer et restaurer séparément l’état maximisé et la géométrie normale de la fenêtre, avec migration automatique de la base de données.
- Utiliser les animations natives de la barre des tâches et les propriétés de relancement propres à chaque dossier.
- Appliquer les préférences de regroupement dans la barre des tâches aux fenêtres nouvellement créées et clarifier ce comportement dans les paramètres.
- Permettre la suppression des dossiers ouverts et effacer leur état enregistré.
- Empêcher l’application et son assistant de récupération de verrouiller des dossiers par leur répertoire de travail.
- Conserver les chemins absolus des dossiers et le mode de test de débogage lors de la récupération après plantage.
- Restaurer immédiatement les libellés normaux après désélection lorsque l’affichage du nom complet est activé.
- Libérer les interfaces Shell en cache à la fermeture des fenêtres de dossiers et corriger la gestion des notifications du Shell.
- Permettre la publication manuelle des versions et inclure le numéro de version dans les noms de fichiers des installeurs.

[Toutes les modifications](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

## 1.0.0 — 2026-10-09

- Première version de FileSpacer pour Windows 10 ou ultérieur, x64, fondée sur ChromaFiler.
- Proposer une navigation spatiale avec une fenêtre par dossier et un état persistant stocké dans SQLite : position et taille des fenêtres, réglages d’affichage, positions des icônes et filtres.
- Suivre les déplacements et renommages de dossiers et activer la fenêtre existante lorsqu’un dossier est rouvert.
- Fermer par défaut la fenêtre source lors de l’ouverture d’un dossier ou de son parent, avec un réglage configurable et Ctrl pour inverser le comportement.
- Ajouter un fil d’Ariane, une barre de chemin modifiable et un filtre de noms, avec filtrage facultatif pendant la saisie.
- Proposer des éléments de barre d’outils configurables, une barre d’état inférieure et des préférences de regroupement dans la barre des tâches.
- Ajouter l’affichage expérimental facultatif du nom complet des éléments sélectionnés.
- Proposer les interfaces française et anglaise, avec l’anglais comme langue de repli.
- Ajouter la récupération après plantage et les commandes de maintenance pour réinitialiser les préférences ou l’état enregistré des dossiers.
- Retirer la navigation en colonnes Miller, l’éditeur de texte intégré, les fenêtres d’aperçu et la barre flottante.
- Fournir des scripts de compilation MSVC/NMAKE, un installeur NSIS, des publications GitHub automatisées et la documentation du projet.

[Historique de la version](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
