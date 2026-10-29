# Tri mélangé : validation et coût du fournisseur — 28 octobre 2026

## Décision

Conserver le fournisseur seulement lorsque « Mélanger les dossiers et les fichiers
dans le tri » est activé. Le coût mesuré du tri mélangé reste faible en temps
absolu jusqu'à 1 000 éléments. Le fournisseur permanent ne simplifie pas assez
le basculement pour justifier son coût et son exposition supplémentaires lorsque
l'option est désactivée. Le réglage reste désactivé par défaut.

Le rendu reste celui de Windows : `SHCreateShellFolderView` crée DefView,
identifié en processus par `IID_CDefView`, comme le prévoit la
[documentation Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreateshellfolderview).
Le nouveau code adapte les identifiants et les contrats de dossier ; il ne
réimplémente ni les contrôles, ni le dessin, ni les commandes de fichiers.

## Comparaison temporaire sur AgentWin10

Windows 10 Professionnel 10.0.19045 x64. Deux passages indépendants, chacun avec
un tour de chauffe et six tours mesurés, ordre des trois stratégies alterné.
Les fixtures contiennent 50, 100, 250, 500 et 1 000 éléments réels sur NTFS :
moitié fichiers de 1 Kio, moitié répertoires vides. Noms et dates sont entrelacés
entre fichiers et dossiers ; l'ordre exact de toutes les identités est vérifié.

Le banc Release utilise les sources courantes de `ShellViewHost` et
`MixedFolder`, le même `IExplorerBrowser`, les mêmes options de production
`FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING`, la vue Détails et une fenêtre 900 × 600.
Le fournisseur testé est une copie de la source courante avec un sélecteur
temporaire : comparaison des propriétés pour le tri mélangé, délégation à
`IShellFolder::CompareIDs` pour le tri séparé. Ce sélecteur n'est pas livré.

L'ouverture attend navigation, `EnumReady` et `EnumDone`, puis le dessin
synchrone. Chaque changement de colonne est précédé d'un tri sur l'autre colonne
pour éviter de mesurer seulement une inversion optimisée. Le tri mesure la
commande, l'observation de l'ordre exact et le dessin ; les inversions sont
mesurées séparément. Noms/dates, sens croissant/décroissant : 12 ouvertures et
48 changements de colonne ou inversions par taille et stratégie, hors chauffe.
Les deux passages ont terminé avec code 0 et toutes leurs assertions vérifiées :
1 890 enregistrements dont 1 620 mesurés, sans ajout au catalogue de régression.

### Médianes en millisecondes

| Éléments | Ouverture native | Ouverture mélangée | Ouverture custom séparée | Tri natif | Tri mélangé | Tri custom séparé |
|---:|---:|---:|---:|---:|---:|---:|
| 50 | 75.6 | 80.5 | 80.5 | 33.5 | 33.4 | 32.9 |
| 100 | 76.9 | 84.1 | 84.7 | 40.0 | 40.7 | 39.3 |
| 250 | 77.5 | 83.3 | 102.1 | 45.0 | 42.8 | 41.3 |
| 500 | 78.8 | 84.0 | 102.9 | 43.9 | 48.8 | 45.7 |
| 1000 | 80.9 | 87.0 | 103.5 | 46.1 | 52.4 | 51.5 |

À 1 000 éléments, le mélange ajoute 6,2 ms à l'ouverture (+7,6 %) et 6,3 ms au
tri (+13,7 %). Le coût est mesurable, mais reste de quelques millisecondes dans
ces conditions ; il ne faut pas le décrire comme une absence de surcoût.
Le 95e percentile du tri est de 63,5 ms en natif et 75,4 ms en mélange ; les
maxima observés sont 65,6 et 79,6 ms. L'inversion médiane passe de 33,0 à 35,6 ms.
Le tri par nom présente plus d'écart que le tri par date.

Le CPU du processus par changement de colonne a une médiane de 46,9 ms en natif
et 62,5 ms en mélange à 1 000 éléments. Sa granularité d'environ 15,6 ms limite
l'interprétation des petits écarts. Les instantanés de mémoire privée sont
proches (environ 27,5 Mio) ; les trois stratégies tournent dans un même processus,
DLL déjà chargée, donc ce chiffre n'isole pas le coût mémoire propre de la DLL.
Le fournisseur ajoute deux handles à l'ouverture à cette taille et davantage
pendant le tri ; le test ne constitue pas un audit de fuite à longue durée.

### Basculement de l'option à 1 000 éléments

Changer le comparateur puis redemander exactement la même colonne ne retrie pas
la vue dans les deux essais bornés à deux secondes. Un `IShellView::Refresh`
public applique le changement en conservant l'identité de la vue : médiane
81,3 ms, plage 76,2–86,3 ms sur 12 essais.

Rebrancher le même hôte du fournisseur natif vers le fournisseur mélangé prend
86,3 ms, plage 82,8–91,0 ms sur 12 essais, avec une nouvelle identité de vue.
Ces mesures n'incluent pas la sauvegarde/restauration de l'état du cadre complet.
Le gain d'environ 5 ms du fournisseur permanent concerne donc une opération rare,
tandis que son tri séparé augmente ici l'ouverture de 80,8 à 103,5 ms.
L'identité de vue conservée après `Refresh` ne prouve pas à elle seule que tous
les détails de sélection, défilement et placement seraient préservés.

### Portée

Cette comparaison isole le fournisseur dans l'hôte Shell de production, avec un
cadre minimal. Elle ne mesure pas le démarrage complet de FileSpacer, SQLite,
un disque à froid, le réseau, les miniatures ou des extensions tierces.
Elle ne couvre pas d'autres versions de Windows ni les tailles supérieures
à 1 000 éléments. La vérification de l'ordre ajoute environ 1,2–1,3 ms à
1 000 éléments, de façon comparable pour les trois stratégies. Les chiffres
ne sont pas une garantie générale de parité avec Explorer.

## Complexité et maintenance

La DLL représente environ 600 lignes et l'adaptation côté application environ
110 lignes, auxquelles s'ajoutent les raccords de sélection, positions et
préférences. Le coût durable concerne surtout les conversions d'identités,
les notifications, les contrats COM, le déploiement et la compatibilité des
commandes/propriétés Shell. Le rendu Windows et la délégation des opérations
réduisent le périmètre à maintenir sans supprimer ce coût.

Le custom permanent nécessiterait toujours le fournisseur natif pour les
dossiers virtuels et les archives, ainsi que les conversions pour les bases
existantes et les actions externes. Il ajouterait une politique de comparaison
par vue et un protocole de rafraîchissement à vérifier. L'activation conditionnelle
réutilise la capture/restauration existante lors du changement de préférence,
et réserve le nouveau fournisseur aux utilisateurs qui demandent cette fonction.

## Preuves conservées

Les binaires du comparatif sont compilés en Release (/O2 /MT, NDEBUG).
Le champ Debug du rapport du transport désigne l'application embarquée dans
le paquet, qui n'est pas lancée pour ces mesures ; seul le banc Release l'est.

Les mesures, sources, binaires et lanceurs ponctuels restent sous `build`, sans
être ajoutés aux tests de non-régression. Passages VM :

- `vm-d1a646fe40d243c5a9b670fb883d848b` : `scaling.csv`, `toggles.csv`, journal, code de sortie et rapport d’isolation.
- `vm-33b8006d54c8425f8da17ea36a0787cb` : `scaling.csv`, `toggles.csv`, journal, code de sortie et rapport d’isolation.

Empreintes SHA-256 des sources mesurées :

- `src/MixedFolderProvider.cpp` : `1d3328f9e1cef019f86e4a511e7bfb77b86d803bad9638404de8d26328a43d0d`.
- `src/ShellViewHost.cpp` : `5d968eaf84a84840efae6691d3a1033681b43f05fc0c6d9d2221a0dc2b41b956`.
- `src/MixedFolder.cpp` : `b49b00657e6fd3f0c7826cd18c491967765d190d0202f529334e0c9d3a9139e1`.
- `build/mixed-scaling/ScalingBench.cpp` : `70775275db4802eb60e5a98c1087e81052f8395f13b49f1ae87918249cb693da`.

Ces empreintes désignent les sources du comparatif et précèdent la
désencapsulation des éléments du fournisseur du 29 octobre 2026 (identifiants
natifs transmis tels quels, inscription aux notifications par `SFVM_GETNOTIFY`,
fournisseur conservé en positions libres). Elles ne correspondent plus aux
sources actuelles et servent seulement à retrouver les binaires mesurés.

## Validation de l'intégration

Les cas conservés sont `30-tri-melange/01-TriEtIdentites` (tri, application à
chaud, notifications, sélection, positions libres et réouverture) et
`30-tri-melange/02-CommandesShell` (réutilisation de cinq scénarios de commandes
Shell et d'un scénario de navigation avec l'option activée). Ils ont passé séparément sur la VM.
Le test existant de l'onglet Affichage est adapté à la dixième option.
Les mesures de montée en charge restent des essais ponctuels.

Builds Debug et Release MSVC x64 : PASS, sans nouveau warning `/W4`.
Le passage complet `vm-3a3377141dc045ad8f763087b8dcc7be` exécute **119 cas :
116 PASS, 3 FAIL**, aucun cas bloqué ou non exécuté, en 1 003,82 secondes de
scénarios. L'onglet Affichage et les deux nouveaux cas passent. État de test
restauré, données de production inchangées, aucune erreur de nettoyage.
Le binaire Debug testé a pour SHA-256
`EC2C5E8F81749333F6163288E48144E65CE8E5865EFE31BB37D5AE94565A365C`,
et sa DLL `1B177271EE51A3B70D1CE39D43B8A44ECDE8A08BE66CD0ECD391DE5AD66E75A6`.

Les échecs sont `01-stockage-sqlite/01-Stockage` (un écrivain concurrent échoue
sur « database is locked »), `19-commandes-shell/01-CreationRenommageSuppression`
et `19-commandes-shell/06-UnicodeEtAnnulation` (focus de l'éditeur de renommage
absent au moment de saisir, après son observation). Le stockage testé compile
uniquement des sources inchangées par cette tâche.

Les trois scénarios inchangés passent avec le binaire de référence présent dans
le checkout principal (`vm-983f90904be148c097f6bf166d986940`, SHA-256
`8D68F10A31F594635A774EC827E12C32487D361723EFF377C260FCCFAA92B9B3`), puis
avec le binaire du worktree (`vm-3236fe9391984ac6a49532323fc9cbbd`) : **3/3 PASS**
dans chacun de ces passages. Aucun geste, assertion ni délai n'a été modifié.
Les échecs initiaux ne sont donc pas reproduits ; leur cause n'est pas établie
et le passage complet reste FAIL. Il n'est pas présenté comme un 119/119 PASS.

Deux tentatives de transport antérieures n'ont exécuté aucun scénario :
`vm-0c79ad3b24114221b97bde9c0e83ba85`, conflit d'accès au journal du contrôleur,
et `vm-6e7c5edafc504bbc9f246d528a0992b9`, ancien manifeste de déploiement copié
dans un paquet ponctuel. Le lanceur temporaire attend désormais la fin de la
tâche avant de lire son journal ; le second paquet recrée son manifeste.
Ces corrections restent dans `build`. Aucun contrôleur installé ni lanceur
suivi n'est modifié ; ces tentatives ne sont pas comptées comme des tests PASS.


L'installateur n'est ni construit ni exécuté, cette action n'étant pas demandée.
La tentative de prétraitement seul NSIS est non exploitable : ce mode échoue
sur la résolution du plugin LockedList, avec le même échec sur la version
antérieure. Le packaging n'est donc pas déclaré
validé par ce contrôle ; les adaptations de distribution restent à vérifier
lors d'un futur build d'installateur autorisé.

## Analyse statique : REVIEW

clang-tidy et Cppcheck ont terminé sur les huit unités de production modifiées,
le test C++ adapté et les sources C++ temporaires du comparatif. Les en-têtes
modifiés sont couverts par leurs unités. Cible Windows x64, C++14 en production
et C++17 pour le test, SDK 10.0.19041.0, MSVC/ATL 14.44.35207 ; vrais en-têtes
pour clang-tidy, modèles officiels Windows/ATL/SQLite pour Cppcheck.
Aucune analyse n'est inachevée et aucun fichier modifié ne manque de couverture.

Les nouveaux diagnostics de production concernent le fournisseur : conversions
entier/pointeur imposées par Win32, opérations binaires sur des types du Shell,
paramètres interchangeables imposés par les signatures COM, casts issus de
macros SDK et parenthésage suggéré. Cppcheck signale aussi l'absence d'initialiseur
de trois champs du petit en-tête POD, dont chaque instance est explicitement
initialisée ou alimentée par `memcpy`. Ces points restent REVIEW : aucune
réécriture stylistique ni désactivation de contrôle n'est appliquée.

Dans le banc temporaire, Cppcheck signale deux déréférencements nuls pour
`MsgWaitForMultipleObjectsEx(0, nullptr, ...)`, alors que le cas zéro handle est
[documenté](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
pour attendre les messages. clang-tidy ne confirme pas ce défaut. Les diagnostics
portant sur les combinaisons de `FOLDERFLAGS` sont également liés aux valeurs
publiques documentées. Aucun défaut nouveau manifeste n'a été confirmé ; aucune
correction de code n'a été faite pour faire disparaître ces faux positifs.

## Intégration dans le checkout principal — 28 octobre 2026

La fusion reprend les traductions ajoutées sur `main` : le nouveau réglage et
le groupe Fichiers sont présents dans les ressources EN, FR, DE, IT, ES, PT-PT
et EL. Builds Debug et Release du checkout principal : PASS, zéro warning `/W4`.
Les sources et en-têtes C++ sont identiques octet pour octet à la branche
analysée ; `build/mixed-main-static-provenance.json` en consigne les empreintes.
Le verdict d'analyse reste REVIEW avec les diagnostics déjà expliqués ci-dessus.

La revue des régressions garde deux cas spécifiques. Le premier utilise 12
éléments et couvre désormais aussi une identité Unicode, le focus et un filtrage
simple. Le second réutilise 19/01–05 et 12/01. La matrice de politiques de
navigation et l'annulation Unicode restent exécutées une seule fois dans leurs
groupes existants. Le tableau du [guide des tests](../tests/regression/README.md#périmètre-propre-au-tri-mélangé)
justifie ce périmètre. Aucun test de charge, de langue ou seuil de performance
n'est ajouté au catalogue pour cette fonctionnalité.

Dans `vm-e9b4d4277ba243d897ce294a83862a9b`, les trois cas fonctionnels concernés
05/01, 30/01 et 30/02 passent. Le scénario de capture ponctuel de ce même
passage échoue dans son script avant toute capture, sur un alias PowerShell.
Les captures sont finalement obtenues dans
`vm-d8b33914dbbf434f979c581403e9ed8e` : PASS, état restauré, données de production
inchangées, aucune erreur de nettoyage. Les captures et leur provenance restent
sous `build/regression/.../output/regression/90-documentation/01-DisplayCaptures/captures`.
Les tentatives intermédiaires conservent leurs échecs dans leurs rapports.

Le français provient du binaire Debug intégré. La capture anglaise utilise des
copies temporaires de deux sources qui choisissent les ressources anglaises par
les API MUI et `PSH_USEPAGELANG` ; aucune modification de langue n'est ajoutée au
produit ni aux réglages Windows. Ces outils de capture ne sont pas des tests
maintenus. Les deux captures ont été examinées visuellement avant transmission
à l'agent chargé de la documentation.

## Raccord avec l'installateur actualisé sur main

La récupération des ajouts déjà publiés intègre aussi la nouvelle page native
listant les processus à fermer. Le conflit avec l'ancienne page est résolu en
conservant cette évolution ; la recherche commune vérifie à la fois l'exécutable
et FileSpacerFolder.dll. La distribution et le retrait de la DLL restent actifs.

Le build complet d'installateur passe, contrairement au prétraitement seul
inexploitable de l'étape précédente. Aucun installateur produit n'est exécuté.
Le banc existant d'installation est étendu à la présence de la DLL, son retrait
et la suppression de son inscription et de son cache appartenant à l'installation.
Ces dernières opérations sont redirigées vers le namespace GUID et le dossier
de fixture du banc. Il ne touche pas à l'inscription ni au cache de production.

PASS : 100 assertions NSIS, sondes de lancement et d'élévation, puis cycle
CurrentUser à payload inerte (installation, verrouillage, upgrade, suppression
des composants, désinstallation et préservation d'un fichier non possédé).
Rapport : build/installer-tests/ee56e33bad6049c69f981301c0368849.
Le consentement UAC réel et le cycle All users ne sont pas exécutés.
