# Third-party notices

FileSpacer is based on [ChromaFiler](https://github.com/vanjac/chromafiler),
Copyright (c) 2023 J. van't Hoog, distributed under the GNU General Public License
v3.0. FileSpacer modifications: Copyright (c) 2026 LeBenco.
The complete license is in [LICENSE](LICENSE) in the source tree, or `LICENSE.txt`
in the installation folder. FileSpacer comes without warranty;
redistribution and modification are permitted under that license.

## SQLite

SQLite 3.53.4 is statically linked into the application. It is in the public domain.
The unmodified sources and their provenance are in `third_party/sqlite`.
See <https://sqlite.org/copyright.html>.

## Lucide and Feather

The Up and Refresh buttons use paths adapted from Lucide's `arrow-up` and
`rotate-cw` icons, drawn with Windows GDI+. The arrow is also covered by the
Feather MIT notice included in Lucide's complete license. Geometry, stroke width,
and refresh orientation have been adapted for FileSpacer.

See [licenses/Lucide-LICENSE.txt](licenses/Lucide-LICENSE.txt) for the complete
ISC and MIT notices in the source tree, or `Lucide-LICENSE.txt` in the installation
folder. See <https://lucide.dev/license>.

## Windows fonts and components

Some interface glyphs use Microsoft's Segoe MDL2 Assets font installed with
Windows. FileSpacer does not redistribute that font. Windows Shell, common
controls, GDI+, and other system components are supplied by Windows.

## Installer

The NSIS installer uses the LockedList plug-in, Copyright (c) 2013 Afrow Soft Ltd,
under the zlib license. The complete notice is in
[installer/plugins/LockedList.txt](installer/plugins/LockedList.txt) and is
included as `LockedList-LICENSE.txt` with the installed program. See
<https://nsis.sourceforge.io/LockedList_plug-in>.

The shortcut AppUserModelID is set by FileSpacer's `ShortcutAppID.nsh` using NSIS
Windows COM macros. The former `nsis-shortcut-properties` submodule is not used.
Material Design Icons and the former bundled Segoe font subset are not used.
