# Étude historique du placement des fenêtres

Les [résultats du 26 octobre 2026](RESULTS.md) sont conservés comme trace de
l'étude comparative ayant précédé le placement hybride. Ils comparent l'ancienne
grille et des prototypes autonomes ; ils ne valident pas le code courant de
FileSpacer et ne constituent pas des seuils de performance.

Les programmes de simulation, la copie de l'ancienne grille et les scripts de
mesure ont été retirés lors de la revue des tests du 27 octobre 2026. Ils avaient
servi à choisir l'algorithme ; leurs assertions portaient sur ces copies et ces
prototypes, sans appeler le placement de production. Leurs sources et leur
protocole restent accessibles dans l'historique Git.

La couverture pérenne est conservée dans
[`26-placement-fenetres`](../regression/cases/26-placement-fenetres/) : calculs
réels de `WindowPlacement.cpp`, déplacements, retrait/réintégration,
réconciliation, limites et tailles, puis intégration spatiale dans le vrai Shell.
Le [catalogue de non-régression](../regression/README.md) reste le point d'entrée
pour exécuter ces contrôles sur la VM.
