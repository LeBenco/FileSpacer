<a id="nilesoft-shell-configuration-reference"></a>

# Référence de configuration de Nilesoft Shell

Pour une présentation générale, voir [Nilesoft Shell dans le guide d’utilisation](user-guide.md#nilesoft-shell).

[Nilesoft Shell](https://nilesoft.org/) peut appliquer ses thèmes et ses personnalisations aux menus de FileSpacer via `shell.nss` et les fichiers qu’il importe. Une adaptation de sa configuration est recommandée pour une bonne intégration visuelle. Les exemples ci-dessous couvrent l’ajout, la modification et la suppression d’entrées, les conditions propres à FileSpacer et les coins carrés recommandés.

<a id="scope-system-menu-rules"></a>

## Limiter les règles des menus système

Pour les règles destinées à l’Explorateur de fichiers, au Bureau ou à la barre des tâches, ajouter `process.is_explorer &&` à leur condition `where` existante. Pour conserver leur portée dans les autres applications tout en excluant FileSpacer, utiliser plutôt `process.name != 'FileSpacer' &&`. Appliquer ce filtre aux règles génériques `menu`, `item`, `modify` et `remove`, y compris dans les fichiers importés.

<a id="apply-themes-to-filespacer"></a>

## Appliquer les thèmes à FileSpacer

Autoriser FileSpacer dans `settings.exclude.where` et conserver l’import du thème. Pour la condition d’exclusion habituelle :

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Les propriétés du thème peuvent aussi utiliser des expressions propres à un processus. Par exemple, `dark = if(process.name == 'FileSpacer', true, sys.dark)` dans `theme { ... }` choisit le mode sombre pour FileSpacer et la préférence système ailleurs.

Après avoir enregistré un fichier NSS, **Ctrl + clic droit** sur le Bureau ou la barre des tâches force Nilesoft à relire sa configuration dans l’Explorateur, sans redémarrer celui-ci ni désenregistrer l’extension. Nilesoft recharge aussi lorsqu’il détecte le bouton gauche maintenu à l’ouverture d’un menu ; ouvrir un bouton FileSpacer peut donc appliquer immédiatement les modifications. Cela dépend du geste d’ouverture, pas d’un mécanisme de rechargement plus rapide dans FileSpacer.

<a id="menu-opening-delay"></a>

## Délai d’ouverture des menus

Pour supprimer le [délai d’ouverture des menus](https://nilesoft.org/docs/configuration/settings) de Nilesoft Shell, définir `showdelay = 0` dans le bloc `settings` existant de `shell.nss`. Pour limiter ce réglage à FileSpacer, utiliser plutôt `showdelay = if(process.name == 'FileSpacer', 0, 200)`, en remplaçant `200` par le délai à conserver pour les autres applications, en millisecondes.

<a id="target-a-specific-menu"></a>

## Cibler un menu précis

Déclarer ces variables avant les imports qui les utilisent :

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_view = (filespacer_toolbar && window.parent.title == 'FileSpacer View Menu')
$filespacer_more = (filespacer_toolbar && !filespacer_view)
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Utiliser les propriétés `where` et `type` correspondantes dans les règles :

| Menu | `where` | `type` |
| --- | --- | --- |
| Menu déroulant de la barre de titre | `filespacer_title` | `'*'` |
| Barre d’outils **…** et **Affichage** | `filespacer_toolbar` | `'*'` |
| **Affichage** uniquement | `filespacer_view` | `'*'` |
| Barre d’outils **…** uniquement | `filespacer_more` | `'*'` |
| Fichiers ou dossiers sélectionnés | `filespacer_context` | `'file\|dir'` |
| Arrière-plan du dossier | `filespacer_context` | `'back'` |

Utiliser `mode='multiple'` pour que les règles restent disponibles quel que soit l’état de sélection. `process.name` ne comprend pas `.exe`. `window.parent.name` identifie une classe de fenêtre.

`filespacer_toolbar` couvre toujours les deux menus de la barre d’outils. Utiliser `filespacer_view` pour Affichage seul ou `filespacer_more` pour **…** seul. Le marqueur interne `window.parent.title` est stable et non traduit. Pour conserver Terminal en dehors d’Affichage, ajouter `!filespacer_view &&` à sa condition `where` existante, en entourant cette condition de parenthèses :

```nss
where=(!filespacer_view && (sel.count || wnd.is_taskbar || wnd.is_edit))
```

<a id="target-an-existing-menu-entry"></a>

## Cibler une entrée de menu existante

Le mécanisme général `modify(find=...)` de Nilesoft identifie une entrée par son libellé. Le combiner avec `where` pour limiter la modification à un menu. `|` sépare les libellés possibles et `*` est un caractère générique :

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin' title='Copier le chemin du dossier')
```

Consulter la [référence de configuration de Nilesoft](https://nilesoft.org/docs/configuration/).

<a id="recommended-menu-appearance"></a>

## Apparence recommandée des menus

Les coins carrés sont recommandés pour les trois menus : le menu déroulant de la barre de titre, **…** et **Affichage**, afin de les assortir aux boutons de FileSpacer. Ajouter ce réglage au bloc `theme { ... }` existant :

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

<a id="use-filespacer-actions"></a>

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
