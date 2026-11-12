<p align="center">
    <img height="200" alt="FileSpacer" src="../../src/res/FileSpacer.svg" />
</p>

<a id="filespacer"></a>

# FileSpacer

[![Завантажити останню версію](https://img.shields.io/badge/%D0%97%D0%B0%D0%B2%D0%B0%D0%BD%D1%82%D0%B0%D0%B6%D0%B8%D1%82%D0%B8-%D0%BE%D1%81%D1%82%D0%B0%D0%BD%D0%BD%D1%8F%20%D0%B2%D0%B5%D1%80%D1%81%D1%96%D1%8F-blue)](https://github.com/LeBenco/FileSpacer/releases/latest) &emsp;
[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![Телеметрія: відсутня](https://img.shields.io/badge/%D1%82%D0%B5%D0%BB%D0%B5%D0%BC%D0%B5%D1%82%D1%80%D1%96%D1%8F-%D0%B2%D1%96%D0%B4%D1%81%D1%83%D1%82%D0%BD%D1%8F-2ea44f)](#privacy)
[![Ліцензія: GPL v3](https://img.shields.io/badge/%D0%9B%D1%96%D1%86%D0%B5%D0%BD%D0%B7%D1%96%D1%8F-GPLv3-blue.svg)](#license-and-credits)

FileSpacer — файловий менеджер для Windows на основі [просторової парадигми](https://en.wikipedia.org/wiki/Spatial_file_manager): кожна папка має одне вікно, що запам’ятовує положення, розмір і настройки подання. Це створює [сталі візуальні орієнтири](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), завдяки яким [просторова пам’ять](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) допомагає впізнавати знайомі папки з першого погляду. У навігаційному файловому менеджері те саме вікно послідовно показує різні папки: лише положення й розмір не визначають поточну папку, тому потрібно дивитися на вміст. Зберігаючи окремі орієнтири для кожної папки, просторова модель може зменшити когнітивне навантаження під час пошуку шляху серед файлів. Вкладки несумісні з просторовою моделлю й ніколи не будуть додані.

<img src="images/spatial-windows.png" alt="Три вікна папок без перекриття з різними розмірами, поданнями й масштабами піктограм, зокрема з вільним розташуванням." width="560">

FileSpacer надає перевагу легкості й простоті. Щоб обмежити накопичення вікон, відомий недолік просторової парадигми, він за замовчуванням закриває вихідне вікно під час відкриття папки. Утримуйте Ctrl, щоб залишити його відкритим для роботи з кількома папками. Інтерфейс навмисно стриманий — на відміну від перевантажених інтерфейсів, де між кнопками ледь видно файл.

<a id="system--languages"></a>**Система й мови:** Windows 10 або новіша, x64. Десять мов: англійська, французька, німецька, італійська, іспанська, португальська (Португалія), грецька, польська, нідерландська й українська.

<a id="privacy"></a>**Конфіденційність:** **Без телеметрії, відстеження використання чи реклами**. Перевірки оновлень звертаються до GitHub. Щотижневі перевірки необов’язкові й за замовчуванням вимкнені. Завантаження й інсталяція потребують підтвердження. [Політика конфіденційності](user-guide.md#privacy).

**Встановлення:**

- **Встановлення вручну:** завантажте й запустіть <a href="https://github.com/LeBenco/FileSpacer/releases/download/v1.4.2/FileSpacer-1.4.2-install.exe" id="last-release-url">інсталятор останньої версії</a>.
- **WinGet:** `winget install --id Benco.FileSpacer --exact`
- **Chocolatey** (термінал із правами адміністратора): `choco install filespacer -y`

Пакети WinGet очікують на схвалення в каталозі.

<a id="features"></a>

## Можливості

- **Одне вікно на папку**. Положення, розмір, режим подання та позиції піктограм [запам’ятовуються](user-guide.md#window-navigation). Піктограми можна розміщувати довільно або вирівнювати за [класичною сіткою](user-guide.md#icon-layout).
- **Розміщення вікон**. Під час відкриття іншої папки вихідне вікно за замовчуванням закривається. Вікна без збереженого положення розміщуються так, щоб [менше перекривати одне одного](user-guide.md#window-placement).
- **Керування вікнами**. Переміщення та змінення розміру вікон за допомогою [жестів миші](user-guide.md#moving-and-resizing). Збереження та повторне відкриття [груп папок](user-guide.md#folder-groups). [Блокування розташування вікон](user-guide.md#locking-a-saved-layout).

**Документація:**

[![Документація](https://img.shields.io/badge/Docs-Guide-blue)](user-guide.md)
[![Документація](https://img.shields.io/badge/Docs-Changelog-orange)](CHANGELOG.md)

<a id="integration"></a>

## Інтеграція

FileSpacer можна зробити файловим менеджером за замовчуванням під час інсталяції або пізніше в настройках. Файли відкриваються у звичних програмах Windows. Мережеві папки використовують звичайну обробку Windows. [Інтеграція з Windows](user-guide.md#windows-integration).

З **Everything** можна запускати розширений пошук із поля FileSpacer у поточній папці та вкладених папках або серед усіх індексованих файлів. Результати відкриваються в Everything. [Інтеграція й використання](user-guide.md#everything).

З **Nilesoft Shell** меню FileSpacer можуть використовувати теми й власні команди. Для узгодженого вигляду рекомендовано адаптувати конфігурацію. [Інтеграція й конфігурація](user-guide.md#nilesoft-shell).

<a id="build"></a>

## Збирання

[![Публікація FileSpacer](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

Збирання використовує MSVC/NMAKE, Visual Studio 2022 Build Tools з ATL і Windows SDK, Inkscape та ImageMagick. Для інсталятора потрібен NSIS. SQLite включено. Див. [посібник зі збирання](build.md).

Регресійні тести поєднують сценарії PowerShell із допоміжними програмами C++ і C# для внутрішніх перевірок і справжніх тестів інтерфейсу та оболонки Windows. [Робочий процес регресії GitHub Actions](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) запускає цей набір тестів програми на вимогу й автоматично перед випуском. Ті самі випадки можна запускати через `tests.cmd` на локальному робочому столі Windows або в налаштованій ВМ Hyper-V. [Посібник із тестування](tests/regression/README.md) описує вибір випадків, вимоги й результати.

<a id="license-and-credits"></a>

## Ліцензія й подяки

<a id="copyright-and-license"></a>

### Авторське право та ліцензія
FileSpacer поширюється за ліцензією <img align="absbottom" src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="Ліцензія: GPL v3">.<br>
Copyright © 2026 Benco за зміни FileSpacer. Повний текст ліцензії — у [LICENSE](../../LICENSE) у вихідному коді або `LICENSE.txt` у папці інсталяції. FileSpacer постачається без гарантій. Поширення й зміни дозволено на умовах цієї ліцензії.

<a id="fork-and-upstream-credits"></a>

### Відгалуження та подяки оригінальному проєкту
FileSpacer — відгалуження [ChromaFiler](https://github.com/vanjac/chromafiler) (оригінальний код ChromaFiler © 2023 J. van't Hoog), створеного J. van't Hoog (vanjac). Дякуємо vanjac за основу!

<a id="third-party-notices"></a>

### Повідомлення про сторонні компоненти
Див. [повідомлення про сторонні компоненти](THIRD_PARTY_NOTICES.md) і [політику конфіденційності](privacy-policy.md).
