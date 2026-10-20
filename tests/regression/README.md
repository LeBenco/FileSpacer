# FileSpacer — bibliothèque de tests de non-régression

Les jeux restent indépendants et sont regroupés par fonctionnalité dans `cases`. La bibliothèque contient 30 jeux dans 15 groupes. Le nombre de jeux est distinct du nombre d'assertions ou d'interactions.

## Installation et fichiers à conserver

Pour installer une base complète propre, renommer d'abord l'ancien dossier `tests` en sauvegarde à la racine du dépôt, puis copier le nouveau dossier `tests`. Ne pas placer la sauvegarde sous `tests/regression/cases`, car le lanceur y découvrirait aussi les anciens jeux. Conserver `tests.cmd` à la racine du dépôt. Une livraison partielle remplace seulement les fichiers qu'elle contient ; conserver les autres jeux et vos ajouts. Réintégrer les éventuels ajouts personnels après comparaison avec la sauvegarde.

Dans `tests/regression`, conserver ce README, `Run-Regression.ps1` et tout le contenu de `cases` et `common`. Les fichiers C++, C#, `.inc`, en-têtes, ressources et `common/TestExecutable.manifest` sont des dépendances des jeux. L'archive complète fournit aussi `tests.cmd`, à copier à la racine du dépôt. Les notes de livraison et rapports ponctuels sont fournis hors de `tests`.

## Lancement

Depuis CMD, à la racine du dépôt :

```bat
tests.cmd
tests.cmd -Category Internal
tests.cmd -Category UI
tests.cmd -Pattern "07-fenetres\*"
tests.cmd -Pattern "12-navigation-spatiale\*"
tests.cmd -Pattern "13-preferences\*"
tests.cmd -Pattern "14-disposition\*"
```

`tests.cmd` construit l'application avec `build.cmd debug`, initialise MSVC x64 et appelle le lanceur sous Windows PowerShell 5.1. Aucun framework de test supplémentaire n'est requis. Les scénarios internes utilisent C++/MSVC ; les interactions UI utilisent PowerShell et C#/.NET avec Win32 et COM.

Prérequis : Windows 10 x64, dépendances habituelles du build du projet, composants C++/ATL/SDK de Visual Studio 2022 Build Tools, et TEMP sur NTFS pour le jeu spatial. Fermer FileSpacer avant les jeux UI ; laisser un bureau interactif et le clavier disponibles pendant leur exécution.

## Découverte et résultats

Le lanceur découvre récursivement les fichiers `NN-Nom.ps1` sous `cases`, trie leurs chemins et les exécute séquentiellement. Les helpers de `common` ne sont pas lancés comme des jeux. Chaque jeu dispose de son propre processus PowerShell, ce qui évite les collisions de classes chargées par `Add-Type`.

Les résultats, journaux, programmes compilés et bases temporaires se trouvent dans `build/regression/<date>-<identifiant>`. `results.json` décrit les jeux exécutés ; `summary.json` indique le résultat global et le nombre de jeux attendus/exécutés. Les traces de sélection et les éventuelles traces temporaires de recherche produites par l'application, lorsqu'elles existent pour les processus des jeux UI, sont copiées dans leur dossier `application-traces`.

Les tableaux JSON des rapports sont explicitement énumérés pour obtenir les mêmes enregistrements sous Windows PowerShell 5.1 et PowerShell 7. Les autotests `00-banc` vérifient notamment qu'un rapport de plusieurs sorties normales reste PASS et que chaque sortie anormale apparaît avec son PID et son code.

Le délai maximal par jeu est de 300 secondes, réglable avec `-TimeoutSeconds`. Un échec, un délai dépassé ou un nettoyage nécessaire reste un FAIL. Le lanceur poursuit les jeux suivants après nettoyage, si aucun processus de l’exécutable testé ne reste actif. Il arrête la suite si cette isolation ne peut pas être garantie. Les jeux non exécutés ne sont jamais considérés comme réussis. Le code de sortie est 0 uniquement si tous les jeux sélectionnés ont réussi et si la restauration a réussi ; sinon il est 1, y compris lorsque des jeux sont bloqués.

Après un jeu UI, le lanceur attend jusqu'à cinq secondes la fin normale des processus FileSpacer de l'exécutable Debug testé, notamment des auxiliaires de récupération. Cette attente s'arrête dès qu'ils ont tous terminé. Si des processus persistent, leurs PID, PID parents et lignes de commande, lorsque accessibles, sont consignés dans `leftover-processes.json` avant le nettoyage ; le jeu reste en échec.

## Isolation

Les jeux de l'application utilisent Debug avec `/test`, des dossiers temporaires et le registre `HKCU/Software/FileSpacer/Test`. Le lanceur sauvegarde puis restaure le registre de test et la base SQLite de test, avec ses fichiers WAL/SHM. Les jeux UI doivent passer par ce lanceur pour bénéficier de cette préparation.

Pour isoler aussi les effets sur les documents récents et les listes de raccourcis du Shell, utiliser un profil Windows réservé aux essais ou une VM : `/test` ne sépare pas ces historiques Shell.

Les programmes de test compilés utilisent un manifeste UTF-8. Cette option nécessite Windows 10 version 1903 ou ultérieure ; sur une version antérieure, utiliser des chemins de travail ASCII.

## Couverture

| Groupe | Objet des jeux |
|---|---|
| `00-banc` | Conservation des causes initiales, classement FAIL/BLOCKED, collecte de toutes les sorties et diagnostics des processus. |
| `01-stockage-sqlite` | Stockage, accès concurrents, identité spatiale, géométrie et adaptateurs de vue. |
| `02-retention-quota` | Péremption, reprise, transactions, quota et priorité des fenêtres ouvertes. |
| `03-menus` | Réentrance et fermeture pendant une commande, libération des ressources. |
| `04-shell-sta` | Durée de vie, références Shell, événements et appels COM réels. |
| `05-dimensions` | Validation des dimensions avec les contrôles natifs Windows. |
| `06-etat-vue` | Restauration complète ou partielle de vue et de positions. |
| `07-fenetres` | Agrandissement, réduction, réouverture et boutons de titre ; commandes synchrones, asynchrones et fermeture précoce avant disponibilité de la vue Shell. |
| `08-suppression` | Suppression du stockage et notifications filesystem/Shell réelles. |
| `09-historique` | Historique des fenêtres fermées et réouverture ; il ne s'agit pas de l'historique de recherche. |
| `10-manifeste-maj` | Validation des manifestes de mise à jour. |
| `11-session-maj` | Instantanés de fenêtres et restauration après mise à jour. |
| `12-navigation-spatiale` | Entrée, Alt+Haut, préférence/Ctrl, sélection d'origine et destination déjà ouverte. |
| `13-preferences` | Propagation aux fenêtres ouvertes de deux processus, Appliquer/Annuler, recherche pendant la saisie, requêtes successives et notifications sous filtre. |
| `14-disposition` | Configurations représentatives, deux largeurs, absence de chevauchement et espace récupéré au DPI réel. |

Les jeux internes C++ compilent les sources courantes du dépôt. Le groupe `00-banc` vérifie les helpers PowerShell directement. Pour certaines méthodes privées, `common/Prepare-Sources.ps1` extrait les corps des fonctions sans réécrire leur logique ; une signature absente ou ambiguë fait échouer la préparation. Les empreintes des sources utilisées sont enregistrées avec le résultat du jeu. Certains jeux utilisent des doubles pour déclencher des erreurs déterministes ; les jeux UI exercent les fenêtres et contrôles réels.

## Ajouter et valider un jeu

Ajouter un fichier `NN-Nom.ps1` dans un groupe existant ou un nouveau sous-répertoire numéroté. La première ligne doit être `# Test-Category: Internal` ou `# Test-Category: UI`, et le script doit accepter `-ProjectRoot`. Une assertion non satisfaite doit lever une exception ou produire un code de sortie non nul.

Valider le nouveau jeu et la fonctionnalité sur Windows, puis lancer toute la suite existante. Après réussite, conserver ce jeu dans la bibliothèque. Le lanceur le découvrira automatiquement sans modification. Cette convention ne constitue pas une promotion automatique ni un workflow CI.

Les échecs de nettoyage doivent rester visibles sans masquer une erreur fonctionnelle antérieure. Une compilation ou une analyse statique réussie ne remplace pas l'exécution des scénarios Win32/COM sous Windows.

## Contrôles contre les faux succès

La disparition d'une fenêtre ne suffit pas à valider la fermeture. Les processus lancés par les jeux UI sont attendus et leur code de sortie doit être nul. Le helper des nouveaux jeux enregistre aussi le processus propriétaire des fenêtres atteintes par navigation, même si celui-ci diffère du lanceur initial. Les jeux historiques de suppression et d'identité spatiale contrôlent maintenant également les sorties finales de leurs processus.

Après Entrée et Alt+Haut, les quatre jeux de politique de navigation attendent le premier plan produit par FileSpacer. Ils n'appellent pas le helper d'activation pour réparer le résultat de la navigation. Le jeu du parent déjà ouvert conserve son contrôle distinct d'activation, de sélection, d'unicité et de positions.

Le classement `BLOCKED` exige un préalable explicitement signalé, une sortie normale des processus FileSpacer suivis et un rapport `cleanup-complete.json` positif. Une erreur de nettoyage, un rapport absent ou illisible, un crash du processus de test ou un délai global dépassé reste `FAIL`. Un résultat bloqué n'est jamais une validation ni une preuve que FileSpacer est exempt de défaut.

Les tests internes d'adaptateurs et d'état de vue extraient les fonctions courantes et neutralisent seulement `TRACE_SELECTION`, journal d'investigation non inclus dans ces unités. Leur réussite ne valide pas ce journal ni son absence de réentrance. Les jeux UI utilisent l'exécutable réel sans désactiver son diagnostic et contrôlent les sorties des processus suivis. Les tests ne corrigent pas les sources de l'application pour obtenir un succès.

## Scénarios indépendants de navigation et de préférences

Les quatre combinaisons préférence/Ctrl sont exécutées dans les jeux 01 à 04 de `12-navigation-spatiale`. Le jeu 05 contrôle le parent déjà ouvert. Les assertions historiques sont conservées ; chaque scénario dispose de sa préparation, de son processus PowerShell et de son nettoyage. Un échec dans une combinaison ne bloque plus les suivantes.

Dans `13-preferences` :

- `01-FenetresOuvertes.ps1` conserve les contrôles entre processus, Appliquer/Annuler, la persistance et l’activation de la recherche instantanée avec un texte déjà présent. L’assertion de filtrage reste inchangée.
- `02-Activation.ps1` vérifie le passage du dialogue au premier plan depuis un propriétaire au premier plan, le retour au propriétaire et une seconde ouverture. Le banc n’active jamais lui-même le dialogue.
- `03-RecherchePendantSaisie.ps1` contrôle les changements du texte, l’absence de résultat et l’effacement lorsque la recherche instantanée est activée avant l’ouverture.
- `04-RechercheExplicite.ps1` contrôle le bouton de recherche et le retrait du filtre lorsque la recherche instantanée est désactivée.

Un PASS du contrôle synchrone des fenêtres ne remplace pas celui de `02-CommandesAsynchrones.ps1`, qui utilise `PostMessage(WM_SYSCOMMAND)` et conserve le passage dans la boucle de messages de l’application.

`PRECONDITION` dans le test d’activation signifie que le banc n’a pas pu établir le propriétaire au premier plan. Le scénario n’est pas validé ; cela ne suffit pas à attribuer un défaut à FileSpacer. Une fois ce préalable établi, l’activation du dialogue est une assertion applicative.

Les recherches par messages modifient le vrai contrôle EDIT et interrogent la vraie vue Shell. Elles ne couvrent pas tous les raccourcis ou toutes les interactions clavier. Dans un même jeu, la première assertion en échec arrête ce scénario. Une vérification de syntaxe ou une simulation du lanceur ne valide pas les interactions réelles. Aucun nombre fini de scénarios ne garantit l’absence de tous les bugs.

## Statuts du résumé

- `PASS` : les assertions du jeu ont réussi.
- `FAIL` : le scénario est en erreur ; le statut ne suffit pas à attribuer la cause à FileSpacer plutôt qu’au banc. La première erreur et les éventuelles erreurs de nettoyage sont affichées.
- `BLOCKED` : un préalable de préparation du premier plan par le banc n’a pas pu être établi. Le scénario n’est pas validé. Ce statut provient d’un signalement explicite du helper, sans déduction à partir des mots contenus dans le journal.
- `NotRun` : nombre de jeux sélectionnés qui n’ont pas été lancés, affiché séparément.

Le bilan global est `INCOMPLETE` s’il ne reste que des blocages ou des jeux non exécutés, et `FAIL` s’il existe aussi des échecs ou une erreur du lanceur. Dans les deux cas, le code de sortie reste 1. Un échec de nettoyage ou un crash détecté ne devient pas BLOCKED sous prétexte qu’un préalable avait échoué avant lui.

Ce classement ne modifie aucune assertion fonctionnelle. Le contrôle de passage au premier plan du dialogue de préférences reste une assertion applicative distincte.

## Préparation du premier plan

`Activate-Ui` essaie `SetForegroundWindow`, puis, si nécessaire, essaie de relever la fenêtre sans activation ni changement de géométrie et envoie un clic de titre vérifié avec `SendInput`. La réussite de `SetWindowPos` ne prouve pas que le titre est découvert ; le helper contrôle le point avant toute entrée. Le point doit appartenir à la fenêtre cible selon `WindowFromPoint` et correspondre à `HTCAPTION` selon `WM_NCHITTEST`. Les boutons et autres contrôles sont exclus. Le curseur est déplacé sur ce point ; laisser la souris et le clavier disponibles pendant les tests UI.

Si le banc ne trouve aucun point utilisable ou si les entrées ne sont pas acceptées, le préalable reste BLOCKED avec son diagnostic. Si le clic vérifié a été envoyé mais que la fenêtre ne s’active pas, le jeu reste FAIL. Les touches du scénario ne sont envoyées qu’après vérification effective du premier plan. Le dialogue de préférences n’est jamais activé par ce helper dans le scénario qui contrôle son activation par FileSpacer.

## Positions dans le parent déjà ouvert

Le jeu `12-navigation-spatiale/05-ParentDejaOuvert.ps1` compare les positions visibles des trois éléments du parent avant et après Alt+Haut depuis son enfant. Il continue de contrôler l'activation du parent existant, l'absence de doublon et la conservation de sa sélection. Les jeux 01 à 04 et leurs assertions de sélection restent indépendants.

La mesure utilise les API publiques Windows UI Automation : `AutomationElement.FromHandle` reçoit le HWND de la vue enregistrée, obtenu par `IShellView::GetWindow`, puis les rectangles des éléments `ListItem` sont lus par `BoundingRectangleProperty`. Le processus cible est vérifié. La recherche reste limitée à cette vue. Aucun nom de classe de contrôle interne du Shell n'est supposé et aucune préférence de rendu n'est changée pour rendre une API disponible.

Les positions sont les coins supérieurs gauches des rectangles visibles, en pixels physiques et relativement au rectangle de la vue. Les éléments sont associés par leurs noms affichés, triés indépendamment de l'ordre de parcours UI Automation. Des extensions masquées par le Shell ne perturbent donc pas la comparaison. Seules les positions sont comparées, pas la largeur du libellé sélectionné. Ce contrôle porte sur la disposition visible lors de la réactivation ; il ne démontre pas à lui seul la persistance de positions personnalisées après fermeture.

Chaque relevé exige exactement trois éléments aux noms non vides et distincts, des rectangles valides et des éléments dans la zone visible. Le relevé doit rester identique pendant au moins 300 ms. Les valeurs sont conservées dans `layout-before.json` et `layout-after.json` dans le dossier de résultats du jeu. Une différence de position reste une erreur fonctionnelle, avec les deux relevés dans le diagnostic.

Un fournisseur UI Automation absent, une propriété non prise en charge, un élément manquant, un relevé instable ou un appel sans réponse produit une erreur explicitement préfixée `LAYOUT MEASUREMENT`. Ce n'est jamais un PASS ni une preuve suffisante d'un bug de FileSpacer. Les appels UI Automation utilisent un thread MTA distinct et une attente maximale de cinq secondes par relevé ; les objets UI Automation ne quittent pas ce thread. Les assemblages .NET Framework fournis par Windows sont chargés uniquement pour ce jeu.

L'ancien appel `IFolderView::GetItemPosition` est une API documentée, mais son échec « non implémenté » dans la configuration testée empêchait le scénario d'atteindre la navigation. La documentation officielle consultée ne fournit pas d'explication spécifique de cet échec. Cette nouvelle mesure vérifie le résultat visible sans supposer la disponibilité de cette méthode.

Références Microsoft :

- [IFolderView::GetItemPosition](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifolderview-getitemposition)
- [AutomationElement.FromHandle](https://learn.microsoft.com/en-us/dotnet/api/system.windows.automation.automationelement.fromhandle?view=netframework-4.8.1)
- [BoundingRectangleProperty : coordonnées physiques](https://learn.microsoft.com/en-us/dotnet/api/system.windows.automation.automationelement.boundingrectangleproperty?view=netframework-4.8.1)
- [Contrat UI Automation des éléments ListItem](https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/ui-automation-support-for-the-listitem-control-type)
- [UI Automation et threads MTA](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-threading)

## Préparation des deux fenêtres du jeu 05

Avant la sélection et le relevé initial, le jeu « parent déjà ouvert » place son parent et son enfant côte à côte dans la zone de travail du moniteur du parent. `MonitorFromWindow` et `GetMonitorInfo` fournissent cette zone ; `SetWindowPos` est utilisé avec `SWP_NOZORDER | SWP_NOACTIVATE`. La largeur est adaptée à l'écran, les positions sont vérifiées et le réglage DPI est pris en compte. Cette préparation évite que les deux fenêtres du scénario cachent mutuellement leurs titres. Elle ne s'exécute plus après le relevé initial, ni après Alt+Haut. L'activation de retour reste entièrement produite et vérifiée par le scénario applicatif.

Une impossibilité de préparer deux fenêtres disjointes reste FAIL, car une contrainte de fenêtre de l'application peut aussi être en cause. Un recouvrement par une autre application, un bureau non interactif ou une restriction Windows peut toujours empêcher l'activation : le banc n'annonce alors aucun succès. Si aucun point de titre utilisable n'est trouvé, le diagnostic donne la géométrie, le premier plan et les compteurs de points recouverts ou non reconnus comme titre.


## Corrections du 20 octobre 2026

Les codes de sortie sont enregistrés dans `application-exits.json`, avec PID, code signé et représentation hexadécimale. `application-processes.json` permet au lanceur de retrouver les diagnostics même si le jeu dépasse son délai. Les traces disponibles des jeux historiques et récents sont copiées dans `application-traces`; les rapports et minidumps de récupération disponibles sont copiés dans `application-recovery`. Un fichier antérieur au démarrage suivi est ignoré. Les dumps WER externes ne sont pas déplacés.

Le détail du résultat conserve la première erreur et ajoute les erreurs de nettoyage. Un auxiliaire de récupération restant actif ne remplace donc plus le message du crash initial. Tous les processus suivis sont contrôlés, y compris après la première sortie anormale. Le nettoyage du jeu historique de géométrie restaure aussi le contexte DPI et tente la suppression de ses fichiers après un crash.

`07-fenetres/03-FermetureInitiale.ps1` ferme huit fenêtres dès observation du HWND, sans attendre la vue Shell, avec les deux valeurs de `FullNamesOnSelection`. `13-preferences/05-FiltresSuccessifs.ps1` vérifie les noms réellement visibles, notamment entre deux recherches produisant le même nombre de résultats, ainsi que les motifs, l'effacement et les modifications de fichiers sous filtre.

Les assertions de sélection lors de la remontée restent actives. Le défaut connu correspondant est hors périmètre de ce correctif et peut donc encore faire échouer les quatre scénarios concernés. Aucun statut de réussite attendu n'est imposé pour masquer ce défaut.
