# FileSpacer — tests de non-régression

## Exécution

Depuis CMD à la racine du projet, fermer FileSpacer et les fenêtres du Panneau
de configuration, puis lancer :

```cmd
tests.cmd
```

Le lanceur construit l'application par `build.cmd debug`, initialise MSVC x64,
et lance les jeux indépendants sélectionnés dans le catalogue du checkout.
Ils ne doivent pas être complétés par
un second passage des mêmes bancs natifs. Chaque jeu possède son processus
PowerShell et son dossier de résultats sous `build/regression/<date>-<id>`.

Windows 10 x64, les dépendances du [guide de compilation](../../docs/build.md), PowerShell 5.1,
un bureau interactif et TEMP sur NTFS sont requis. Laisser clavier et souris
disponibles pendant les jeux UI. Les scripts, C++, C#, `.inc`, headers, ressources
et manifests sous `tests` sont des dépendances des jeux ; ne pas les dissocier.
Conserver les exceptions antivirus déjà configurées sur le projet.

Lorsqu'au moins un jeu UI est sélectionné, le lanceur maintient temporairement
l'écran et le système éveillés avec une requête d'alimentation Windows et suspend
l'économiseur d'écran. Le nettoyage final rétablit l'état initial, même après un
échec, puis ferme la requête. Le profil d'alimentation et les préférences
enregistrées restent inchangés ; les jeux internes seuls ne font rien de cela.
`desktop-activity.json` consigne l'état initial et sa restauration. Un verrouillage
explicite de la session ou une politique de sécurité peut toujours bloquer les
tests ; le lanceur ne déverrouille pas Windows.

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

Le même point d'entrée accepte `-VM "nom"` pour une VM Hyper-V configurée :

```cmd
tests.cmd -VM AgentWin10
tests.cmd -VM AgentWin10 -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Sans `-VM`, l'exécution reste locale. `-Category`, `-Pattern`, leur combinaison,
`-TimeoutSeconds` et `-RunDirectory` restent disponibles. La sélection précède
la préparation : seuls les scénarios choisis et leurs dépendances sont transférés.
L'application, les exécutables natifs et les helpers C# sont compilés sur l'hôte.
L'invité charge les artefacts préparés et échoue si un helper manque ou diffère ;
il ne compile pas. Les scénarios et assertions sont ceux du banc local.

Le compte invité doit avoir une session graphique ouverte, la VM doit être
allumée et VMConnect fermé. Aucun geste n'est envoyé au bureau hôte. Une VM non
configurée provoque une erreur, sans repli local. La configuration initiale et
les mises à jour du contrôleur protégé demandent UAC ; les passages ordinaires
réutilisent la tâche et les identifiants chiffrés. Voir le
[guide Hyper-V](../client/README.md).

Les rapports distants sont récupérés sous `build/regression/vm-<id>/output/` :
`regression/results.json`, `regression/summary.json`, les journaux des cas et
`result.json` pour les contrôles d'identité, de session et de restauration.
`connection.json`, `smoke.json` (nom conservé du transport) et les journaux du
contrôleur restent au niveau du passage. Avec `-RunDirectory`, une copie complète
est créée à l'emplacement demandé, sans écraser de rapport existant. Un timeout,
un cas bloqué ou une sortie non nulle ne donne jamais PASS.

## Revue du catalogue — 27 octobre 2026

Les 117 cas (21 Internal, 96 UI) sont conservés après examen des scénarios,
assertions et dépendances. Un succès passé ne rend pas un comportement immuable :
les ressources, préférences, notifications Shell, migrations de données et
frontières de durée de vie peuvent tous régresser lors d'une modification.

Les jeux internes de captures partielles et d'échecs injectés exercent le code
courant extrait ou compilé ; ils couvrent des chemins que le vrai Shell ne
permet pas de provoquer de manière déterministe. Les tests du banc gardent les
crashs, sorties, blocages et erreurs de nettoyage visibles, et vérifient
l'identité des artefacts préparés. Ils restent nécessaires même après un passage
complet réussi. Les petits lanceurs des groupes 12, 15 et 16 sélectionnent des
préférences, gestes ou modes distincts : leur taille ne les rend pas temporaires.

Seuls les six fichiers de programmes et scripts de
[l'étude historique du placement](../placement-simulation/README.md) sont
supprimés : ils comparaient l'ancienne grille et des prototypes sans appeler le
placement de production. Les résultats historiques sont conservés. Les deux cas
du groupe 26 continuent de tester l'algorithme livré et son intégration réelle.
Les lanceurs de diagnostic natif et Hyper-V sont conservés : ils utilisent
encore les composants actuels et permettent de diagnostiquer leur exécution.

Les dépendances manquantes des deux bancs internes ont été mises à jour pour
l'option d'aperçu Affichage : le groupe 03 conserve ses fermetures injectées sur
le chemin sans aperçu, tandis que le groupe 28 exerce l'aperçu réel ; le groupe
05 compte et fournit les neuf options actuelles de la page Affichage. Les tests
de recherche utilisent désormais le champ d'historique dans Général et les
autres options dans Intégration. Le groupe 26 attend le libellé actuel « Placer
les nouvelles fenêtres sur un espace libre ». Les résultats attendus de
persistance, de recherche et de placement restent conservés.

### Charge fonctionnelle conservée

Le cas `20-validation-elargie/01-ChargeEtMesures.ps1` reste dans la suite :
5 000 identités réelles, filtre exact, résultat vide, retour au contenu complet,
rafraîchissement et trois ouvertures/fermetures de processus. Les petits dossiers
ne couvrent pas les mêmes volumes d'énumération, de filtrage et de restauration.
Les trois cycles vérifient aussi la réutilisation de l'état sauvegardé. Le dernier
passage complet dans la VM a pris 27,79 secondes pour ce cas, environ 2,9 % du temps
des 117 scénarios ; ce coût ne justifie pas d'en réduire la couverture.

Les durées, la mémoire et les handles sont des preuves de diagnostic sans seuil
de performance : ils ne constituent pas un benchmark de comparaison validé une
fois pour toutes. Le test de rétention/quota conserve aussi ses volumes, car il
vérifie la limite produit de 100 Mo, la purge et la réutilisation des pages. Les
matrices de raccourcis couvrent des combinaisons fonctionnelles distinctes.

### Dernier passage complet — 28 octobre 2026

`tests.cmd -VM AgentWin10` : **117/117 PASS** (21 Internal, 96 UI), aucun FAIL,
BLOCKED ou NOT_RUN. Les 183 sorties applicatives enregistrées sont normales,
sans erreur de nettoyage. Les 96 cas UI ont leur trace d'isolation des listes
de tâches ; préférences et bases de test sont restaurées, données de production
inchangées. Application et helpers sont compilés sur l'hôte, puis exécutés sous
le jeton limité de la VM, y compris le vrai Shell et Everything.
Le passage est `d2a05d49960f47dc9f836c76c76edd00`, sous `build/regression/vm-<id>/`.
Les échecs initiaux, leur diagnostic et le verdict d'analyse statique REVIEW sont
conservés dans [l'historique](../../docs/development-history.md).

## Couverture conservée

Le catalogue courant contient 132 cas : 25 Internal et 107 UI. Ces nombres
décrivent les scénarios disponibles, pas un passage complet validé.

Pour le renommage, le banc vérifie le focus dans la fenêtre de la vue Shell,
puis attend son éditeur natif. Ctrl+A et le texte forment un seul lot d'entrées ;
le texte affiché et la fermeture de l'éditeur sont observés avant la suite.
Le champ de recherche du cadre est refusé comme cible de renommage.
Les attentes ne rejouent aucune touche et les échecs conservent leur première cause.

| Groupe | Jeux | Contrat |
|---|---:|---|
| 00-banc | 4 | Rapports, sorties de processus, nettoyage du lanceur, intégrité des artefacts client et refus de saisie de renommage dans le champ de recherche. |
| 01-stockage-sqlite | 4 | Stockage, identités spatiales, adaptateurs et géométrie. |
| 02-retention-quota | 1 | Conservation et limites des données. |
| 03-menus | 1 | Réentrance et durée de vie des commandes de menus. |
| 04-shell-sta | 1 | Références Shell/STA et durée de vie du processus. |
| 05-dimensions | 1 | Validation des dimensions des préférences. |
| 06-etat-vue | 1 | Captures partielles et erreurs de restauration. |
| 07-fenetres | 6 | Géométrie, commandes asynchrones, fermetures initiales/après commande, défilement retrouvé à la réouverture, titre visible et barre d’outils sur toute la largeur après maximisation, réduction et restauration. |
| 08-suppression | 2 | Stockage, suppressions directes et d’ancêtres via le Shell ou le système de fichiers, fermeture groupée et réouverture dans un même processus, conservation des fenêtres voisines et nettoyage SQLite. |
| 09-historique | 2 | Historique des fenêtres fermées ; réessai des Jump Lists après lien ou commit refusé et retour à un ancien modèle après remplacement partiel. |
| 10-manifeste-maj | 1 | Validation du manifeste de mise à jour. |
| 11-session-maj | 1 | Conservation de la session pendant une mise à jour. |
| 12-navigation-spatiale | 6 | Politique Ctrl, parent existant et remontée sans sélection automatique. |
| 13-preferences | 10 | Apply/Cancel, refus des raccourcis invalides, propagation entre processus, unicité séquentielle et concurrente, réservation en cours d'initialisation/fermeture, appelant fermé et mutex abandonné ; recherche et persistance sous filtre, icône À propos à 48 pixels à 96 DPI. |
| 14-disposition | 1 | Visibilité et disposition des contrôles de FileSpacer. |
| 15-deplacement-alt | 32 | Déplacement, sélection, propriétés et exclusions des gestes Alt. |
| 16-redimensionnement-alt | 18 | Redimensionnement, annulation, exclusions et géométrie persistante. |
| 17-fermeture-interactions | 1 | Fermeture après interactions de menus et Alt. |
| 18-vue-shell-native | 1 | Hôte réel, SQLite, réglages, sélection/focus, positions, filtre, recréation. |
| 19-commandes-shell | 6 | Création, renommage, suppression, presse-papiers, drag/drop, menus, Unicode. |
| 20-validation-elargie | 5 | Charge, images/modes, Ce PC, écrans/DPI disponibles, Panneau externe. |
| 21-clic-activation | 2 | Clic sur le fond d'une fenêtre inactive : activation sans perte de sélection, clic d'élément, préférence désactivée ; activation par la légende suivie d'un clic sur le fond. |
| 22-verrouillage | 2 | Activation, menu natif, référence visible, géométrie/vue temporaires, placements individuels, déverrouillage, confirmation et suppression globale entre processus, réouverture et redémarrage ; rendu ICO natif, transparence, DPI et couleurs de contraste élevé. |
| 23-noms-selection-simple | 1 | Fond opaque du libellé natif en sélection simple ; rendu natif conservé en sélection multiple, sans sélection et avec le focus seul ; sélection, positions et captures valides. |
| 24-raccourcis | 3 | Onglet natif FR/EN, 2 304 couples de réglages, persistance atomique, capture directe, conflits, valeurs par défaut par action, annulation, 64 gestes réels dans deux processus et comparaison des propriétés, de la sélection, de la géométrie et des fichiers avec le Shell sans raccourcis, y compris après mouvement du second appui. |
| 25-recherche-integration | 6 | Historique SQLite isolé, migration, entrées Win32, arguments et erreurs, parcours Shell réel, préférences 0/5/20, application Everything réelle, chemin manuel normalisé et suggestions sans chevron au focus/clic. |
| 26-placement-fenetres | 2 | Fonction pure de placement hybride libre/encombré sur un instantané de fenêtres : préférence libre ou occupée, instantanés distincts, fenêtres hors écran ou rognées, indépendance de l’ordre, bords, coordonnées négatives et tailles ; contrôles fonctionnels sans mesure de temps ni oracle d’optimalité ; libellé et option Général, maintenance À propos, processus distincts, réduction, remplacement spatial et priorité de la position mémorisée sur le vrai Shell. |
| 28-apercu-affichage | 4 | Aperçus natifs au survol et au clavier, changements de mode et de taille, sortie d’entrée, annulation par Échap ou clic extérieur, validation au clic, modification du dossier par un autre processus, restauration de la vue et préférence Affichage avec Annuler/Appliquer/OK. |
| 29-menu-gestionnaire | 1 | Bascule vers l’autre gestionnaire dans les menus, défauts indépendants par classe, resynchronisation, ressources EN/FR, commandes et délégations COM conservées, vrais menus Shell et invocation du défaut masqué par les deux voies ShellExecuteEx. |
| 30-documentation | 1 | Lanceur d'aide local extrait du code de production, langues et replis, erreurs, menus et F1. |
| 30-tri-melange | 4 | Tri et identités persistées, commandes Shell, dossier affiché renommé/supprimé ; contrat privé du fournisseur et identité canonique d'un enfant réel. |
| 31-langues | 1 | Chargement des traductions pour les variantes régionales. |

Le cas `13-preferences/07-APropos.ps1` vérifie l’icône chargée, sa taille adaptée
au DPI et l’absence de chevauchement des textes. Les comptes du catalogue ne
constituent pas un résultat de validation : consulter les rapports
datés dans [l’historique](../../docs/development-history.md).

Le groupe 25 réutilise `Native.ps1`, `EssentialUi.ps1` et le mode `/test` du
lanceur. Les deux premiers jeux sont internes : ils ne lancent pas Everything.
Pour vérifier une machine dépourvue d'Everything avant installation, définir
`FILESPACER_EXPECT_EVERYTHING_ABSENT=1` avant le jeu 02 ; cette assertion vérifie la
détection réelle, sans faux registre d'installation.

Le jeu 04 nécessite une installation réelle d'Everything et aucune interface
Everything ouverte dans la session de bureau testée. Le service Windows
Everything, exécuté en session 0, peut rester actif. Par défaut, le helper natif
interroge la détection de production pour trouver l'installation réelle, y compris
sous Program Files ; `FILESPACER_EVERYTHING_EXE` permet de préciser une autre
installation. Le bouton Détecter doit retrouver cette installation, qui doit
donc se trouver à un emplacement détectable. Le chemin renvoyé par le helper est
conservé en UTF-16 dans `everything-detected.txt`, sans perte des caractères Unicode.
Il vérifie la détection sur l'installation réelle, puis lance une copie de
l'exécutable officiel placée avec son propre `Everything.ini` et sa base de
fixtures. Ce bootstrap est nécessaire : Everything lit `run_as_admin` à côté de
l'exécutable même avec `-config`. Le réglage d'élévation et l'INI de l'installation
restent inchangés ; leur empreinte est vérifiée. Les lancements réels passent par
le chemin manuel de la copie isolée.
Le test indexe uniquement ces fixtures et lit les requêtes et noms effectivement affichés.
Il contrôle les deux portées, les sous-dossiers, le chemin manuel Unicode, les
guillemets, métacaractères et erreurs visibles. Il ne remplace pas Everything par
un lanceur simulé. En l'absence de ce prérequis, le parcours réel reste non validé.

Le jeu 05 vérifie la liste sans chevron avec de vrais événements clavier/souris :
ouverture au focus et au clic, saisie et recherche instantanée conservées, aucune
présélection, Entrée locale, flèches et séparateur, Échap sans effacement, Tab,
réouverture, choix d'un historique à la souris et fermeture avec son propriétaire.

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

Le fond opaque du nom sélectionné est disponible depuis 1.1.3 pour une sélection
simple en mode Icônes. Le groupe 23 vérifie ce rendu et le traitement natif
conservé en sélection multiple, avec le focus seul et dans les autres cas.
Le jeu 13/01 vérifie que l'option est disponible, qu'Annuler abandonne les
éditions et qu'Appliquer enregistre ses changements ; il vérifie aussi la
disponibilité de « Conserver la sélection à l'activation ».

À la réouverture, le défilement retrouve une ancre de ligne en vues Détails et
Liste, couverte par 07/05 et le banc 18. La restitution de l'offset exact reste
absente ; les vues à icônes ouvrent en haut. Le dépliage personnalisé de tous
les noms d'une sélection multiple n'est pas une fonctionnalité livrée.

Le clic d'activation est couvert par `21-clic-activation/01-FondInactif` : un
vrai clic (SendInput) sur le fond de la vue d'une fenêtre inactive doit l'activer
sans changer la sélection, un clic sur un élément doit le sélectionner, le clic
dans la fenêtre déjà active et la préférence désactivée gardent la désélection
native. Le point de clic est trouvé par UI Automation, jamais par une classe de
contrôle supposée.

```cmd
tests.cmd -Pattern "21-clic-activation\*"
```

## Tri mélangé

Le groupe `30-tri-melange` comprend trois cas UI et un cas Internal.
Le premier vérifie le tri réel, les changements de
préférence, les notifications et les identités persistées en positions libres,
où le fournisseur reste actif, et lors des changements de préférence.
Le second réutilise cinq scénarios de commandes Shell du groupe 19 et un
scénario de navigation du groupe 12 avec le fournisseur actif. Le troisième
renomme puis supprime, depuis le système de fichiers, le dossier affiché par
une fenêtre sous fournisseur, aussi listé par une fenêtre parente sous
fournisseur. Le quatrième interroge les deux versions de l'interface privée
sur le vrai Shell et compare l'enfant converti à son identité canonique.
Le test de réessai `09/02` utilise la méthode de production extraite avec
SQLite et registre réels, mais injecte les échecs à la frontière Shell :
il ne vérifie pas l'affichage de la Jump List Windows.
Les mesures ponctuelles de montée en charge ne font pas
partie de ce catalogue ; leur protocole et leurs résultats sont dans la
[validation du tri mélangé](../../docs/validation-mixed-sorting.md).

### Périmètre propre au tri mélangé

| Traitement | Couverture | Justification |
|---|---|---|
| Réutiliser avec le fournisseur actif | 19/01 création, renommage, suppression ; 19/02 presse-papiers ; 19/03 glisser-déposer ; 19/04 menus | Identifiants transmis à `SetNameOf`, `IDataObject`, `IDropTarget` et aux menus natifs. |
| Réutiliser avec le fournisseur actif | 19/05 renommage d'un dossier ouvert ; 12/01 navigation Entrée/Alt+Haut | Raccord des identités du fournisseur aux vrais dossiers et fenêtres spatiales. Une combinaison de politique suffit ; la matrice Ctrl/préférences reste dans le groupe 12. |
| Ajouter dans 30/01, avec 12 éléments | Ordre nom/date dans les deux sens et modes Détails/Liste/Icônes automatiques ; basculement de préférence ; notifications | Ce sont les nouveaux comportements observables. |
| Ajouter dans le même petit scénario | Identité Unicode, sélection/focus, positions libres conservées sous fournisseur après changement de rangement, de préférence et réouverture ; un filtrage puis son effacement | Vérifie les identités natives aux frontières du fournisseur sans répéter les suites de persistance, de rendu ou de recherche. |
| Ajouter dans 30/03 | Renommage puis suppression du dossier affiché lui-même, depuis le système de fichiers, sous fournisseur ; la fenêtre parente sous fournisseur suit | La vue s'inscrit aux notifications de la racine native (`SFVM_GETNOTIFY`) ; 19/05 ne couvre que le renommage par l'interface. |
| Garder une seule fois | 19/06 Unicode/annulation ; matrices de navigation, rendu, recherche ; stockage/quotas, mises à jour, géométrie/gestes Alt, historique/Everything | Les algorithmes et politiques correspondants ne changent pas. L'identité Unicode passe déjà par le nouveau fournisseur dans 30/01 ; l'annulation reste native. |

Les lanceurs réutilisent les scénarios existants : leurs gestes, assertions et
délais n'ont pas de seconde copie. Aucun scénario de montée en charge ni seuil
de performance n'est ajouté pour cette option. Les cas de charge déjà présents
ne sont pas dédoublés pour le fournisseur mélangé.

## Résultats et isolation

`results.json`, `summary.json`, les sorties applicatives, codes de sortie et
preuves par jeu sont conservés. Un processus anormal, un nettoyage nécessaire,
un compte incorrect ou un dépassement de délai reste FAIL. Le lanceur distingue
BLOCKED et NOT_RUN ; ils ne valent jamais PASS et rendent le bilan incomplet.
Les résultats des jeux suivants ne sont pas déduits de ceux déjà exécutés.

Les préférences et la base de test sont sauvegardées/restaurées par le runner.
Ne pas réinitialiser les données de production pour lancer les tests. Les
identités de listes de tâches laissées par les anciens cas UI sont retirées
avant le cas suivant, uniquement sous `Software\FileSpacer\Test\JumpLists`.
Le fichier `case-fixture.json` consigne leur nombre. Les identités créées pendant
un même scénario sont conservées jusqu'à sa fin : ses cycles, volumes et
réouvertures restent testés ensemble. Sans cette isolation, les écritures Shell
`CommitList`/`FlushFileBuffers` des anciens dossiers s'accumulent et prolongent
la sortie des processus des cas suivants, sans rapport avec leur fixture.

Les fenêtres du Panneau déjà ouvertes bloquent 20/05 ; seules les fenêtres créées
et suivies pendant ce jeu sont fermées, jamais le processus Explorer entier.
La reconnaissance utilise le dossier de la vue Shell active, avec rattachement
au cadre par `GetAncestor(GA_ROOT)`, sans titre localisé comme preuve d'identité.

Les mesures de charge ne démontrent pas une parité de performances avec une
autre version sans comparaison dans les mêmes conditions. Les configurations
non présentes (écrans, DPI, extensions ou versions Windows) ne sont pas déclarées
validées automatiquement. Voir [l'architecture](../../docs/native-shell-view.md).

Pour diagnostiquer la recherche, `set FS_SEARCH_TIMINGS=1` active les durées des
lectures réelles dans `search-observations.jsonl`. Le jeu 20/01 écrit aussi les
phases dans `load-phases.jsonl` (temps, CPU et présence du helper Computer Use).
Ces journaux ne changent ni les attentes ni les délais. Une inspection
d'accessibilité concurrente peut ralentir fortement la vue de 5 000 éléments ;
le banc n'arrête aucun outil externe. Voir la [comparaison contrôlée](../../docs/development-history.md#load-investigation-and-observer-correction).

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
