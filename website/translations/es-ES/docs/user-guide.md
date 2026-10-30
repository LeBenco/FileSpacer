# Usar FileSpacer

## Navegación entre ventanas

Cada carpeta tiene una ventana que recuerda su posición, tamaño y opciones de visualización. Volver a abrir una carpeta que ya está abierta activa su ventana. Mover o cambiar el nombre de una carpeta conserva la disposición guardada.

- Abrir una carpeta o su carpeta superior cierra de forma predeterminada la ventana de origen. Mantén pulsada Ctrl para invertir este comportamiento una vez, o cambia la opción en **Configuración > General**. Abrir varias carpetas seleccionadas conserva la ventana de origen.
- El menú junto al título de la carpeta permite cambiar entre las carpetas abiertas. **Cerrar ventanas de carpetas superiores** cierra las ventanas abiertas de las carpetas que la contienen, dejando abiertas las demás.
- **Ctrl+Mayús+T**, o **Volver a abrir la última ventana cerrada** en el menú de la barra de título o en la lista de accesos directos de la barra de tareas, vuelve a abrir una carpeta cerrada sin cerrar la actual. Se restauran su disposición y selección; las vistas Detalles y Lista también recuperan la primera fila visible. El comando de la barra de tareas permite recuperar cierres anteriores sucesivamente.
- La lista de accesos directos de la barra de tareas también abre las carpetas ancladas a Acceso rápido en el Explorador de archivos.

Subir a la carpeta superior no selecciona la carpeta de la que vienes. Una carpeta superior que ya está abierta conserva su selección. El Panel de control se abre en su propia ventana de Windows.

![El menú del título de la carpeta con navegación entre ventanas y bloqueo del estado guardado.](images/window-menu.png)

## Visualización y disposición

### Disposición de iconos

En una vista de iconos puedes colocar libremente archivos y subcarpetas cuando **Organizar iconos automáticamente** está desactivado. Sus posiciones se guardan de forma independiente del tamaño y la posición de la ventana, incluso cuando su estado guardado está bloqueado.

### Opciones de visualización

**Mostrar el nombre completo del archivo seleccionado**, en **Configuración > Visualización**, permite leer entero un único nombre seleccionado en la vista de iconos. Está activado de forma predeterminada y no cambia las selecciones múltiples ni las demás vistas.

![Un nombre de archivo largo seleccionado y visible por completo en la vista de iconos.](images/selected-name.png)

**Vista previa de los modos de presentación en el menú Vista**, también activada de forma predeterminada, muestra cada modo al pasar el puntero. Un clic lo conserva; Esc o salir de la vista previa restaura la vista inicial. La vista previa no sustituye la disposición guardada.

**Mezclar carpetas y archivos al ordenar** los ordena juntos, como en [macOS](https://support.apple.com/en-au/guide/mac-help/mchlp1745/27/mac/27) y [Linux](https://www.gnu.org/s/coreutils/manual/html_node/Sorting-the-output.html). Está desactivado de forma predeterminada en **Configuración > Visualización > Archivos**. Se aplica a Detalles, Lista e iconos organizados automáticamente; se conservan las disposiciones libres. Las carpetas virtuales y los archivos comprimidos mantienen el orden de Windows.

![Carpetas y archivos en un mismo orden alfabético.](images/mixed-sorting.png)

La visibilidad de las barras de herramientas y de estado también se cambia en la configuración.

### Colocación de ventanas

**Configuración > General > Colocar ventanas nuevas en espacios libres** está activado de forma predeterminada. Las carpetas sin posición guardada se abren en el espacio disponible, reduciendo la superposición cuando ya no queda espacio libre. Las posiciones guardadas tienen prioridad. Al desactivar la opción se usa la colocación predeterminada de Windows sin mover las ventanas existentes.

### Selección y activación

Con **Configuración > General > Mantener la selección al hacer clic en una ventana inactiva** activado, hacer clic en el fondo de una carpeta inactiva activa su ventana sin borrar la selección. Pulsa una segunda vez para iniciar un rectángulo de selección. Los clics sobre elementos, los clics con el botón derecho y los gestos de ventana mantienen su comportamiento habitual.

## Gestión de ventanas

### Movimiento y cambio de tamaño

Las ventanas se pueden mover o redimensionar arrastrando desde el área de archivos, incluso sobre un elemento. Los métodos abreviados predeterminados son:

| Acción | Método abreviado |
| --- | --- |
| Mover la ventana | Alt + arrastrar con el botón izquierdo |
| Cambiar el tamaño desde la esquina inferior derecha | Alt + arrastrar con el botón derecho |

![Mover una ventana con Alt y el botón izquierdo.](images/move-window.gif)

![Cambiar el tamaño de una ventana con Alt y el botón derecho.](images/resize-window.gif)

La selección se conserva. Esc cancela el gesto. Campos de texto, encabezados, barras de desplazamiento, barras de herramientas y menús funcionan como de costumbre; Alt + doble clic abre las propiedades de un elemento.

Cada acción tiene su propia combinación configurable en **Configuración > Métodos abreviados**. Los botones izquierdo y derecho requieren Alt; el central también funciona sin Alt. Los modificadores deben coincidir exactamente; se rechazan las combinaciones reservadas o duplicadas. Los cambios se aplican a las ventanas abiertas. Restablecer vuelve a las últimas combinaciones aplicadas y Cancelar descarta los cambios pendientes.

### Bloqueo del estado guardado

El bloqueo conserva la posición, el tamaño y la vista de una carpeta para la próxima apertura. Está desactivado de forma predeterminada en **Configuración > General > Habilitar el bloqueo del estado de ventana**. Una vez activado, **Bloquear el estado de la ventana** aparece junto al título. Un candado sobre el icono de la carpeta señala el bloqueo.

<img src="images/locked-title.png" alt="Detalle del título de una carpeta con el distintivo de candado." width="280">

Se permiten cambios temporales; al volver a abrir la carpeta se recupera el estado bloqueado. Las posiciones individuales de los iconos se siguen guardando. **Desbloquear el estado de la ventana** conserva la disposición actual. Ocultar el candado no elimina el bloqueo.

Desactivar la función pide confirmación y elimina todos los bloqueos, incluidos los de carpetas cerradas. Volver a activarla no los restaura.

## Búsqueda e historial

Escribe todo o parte del nombre de un archivo o una carpeta y pulsa Entrar o haz clic en la lupa para filtrar la carpeta actual. **Configuración > General > Filtrar mientras se escribe** aplica el filtro durante la escritura. Este filtro local nunca inicia Everything automáticamente.

Activar el cuadro muestra las búsquedas recientes y, cuando la integración está activada, **Buscar en Everything**. Usa las flechas Arriba/Abajo y Entrar, o haz clic, para elegir una entrada. Entrar sin una sugerencia seleccionada aplica el filtro local. Esc cierra la lista sin borrar el texto; Ctrl+F vuelve a activar el cuadro.

El historial conserva de forma predeterminada cinco búsquedas distintas confirmadas por carpeta. Puedes cambiar el límite de 0 a 20 en la configuración general; 0 vacía y desactiva el historial. Escribir sin confirmar no añade ninguna entrada. Consulta [Everything](#everything) para buscar en subcarpetas o en todos los archivos indexados.

![Sugerencias de búsqueda con Everything y consultas recientes sobre archivos de proyecto ficticios.](images/search.png)

## Integración

### Integración con Windows

FileSpacer se puede establecer como gestor de archivos predeterminado durante la instalación o después, en **Configuración > Integración**, con **Establecer como explorador predeterminado**. **Restaurar Explorador de archivos predeterminado** devuelve la apertura de carpetas al Explorador de archivos.

El menú contextual de las carpetas ofrece **Abrir en FileSpacer**, o **Abrir en el Explorador de archivos de Windows** cuando FileSpacer es el predeterminado. También funciona en unidades, carpetas ZIP, ubicaciones de red y fondos de carpetas, sin exigir un cambio de aplicación predeterminada.

Los archivos se abren con su aplicación habitual de Windows. Las carpetas de red, las vistas previas y los comandos añadidos al menú contextual también siguen el funcionamiento de Windows. **Opciones del Explorador de archivos** abre las preferencias compartidas de las carpetas; algunas también afectan a FileSpacer.

### Everything

Una instalación existente de [Everything](https://www.voidtools.com/) se puede integrar para lanzar búsquedas avanzadas desde el campo de FileSpacer. Los resultados se abren en Everything.

La opción se activa en **Configuración > Integración**. Everything se encuentra automáticamente o con **Detectar**; una copia portátil se puede seleccionar manualmente. La búsqueda abarca la carpeta actual y sus subcarpetas, o **En todas partes**. Las carpetas virtuales requieren En todas partes.

**Buscar en Everything** envía la consulta escrita, con la sintaxis de Everything. Si se inicia correctamente, se vacían el campo y el filtro local de FileSpacer; si falla, se conservan para volver a intentarlo.

![Una consulta en FileSpacer y, al lado, los resultados correspondientes en Everything.](images/everything-search.png)

### Nilesoft Shell

[Nilesoft Shell](https://nilesoft.org/) puede dar formato a los menús de FileSpacer y personalizar sus comandos. Se recomienda adaptar su configuración para lograr una apariencia coherente, sobre todo con esquinas rectas en los menús de la barra de título y de herramientas.

La [referencia de configuración](nilesoft-shell.md) explica cómo permitir FileSpacer, conservar el tema y limitar al Explorador las reglas específicas de este. Después de un cambio, anular y repetir el registro de Nilesoft Shell lo aplica a los menús que se abran a continuación.

El tema Modern de abajo compara sus esquinas predeterminadas con las esquinas rectas recomendadas.

**Menú de navegación**

| Esquinas predeterminadas | Esquinas rectas recomendadas |
| --- | --- |
| <img src="images/nilesoft-navigation-default.png" alt="Menú de navegación de la barra de título con las esquinas redondeadas predeterminadas del tema Modern." width="320"> | <img src="images/nilesoft-navigation-adapted.png" alt="El mismo menú de navegación con las esquinas rectas recomendadas." width="320"> |

**Menú principal (…)**

| Esquinas predeterminadas | Esquinas rectas recomendadas |
| --- | --- |
| <img src="images/nilesoft-main-menu-default.png" alt="Menú principal con las esquinas redondeadas predeterminadas del tema Modern." width="320"> | <img src="images/nilesoft-main-menu-adapted.png" alt="El mismo menú principal con las esquinas rectas recomendadas." width="320"> |

## Configuración y actualizaciones

Ctrl+, abre la configuración aunque la barra de herramientas esté oculta. La interfaz sigue el idioma de visualización de Windows y recurre al inglés si no está disponible; cambiar de idioma requiere reiniciar. F1 abre la guía instalada, también disponible sin conexión.

Las actualizaciones se buscan en **Configuración > Acerca de**. La descarga y la instalación requieren confirmación; la instalación nunca es automática. Al terminar, se vuelven a abrir las carpetas anteriores con sus disposiciones guardadas. Las comprobaciones semanales son opcionales y están desactivadas de forma predeterminada; se activan en Acerca de.

## Datos locales

Las disposiciones de las carpetas, los bloqueos, las búsquedas y el historial de carpetas cerradas se guardan localmente en `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. Las preferencias generales están en el Registro de Windows, en `HKEY_CURRENT_USER\Software\FileSpacer`. El estado guardado puede contener rutas de carpetas, incluidas las de red. Los archivos temporales de actualización y la lista de carpetas que se volverán a abrir permanecen en `%TEMP%\FileSpacerUpdate-{GUID}`.

**Configuración > Acerca de > Mantenimiento > Restablecer el estado de todas las carpetas** cierra las ventanas de carpetas y borra sus disposiciones, historiales y bloqueos guardados. Conserva los archivos y las preferencias generales.

Para solucionar problemas, `FileSpacer.exe /reset-options` restablece las preferencias generales; `/reset-all` borra también el estado guardado de las carpetas. Ninguno elimina archivos, cambia las asociaciones de Windows ni borra informes de diagnóstico existentes.

## Privacidad

FileSpacer no incluye telemetría, seguimiento del uso ni publicidad. Los informes de errores no se envían automáticamente. Los posibles informes de diagnóstico permanecen en `%LOCALAPPDATA%\FileSpacer`; compartirlos es opcional y puede revelar rutas de archivos o partes de la memoria del proceso.

Las comprobaciones de actualizaciones y las descargas contactan con GitHub mediante HTTPS. GitHub recibe la información habitual de conexión, como la dirección IP y la versión de la aplicación, pero ningún estado de carpeta, ruta de archivo ni informe de diagnóstico.

Windows recoge y gestiona las credenciales de las unidades de red, igual que en el Explorador de archivos. FileSpacer no las lee ni las almacena. Una búsqueda en Everything envía la consulta y, si está limitada a una carpeta, su ruta a la aplicación Everything local.
