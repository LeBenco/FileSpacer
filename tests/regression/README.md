# FileSpacer — tests de non-régression

## Exécution

Depuis CMD à la racine du projet, fermer FileSpacer et les fenêtres du Panneau
de configuration, puis lancer :

```cmd
tests.cmd
```

Le lanceur construit l'application par `build.cmd debug`, initialise MSVC x64,
et lance les **104 jeux** indépendants. Ils ne doivent pas être complétés par
un second passage des mêmes bancs natifs. Chaque jeu possède son processus
PowerShell et son dossier de résultats sous `build/regression/<date>-<id>`.

Windows 10 x64, les dépendances de [BUILD.md](../../BUILD.md), PowerShell 5.1,
un bureau interactif et TEMP sur NTFS sont requis. Laisser clavier et souris
disponibles pendant les jeux UI. Les scripts, C++, C#, `.inc`, headers, ressources
et manifests sous `tests` sont des dépendances des jeux ; ne pas les dissocier.
Conserver les exceptions antivirus déjà configurées sur le projet.

Pour sélectionner un périmètre :

```cmd
tests.cmd -Category Internal
tests.cmd -Category UI
tests.cmd -Pattern "12-navigation-spatiale\*"
tests.cmd -Pattern "1[56]-*"
tests.cmd -Pattern "18-vue-shell-native\*"
tests.cmd -Pattern "19-commandes-shell\*"
tests.cmd -Pattern "20-validation-elargie\05-*"
tests.cmd -Pattern "24-*"
```

## Couverture conservée

| Groupe | Jeux | Contrat |
|---|---:|---|
| 00-banc | 2 | Rapports, sorties de processus et nettoyage du lanceur. |
| 01-stockage-sqlite | 4 | Stockage, identités spatiales, adaptateurs et géométrie. |
| 02-retention-quota | 1 | Conservation et limites des données. |
| 03-menus | 1 | Réentrance et durée de vie des commandes de menus. |
| 04-shell-sta | 1 | Références Shell/STA et durée de vie du processus. |
| 05-dimensions | 1 | Validation des dimensions des préférences. |
| 06-etat-vue | 1 | Captures partielles et erreurs de restauration. |
| 07-fenetres | 5 | Géométrie, commandes asynchrones, fermetures initiales/après commande, défilement retrouvé à la réouverture. |
| 08-suppression | 2 | Stockage et notifications de suppression. |
| 09-historique | 1 | Historique des fenêtres fermées. |
| 10-manifeste-maj | 1 | Validation du manifeste de mise à jour. |
| 11-session-maj | 1 | Conservation de la session pendant une mise à jour. |
| 12-navigation-spatiale | 6 | Politique Ctrl, parent existant et remontée sans sélection automatique. |
| 13-preferences | 6 | Apply/Cancel, propagation entre processus, recherche et persistance sous filtre. |
| 14-disposition | 1 | Visibilité et disposition des contrôles de FileSpacer. |
| 15-deplacement-alt | 32 | Déplacement, sélection, propriétés et exclusions des gestes Alt. |
| 16-redimensionnement-alt | 18 | Redimensionnement, annulation, exclusions et géométrie persistante. |
| 17-fermeture-interactions | 1 | Fermeture après interactions de menus et Alt. |
| 18-vue-shell-native | 1 | Hôte réel, SQLite, réglages, sélection/focus, positions, filtre, recréation. |
| 19-commandes-shell | 6 | Création, renommage, suppression, presse-papiers, drag/drop, menus, Unicode. |
| 20-validation-elargie | 5 | Charge, images/modes, Ce PC, écrans/DPI disponibles, Panneau externe. |
| 21-clic-activation | 1 | Clic sur le fond d'une fenêtre inactive : activation sans perte de sélection, clic d'élément, préférence désactivée. |
| 22-verrouillage | 2 | Activation, menu natif, référence visible, géométrie/vue temporaires, placements individuels, déverrouillage, confirmation et suppression globale entre processus, réouverture et redémarrage ; rendu ICO natif, transparence, DPI et couleurs de contraste élevé. |
| 24-raccourcis | 3 | Onglet natif FR/EN, 2 304 couples de réglages, persistance atomique, capture directe, conflits, valeurs par défaut par action, annulation, 64 gestes réels dans deux processus et comparaison des propriétés avec le Shell sans raccourcis. |

Les 50 jeux Alt restent distincts : ils vérifient des gestes ou zones différents.
Leur instrumentation peut observer un contrôle Windows ; l'application utilise
les interfaces publiques du Shell et les rôles MSAA, sans cette dépendance aux
classes internes. Les états sont préparés puis observés sans sélection corrective.

Les tests internes d'échecs partiels, de stockage et de réentrance sont conservés.
Le banc réel ne force pas les mêmes erreurs ; ils ne constituent pas des doublons
de son seul chemin de succès. Aucun de leurs échecs n'est requalifié en PASS.

## Un seul banc natif de production

`18-vue-shell-native/02-Persistance.ps1` compile les sources réelles de l'hôte,
des réglages, de la sélection, des positions, du filtre, du stockage et SQLite.
Il conserve les assertions déjà validées : 150 éléments, réglages exacts après
recréation et refresh, sélection/focus, positions, blobs anciens/tronqués,
filtrage et destruction/recréation. Les signaux publics conditionnent les étapes ;
une attente maximale fait échouer, jamais réussir, le test.

Pour enquêter sur ce seul banc sans le runner PowerShell :

```cmd
test-native-state.cmd --diagnostic
```

Ce lancement alternatif emploie `/Z7`, sans PDB de compilation partagé. Ne pas
le rejouer lorsque le groupe 18 vient déjà de passer dans la suite complète.

Les comparaisons des profils exploratoires default/spatial/classic, les sondes
d'hébergement interne du Panneau et le bilan ponctuel de migration ont été retirés.
Leur historique reste accessible sur la branche de migration. Les notifications
normales de Ce PC et l'ouverture externe du Panneau sont toujours testées par
20/03 et 20/05. Le jeu de fermeture initiale effectue quatre passages avec le
backend de production ; son ancienne matrice de préférence de rendu inactive
était redondante.

## Écarts acceptés et préférences

La remontée ne sélectionne plus le dossier d'origine pour des raisons d'ergonomie.
Les six jeux de navigation conservent cette exigence, la sélection d'un parent
déjà ouvert et l'ouverture du seul dossier explicitement choisi avec Ctrl.

Les noms complets sélectionnés à fond opaque et le défilement exact à la
réouverture restent absents, par décision explicite de l'utilisateur. L'option
sans effet est désactivée dans les préférences ; 13/01 vérifie cet état et vérifie
que « Conserver la sélection à l'activation » est de nouveau active. Ces écarts
ne sont pas comptabilisés comme des fonctionnalités testées et conformes.

Le clic d'activation est couvert par `21-clic-activation/01-FondInactif` : un
vrai clic (SendInput) sur le fond de la vue d'une fenêtre inactive doit l'activer
sans changer la sélection, un clic sur un élément doit le sélectionner, le clic
dans la fenêtre déjà active et la préférence désactivée gardent la désélection
native. Le point de clic est trouvé par UI Automation, jamais par une classe de
contrôle supposée.

```cmd
tests.cmd -Pattern "21-clic-activation\*"
```

## Résultats et isolation

`results.json`, `summary.json`, les sorties applicatives, codes de sortie et
preuves par jeu sont conservés. Un processus anormal, un nettoyage nécessaire,
un compte incorrect ou un dépassement de délai reste FAIL. Le lanceur distingue
BLOCKED et NOT_RUN ; ils ne valent jamais PASS et rendent le bilan incomplet.
Les résultats des jeux suivants ne sont pas déduits de ceux déjà exécutés.

Les préférences et la base de test sont sauvegardées/restaurées par le runner.
Ne pas réinitialiser les données de production pour lancer les tests. Les
fenêtres du Panneau déjà ouvertes bloquent 20/05 ; seules les fenêtres créées
et suivies pendant ce jeu sont fermées, jamais le processus Explorer entier.
La reconnaissance utilise le dossier de la vue Shell active, avec rattachement
au cadre par `GetAncestor(GA_ROOT)`, sans titre localisé comme preuve d'identité.

Les mesures de charge ne démontrent pas une parité de performances avec une
autre version sans comparaison dans les mêmes conditions. Les configurations
non présentes (écrans, DPI, extensions ou versions Windows) ne sont pas déclarées
validées automatiquement. Voir [l'architecture](../../docs/native-shell-view.md).

## Verrouillage de l’état mémorisé

Le jeu 22 utilise le menu natif du titre et observe les fenêtres et les vues
Shell réelles : état initial désactivé, activation, cadenas accessible, titres
longs/étroits, capture, géométrie et mode temporaires, références normale et
maximisée, positions exactes des icônes après réouverture, déverrouillage,
annulation et confirmation de la désactivation. Deux processus ouverts et un
dossier fermé vérifient l’effacement global, puis la réactivation et de nouveaux
processus vérifient qu’aucun verrou ne revient. Le déplacement/renommage NTFS
est effectué après la sortie du processus du dossier ; les notifications de
renommage d’un dossier ouvert restent couvertes séparément par 19/05.
Les captures PNG du titre sont conservées avec les résultats. Ce jeu n’active
pas de thème Windows global ni de DPI indisponible sur le poste.

Le stockage vérifie aussi les échecs transactionnels de verrouillage,
déverrouillage et effacement global, les écritures d’un instantané ancien,
la purge/expiration sans exemption et le transfert de clé. Le banc 18 relit
la référence protégée (colonnes, largeurs, tri, regroupement, mode et taille)
et l’applique à une nouvelle vue Shell, dont il contrôle les valeurs effectives.
