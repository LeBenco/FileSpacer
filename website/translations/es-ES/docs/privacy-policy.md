# Política de privacidad

FileSpacer no incluye telemetría, análisis de uso ni publicidad. No envía automáticamente al desarrollador información de uso ni informes de errores.

## Comprobaciones de actualizaciones

Las comprobaciones manuales y las semanales automáticas, si están activadas, solicitan a GitHub el manifiesto de la última versión de FileSpacer mediante HTTPS. Las comprobaciones automáticas están desactivadas de forma predeterminada. GitHub y su infraestructura de descarga reciben la información habitual de conexión, incluida tu dirección IP y un agente de usuario con la versión de FileSpacer. La solicitud no contiene rutas de archivos, estado de carpetas ni informes de diagnóstico.

Cuando hay una actualización instalable, FileSpacer pide confirmación antes de descargar el instalador desde GitHub mediante HTTPS. Comprueba el tamaño y el SHA-256 de la descarga con el manifiesto antes de iniciar la instalación. El instalador requiere tu intervención; las actualizaciones nunca se instalan automáticamente. Las versiones sin metadatos del instalador abren su página en el navegador predeterminado.

## Datos locales

Las preferencias globales se guardan en el Registro de Windows del usuario actual, en `HKEY_CURRENT_USER\Software\FileSpacer`. Las posiciones y tamaños de las ventanas de carpetas, las opciones de visualización, las posiciones de iconos, los bloqueos de estado, las búsquedas recientes y el historial de carpetas cerradas se guardan en una base de datos SQLite en `%LOCALAPPDATA%\FileSpacer\folder-state.sqlite3`. SQLite puede crear archivos de diario asociados en el mismo directorio. El estado guardado puede incluir identidades y rutas de carpetas, también rutas de red.

Una actualización guarda temporalmente el instalador, una copia del programa que inicia la actualización y una lista SQLite de las rutas de carpetas abiertas en `%TEMP%\FileSpacerUpdate-{GUID}`. Esta lista local se usa para volver a abrir las carpetas después de instalar o cancelar; no se envía.

La protección frente a cierres inesperados puede guardar informes de diagnóstico y minivolcados de memoria en `%LOCALAPPDATA%\FileSpacer`. Permanecen en el equipo salvo que el usuario decida compartirlos. El diagnóstico puede contener rutas de archivos, detalles de procesos y partes de su memoria. La misma carpeta contiene un archivo de bloqueo de la configuración.

Las compilaciones de depuración pueden usar preferencias de prueba y datos locales separados con `/test`.

## Windows y aplicaciones externas

Abrir carpetas de red, resolver elementos del shell, mostrar vistas previas o miniaturas de Windows y usar extensiones del menú contextual recurre a Windows y a los componentes del shell instalados. Esos componentes y los servidores remotos pueden acceder a la red según su funcionamiento y sus propias políticas.

Los archivos se abren con la aplicación asociada o con el selector de aplicaciones de Windows. Su tratamiento por esas aplicaciones queda fuera del control de FileSpacer.

Si la integración con Everything está activada, elegir su acción de búsqueda transmite la consulta visible a Everything, instalado por separado. Las búsquedas limitadas a una carpeta también transmiten la ruta actual. El filtro local no inicia Everything; el índice y la configuración de la aplicación externa determinan los resultados.

## Restablecimiento de preferencias

`FileSpacer.exe /reset-options` restablece las preferencias globales. `FileSpacer.exe /reset-all` borra también el estado guardado de las carpetas. Ninguno de los dos comandos elimina los archivos del usuario ni cambia las asociaciones de archivos de Windows. No eliminan los informes de diagnóstico guardados anteriormente.
