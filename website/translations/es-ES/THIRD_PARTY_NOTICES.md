# Avisos sobre componentes de terceros

FileSpacer está basado en [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, distribuido bajo la GNU General Public License v3.0. Modificaciones de FileSpacer: Copyright (c) 2026 Benco. La licencia completa se encuentra en [LICENSE](LICENSE) en el código fuente, o en `LICENSE.txt` en la carpeta de instalación. FileSpacer se proporciona sin garantía; su redistribución y modificación están permitidas según las condiciones de esa licencia.

## SQLite

SQLite 3.53.4 está enlazado estáticamente con la aplicación y es de dominio público. El código fuente sin modificar y su procedencia se encuentran en `third_party/sqlite`. Consulta <https://sqlite.org/copyright.html>.

## Iconos de los botones Subir y Actualizar (Lucide y Feather)

En la barra de herramientas, la **flecha hacia arriba** abre la **carpeta superior** y la **flecha circular** **actualiza el contenido de la carpeta**. Estos dos iconos están adaptados de **Lucide** (`arrow-up` y `rotate-cw`). El icono de la flecha hacia arriba procede originalmente de **Feather**.

Consulta [licenses/Lucide-LICENSE.txt](licenses/Lucide-LICENSE.txt) para los avisos ISC y MIT completos del código fuente, o `Lucide-LICENSE.txt` en la carpeta de instalación. Consulta <https://lucide.dev/license>.

## Fuentes y componentes de Windows

Algunos iconos de la interfaz usan la fuente Microsoft Segoe MDL2 Assets instalada con Windows. FileSpacer no redistribuye esta fuente. Windows Shell, los controles comunes, GDI+ y los demás componentes del sistema los proporciona Windows.

## Instalador

El instalador NSIS usa el complemento LockedList, Copyright (c) 2013 Afrow Soft Ltd, bajo la licencia zlib. El aviso completo está en [installer/plugins/LockedList.txt](installer/plugins/LockedList.txt) y se incluye como `LockedList-LICENSE.txt` en la carpeta de instalación. Consulta <https://nsis.sourceforge.io/LockedList_plug-in>.

El AppUserModelID de los accesos directos lo establece `ShortcutAppID.nsh` de FileSpacer usando las macros Windows COM de NSIS. No se usa el antiguo submódulo `nsis-shortcut-properties`. Tampoco se usan Material Design Icons ni el subconjunto de la fuente Segoe incluido anteriormente.

## Icono de candado Small & Flat

El distintivo de bloqueo del estado de la ventana (`src/res/window-state-lock.ico`) es el icono **lock** sin modificar de la colección Small & Flat de paomedia, dedicado al dominio público bajo [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

Colección original y licencia: https://github.com/paomedia/small-n-flat

Distribución ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
