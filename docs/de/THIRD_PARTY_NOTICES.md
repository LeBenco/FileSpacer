<a id="third-party-notices"></a>

# Drittanbieterhinweise

FileSpacer basiert auf [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van’t Hoog, veröffentlicht unter der [GNU General Public License v3.0](../../LICENSE).

<a id="installer"></a>

## NSIS

Das NSIS-Installationsprogramm verwendet das LockedList-Plug-in, Copyright (c) 2013 Afrow Soft Ltd, unter der zlib-Lizenz. Der vollständige Hinweis steht in [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) und liegt als `LockedList-LICENSE.txt` im Installationsordner bei. Siehe <https://nsis.sourceforge.io/LockedList_plug-in>.

Die AppUserModelID von Verknüpfungen wird durch FileSpacers `ShortcutAppID.nsh` mithilfe der Windows-COM-Makros von NSIS festgelegt. Das frühere Untermodul `nsis-shortcut-properties` wird nicht verwendet. Material Design Icons und der früher enthaltene Segoe-Schriftartauszug werden ebenfalls nicht verwendet.

<a id="sqlite"></a>

## SQLite

SQLite 3.53.4 wird statisch in die Anwendung eingebunden. Es ist gemeinfrei. Die unveränderten Quellen und ihre Herkunft sind in `third_party/sqlite` enthalten. Siehe <https://sqlite.org/copyright.html>.

<a id="icons"></a>

## Symbole

<div id="up-and-refresh-button-icons-lucide-and-feather"></div>

In der Symbolleiste öffnet der **Pfeil nach oben** den **übergeordneten Ordner**. Der **kreisförmige Pfeil** **aktualisiert den Ordnerinhalt**. Beide Symbole sind angepasste Versionen der **Lucide**-Symbole `arrow-up` und `rotate-cw`. Das Symbol mit dem Pfeil nach oben stammt ursprünglich aus **Feather**.

Die vollständigen ISC- und MIT-Hinweise stehen in [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) im Quelltext oder `Lucide-LICENSE.txt` im Installationsordner. Siehe <https://lucide.dev/license>.

<div id="small--flat-lock-icon"></div>

Die Symbole für das geschlossene und das offene Vorhängeschloss (`src/res/window-state-lock.ico` und `src/res/window-state-unlock.ico`) basieren auf dem **lock**-Symbol aus Small & Flat von paomedia, das unter [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) als gemeinfrei veröffentlicht wurde. Das offene Vorhängeschloss ist eine abgewandelte Version des Originals.

Ursprüngliche Sammlung und Lizenz: https://github.com/paomedia/small-n-flat

ICO-Datei: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
