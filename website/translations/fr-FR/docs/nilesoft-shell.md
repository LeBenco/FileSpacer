# Référence de configuration de Nilesoft Shell

Pour une présentation générale, consultez [Nilesoft Shell dans le guide d’utilisation](user-guide.md#nilesoft-shell).

Si vous utilisez [Nilesoft Shell](https://nilesoft.org/), vous pouvez aussi appliquer ses thèmes et ses personnalisations aux menus de FileSpacer à travers `shell.nss` et les fichiers de configuration qu’il importe. Pour cette intégration facultative, une adaptation de la configuration est recommandée afin que les menus s’intègrent visuellement à FileSpacer. Les exemples ci-dessous couvrent l’ajout, la modification et la suppression d’entrées, les conditions propres à FileSpacer et les coins carrés recommandés.

## Limiter les règles des menus système

Pour les règles destinées à l’Explorateur de fichiers, au Bureau ou à la barre des tâches, ajoutez `process.is_explorer &&` à leur condition `where` existante. Pour conserver leur portée dans les autres applications tout en excluant FileSpacer, utilisez plutôt `process.name != 'FileSpacer' &&`. Appliquez ce filtre aux règles génériques `menu`, `item`, `modify` et `remove`, y compris dans les fichiers importés.

## Appliquer les thèmes à FileSpacer

Autorisez FileSpacer dans `settings.exclude.where` et conservez l’import du thème. Pour la condition d’exclusion habituelle :

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Les propriétés du thème peuvent aussi utiliser des expressions propres à un processus. Par exemple, `dark = if(process.name == 'FileSpacer', true, sys.dark)` dans `theme { ... }` choisit le mode sombre pour FileSpacer et la préférence système ailleurs.

Avec FileSpacer, les modifications du thème s’appliquent aux nouveaux menus après avoir désenregistré puis réenregistré Nilesoft Shell, sans redémarrer l’Explorateur.

## Cibler un menu précis

Déclarez ces variables avant les imports qui les utilisent :

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Utilisez les propriétés `where` et `type` correspondantes dans vos règles :

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu déroulant de la barre de titre | `filespacer_title` | `'*'` |
| Barre d’outils **…** | `filespacer_toolbar` | `'*'` |
| Fichiers ou dossiers sélectionnés | `filespacer_context` | `'file\|dir'` |
| Arrière-plan du dossier | `filespacer_context` | `'back'` |

Utilisez `mode='multiple'` pour que les règles restent disponibles quel que soit l’état de sélection. `process.name` ne comprend pas `.exe` ; `window.parent.name` identifie une classe de fenêtre.

## Cibler une entrée de menu existante

Le mécanisme général `modify(find=...)` de Nilesoft identifie une entrée par son libellé. Associez-le à `where` pour limiter la modification à un menu ; `|` sépare les libellés possibles et `*` est un caractère générique :

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin' title='Copier le chemin du dossier')
```

Consultez la [référence de configuration de Nilesoft](https://nilesoft.org/docs/configuration/).

## Apparence recommandée des menus

Les coins carrés sont recommandés pour les menus de la barre de titre et de la barre d’outils, afin de les assortir aux boutons de FileSpacer. Ajoutez ce réglage à votre bloc `theme { ... }` existant :

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

Si vous utilisez déjà un rayon personnalisé, remplacez `default` par sa valeur précédente.
