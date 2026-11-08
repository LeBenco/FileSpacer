<a id="third-party-notices"></a>

# Повідомлення про сторонні компоненти

FileSpacer засновано на [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van't Hoog, що поширюється за [GNU General Public License v3.0](../../LICENSE).

<a id="installer"></a>

## NSIS

Інсталятор NSIS використовує компонент LockedList, Copyright (c) 2013 Afrow Soft Ltd, за ліцензією zlib. Повне повідомлення міститься в [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) і включається до інстальованої програми як `LockedList-LICENSE.txt`. Див. <https://nsis.sourceforge.io/LockedList_plug-in>.

AppUserModelID ярлика встановлюється файлом FileSpacer `ShortcutAppID.nsh` через макроси Windows COM для NSIS. Колишній підмодуль `nsis-shortcut-properties` не використовується. Material Design Icons і колишня включена підмножина шрифту Segoe не використовуються.

<a id="sqlite"></a>

## SQLite

SQLite 3.53.4 статично скомпоновано з програмою. Він належить до суспільного надбання. Незмінений вихідний код і відомості про походження містяться в `third_party/sqlite`. Див. <https://sqlite.org/copyright.html>.

<a id="icons"></a>

## Піктограми

<div id="up-and-refresh-button-icons-lucide-and-feather"></div>

На панелі інструментів **стрілка вгору** відкриває **батьківську папку**, а **кругова стрілка** **оновлює вміст папки**. Обидві піктограми адаптовано з **Lucide** (`arrow-up` і `rotate-cw`). Стрілка вгору спочатку походить із **Feather**.

Повні повідомлення ISC та MIT наведено в [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) у вихідному коді або `Lucide-LICENSE.txt` у папці інсталяції. Див. <https://lucide.dev/license>.

<div id="small--flat-lock-icon"></div>

Піктограми закритого й відкритого замка (`src/res/window-state-lock.ico` і `src/res/window-state-unlock.ico`) походять від піктограми **lock** із колекції Small & Flat автора paomedia, переданої до суспільного надбання за [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/). Піктограма відкритого замка — похідна версія оригіналу.

Оригінальна колекція й ліцензія: https://github.com/paomedia/small-n-flat

Поширення ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
