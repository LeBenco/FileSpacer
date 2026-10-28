<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

FileSpacer es un gestor de archivos para Windows basado en el [paradigma espacial](https://en.wikipedia.org/wiki/Spatial_file_manager): cada carpeta tiene una única ventana que recuerda su posición, tamaño y opciones de visualización. Esto crea [referencias visuales estables](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf) y permite que la [memoria espacial](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) te ayude a reconocer las carpetas habituales de un vistazo. En un gestor de archivos basado en la navegación, una misma ventana muestra distintas carpetas sucesivamente: su posición y sus dimensiones no bastan para identificar la carpeta abierta, y tienes que mirar su contenido para saber dónde estás. Al conservar referencias distintas para cada carpeta, el enfoque espacial puede reducir el esfuerzo necesario para orientarte entre tus archivos.

Para limitar la acumulación de ventanas, un inconveniente conocido del paradigma espacial, FileSpacer cierra de forma predeterminada la ventana de origen al abrir una carpeta. Mantén pulsada Ctrl al abrirla para conservar la ventana de origen cuando quieras trabajar con varias carpetas a la vez.

FileSpacer mantiene una interfaz deliberadamente sobria, a diferencia de ciertas interfaces recargadas que dejan justo el espacio entre botones para entrever un archivo.

Las pestañas son incompatibles con el modelo espacial y no se añadirán nunca.

Windows 10 o posterior, x64. Disponible en inglés, francés, alemán, italiano, español, portugués (Portugal) y griego.

[Descargar la última versión](https://github.com/LeBenco/FileSpacer/releases/latest)
· [Guía de uso](docs/user-guide.md)
· [Historial de versiones](CHANGELOG.md)

![Una ventana de FileSpacer con archivos y carpetas de ejemplo distribuidos libremente.](docs/images/overview.png)

*Cada carpeta conserva su ventana y la disposición de sus iconos.*

## Funciones

- **Coloca los iconos libremente**, con las posiciones guardadas y restauradas para cada carpeta. [Disposición de iconos](docs/user-guide.md#icon-layout).
- **Mueve y cambia el tamaño de las ventanas** desde cualquier punto del área de archivos sin perder la selección. Los gestos del ratón son configurables; por defecto, Alt + arrastrar con el botón izquierdo y Alt + arrastrar con el botón derecho. [Gestos de ventana](docs/user-guide.md#moving-and-resizing).
- **Cambia entre carpetas abiertas** desde el menú de la barra de título, cierra las ventanas de las carpetas superiores o vuelve a abrir la última ventana cerrada con Ctrl+Mayús+T. [Navegación entre ventanas](docs/user-guide.md#window-navigation).
- **Bloquea el estado guardado de una ventana**, manteniendo la posibilidad de cambiar temporalmente su posición, tamaño y visualización. [Bloqueo del estado de las ventanas](docs/user-guide.md#locking-a-saved-layout).
- **Filtra la carpeta actual** y reutiliza sus búsquedas recientes. [Búsqueda e historial](docs/user-guide.md#search-and-history).
- **Conserva la selección al activar una ventana** haciendo clic en su fondo. [Selección y activación](docs/user-guide.md#selection-and-activation).
- **Reduce la superposición de ventanas** al abrir carpetas sin una posición guardada. [Colocación de ventanas](docs/user-guide.md#window-placement).

## Integración

**Nilesoft Shell** permite cambiar el aspecto de los menús de FileSpacer y personalizar sus comandos, con reglas para cada menú. [Instalación y configuración](docs/user-guide.md#nilesoft-shell).

**Everything** puede buscar en la carpeta actual y sus subcarpetas, o en todas partes, usando la consulta introducida en FileSpacer. Los resultados se abren en Everything, que debe instalarse por separado. [Instalación y uso](docs/user-guide.md#everything).

También puedes establecer FileSpacer como aplicación predeterminada para abrir carpetas. [Integración con Windows](docs/user-guide.md#windows-integration).

## Compilación

La compilación usa MSVC/NMAKE, Visual Studio 2022 Build Tools con ATL y un Windows SDK, Inkscape e ImageMagick. El instalador requiere NSIS; SQLite está incluido. Consulta la [guía de compilación](docs/build.md).

Las pruebas de regresión combinan escenarios PowerShell con herramientas C++ y C# para las comprobaciones internas y las pruebas reales de la interfaz de Windows y del shell. Ejecútalas mediante `tests.cmd`, en local o en una VM Hyper-V configurada. Consulta la [guía de pruebas](tests/regression/README.md).

---

FileSpacer es una bifurcación de [ChromaFiler](https://github.com/vanjac/chromafiler), creado por J. van’t Hoog. ¡Muchas gracias a vanjac por esta base!

Distribuido bajo la licencia [GNU GPL v3.0](LICENSE). Copyright © 2026 Benco por las modificaciones de FileSpacer; código original de ChromaFiler © 2023 J. van’t Hoog. Consulta los [avisos sobre componentes de terceros](THIRD_PARTY_NOTICES.md) y la [política de privacidad](docs/privacy-policy.md).
