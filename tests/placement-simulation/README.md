# Simulation du placement des fenetres

Banc console independant : aucun dossier ouvert, aucune fenetre enumeree,
deplacee ou creee, aucune preference ou base utilisateur lue ou modifiee.
L'application FileSpacer n'est ni reconstruite ni lancee. Le seul acces au
Registre lit le nom du processeur pour les metadonnees.

Ce banc de mesure historique conserve la grille anterieure dans `LegacyGrid.inc`.
Il reste independant du code de placement hybride integre depuis dans l'application.
Il ne fait pas partie des tests de regression fonctionnels et n'est pas relance
pour valider les changements courants du produit.

Les dimensions sont des **pixels logiques**, sans parametre de zoom Windows.
Les trois espaces de travail modelises sont 1920 x 1080, 2560 x 1440 et
3840 x 2160. Ils representent directement la zone disponible : aucune barre
des taches n'est retranchee.

## Executer

Depuis CMD, a la racine du projet, avec la chaine MSVC du [guide de compilation](../../docs/build.md) :

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Run-Simulation.ps1
```

Le lancement compile uniquement le banc en Release, verifie ses calculs puis
mesure 324 scenes (3 dispositions aleatoires par combinaison), avec 3 repetitions
par methode. Les variantes sont executees dans un ordre tournant. Les resultats
vont dans `build/placement-simulation/release/<date-heure>/`.

```cmd
rem Verification sans mesures, avec controles Debug et /W4 /WX
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Run-Simulation.ps1 -Configuration Debug -VerifyOnly

rem Passe courte : 1080p, 5 et 60 fenetres
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Run-Simulation.ps1 -Quick -Seeds 1 -Repetitions 1

rem Davantage de dispositions independantes et de repetitions
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Run-Simulation.ps1 -Seeds 5 -Repetitions 5

rem Remplacer le chemin par celui affiche a la fin du lancement
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Summarize-Results.ps1 -RunDirectory build\placement-simulation\release\<date-heure>
```

Ne pas executer plusieurs mesures ou une analyse statique en parallele pour
produire les chiffres de reference. Les temps dependent du processeur, des
caches et des autres activites du poste ; ils ne constituent pas un engagement
de latence.

## Methodes comparees

| Methode | Calcul |
| --- | --- |
| `grid10` | Ancien coeur de production, pas de 10 unites |
| `grid5`, `grid2` | Meme code, seule la constante de pas change |
| `grid1` | Meme code au pixel, surface et recherche exactes |
| `exact` | Prototype avec compression des bords reels, union ponderee par les dimensions, integration des bandes et candidats geometriques ; O(n log n + n²) |
| `exact-free` | Verification de la position preferee, puis recherche exacte d'une place libre par balayage et arbre de couverture en O(n log n) ; repli sur `exact` si le recouvrement est inevitable |

Le script specialise le code historique de `LegacyGrid.inc` dans un include
genere. Il conserve ses calculs, ses compteurs et ses mises a
jour par differences. Les identifiants HWND sont uniquement des jetons opaques
fondes sur des adresses de rectangles ; aucune fonction ne les utilise pour
acceder a une fenetre. Les seuls appels User32 conserves sont les operations
geometriques `IntersectRect`, `EqualRect` et `IsRectEmpty`.

Les prototypes exacts recherchent les alignements critiques des bords et de
leurs translations par la taille de la nouvelle fenetre, ainsi que les limites
de l'ecran et la position preferee. La somme des intersections de plusieurs
fenetres n'est jamais utilisee : une surface occupee compte une seule fois.
Les dimensions de la nouvelle fenetre sont conservees. A surface egale, les
methodes privilegient la distance a la position preferee. Un dernier ex aequo
peut donner des positions differentes mais de meme surface et meme distance.

## Scenes et controles

Les nombres de fenetres sont 1, 5, 10, 20, 40, 60, 100, 200 et 400. Les deux
derniers servent aussi de cas de charge. Les tailles existantes varient entre
420-920 et 320-760 unites. La nouvelle fenetre mesure generalement 580 x 435,
avec aussi des tailles variees. Toutes les fenetres sont incluses dans la zone
de travail, et la nouvelle fenetre y tient integralement.

Quatre dispositions sont generees : dispersion sur tout l'ecran, regroupement
avec de forts recouvrements, alignements avec quelques decalages, et bande
libre assez large pour la nouvelle fenetre. La preference est soit l'origine
de l'ecran, soit une position ressemblant a celle d'une fenetre source. Ces
scenes sont plausibles, mais ne proviennent pas d'une mesure d'usage utilisateur.

Chaque lancement commence par 600 scenes aleatoires petites, comparees a un
calcul exhaustif pixel par pixel. Les deux prototypes exacts et la grille au
pixel doivent retrouver la surface minimale et la distance minimale ; le
balayage doit aussi reconnaitre une place qui laisse exactement une seule
origine entiere possible. Sur les grandes scenes, un autre oracle decoupe
l'intersection en bandes et fusionne les intervalles pour verifier les surfaces.
La grille au pixel et les deux prototypes doivent obtenir la meme surface.

## Lecture des mesures

Chaque ligne de `measurements.csv` contient les medianes de repetitions d'une
meme scene. `summary.csv` regroupe ensuite les scenes par resolution,
disposition, nombre de fenetres et methode ; les percentiles portent sur ces
medianes. Avec peu de scenes, le percentile 90 est surtout un indicateur de
dispersion, pas une estimation fiable des latences rares.

- `setup_us` : allocation et alimentation initiale du cache de grille, ou copie
  de la liste de rectangles pour les prototypes exacts. Les preparatifs
  geometriques des prototypes restent dans le temps de recherche.
- `cold_us` : premiere recherche, avec construction des sommes ou des tables.
- `warm_us` : meme recherche sans changement de scene. La methode exacte peut
  reutiliser ses tables pour cette meme taille et cette meme preference ; la
  variante libre refait son balayage.
- `update_us` : cout moyen de 65 notifications simulees alternant la premiere
  fenetre entre deux positions proches (17 et 11 unites). Pour les prototypes
  exacts, seule la geometrie est enregistree ; les tables restent invalidees
  jusqu'a la prochaine recherche. Les valeurs tres petites sont des ordres de
  grandeur : elles incluent la boucle et ne mesurent pas un message Windows.
- `after_update_us` : recherche apres ces deplacements, la scene finale etant
  modifiee. Une grille ne reconstruit ses sommes que si l'union rasterisee a
  change. La destruction des anciennes tables exactes est incluse ici.
- `estimated_peak_bytes` : estimation des tableaux principaux du calcul,
  incluant les temporaires des prototypes. Ce n'est pas la memoire totale du
  processus ; l'allocateur et le dictionnaire d'identifiants de la grille ne
  sont notamment pas comptes.
- `area`, `optimal_area`, `extra_area`, `missed_free` : qualite de la position
  finale apres deplacement, verifiee sur les vrais rectangles, hors chronometrage.
  Les surfaces sont en unites logiques au carre. Une grille grossiere peut
  surestimer les surfaces aux bords et manquer une position libre.

Les graines, dimensions, positions preferees et rectangles initiaux sont
conserves dans `scenes.csv` et `windows.csv`. Le deplacement final est defini
par `movedFirst` dans le source. Les options, le processeur et les empreintes
SHA-256 des sources sont dans `metadata.json`.

## Ce que ce banc permet de conclure

Il compare le cout du placement et des mises a jour a geometrie identique,
ainsi que la qualite des positions trouvees. Un seuil de croisement doit etre
lu par disposition et par resolution : le nombre de fenetres seul ne suffit pas.

Il ne mesure pas l'enumeration native des fenetres, les notifications, le Shell,
le chargement d'un dossier, le rendu ni la latence complete d'ouverture. Il ne
valide pas les chemins reels de fermeture spatiale, les ecrans multiples ou le
DPI de Windows. Ces controles restent distincts des calculs geometriques.

## Cas adverses jusqu'a 60 fenetres

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Run-Simulation.ps1 -WorstCases -Seeds 30 -Repetitions 11
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\placement-simulation\Summarize-WorstCases.ps1 -RunDirectory build\placement-worst-cases\release\<date-heure>
```

Ce profil compare uniquement les deux variantes exactes, en 1080p et 4K
logiques, avec 20, 40 et 60 fenetres. Trois nouvelles tailles sont essayees :
320 x 240, 580 x 435 et 960 x 720. Les profils forcent des situations adverses,
avec des fenetres parfois tres grandes, plutot qu'une distribution d'usage :

- `covered` : deux fenetres recouvrent ensemble tout l'ecran ; les suivantes
  multiplient les bords distincts. Aucun rectangle individuel ne couvre l'ecran.
- `slits` : des bandes verticales laissent des intervalles d'une unite trop
  etroits pour la nouvelle fenetre ; le balayage echoue obligatoirement.
- `mesh` : ajout de bandes horizontales aux bandes verticales.
- `last-fit` : une seule origine entiere libre existe, dans le coin oppose.

Pour chaque fenetre supplementaire, le generateur conserve la disposition la
plus couteuse geometriquement parmi 40 propositions aleatoires, selon le nombre
de candidats et de cellules comprimees. La generation est hors chronometrage.
Chaque calcul chronometre reconstruit ses tables depuis zero et inclut la copie
des rectangles, les allocations, le balayage, le repli et les destructions. Les
repetitions ne reutilisent pas de cache algorithmique ; les pages de code et les
caches CPU peuvent naturellement etre chauds.

Les chemins de calcul, la surface, la distance et l'origine unique de `last-fit`
sont verifies. Tous les temps individuels et les rectangles sont conserves.
Le resume distingue le plus grand temps brut observe de la mediane de la scene
la plus lente. Une preemption par une autre activite du poste peut affecter le
maximum brut : aucune de ces mesures n'est une borne garantie du temps Windows.
