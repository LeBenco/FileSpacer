# Référence de configuration de Nilesoft Shell

Pour une présentation générale, voir [Nilesoft Shell dans le guide d’utilisation](user-guide.md#nilesoft-shell).

[Nilesoft Shell](https://nilesoft.org/) peut appliquer ses thèmes et ses personnalisations aux menus de FileSpacer via `shell.nss` et les fichiers qu’il importe. Une adaptation de sa configuration est recommandée pour une bonne intégration visuelle. Les exemples ci-dessous couvrent l’ajout, la modification et la suppression d’entrées, les conditions propres à FileSpacer et les coins carrés recommandés.

## Limiter les règles des menus système

Pour les règles destinées à l’Explorateur de fichiers, au Bureau ou à la barre des tâches, ajouter `process.is_explorer &&` à leur condition `where` existante. Pour conserver leur portée dans les autres applications tout en excluant FileSpacer, utiliser plutôt `process.name != 'FileSpacer' &&`. Appliquer ce filtre aux règles génériques `menu`, `item`, `modify` et `remove`, y compris dans les fichiers importés.

## Appliquer les thèmes à FileSpacer

Autoriser FileSpacer dans `settings.exclude.where` et conserver l’import du thème. Pour la condition d’exclusion habituelle :

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Les propriétés du thème peuvent aussi utiliser des expressions propres à un processus. Par exemple, `dark = if(process.name == 'FileSpacer', true, sys.dark)` dans `theme { ... }` choisit le mode sombre pour FileSpacer et la préférence système ailleurs.

Avec FileSpacer, les modifications du thème s’appliquent aux nouveaux menus après avoir désenregistré puis réenregistré Nilesoft Shell, sans redémarrer l’Explorateur.

## Cibler un menu précis

Déclarer ces variables avant les imports qui les utilisent :

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Utiliser les propriétés `where` et `type` correspondantes dans les règles :

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu déroulant de la barre de titre | `filespacer_title` | `'*'` |
| Barre d’outils **…** | `filespacer_toolbar` | `'*'` |
| Fichiers ou dossiers sélectionnés | `filespacer_context` | `'file\|dir'` |
| Arrière-plan du dossier | `filespacer_context` | `'back'` |

Utiliser `mode='multiple'` pour que les règles restent disponibles quel que soit l’état de sélection. `process.name` ne comprend pas `.exe`. `window.parent.name` identifie une classe de fenêtre.

## Cibler une entrée de menu existante

Le mécanisme général `modify(find=...)` de Nilesoft identifie une entrée par son libellé. Le combiner avec `where` pour limiter la modification à un menu. `|` sépare les libellés possibles et `*` est un caractère générique :

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin' title='Copier le chemin du dossier')
```

Consulter la [référence de configuration de Nilesoft](https://nilesoft.org/docs/configuration/).

## Apparence recommandée des menus

Les coins carrés sont recommandés pour les menus de la barre de titre et de la barre d’outils, afin de les assortir aux boutons de FileSpacer. Ajouter ce réglage au bloc `theme { ... }` existant :

```nss
border.radius = if(
    process.name == 'FileSpacer' &&
    (
        window.parent.name == 'FileSpacer Item Window' ||
        window.parent.name == 'FileSpacer Toolbar Host'
    ),
    0,
    default
)
```

Si un rayon personnalisé est déjà défini, remplacer `default` par sa valeur précédente.

## Utiliser les actions de FileSpacer

Utiliser [`window.command`](https://nilesoft.org/docs/functions/window) avec l’un des identifiants ci-dessous pour ajouter une action de FileSpacer, même si elle ne figure pas dans sa propre composition de menu. Limiter la règle au menu de la barre de titre avec la condition `filespacer_title` définie plus haut. Les conditions habituelles de l’action s’appliquent : rétablir une disposition verrouillée nécessite par exemple un verrouillage existant.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Rétablir la disposition verrouillée' cmd=window.command(1123))
```

| Action | Identifiant |
| --- | ---: |
| Rétablir la disposition verrouillée | 1123 |
| Remplacer la disposition de référence | 1124 |
| Fermer les fenêtres descendantes | 1126 |
| Fermer les autres fenêtres | 1127 |
| Ajuster la fenêtre au contenu | 1128 |
| Enregistrer / rappeler un groupe de dossiers | 1129 |
