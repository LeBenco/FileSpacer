# Demande historique : parité de la vue Shell

Ce document conserve la demande initiale de recherche pour la migration 1.1.
Il ne décrit pas l'état actuel du produit : le clic d'activation et l'ancrage
du défilement en vues Détails/Liste ont été rétablis, puis le fond opaque en
sélection simple a été publié en 1.1.3. Le dépliage personnalisé de tous les noms
en sélection multiple et la restitution de l'offset exact ne sont pas livrés.
Voir [l'architecture actuelle](native-shell-view.md) et le
[guide utilisateur](user-guide.md). La demande d'origine est conservée ci-dessous.

---

Je développe FileSpacer, un gestionnaire de fichiers spatial C++/Win32 pour
Windows 10 x64, issu de ChromaFiler et inspiré de Nautilus/GNOME 2. Chaque dossier
possède une fenêtre propre, dont l'état est conservé dans SQLite.

Nous avons remplacé le pilotage des contrôles internes de la vue Shell par les
interfaces COM publiques de Windows. L'application héberge la vue système avec
`IExplorerBrowser` ; le Shell fournit le rendu, les interactions et les commandes
de fichiers. Les options spatiales sont `FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING`,
sans imposer `FVO_VISTALAYOUT`. Une réimplémentation complète de la vue a été
abandonnée afin de réduire la maintenance et de réutiliser les fonctions Windows.

Trois écarts ont été acceptés pour la release 1.1. Je souhaite étudier s'il est
possible de retrouver les comportements suivants sans compromettre ce choix :

1. **Noms sélectionnés — priorité principale.** Afficher entièrement les noms
   longs des éléments sélectionnés avec un fond opaque qui les rend lisibles,
   au lieu de conserver les libellés tronqués ou leur rendu natif habituel.
   Préserver la sélection multiple, le focus, le renommage, l'accessibilité et
   les autres interactions natives. L'ancienne adaptation dépendait du rendu
   interne du Shell ; elle n'a pas été portée dans la nouvelle architecture.
2. **Clic d'activation.** Lorsqu'une fenêtre est inactive, un clic sur le fond
   de sa zone de fichiers doit pouvoir l'activer sans effacer sa sélection.
   Distinguer ce premier clic d'un clic effectué dans une fenêtre déjà active,
   et éviter d'altérer la sélection ou l'ouverture normale d'un élément.
3. **Défilement à la réouverture.** Retrouver la position de défilement précédente
   d'un dossier fermé puis rouvert, aussi fidèlement que les contrats publics
   le permettent. Actuellement, nous restaurons sélection et focus avec
   `IFolderView`/`IFolderView2`, et pouvons révéler l'élément focalisé avec
   `SVSI_ENSUREVISIBLE`. Cela ne restitue pas l'offset exact. Distinguer une
   véritable restauration d'offset d'un simple ancrage sur un élément, notamment
   sans sélection et après modification du contenu, de la taille ou du DPI.

Contraintes : garder la vue Shell native et utiliser des mécanismes publics
documentés. Ne proposer ni découverte/sous-classement de ses contrôles internes,
ni messages `LVM_*` vers ces contrôles, ni décodage de PIDL, injection, API privée
ou contrat déduit d'une expérience. Ne pas réintroduire une property bag de
registre par fenêtre. L'état appartient à SQLite et la restauration attend les
notifications publiques de navigation/énumération ; un délai arbitraire ne doit
pas servir de preuve de disponibilité. Les mécanismes Win32 appliqués aux
fenêtres et contrôles appartenant à FileSpacer restent envisageables.

Pour chaque comportement, recherche d'abord les API Shell/Win32 existantes et
leurs contrats dans la documentation Microsoft. Indique ce qu'elles garantissent,
leurs limites et le point de raccordement à l'hôte. Si un complément visuel ou
une adaptation de l'hôte est nécessaire, sépare clairement les parties fondées
sur un contrat documenté de celles qui resteraient hypothétiques et estime
leur coût de maintenance et leurs effets sur les interactions natives.

Je préfère une conclusion d'absence de solution publique établie à une recette
expérimentale présentée comme fiable. Distingue une solution fidèle, un compromis
fonctionnel et un point non résolu. Donne les liens Microsoft précis qui fondent
tes conclusions et un protocole de validation court pour les solutions retenues.
