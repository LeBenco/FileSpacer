# FileSpacer 1.1 — vue Shell et API publiques

## Architecture retenue

FileSpacer conserve la vue système hébergée par `IExplorerBrowser`. Le Shell
assure le rendu, l'énumération, les images, les modes d'affichage et les commandes
de fichiers. `ShellViewHost` utilise les interfaces COM publiques ; l'application
ne cherche plus les contrôles enfants du Shell, ne les sous-classe plus et ne
leur envoie plus de messages `LVM_*`. L'ancienne vue entièrement réimplémentée
est abandonnée pour limiter le code de rendu et d'interaction à maintenir.

La configuration spatiale demande `FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING`,
sans imposer `FVO_VISTALAYOUT`. Les dossiers virtuels choisissent leur présentation
initiale. Navigation et énumération sont observées par les événements publics ;
les générations d'observation sont invalidées avant détachement ou fermeture.
Les temporisations de sauvegarde ne servent pas à déclarer une vue prête.

`ShellViewSettings`, `ShellViewSelection` et `ShellViewIcons` raccordent les API
publiques à la persistance SQLite existante. `ShellViewFilter` fournit le filtre
par nom. Les formats de stockage existants restent lisibles. Aucune property
bag par fenêtre ni nouvelle base de registres de vues n'est ajoutée.

Les commandes de création, renommage, suppression, copie et déplacement passent
par les mécanismes Shell existants. Les gestes Alt de FileSpacer utilisent
Win32 et les informations MSAA publiques. Les tests peuvent observer les
contrôles Windows pour vérifier un geste ; cette instrumentation n'est pas une
dépendance de l'application aux contrôles internes du Shell.

## Navigation et Panneau de configuration

La sélection automatique du dossier d'origine lors d'une remontée est retirée
pour des raisons d'ergonomie : elle ajoutait un dossier involontaire à une
sélection faite avec Ctrl et pouvait provoquer l'ouverture des deux dossiers.
Ce retrait concerne aussi les ancêtres, la barre de chemin et l'accès rapide.
La sélection d'un parent déjà ouvert et les instantanés de réouverture restent
conservés ; la remontée ne doit pas les remplacer.

Le Panneau de configuration s'ouvre dans Windows par
`IOpenControlPanel::Open(nullptr, nullptr, nullptr)`. Les racines sont comparées
par leurs identités Shell, sans décoder les PIDL ou comparer des noms traduits.
Le dossier FileSpacer source reste ouvert avec sa sélection. Aucun site de
navigation déduit ou API privée n'est utilisé pour tenter un hébergement interne.

## Écarts acceptés pour 1.1 et clic d'activation rétabli

L'utilisateur a accepté trois écarts pour la release 1.1, après validation du
Panneau externe. Deux subsistent :

- Les noms sélectionnés utilisent le rendu natif Windows. L'affichage spécifique
  des noms complets avec fond opaque n'est pas disponible : aucune interface
  publique de la vue (`IVisualProperties`, `FOLDERFLAGS`, `FOLDERVIEWOPTIONS`,
  messages `SFVM_*`) ne permet d'intervenir dans le dessin des libellés.
- La réouverture restaure sélection et focus et peut révéler l'élément focalisé,
  mais ne restitue pas l'offset exact de défilement. Le flux
  `IShellBrowser::GetViewStateStream` est bien demandé par la vue, mais sous
  Windows 10 il ne contient que les réglages de présentation (mode, tri,
  groupement, colonnes), pas la position de défilement.

Le clic d'activation est rétabli depuis l'audit du 24 octobre 2026 : lorsqu'une
fenêtre est activée par un clic (`WM_ACTIVATE` avec `WA_CLICKACTIVE`), Windows
publie ensuite le message bouton vers la fenêtre cliquée ; la boucle de messages
de FileSpacer écarte ce premier `WM_LBUTTONDOWN` quand il vise le fond de la vue
(rôle MSAA `ROLE_SYSTEM_LIST`), sans Alt, et que la préférence « Conserver la
sélection à l'activation » est active. Un clic sur un élément, un clic droit, les
en-têtes et ascenseurs, les gestes Alt et le clic dans une fenêtre déjà active
gardent leur traitement natif ou FileSpacer. Un rectangle de sélection commencé
par le clic d'activation est absorbé ; un second appui est nécessaire.
`WM_MOUSEACTIVATE` n'atteint pas le cadre à travers les fenêtres du Shell et
n'est donc pas utilisé.

L'option « Afficher les noms longs en entier à la sélection » reste désactivée
et expliquée dans les ressources françaises et anglaises ; sa valeur enregistrée
est conservée. Le réexamen des écarts est décrit dans
[le prompt pour Claude](prompt-claude-parite-vue-shell.md) et dans
`audit/rapport.md`.

## Régression maintenue

La commande de référence est `tests.cmd`, soit **97 jeux**. Elle inclut un seul
banc Shell réel dans la configuration de production : `18-vue-shell-native/02-Persistance`.
Il teste les réglages, la sélection, le focus, les positions, le filtrage et la
destruction/recréation avec SQLite. Ne pas ajouter l'exécution du lanceur natif
à une exécution complète qui contient déjà ce groupe.

Les 50 cas Alt, les commandes Shell, Ce PC, le Panneau externe, la navigation,
les préférences, les notifications, la fermeture et les autres contrats de
stockage restent couverts. Les tests internes d'erreurs partielles et de durée
de vie sont conservés : ils ne se réduisent pas au succès d'une vue réelle.

Les comparaisons de profils ExplorerBrowser, les sondes d'hébergement du Panneau
et le générateur de bilan ponctuel sont retirés. Les rapports incrémentaux et
leurs sources restent dans l'historique de la branche de migration. Les tests
de comportement ne sont pas supprimés pour transformer un échec en succès.
Le double passage de fermeture initiale selon une préférence de rendu désormais
inactive est remplacé par quatre fermetures de la vue de production.

Les validations Windows ont été communiquées au fil des incréments ; le dernier
jeu Panneau externe est confirmé passant par l'utilisateur. Le changement des
deux contrôles de préférences requiert seulement la reprise ciblée du jeu
`13-preferences/01-FenetresOuvertes` après intégration. Une validation exhaustive
de toutes les configurations Windows, extensions et DPI n'est pas déduite de
ces résultats.

## Références

- [IExplorerBrowser](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorerbrowser)
- [IFolderView2](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ifolderview2)
- [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-folderviewoptions)
- [IOpenControlPanel::Open](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iopencontrolpanel-open)
