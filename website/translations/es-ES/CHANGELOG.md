# Historial de versiones

## Próxima versión

## 1.2.2 — 2026-10-28

- Corrige el uso del proveedor de carpetas personalizado con el orden mixto y los iconos colocados libremente. Usa el proveedor nativo para la colocación libre y cambia de proveedor al confirmar una disposición ordenada, conservando las posiciones guardadas de los iconos, la selección y el foco.

## 1.2.1 — 2026-10-28

- Reduce la aparición transitoria de iconos genéricos al abrir carpetas con orden mixto. Prepara los iconos visibles en la caché de Windows antes de mostrar la vista nativa, con un tiempo de espera limitado para los controladores de iconos lentos.

## 1.2.0 — 2026-10-28

- Añade un orden mixto opcional para carpetas y archivos del sistema de archivos, en Visualización → Archivos, conservando la vista nativa de Windows y el estado guardado de las carpetas. La opción está desactivada de forma predeterminada.
- Añade traducciones de la interfaz al alemán, italiano, español, portugués (Portugal) y griego, además de inglés y francés.
- Muestra el título y el subtítulo del instalador como texto de Windows más grande, adaptado a los PPP y suavizado con ClearType.
- Amplía la guía en línea con capturas inglesas y francesas, demostraciones de gestos de ventana y comparaciones de los menús de Nilesoft Shell.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

## 1.1.10 — 2026-10-27

- Solicita la confirmación de Windows solo al instalar o desinstalar para todos los usuarios y aplica las opciones personales de integración al usuario que inició la instalación.
- Muestra el historial completo de versiones en un panel Novedades con desplazamiento junto a las opciones de la primera pantalla del instalador.
- Corrige el restablecimiento de los componentes elegidos durante las actualizaciones; recupera las selecciones guardadas y conserva el gestor de archivos predeterminado en las actualizaciones silenciosas.
- Usa la misma barra lateral de FileSpacer, el logotipo grande y la paleta gris claro y blanca en todas las pantallas del instalador y del desinstalador. Asigna al instalador un icono propio, compuesto por los mapas de bits nativos de la aplicación y un distintivo de flecha de descarga optimizado por separado para cada tamaño, manteniendo un borde amarillo fino a la derecha y abajo de la carpeta.
- Corrige cierres inesperados al salir provocados por la inicialización anticipada de datos de carpetas locales al hilo en hilos de trabajo tardíos del shell.
- Ofrece Abrir en el Explorador de archivos de Windows en los menús contextuales de carpetas cuando FileSpacer es predeterminado, y Abrir en FileSpacer en caso contrario; actualiza los menús al cambiar la opción, después de instalar y al iniciar.
- Corrige la apertura de accesos directos a servidores y recursos compartidos de red en el Explorador de archivos cuando FileSpacer es predeterminado. Actualiza el componente del menú contextual de carpetas y vuelve a seleccionar FileSpacer como predeterminado para activar la integración de red.
- Amplía el icono de la aplicación de Acerca de a 48 píxeles con escala del 100 %, colocando el texto a su lado.
- Mantiene completamente visibles los títulos de las ventanas maximizadas y extiende la barra de herramientas por todo el ancho disponible.
- Permite ejecutar las pruebas de regresión en un cliente Hyper-V configurado mientras la aplicación y las herramientas de prueba se compilan en el host.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

## 1.1.9 — 2026-10-27

- Muestra una vista previa de los modos al pasar el puntero por el menú Vista, con restauración progresiva al cancelar y una opción activada de forma predeterminada en Visualización.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

## 1.1.8 — 2026-10-27

- Sustituye el icono de la aplicación y el logotipo del README por una proyección de un teseracto, con una relación de 11/20 entre las aristas moradas y verdes del cubo, manteniendo optimizados los tamaños pequeños.
- Aclara las etiquetas inglesas y francesas de la configuración y agrupa las opciones de bloqueo de ventanas.
- Añade una opción para ocultar el distintivo de candado de las ventanas bloqueadas sin cambiar el bloqueo de su estado.
- Usa los fondos nativos del tema de Windows en las páginas de configuración y los controles de métodos abreviados.
- Explica la colocación de ventanas y la agrupación en la barra de tareas con información sobre herramientas estándar de Windows junto a las opciones.
- Vincula la configuración a la ventana de carpeta desde la que se abrió y descarta los cambios pendientes al cerrarla. Las operaciones de mantenimiento confirmadas cierran la configuración sin aplicar los cambios pendientes.
- Reorganiza el README alrededor de la navegación espacial y las funciones, con referencias de investigación, un enlace a la última versión, guías de compilación y de Nilesoft Shell y documentación de usuario ampliada.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

## 1.1.7 — 2026-10-26

- Minimiza la superposición de ventanas nuevas con geometría exacta, buscando primero espacio libre. Cambia el nombre de la opción general a «Minimizar la superposición de ventanas».
- Ofrece la elección opcional del gestor de archivos predeterminado en los instaladores inglés y francés, con instrucciones para activarla o revertirla después desde la configuración de integración de FileSpacer.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

## 1.1.6 — 2026-10-26

- Corrige las ventanas de carpetas que permanecían abiertas al eliminar una carpeta superior o moverla a la Papelera de reciclaje.
- Evita que las listas de accesos directos ofrezcan volver a abrir carpetas que ya no están disponibles.
- Reduce la superposición al colocar ventanas sin posición guardada, ignorando la ventana de origen sustituida durante la navegación espacial. Activa este comportamiento de forma predeterminada con una opción en General > Navegación de carpetas.
- Mueve el mantenimiento del estado de carpetas de General a Acerca de.
- Abre las ventanas nuevas con una proporción equilibrada de 4:3 (580 × 435 a 96 PPP), conservando la altura existente y respetando las dimensiones configuradas y el tamaño guardado de cada carpeta.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

## 1.1.5 — 2026-10-26

- Corrige la creación involuntaria de un acceso directo después de Alt + doble clic seguido de un movimiento del ratón. Conserva las propiedades nativas, la selección y el foco sin reproducir una pulsación mantenida.
- Corrige las pruebas de los tiempos nativos de doble clic y los destinos físicos del ratón; compara propiedades, selección, foco, anclaje de la selección, geometría y contenidos de carpetas con y sin gestos de ventana, incluidas las combinaciones Ctrl/Mayús.
- Aísla el ejecutable real de prueba de Everything y su INI de inicio para que la opción de administrador instalada por el usuario no pueda redirigir las búsquedas de prueba ni solicitar una elevación de privilegios. Comprueba que la configuración instalada no cambie.
- Mantiene activo el escritorio y suspende temporalmente el protector de pantalla durante las pruebas de interfaz, restaurando el estado inicial durante la limpieza incluso después de errores.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

## 1.1.4 — 2026-10-25

Esta versión se basa en la 1.1.3, que ya ofrece gestos configurables de teclado y ratón para mover las ventanas y cambiar su tamaño.

- Añade una búsqueda externa opcional con Everything, con detección automática, ruta manual del ejecutable, ámbito limitado a la carpeta y subcarpetas o a todas partes y errores de inicio explícitos.
- Muestra automáticamente Everything y las búsquedas recientes al activar el cuadro existente, sin añadir un botón. Conserva la búsqueda nativa, el filtro inmediato y la reutilización de consultas locales.
- Recuerda de forma predeterminada cinco búsquedas recientes por carpeta entre sesiones, con un límite de cero a veinte independiente de Everything. Conserva los datos existentes y limpia el historial según la política prudente del estado de carpetas.
- Muestra el icono del ejecutable de Everything al tamaño adecuado, con extracción en segundo plano y una caché en memoria que se actualiza al cambiar el ejecutable.
- Añade pruebas de regresión para almacenamiento aislado, migración e integración real Shell/Everything; hace más robusto el lector de pruebas del shell cuando los filtros sucesivos cambian la vista.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

## 1.1.3 — 2026-10-25

- Añade la pestaña Métodos abreviados a la configuración para personalizar o desactivar los gestos de movimiento y cambio de tamaño con teclas modificadoras y los botones izquierdo, central o derecho, captura directa y validación de conflictos.
- Deja a la vista nativa del Explorador la gestión de Alt + doble clic para abrir las propiedades.
- Restaura los nombres completos con fondo opaco para un único elemento seleccionado en la vista Iconos. Las selecciones múltiples conservan el aspecto nativo de Windows; el texto de la preferencia precisa ahora su ámbito.
- Añade pruebas exhaustivas de los métodos abreviados y de los gestos reales del ratón, con recursos de configuración franceses e ingleses.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

## 1.1.2 — 2026-10-25

- Añade el bloqueo opcional de la geometría y la vista guardadas de una ventana desde el menú del título. Permite cambios temporales y sigue guardando normalmente las posiciones individuales de los iconos.
- Muestra un distintivo de candado Small & Flat sobre los iconos de carpetas bloqueadas, con ICO nativos de varias resoluciones, adaptación a los PPP y colores de contraste alto.
- Confirma la desactivación global del bloqueo, elimina los bloqueos guardados de carpetas abiertas y cerradas y reanuda el guardado normal entre procesos.
- Amplía las pruebas de regresión a transiciones del bloqueo, errores de persistencia, restauración del shell y representación nativa del distintivo.
- Volver a abrir una carpeta cerrada con Ctrl+Mayús+T o las listas de accesos directos restaura en Detalles y Lista la fila que estaba arriba al cerrarla.
- El diagnóstico de cierres inesperados sigue escribiendo un minivolcado y un informe en los datos locales de la aplicación; elimina la atribución a la antigua representación de nombres completos, el cambio automático de su preferencia y el reinicio.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

## 1.1.1 — 2026-10-24

- Restaura la activación que conserva la selección: el primer clic sobre el fondo vacío de una carpeta inactiva activa su ventana sin borrar la selección. La preferencia vuelve a estar habilitada.
- Corrige un cierre inesperado al cerrar una ventana de carpeta mientras se actualiza la vista del shell, con Actualizar, filtro por nombre o comandos del shell Actualizar/Nuevo.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

## 1.1.0 — 2026-10-24

- Reorganiza las vistas de carpetas para usar las implementaciones de Windows Shell mediante API COM públicas en vez de adaptar controles internos del shell.
- Mantiene el estado espacial de las ventanas, la selección y el foco, las posiciones de los iconos y el filtrado conectados al almacenamiento SQLite de producción y a los eventos públicos del shell.
- Conserva los comandos nativos de archivos y los gestos Alt de FileSpacer para mover, cambiar el tamaño y abrir propiedades.
- Abre el Panel de control en una ventana propia de Windows, conservando la carpeta de origen y su selección.
- Elimina la selección automática de la carpeta de origen al subir a una carpeta superior por motivos ergonómicos: las selecciones posteriores con Ctrl no deben abrir ambas carpetas por error.
- Acepta tres diferencias en esta versión: no están disponibles la representación personalizada de nombres seleccionados completos con fondo opaco, la conservación de la selección al activar ni la restauración exacta del desplazamiento. Desactiva los dos controles de configuración inactivos, conservando sus preferencias guardadas.
- Simplifica las pruebas de regresión alrededor de la configuración nativa de producción del shell; retira las comparaciones exploratorias de perfiles, las pruebas de integración de controles y los informes reservados a la migración.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

## 1.0.8 — 2026-10-22

- Mueve las ventanas con Alt + arrastrar con el botón izquierdo y cambia su tamaño con Alt + arrastrar con el derecho desde el área principal de archivos, conservando la selección.
- Mantiene activa la vista clásica del shell independientemente de la preferencia de mostrar completos los nombres seleccionados.
- Aplica el tema de Nilesoft Shell al menú de modos de visualización.
- Evita retrasos prolongados al salir debidos a las cachés pasivas de iconos del shell, conservando las referencias de ciclo de vida adquiridas por los comandos del shell invocados explícitamente.
- Aísla el alojamiento de la vista del shell y amplía las pruebas de gestos Alt, reentrada de menús y ciclo de vida Shell/STA.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

## 1.0.7 — 2026-10-20

- Selecciona la carpeta secundaria de origen al abrir una carpeta superior, conservando la selección de las ventanas ya abiertas.
- Corrige el filtrado por nombre al enviar, cambiar o borrar una consulta.
- Restaura el icono nativo de Acceso rápido de Windows y centra los botones Acceso rápido y Configuración.
- Elimina el diagnóstico temporal y usa Benco en los reconocimientos del proyecto.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

## 1.0.6 — 2026-10-11

- Restaura automáticamente las ventanas de carpetas abiertas después de una actualización desde la aplicación, conservando las posiciones, tamaños y opciones de visualización guardados.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

## 1.0.5 — 2026-10-11

- Busca actualizaciones de forma asíncrona desde Acerca de, con un botón de instalación y un texto de estado adaptados.
- Descarga las actualizaciones en un cuadro de progreso independiente y cancelable, manteniendo utilizables las ventanas de carpetas.
- Verifica el tamaño y el SHA-256 del instalador antes de cerrar FileSpacer de forma ordenada e iniciar la instalación.
- Publica los metadatos de descarga del instalador junto con cada versión.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

## 1.0.4 — 2026-10-11

- Añade un comando al menú del título para cerrar las ventanas de las carpetas superiores.
- Vuelve a abrir carpetas cerradas desde el menú del título, las listas de accesos directos o Ctrl+Mayús+T, restaurando vista, selección y posición de desplazamiento.
- Añade a la lista de accesos directos de la barra de tareas las carpetas ancladas en Acceso rápido del Explorador.
- Muestra el comando para volver a abrir solo si la última carpeta cerrada no está ya abierta, con su nombre e icono nativo.
- Excluye la ventana actual de la lista de ventanas del menú del título.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

## 1.0.3 — 2026-10-11

- Restaura las comprobaciones manuales y las semanales opcionales de actualizaciones mediante las versiones GitHub de FileSpacer, desactivadas de forma predeterminada.

- Publica el manifiesto de actualización junto a cada instalador.

- Conserva la gestión nativa del shell de Alt + doble clic y Alt + Entrar en las propiedades de carpetas.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

## 1.0.2 — 2026-10-11

- Aplica los temas de Nilesoft Shell al menú de carpetas de la barra de título independientemente de la selección de archivos, manteniendo los comandos nativos del shell.
- Permite personalizaciones Nilesoft separadas para los menús de la barra de título y de la barra de herramientas.
- Centra la flecha del menú desplegable de la barra de título.
- Documenta la configuración de Nilesoft Shell, la aplicación de temas y el filtrado de menús.
- Incluye a Benco en los avisos de copyright.
- Permite al workflow de publicación reemplazar el instalador de una versión existente.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

## 1.0.1 — 2026-10-09

- Activa los controles nativos Minimizar y Maximizar y corrige los botones del título en ventanas maximizadas.
- Guarda y restaura por separado el estado maximizado y la geometría normal de la ventana, con migración automática de la base de datos.
- Usa las animaciones nativas de la barra de tareas y las propiedades de reinicio específicas de cada carpeta.
- Aplica las preferencias de agrupación de la barra de tareas a las ventanas nuevas y aclara este comportamiento en la configuración.
- Permite eliminar carpetas abiertas y borra su estado guardado.
- Impide que la aplicación y su asistente de recuperación bloqueen carpetas a través de su directorio de trabajo.
- Conserva las rutas absolutas de carpetas y el modo de pruebas de depuración durante la recuperación tras cierres inesperados.
- Restaura las etiquetas normales inmediatamente al quitar la selección cuando está activada la visualización del nombre completo.
- Libera las interfaces del shell almacenadas en caché al cerrar ventanas de carpetas y corrige la gestión de notificaciones del shell.
- Permite publicar versiones manualmente e incluye el número de versión en los nombres de los instaladores.

[Todos los cambios](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

## 1.0.0 — 2026-10-09

- Primera versión de FileSpacer para Windows 10 o posterior, x64, basada en ChromaFiler.
- Ofrece navegación espacial con una ventana por carpeta y estado persistente de ventanas, vistas, iconos y filtros guardado en SQLite.
- Sigue los cambios de ubicación y nombre de las carpetas y activa la ventana existente al volver a abrir una carpeta.
- Cierra de forma predeterminada la ventana de origen al abrir una carpeta o su carpeta superior, con comportamiento configurable y Ctrl para invertirlo.
- Añade una ruta de navegación, una barra de ruta editable y un filtro por nombre con filtrado opcional mientras se escribe.
- Ofrece elementos configurables de la barra de herramientas, una barra de estado inferior y preferencias de agrupación en la barra de tareas.
- Añade una opción experimental para mostrar completos los nombres de los elementos seleccionados.
- Ofrece interfaces inglesa y francesa, con el inglés como alternativa.
- Añade recuperación tras cierres inesperados y comandos de mantenimiento para restablecer preferencias o el estado guardado de las carpetas.
- Elimina la navegación en columnas Miller, el editor de texto integrado, las ventanas de vista previa y la bandeja flotante.
- Proporciona scripts de compilación MSVC/NMAKE, un instalador NSIS, publicaciones GitHub automatizadas y documentación del proyecto.

[Historial de la versión](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)
