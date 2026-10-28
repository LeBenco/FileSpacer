# Politique de confidentialité

FileSpacer ne contient ni télémétrie, ni analyse d’utilisation, ni publicité. Il n’envoie automatiquement au développeur aucune information d’utilisation ni aucun rapport de plantage.

## Recherche de mises à jour

Les vérifications manuelles et les vérifications hebdomadaires automatiques, lorsqu’elles sont activées, demandent à GitHub le manifeste de la dernière version de FileSpacer en HTTPS. Les vérifications automatiques sont désactivées par défaut. GitHub et son infrastructure de téléchargement reçoivent les informations habituelles de connexion, notamment votre adresse IP et un agent utilisateur indiquant la version de FileSpacer. La requête ne contient ni chemin de fichier, ni état de dossier, ni rapport de diagnostic.

Lorsqu’une mise à jour installable est disponible, FileSpacer demande confirmation avant de télécharger son installeur depuis GitHub en HTTPS. Il vérifie la taille et le SHA-256 du téléchargement par rapport au manifeste avant de lancer l’installation. L’installeur nécessite votre intervention ; les mises à jour ne s’installent jamais automatiquement. Les versions sans métadonnées d’installeur ouvrent leur page dans votre navigateur par défaut.

## Données locales

Les préférences générales sont stockées dans le registre Windows de l’utilisateur courant, sous `HKEY_CURRENT_USER\Software\FileSpacer`. Les positions et tailles des fenêtres de dossiers, les réglages d’affichage, les positions des icônes, les verrouillages d’état, les recherches récentes et l’historique des dossiers fermés sont conservés dans une base SQLite située à `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. SQLite peut créer des journaux associés dans le même dossier. L’état enregistré peut contenir des identités et chemins de dossiers, y compris des chemins réseau.

Une mise à jour conserve temporairement son installeur, une copie du lanceur de mise à jour et une liste SQLite des chemins de dossiers ouverts dans `%TEMP%\FileSpacerUpdate-{GUID}`. Cette liste locale sert à rouvrir les dossiers après une installation ou une annulation ; elle n’est pas transmise.

La protection contre les plantages peut enregistrer des rapports de diagnostic et des minidumps dans `%LOCALAPPDATA%\FileSpacer`. Ils restent sur l’ordinateur, sauf si l’utilisateur choisit de les partager. Les diagnostics peuvent contenir des chemins de fichiers, des informations sur les processus et des portions de leur mémoire. Ce même dossier contient un fichier de verrouillage des paramètres.

Les versions de débogage peuvent utiliser des préférences et données de test locales séparées avec `/test`.

## Windows et applications externes

L’ouverture de dossiers réseau, la résolution d’éléments du Shell, l’affichage d’aperçus ou de miniatures fournis par Windows et l’utilisation d’extensions de menu contextuel font appel à Windows et aux composants Shell installés. Ces composants et les serveurs distants peuvent accéder au réseau selon leur propre fonctionnement et leurs propres règles.

Les fichiers sont ouverts avec leur application associée ou avec le sélecteur d’applications de Windows. Leur traitement par ces applications échappe au contrôle de FileSpacer.

Si l’intégration Everything est activée, choisir son action de recherche transmet la requête visible à l’application Everything installée séparément. Une recherche limitée au dossier transmet aussi le chemin du dossier courant. Le filtrage local ne lance pas Everything ; l’index et les réglages de cette application déterminent ses résultats.

## Réinitialisation des préférences

`FileSpacer.exe /reset-options` réinitialise les préférences générales. `FileSpacer.exe /reset-all` efface aussi l’état enregistré des dossiers. Aucune de ces commandes ne supprime les fichiers de l’utilisateur ni ne modifie les associations de fichiers Windows. Elles ne suppriment pas les rapports de diagnostic déjà enregistrés.
