# Validation : libellé opaque en sélection simple

Validation du 25 octobre 2026. Le comportement décrit a été publié en
[1.1.3](../CHANGELOG.md#113--2026-10-25). Les résultats et les réserves ci-dessous
restent ceux de cette validation.

## Comportement livré

L'option « Afficher le nom complet en sélection simple » est réactivée dans les
ressources EN/FR. Le fond du libellé est rempli avant le dessin natif uniquement
si la vue est en mode Icônes, qu'un seul élément est effectivement sélectionné
et que la notification concerne cet élément.

Le traitement natif est transmis sans modification en sélection multiple ou
vide, pour un élément seulement focalisé et dans les autres modes. Le dépliage
reste celui de Windows ; le code ne replie pas un nom que Windows déplie de
lui-même. Aucun volet, texte redessiné ou changement de focus n'est ajouté.

`ShellViewLabels.h` contient l'adaptation facultative. `FolderWindow.cpp` la
raccorde à la création de vue et aux changements de préférence. La notification
existante de `SettingsDialog.cpp` inclut désormais le changement de cette seule
option, afin de mettre à jour les fenêtres ouvertes. Sa valeur de registre et
la configuration spatiale d'ExplorerBrowser sont conservées.

## Build et tests

Windows 10 Pro N 22H2, thème courant, 96 DPI ; MSVC x64 14.44.35207 et
Windows SDK 10.0.19041.0. Les fixtures, préférences et bases de test sont isolées.

- **Build Debug PASS**, `/W4`, aucun warning lors de la compilation finale.
- **23-noms-selection-simple/01-Rendu PASS** : vrai Shell, code de dessin de
  production inclus directement par le banc. Le libellé simple mesure 122 pixels
  de haut ; son fond modifie 1 640 pixels dans la fixture avec des icônes dessous.
  Images identiques au natif en sélection multiple, avec le focus seul et après
  désactivation. Le libellé focalisé non sélectionné reste identique quand un
  autre élément est sélectionné. Sélection exacte, focus et positions préservés ;
  retour à une sélection simple, éditeur de renommage, rafraîchissement et
  destruction contrôlés.
- **18-vue-shell-native/02-Persistance PASS** : persistance, sélection, focus,
  positions, filtrage et durée de vie du banc Shell/SQLite existant.
- **13-preferences/01-FenetresOuvertes PASS** après extension du test : option
  disponible, Annuler sans enregistrement et Appliquer activant/désactivant la
  valeur, avec les fenêtres de deux processus ouvertes.
- Le premier passage de **13-preferences** a donné **5 PASS / 1 FAIL**. Le cas
  `05-FiltresSuccessifs` a rencontré `E_FAIL` lors d'une lecture COM transitoire,
  puis a passé son rejeu isolé. Une interaction utilisateur a été signalée comme
  possible ; son rôle dans cet échec n'est pas établi. L'échec initial est conservé.
- **29 cas SettingsSizeTests PASS**, dont un nouveau cas vérifiant qu'un
  changement de la seule option de libellé notifie les fenêtres et qu'une
  seconde application sans changement ne notifie pas à nouveau.

Le lancement standard de ce dernier contrôle a été empêché avant compilation :
`Get-FileHash` était introuvable dans le processus de préparation après `vcvars`.
Le générateur original a été exécuté séparément sous PowerShell 5.1, puis les
mêmes sources extraites ont été compilées et exécutées par `Build-NativeTest`,
avec MSVC initialisé en CMD. Aucun attendu ni helper commun n'a été modifié.
Le contrôle C++ a passé ses 29 cas ; son lancement standard demeure non validé
dans ce contexte d'environnement.

Une erreur initiale du nouveau banc désélectionnait l'élément en posant son focus
avec `SVSI_FOCUSED` seul. La préparation a été corrigée pour préserver son état
sélectionné. Les assertions de sélection exacte et de comparaison d'images ont
été conservées. Les images sont produites par le vrai contrôle dans un HDC de
test, avec son protocole `NM_CUSTOMDRAW`, et non par une simulation du rendu.

La suite complète n'a pas été lancée. Les autres thèmes/DPI et la fermeture
réentrante au milieu d'un dessin ne sont pas déclarés validés.

## Analyse statique : REVIEW

Clang-tidy 23.1.4 et Cppcheck 2.22.0 ont terminé avec code de sortie 0 sur :

- `FolderWindow.cpp`, couvrant également le nouvel en-tête `ShellViewLabels.h` ;
- `SettingsDialog.cpp` ;
- `NativeSelectedLabelTests.cpp`, couvrant également `ShellViewLabels.h` ;
- `SettingsSizeTests.cpp`, avec le bloc de production extrait réellement utilisé.

Aucun fichier C++ livré n'est laissé sans analyse. Aucun plantage, dépassement de
délai ou réduction de couverture ATL n'a été rencontré.

Clang-tidy utilise les vrais en-têtes SDK, MSVC et ATL, la cible Windows x64,
C++14 et `FILESPACER_DEBUG` pour l'application. Les tests suivent leur compilation
C++17, `/MTd`, sans `FILESPACER_DEBUG`. Les contrôles activés comprennent
`clang-analyzer-*`, `bugprone-*` et `performance-*`.
Cppcheck utilise `win64`, le standard et les macros correspondants, le niveau
exhaustif et les modèles officiels `windows`, `microsoft_atl`, `sqlite3`, sans
l'inclusion intégrale des en-têtes système. Aucune erreur de parsing du code
local n'a été observée.

Les fichiers préexistants ont une base de comparaison analysée avec les mêmes
options. Les diagnostics existants de `SettingsDialog.cpp`, `FolderWindow.cpp`
et `SettingsSizeTests.cpp` restent hors périmètre ; les avertissements du
nouvel en-tête et du nouveau banc sont bien considérés comme nouveaux.

Points laissés à **REVIEW**, regroupés entre les deux unités qui incluent
l'en-tête :

| Diagnostic nouveau | Appréciation |
| --- | --- |
| `bugprone-easily-swappable-parameters` | Signature Win32 `SUBCLASSPROC` et signatures COM imposées ; ordre non modifiable. |
| `performance-no-int-to-ptr` | Conversions de `LPARAM` vers les structures de notification prévues par le contrat Win32. |
| `bugprone-signed-bitwise` | Masques `CDRF_*` portés par `LRESULT`, et expressions équivalentes dans le banc. |
| `bugprone-implicit-widening-of-multiplication-result` | Calcul du nombre de pixels du banc ; surface fixe de 780 × 600, pas de débordement constaté. |
| Cppcheck `shadowFunction`, `useStlAlgorithm` | Suggestions de forme du nouveau banc : variable locale `start` et boucle de normalisation des pixels. |

Aucun défaut nouveau manifeste n'est confirmé par les analyses ; aucun
changement de forme n'a été appliqué pour masquer ces signalements. Le verdict
est **REVIEW**, distinct du succès du build et des contrôles fonctionnels.
Aucune correction de produit n'a été nécessaire à la suite des analyses.

## Relecture complémentaire de la session

Les changements conservés depuis `9550f52` ont été réexaminés : comptes rendus
et captures des expériences, adaptation livrée, raccordement des préférences,
ressources et tests. Les essais de volet et de redessin ont été retirés ; leur
présence dans les rapports ne constitue pas une fonctionnalité du produit.
L'implémentation conservée reste limitée au fond du libellé natif. Les API
Win32/Shell absentes du cache WinMD ont été revérifiées dans le SDK local et la
documentation officielle. Aucun remplacement de l'implémentation n'est requis.

Une faiblesse du banc a été corrigée : `differences` rejette aussi une seconde
capture vide, évitant un accès hors limites après un échec de capture. Un cas
de régression couvre cette situation. L'assertion du retour à une sélection
simple rejette explicitement le marqueur d'erreur `SIZE_MAX`.

Le test `23-noms-selection-simple/01-Rendu` passe après cette correction.
Clang-tidy et Cppcheck terminent avec les mêmes diagnostics qu'avant : verdict
**PASS pour cette correction du banc**. Le **REVIEW antérieur** sur les
signalements de forme et les contrats Win32 n'est pas levé.

Les sources de l'application sont inchangées. Le SHA-256 de l'exécutable Debug
est identique avant et après la vérification :
`2694C1635CB767F742A11B66B6C6AD0C74BBB078F0C7DBB00E783FE07E7D782D`.
Cette correction de test ne modifie donc ni le comportement ni les performances
du produit ; aucune équivalence de performances entre deux implémentations
différentes n'est supposée.
