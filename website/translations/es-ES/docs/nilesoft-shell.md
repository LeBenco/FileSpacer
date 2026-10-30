# Referencia de configuración de Nilesoft Shell

Para una introducción, consulta [Nilesoft Shell en la guía de usuario](user-guide.md#nilesoft-shell).

Si usas [Nilesoft Shell](https://nilesoft.org/), puedes aplicar sus temas y personalizaciones de menús también a FileSpacer mediante `shell.nss` y los archivos de configuración que importa. Para esta integración opcional, se recomienda adaptar la configuración para que los menús encajen visualmente con FileSpacer. Los ejemplos siguientes muestran cómo añadir, modificar o eliminar entradas, definir condiciones para FileSpacer y usar las esquinas rectas recomendadas.

## Limitar las reglas de los menús del sistema

Para las reglas destinadas al Explorador de archivos, al escritorio o a la barra de tareas, añade `process.is_explorer &&` a su condición `where` actual. Para mantener su efecto en otras aplicaciones y excluir FileSpacer, usa en su lugar `process.name != 'FileSpacer' &&`. Aplica el filtro a las reglas genéricas `menu`, `item`, `modify` y `remove`, también en los archivos importados.

## Aplicar temas a FileSpacer

Permite FileSpacer en `settings.exclude.where` y conserva la importación del tema. Para la condición de exclusión habitual:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Las propiedades del tema también pueden usar expresiones específicas del proceso. Por ejemplo, `dark = if(process.name == 'FileSpacer', true, sys.dark)` dentro de `theme { ... }` selecciona el modo oscuro para FileSpacer y la preferencia del sistema en el resto de aplicaciones.

Con FileSpacer, los cambios del tema se aplican a los menús que se abran después de anular el registro de Nilesoft Shell y volver a registrarlo, sin reiniciar el Explorador.

## Identificar un menú concreto

Declara estas variables antes de las importaciones que las utilizan:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Usa las propiedades `where` y `type` correspondientes en tus reglas:

| Menú | `where` | `type` |
| --- | --- | --- |
| Menú desplegable de la barra de título | `filespacer_title` | `'*'` |
| Barra de herramientas **…** | `filespacer_toolbar` | `'*'` |
| Archivos o carpetas seleccionados | `filespacer_context` | `'file\|dir'` |
| Fondo de la carpeta | `filespacer_context` | `'back'` |

Usa `mode='multiple'` para las reglas que deban seguir disponibles con cualquier estado de selección. `process.name` no incluye `.exe`; `window.parent.name` identifica una clase de ventana.

## Identificar una entrada de menú existente

El mecanismo general `modify(find=...)` de Nilesoft identifica una entrada por su etiqueta. Combínalo con `where` para limitar el cambio a un menú; `|` separa etiquetas alternativas y `*` es un comodín:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copiar ruta de acceso' title='Copiar ruta de acceso de la carpeta')
```

Consulta la [referencia de configuración de Nilesoft](https://nilesoft.org/docs/configuration/).

## Aspecto recomendado de los menús

Se recomiendan esquinas rectas para los menús de la barra de título y de la barra de herramientas, a juego con los botones de FileSpacer. Añade este ajuste al bloque `theme { ... }` existente:

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

Si ya usas un radio personalizado, sustituye `default` por su valor anterior.
