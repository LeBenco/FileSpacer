# Повідомлення про сторонні компоненти

FileSpacer засновано на [ChromaFiler](https://github.com/vanjac/chromafiler), Copyright (c) 2023 J. van't Hoog, що поширюється за [GNU General Public License v3.0](../../LICENSE).

## SQLite

SQLite 3.53.4 статично скомпоновано з програмою. Він належить до суспільного надбання. Незмінений вихідний код і відомості про походження містяться в `third_party/sqlite`. Див. <https://sqlite.org/copyright.html>.

## Піктограми кнопок Вгору й Оновити (Lucide та Feather)

На панелі інструментів **стрілка вгору** відкриває **батьківську папку**, а **кругова стрілка** **оновлює вміст папки**. Обидві піктограми адаптовано з **Lucide** (`arrow-up` і `rotate-cw`). Стрілка вгору спочатку походить із **Feather**.

Повні повідомлення ISC та MIT наведено в [licenses/Lucide-LICENSE.txt](../../licenses/Lucide-LICENSE.txt) у вихідному коді або `Lucide-LICENSE.txt` у папці інсталяції. Див. <https://lucide.dev/license>.

## Шрифти й компоненти Windows

Деякі символи інтерфейсу використовують шрифт Microsoft Segoe MDL2 Assets, інстальований із Windows. FileSpacer не поширює цей шрифт повторно. Оболонка Windows, спільні елементи керування, GDI+ та інші системні компоненти постачаються з Windows.

## Інсталятор

Інсталятор NSIS використовує компонент LockedList, Copyright (c) 2013 Afrow Soft Ltd, за ліцензією zlib. Повне повідомлення міститься в [installer/plugins/LockedList.txt](../../installer/plugins/LockedList.txt) і включається до інстальованої програми як `LockedList-LICENSE.txt`. Див. <https://nsis.sourceforge.io/LockedList_plug-in>.

AppUserModelID ярлика встановлюється файлом FileSpacer `ShortcutAppID.nsh` через макроси Windows COM для NSIS. Колишній підмодуль `nsis-shortcut-properties` не використовується. Material Design Icons і колишня включена підмножина шрифту Segoe не використовуються.

## Піктограма замка Small & Flat

Позначка блокування стану вікна (`src/res/window-state-lock.ico`) — незмінена піктограма **lock** із Small & Flat автора paomedia, передана до суспільного надбання за [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

Оригінальна колекція й ліцензія: https://github.com/paomedia/small-n-flat

Поширення ICO: https://www.iconarchive.com/show/small-n-flat-icons-by-paomedia/lock-icon.html
