# Mesures du 26 octobre 2026

Le banc est decrit dans [README.md](README.md). Aucun code de l'application
n'a ete modifie pour cette etude. Tous les nombres ci-dessous utilisent des
resolutions et dimensions logiques, sans facteur de zoom.

## Protocole

- AMD Ryzen 9 8945HS, MSVC x64 /O2 /MT, C++14, /W4 /WX.
- 324 scenes : 3 resolutions, 4 dispositions, 9 nombres de fenetres et
  3 graines par combinaison. Chaque methode est mesuree 3 fois par scene,
  dans un ordre tournant : 5 832 essais au total pour les 6 methodes.
- La passe de reference a dure environ 3 minutes. Aucune analyse statique
  ni autre banc volontairement lance en parallele ; les passes exploratoires
  interrompues ne sont pas utilisees.
- Les valeurs du tableau sont les medianes des medianes par scene, apres
  deplacement d'une fenetre. Elles mesurent la recherche et la reconstruction
  eventuelle des tables. L'alimentation initiale du cache est mesuree a part.
- Donnees locales : `build/placement-simulation/release/20261026-202020/`.
  `measurements.csv`, `summary.csv`, `scenes.csv`, `windows.csv`,
  `metadata.json` et `run.log` conservent les mesures et les entrees.

## Temps observes

Temps en **millisecondes**, pour 60 fenetres dispersees sur la zone de travail :

| Methode | 1920 x 1080 | 3840 x 2160 |
| --- | ---: | ---: |
| Grille, pas de 10 | 0,204 | 1,385 |
| Grille, pas de 5 | 0,732 | 5,046 |
| Grille, pas de 2 | 4,144 | 29,011 |
| Grille au pixel | 8,639 | 64,178 |
| Exacte generale | 0,265 | 0,378 |
| Exacte avec recherche libre prealable | 0,259 | 0,368 |

Dans ces scenes dispersees a 60 fenetres, le recouvrement est inevitable :
le balayage doit se replier sur la minimisation generale. Quand une bande
libre est preservee, avec toujours 60 fenetres :

| Methode | 1920 x 1080 | 3840 x 2160 |
| --- | ---: | ---: |
| Grille, pas de 10 | 0,205 | 1,305 |
| Exacte generale | 0,271 | 0,410 |
| Exacte avec recherche libre prealable | 0,0054 | 0,0123 |

Pour la methode exacte generale face a la grille de 10, les croisements des
medianes **dans ce corpus** se situent dans les intervalles suivants :

| Disposition | 1920 x 1080 | 3840 x 2160 |
| --- | --- | --- |
| Dispersee | entre 40 et 60 fenetres | entre 100 et 200 fenetres |
| Alignee | entre 100 et 200 fenetres | aucun croisement jusqu'a 400 fenetres |

La grille devient plus rapide apres le croisement. Ce ne sont pas des seuils
universels : les alignements reduisent les coordonnees distinctes, l'existence
d'une position libre change le chemin de calcul, et une occupation rasterisee
inchangee permet a la grille de conserver ses sommes. Les anciennes bornes
23/59 comparees a `(4n+3)^2` ne predisaient pas ces temps reels.

La grille au pixel reste nettement plus couteuse que les prototypes exacts
sur ces scenes. Ces chiffres comparent des implementations precises ; ils ne
prouvent pas une borne optimale pour tous les algorithmes possibles.

## Qualite des placements

Sur les 324 scenes finales apres deplacement :

| Methode | Surface strictement superieure au minimum | Places libres manquees |
| --- | ---: | ---: |
| Grille, pas de 10 | 32 | 0 |
| Grille, pas de 5 | 31 | 0 |
| Grille, pas de 2 | 22 | 0 |
| Grille au pixel | 0 | 0 |
| Deux variantes exactes | 0 | 0 |

213 scenes possedent une position sans recouvrement. La grille de 10 ne la
manque pas dans cet echantillon, mais obtient une surface non minimale dans
environ 10 % des 324 scenes. Son ecart
maximal observe est de 3 730 unites carrees ; le plus grand ecart relatif a
la surface de la nouvelle fenetre est de 1,12 %. Ces resultats ne reproduisent
pas le cas signale d'une place libre assez grande ignoree dans l'application.

Pour la variante hybride, le chemin final est : preference deja libre dans
39 scenes, balayage libre dans 174, minimisation positive dans 111. Une grille
plus fine ne garantit pas un meilleur resultat a chaque scene : le classement
des positions repose encore sur une occupation arrondie aux cellules.

## Verification et analyse

- Build du banc Debug et Release : **PASS**, sans warning /W4 /WX.
- Verification exhaustive de 600 petites scenes : **PASS**, surface et
  distance minimales pour les deux prototypes et la grille au pixel.
- Passage laissant exactement une origine entiere possible : **PASS**.
- 324 grandes scenes : **PASS** ; les deux prototypes et la grille au pixel
  obtiennent toujours la meme surface, verifiee par un oracle d'union par bandes.
- clang-tidy 23.1.4 et Cppcheck 2.22.0 : analyses terminees de l'unite C++ du
  banc et des quatre grilles generees, cible Windows x64 / C++14, avec les vrais
  en-tetes SDK 10.0.19041.0 et MSVC/ATL 14.44.35207 pour clang-tidy. Cppcheck
  utilise les modeles officiels sans les en-tetes systeme complets.

Verdict statique : **REVIEW**. Aucun defaut fonctionnel confirme. Les points
restants sont conserves sans modifier les calculs pour faire taire les outils :

- clang-tidy signale la graine constante (indispensable a la reproductibilite)
  et deux paires de parametres de meme type ; leur ordre a ete relu.
- Il signale une exception pouvant sortir de `main`, bien que les exceptions
  standard soient interceptees. Ce diagnostic reste a REVIEW.
- Cppcheck signale les champs de l'agregat `Choice` alors que chaque creation
  fournit tous ses champs ou utilise `{}` ; il ne suit pas la creation du cache
  dans la lambda et considere a tort ce cache toujours absent. Les mesures
  comportent bien les tableaux du cache exact.
- La proposition de rendre le generateur constant est incompatible avec son
  avancement ; le modulo par un vient de la specialisation au pixel.
- Le diagnostic sur `Entry::generation` dans le code genere est deja present
  dans le coeur de production : la valeur est fournie a l'insertion de l'agregat.

Tous les fichiers C++ ajoutes ont ete couverts. L'estimation memoire du balayage
a ete corrigee avant la passe finale pour compter ses tableaux temporaires ;
les deux analyseurs ont ensuite ete relances. Les validations Debug, Release
et les mesures de reference utilisent le meme source C++.

Ce banc valide le calcul, pas son integration au vrai Shell. Il ne mesure ni
l'enumeration des fenetres ni le temps complet d'ouverture d'un dossier. Aucun
test UI ni build de l'application n'est necessaire a cette simulation autonome.

## Recherche de cas defavorables jusqu'a 60 fenetres

Le profil `-WorstCases` decrit dans le guide utilise le meme calcul exact, sans
modifier le code principal. Quatre dispositions sont construites : ecran
entierement couvert par plusieurs rectangles, passages d'une unite trop etroits,
maillage de bandes, et une unique origine libre dans le coin oppose. Les trois
premieres imposent le balayage puis le repli exact ; la derniere controle le
chemin libre et la precision des limites.

Chaque ajout de rectangle est choisi parmi 40 propositions pour augmenter les
coordonnees critiques distinctes. Les scenes sont volontairement adverses et
ne representent pas une distribution d'usage. Les tailles de la nouvelle fenetre
sont 320 x 240, 580 x 435 et 960 x 720 ; les effectifs sont 20, 40 et 60.

Passe principale : **2 160 scenes, 11 repetitions par methode**, soit
23 760 mesures de l'hybride et autant de l'exacte generale. Les mesures incluent
la copie des rectangles, les allocations, le balayage, le repli et la liberation
des tables. Chaque calcul repart de tables neuves. Les controles de surface,
de distance et du chemin emprunte passent pour toutes les scenes.

Temps de l'hybride en **millisecondes**, sur les scenes sans aucune place libre :

| Resolution logique | Fenetres | Mediane | Mediane de la scene la plus lente | Percentile 99 | Maximum brut |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1920 x 1080 | 20 | 0,0432 | 0,0613 | 0,0665 | 0,1033 |
| 1920 x 1080 | 40 | 0,1774 | 0,3244 | 0,3525 | 0,6723 |
| 1920 x 1080 | 60 | 0,4102 | 0,7203 | 0,7182 | 5,7044 |
| 3840 x 2160 | 20 | 0,0504 | 0,0629 | 0,0692 | 0,1022 |
| 3840 x 2160 | 40 | 0,2080 | 0,3149 | 0,3098 | 0,4215 |
| 3840 x 2160 | 60 | 0,4723 | 0,5707 | 0,6982 | 1,1438 |

Chaque ligne repose sur 270 scenes et 2 970 appels. Le percentile 99 porte
sur tous les appels de la ligne ; il peut donc etre inferieur a la mediane
d'une seule scene rare. Les temps sont des mesures murales, avec l'autre
travail du poste autorise en parallele ; une preemption reste comprise dans
le maximum, sans que sa cause puisse etre identifiee par ce banc.

A 60 fenetres, la passe atteint **55 695 positions candidates**, contre une
borne `(4*60+3)^2 = 59 049`, soit environ 94 %. Les grilles comprimees atteignent
14 278 entrees du tableau de compteurs, contre `(2*60+2)^2 = 14 884`. Ces chiffres rapprochent le test
des bornes de travail de cette implementation, sans prouver une borne de temps.

Le pic de 5,7044 ms correspond a une scene 1080p de 60 fenetres, cible 580 x 435,
profil `mesh`, graine 129766. Sa mediane sur les 11 repetitions est de 0,5103 ms.
Une seconde passe de **144 scenes avec 100 repetitions par methode** inclut
exactement cette geometrie : mediane **0,3661 ms**, percentile 99 **0,5731 ms**,
maximum **0,7282 ms** pour cette scene. Le pic initial n'est pas reproduit ;
sa cause reste non instrumentee. Il est conserve dans les resultats.

Les scenes avec une unique place libre restent rapides : pour 60 fenetres,
maximum brut 0,0541 ms en 1080p et 0,0669 ms en 4K lors de la passe principale.

Donnees locales :

- `build/placement-worst-cases/release/20261026-204529/` : passe principale.
- `build/placement-worst-cases/release/20261026-204820/` : confirmation sur
  100 repetitions, avec le meme source C++.
- `worst-measurements.csv` conserve tous les appels ; les resumes distinguent
  les statistiques par scene et par effectif/resolution. Les rectangles et
  empreintes des sources sont conserves avec chaque passe.

Build Debug et Release du profil : **PASS**, sans warning /W4 /WX. Le profil
Debug a aussi execute 72 scenes adverses et les 600 controles exhaustifs.
Les deux analyseurs ont termine sur `PlacementSimulation.cpp` et `WorstCases.cpp`,
avec couverture des grilles generees. Verdict statique : **REVIEW**, sans defaut
confirme. Aux signalements du banc initial s'ajoutent l'inclusion volontaire
du source du banc pour reutiliser exactement ses calculs, les copies volontaires
de scene chargees au chronometrage et deux paires de parametres de meme type.
Les calculs du banc initial ne changent pas ; son point d'entree peut simplement
etre omis lorsqu'il est inclus par le profil adverse. Aucun fichier C++ modifie
n'est reste sans analyse.

Ces essais soutiennent un cout de calcul tres contenu jusqu'a 60 fenetres sur
ce processeur, y compris lorsque le balayage ne trouve aucune place libre.
Ils ne garantissent pas un temps maximal sur tout ordinateur et ne mesurent
pas l'integration au Shell.
