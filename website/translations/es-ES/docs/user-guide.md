# Usar FileSpacer

## Navegación entre ventanas

Cada carpeta tiene una ventana que recuerda su posición, tamaño y opciones de visualización. Volver a abrir una carpeta que ya está abierta activa su ventana. Mover o cambiar el nombre de una carpeta conserva la disposición guardada.

- Abrir una carpeta o su carpeta superior cierra de forma predeterminada la ventana de origen. Mantén pulsada Ctrl para invertir este comportamiento una vez, o cambia la opción en **Configuración > General**. Abrir varias carpetas seleccionadas conserva la ventana de origen.
- El menú junto al título de la carpeta permite cambiar entre las carpetas abiertas. **Cerrar ventanas de carpetas superiores** cierra las ventanas abiertas de las carpetas que la contienen, dejando abiertas las demás.
- **Ctrl+Mayús+T**, o **Volver a abrir la última ventana cerrada** en el menú de la barra de título o en la lista de accesos directos de la barra de tareas, vuelve a abrir una carpeta cerrada sin cerrar la actual. Se restauran su disposición y selección; las vistas Detalles y Lista también recuperan la primera fila visible. El comando de la barra de tareas permite recuperar cierres anteriores sucesivamente.
- La lista de accesos directos de la barra de tareas también abre las carpetas ancladas a Acceso rápido en el Explorador de archivos.

Subir a la carpeta superior no selecciona la carpeta de la que vienes. Una carpeta superior que ya está abierta conserva su selección. El Panel de control se abre en su propia ventana de Windows.

![El menú del título de la carpeta con navegación entre ventanas y bloqueo del estado guardado.](images/window-menu.png)

*El menú del título, aquí con el estado de la ventana bloqueado.*

## Visualización y disposición

### Disposición de iconos

Desactiva **Organizar iconos automáticamente** en una vista de iconos para colocar libremente archivos y subcarpetas. Sus posiciones se guardan de forma independiente del tamaño y la posición de la ventana, incluso cuando su estado guardado está bloqueado.

### Opciones de visualización

La visibilidad de la barra de herramientas y de la barra de estado se cambia en **Configuración**.

En **Configuración > Visualización**, dos opciones están activadas de forma predeterminada:

- **Mostrar el nombre completo del archivo seleccionado** mantiene legible un nombre seleccionado en la vista Iconos. No modifica las selecciones múltiples ni las otras vistas.
- **Vista previa de los modos de presentación en el menú Vista** muestra una vista previa del modo sobre el que pasas el puntero. Haz clic para conservarlo; Esc o salir de la vista previa restaura la vista inicial. La vista previa no sustituye la disposición guardada.

**Mezclar carpetas y archivos al ordenar**, en **Configuración > Visualización > Archivos**, está desactivado de forma predeterminada. Actívalo para ordenar carpetas y archivos juntos, como en [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) y [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Se aplica a las vistas Detalles, Lista e Iconos organizados automáticamente. Las disposiciones libres se conservan. Las carpetas virtuales y los archivos comprimidos mantienen el orden de Windows.

![Una ventana con carpetas y archivos de ejemplo mezclados en orden alfabético.](images/mixed-sorting.png)

*Las carpetas y los archivos comparten el mismo orden alfabético.*

### Colocación de ventanas

**Configuración > General > Colocar ventanas nuevas en espacios libres** está activado de forma predeterminada. Las carpetas sin posición guardada se abren en el espacio disponible, reduciendo la superposición cuando ya no queda espacio libre. Las posiciones guardadas tienen prioridad. Al desactivar la opción se usa la colocación predeterminada de Windows sin mover las ventanas existentes.

### Selección y activación

Con **Configuración > General > Mantener la selección al hacer clic en una ventana inactiva** activado, hacer clic en el fondo de una carpeta inactiva activa su ventana sin borrar la selección. Pulsa una segunda vez para iniciar un rectángulo de selección. Los clics sobre elementos, los clics con el botón derecho y los gestos de ventana mantienen su comportamiento habitual.

## Gestión de ventanas

### Movimiento y cambio de tamaño

Arrastra en el área de archivos, sobre el fondo o sobre un elemento, con estos gestos predeterminados:

| Acción | Método abreviado |
| --- | --- |
| Mover la ventana | Alt + arrastrar con el botón izquierdo |
| Cambiar el tamaño desde la esquina inferior derecha | Alt + arrastrar con el botón derecho |

**Mover la ventana — Alt + arrastrar con el botón izquierdo**

![Movimiento de una ventana de carpeta manteniendo Alt y arrastrando con el botón izquierdo en el área de archivos.](images/move-window.gif)

**Cambiar el tamaño — Alt + arrastrar con el botón derecho**

![Cambio de tamaño de una ventana de carpeta manteniendo Alt y arrastrando con el botón derecho en el área de archivos.](images/resize-window.gif)

La selección se conserva. Esc cancela el gesto y restaura la posición y las dimensiones iniciales. Los cuadros de texto, encabezados, barras de desplazamiento, barras de herramientas y menús mantienen su comportamiento normal. Alt + doble clic sobre un elemento abre sus propiedades.

En **Configuración > Métodos abreviados**, activa o desactiva cada acción y despliégala para cambiar su combinación. El área de captura directa registra una combinación de teclas y botón del ratón. Los botones izquierdo y derecho requieren Alt; el central también admite combinaciones sin Alt. Las teclas modificadoras deben coincidir exactamente. No se pueden aplicar combinaciones reservadas o duplicadas.

El botón de valor predeterminado restablece una acción. **Restablecer** recupera los últimos métodos abreviados aplicados; **Cancelar** descarta los cambios pendientes. **Aplicar** o **Aceptar** hace que los cambios estén disponibles en las ventanas abiertas de FileSpacer.

### Bloqueo del estado guardado

Activa **Configuración > General > Habilitar el bloqueo del estado de ventana** (desactivado de forma predeterminada) y elige **Bloquear el estado de la ventana** en el menú junto al título de la carpeta. El bloqueo guarda la posición, el tamaño y las opciones de visualización. Un **distintivo de candado sobre el icono de la carpeta en la barra de título**, junto al nombre, identifica un estado bloqueado.

<img src="images/locked-title.png" alt="Detalle de la barra de título: un distintivo de candado se superpone al icono de la carpeta junto a Studio project." width="420">

*El distintivo de candado indica que el estado guardado está bloqueado.*

Puedes seguir moviendo y cambiando temporalmente el tamaño o la vista de la ventana. Volver a abrir la carpeta restaura el estado bloqueado. Las posiciones individuales de los iconos siguen guardándose normalmente. **Desbloquear el estado de la ventana** conserva la disposición actual. Puedes ocultar el distintivo en la configuración general sin quitar el bloqueo.

Desactivar la función pide confirmación y elimina todos los bloqueos guardados, incluidos los de carpetas cerradas. Volver a activarla no los restaura.

## Búsqueda e historial

Escribe en el cuadro de búsqueda y pulsa Entrar o haz clic en la lupa para filtrar la carpeta actual. **Configuración > General > Filtrar mientras se escribe** aplica el filtro durante la escritura. Este filtro local nunca inicia Everything automáticamente.

Activar el cuadro muestra las búsquedas recientes y, cuando la integración está activada, **Buscar en Everything**. Usa las flechas Arriba/Abajo y Entrar, o haz clic, para elegir una entrada. Entrar sin una sugerencia seleccionada aplica el filtro local. Esc cierra la lista sin borrar el texto; Ctrl+F vuelve a activar el cuadro.

El historial conserva de forma predeterminada cinco búsquedas distintas confirmadas por carpeta. Puedes cambiar el límite de 0 a 20 en la configuración general; 0 vacía y desactiva el historial. Escribir sin confirmar no añade ninguna entrada. Consulta [Everything](#everything) para buscar en subcarpetas o en todos los archivos indexados.

![Sugerencias de búsqueda con Everything y consultas recientes sobre archivos de proyecto ficticios.](images/search.png)

*Everything es una acción independiente; las búsquedas recientes filtran la carpeta actual.*

## Integración

### Integración con Windows

El instalador ofrece **Establecer FileSpacer como gestor de archivos predeterminado**, sin marcar de forma predeterminada. También instala el componente del menú contextual de carpetas. Puedes cambiar la elección más adelante en **Configuración > Integración** con **Establecer como explorador predeterminado**; **Restaurar Explorador de archivos predeterminado** devuelve la apertura de carpetas al Explorador de archivos de Windows.

El componente del menú contextual ofrece **Abrir en el Explorador de archivos de Windows** cuando FileSpacer es el gestor predeterminado, y **Abrir en FileSpacer** en caso contrario. Se aplica a carpetas, unidades, carpetas comprimidas, ubicaciones de red y fondos de carpetas. Instalar solo este componente no cambia la aplicación predeterminada.

**Opciones del Explorador de archivos**, en la misma página de configuración, abre las opciones de carpeta de Windows; algunas también afectan a la vista de archivos de FileSpacer.

### Everything

Instala [Everything](https://www.voidtools.com/) por separado y actívalo en **Configuración > Integración**. Deja vacía la ruta del ejecutable para la detección automática, usa **Detectar** o elige manualmente su archivo `.exe` para una instalación portable.

Elige el ámbito de búsqueda: la carpeta actual y sus subcarpetas, o **En todas partes**. Las carpetas virtuales no tienen un ámbito en el sistema de archivos; usa En todas partes para ellas.

Escribe una consulta en el cuadro de búsqueda de FileSpacer y elige **Buscar en Everything** entre las sugerencias. Los resultados se abren en Everything, con su índice y su sintaxis de búsqueda. Si el inicio es correcto, FileSpacer vacía el cuadro y elimina el filtro local; si falla, conserva ambos para que puedas volver a intentarlo.

### Nilesoft Shell

Si usas [Nilesoft Shell](https://nilesoft.org/), puedes dejar que gestione también el aspecto de los menús de FileSpacer y personalice sus entradas.

Para esta integración opcional, se recomienda adaptar la configuración de Nilesoft para que los menús encajen visualmente con FileSpacer, en particular usando esquinas rectas en los menús de la barra de título y de la barra de herramientas.

La [referencia de configuración con ejemplos](nilesoft-shell.md) explica cómo permitir FileSpacer, conservar la importación del tema y limitar al Explorador las reglas reservadas para él. Si modificas la configuración, anula el registro de Nilesoft Shell y vuelve a registrarlo para aplicar los cambios a los menús que se abran a continuación.

El tema Modern que aparece abajo usa sus esquinas redondeadas predeterminadas a la izquierda y las esquinas rectas recomendadas a la derecha.

**Menú de navegación**

| Esquinas predeterminadas | Esquinas rectas recomendadas |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menú de navegación de la barra de título con las esquinas redondeadas predeterminadas del tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="El mismo menú de navegación con las esquinas rectas recomendadas." width="320"> |

**Menú principal (…)**

| Esquinas predeterminadas | Esquinas rectas recomendadas |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menú principal con las esquinas redondeadas predeterminadas del tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="El mismo menú principal con las esquinas rectas recomendadas." width="320"> |

## Configuración y actualizaciones

**Preferencias.** **Ctrl+,** abre la configuración incluso con la barra de herramientas oculta. La interfaz sigue el idioma de visualización de Windows y usa el inglés como alternativa; cambiar de idioma requiere reiniciar la aplicación.

**Actualizaciones.** Busca una nueva versión en **Configuración > Acerca de** y elige **Instalar FileSpacer vX.Y.Z** para actualizar. La instalación requiere confirmación; las actualizaciones nunca se instalan automáticamente. Tras la instalación, FileSpacer vuelve a abrir las carpetas que estaban abiertas con sus disposiciones guardadas. Las comprobaciones semanales opcionales están desactivadas de forma predeterminada y pueden activarse en Acerca de.

**Mantenimiento.** **Configuración > Acerca de > Mantenimiento > Restablecer el estado de todas las carpetas** cierra las ventanas de carpetas abiertas y borra las disposiciones guardadas, el historial y los bloqueos del estado. Tus archivos y las preferencias globales se conservan.
