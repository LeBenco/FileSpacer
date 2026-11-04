<a id="website-translations"></a>

# Переклади сайту

Основні README й документація залишаються англійськими джерелами. Повні Markdown-варіанти забезпечують сайт і локалізовані GitHub-посилання; не додають мовного перемикача чи посилань до README репозиторію.

Кожен каталог мова-регіон містить `locale.json`, `README.md`, `docs/user-guide.md`, `CHANGELOG.md`, `THIRD_PARTY_NOTICES.md`. Конфіденційність/обслуговування даних мають окремі розділи; `docs/privacy-policy.md` — коротка політика для GitHub. `docs/nilesoft-shell.md` — повний перекладений довідник із посібника, окремо на GitHub. Відсутність перериває генерацію замість тихого переходу на англійську. Генератор виявляє каталоги; англійська використовує оригінали й `website/locale.json`.

Під час оновлення зберігайте порядок/ієрархію заголовків. `source-state.json` записує англійські хеші й ID останньої синхронізації. Англійські зміни публікуються відразу. Переклади зберігають текст та ID; застарілість дає попередження й таблицю, а не блокування. Виявляються й суто текстові зміни.

Після перекладу змін запишіть англійське джерело в CMD у корені (замініть локаль):

```cmd
node website\generate.mjs --record-translation-sources fr-FR
```

Без локалі записуються всі. Комітьте стан разом із Markdown. Реєстрація перевіряє заголовки/посилання, не перекладає й не оцінює актуальність змісту. Не записуйте англійську зміну до завершення перекладу. Відсутні документи, зламані посилання/медіа й невідповідність структури лишаються помилками.

Записані англійські ID зберігають посилання після зміни перекладених заголовків. Посилання можуть мати перекладений або англійський slug. Користувацька документація веде до вибраної мови GitHub. Польська, нідерландська й українська також мають технічні документи поза сайтом; посилання ведуть до них. Інші мови зберігають англійські технічні джерела. Зберігайте явні HTML ID.

Зберігайте англійські імена зображень у посиланнях. Генератор підставляє справжні локальні знімки, як `spatial-windows-de.png`; нейтральний логотип спільний. Відсутній знімок/анімація — помилка. Не перекладайте текст усередині експортованих зображень.

Кожна мова збирається окремо з тією самою закріпленою Just the Docs, власними навігацією, індексом, написами й медіа. Процес визначає матрицю з `locales.json`.

<a id="terminology-and-style"></a>

## Термінологія й стиль

Для системних понять використовуйте терміни локальної документації Windows. Точні шляхи настройок зберігають справжні написи FileSpacer. Для інших понять — усталена програмна лексика й природні формулювання. Зберігайте сенс, приклади, посилання й стриманий гумор замість дослівності. Ліцензії, команди, шляхи реєстру, ID й імена програм незмінні.

Надавайте перевагу коротким описам поведінки й нюансів замість інструкцій очевидних кнопок. Французька використовує нейтральні описи/інфінітиви без постійного звертання. Це редакційний вибір, не універсальна норма: Microsoft/GNOME звертаються прямо, Everything має короткі англійські команди, німецька часто — ввічливе звертання. Дотримуйтеся ідіому мови, не французького синтаксису. Знімки — після опису з описовим alt-текстом без зайвого підпису.

Знімок Everything використовує бразильську португальську поруч із FileSpacer та європейськими вигаданими іменами. Everything не має європейської португальської в офіційному [списку мов](https://www.voidtools.com/support/everything/supported_languages/).

| Мова | Файловий провідник | Папка | Панель завдань | Сполучення клавіш |
| --- | --- | --- | --- | --- |
| Français | Explorateur de fichiers | dossier | barre des tâches | raccourcis clavier |
| Deutsch | Datei-Explorer | Ordner | Taskleiste | Tastenkombinationen |
| Italiano | Esplora file | cartella | barra delle applicazioni | scelte rapide da tastiera |
| Español | Explorador de archivos | carpeta | barra de tareas | métodos abreviados de teclado |
| Português (Portugal) | Explorador de Ficheiros | pasta | barra de tarefas | atalhos de teclado |
| Ελληνικά | Εξερεύνηση αρχείων | φάκελος | γραμμή εργασιών | συντομεύσεις πληκτρολογίου |
| Polski | Eksplorator plików | folder | pasek zadań | skróty klawiaturowe |
| Nederlands | Bestandenverkenner | map | taakbalk | sneltoetsen |
| Українська | Файловий провідник | папка | панель завдань | сполучення клавіш |

Джерела стилю: [Файловий провідник Microsoft](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [Everything](https://www.voidtools.com/support/everything/searching/), GNOME Files ([FR](https://help.gnome.org/gnome-help/files-browse.html.fr), [DE](https://help.gnome.org/gnome-help/files-browse.html.de)). Відповідно: велике комерційне ПЗ, мала утиліта й відкрита документація.

Джерела термінології:

- Французька: [Файловий провідник](https://support.microsoft.com/fr-fr/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/fr-fr/accessibility/windows/keyboard-shortcuts-in-windows).
- Німецька: [Файловий провідник](https://support.microsoft.com/de-de/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/de-de/accessibility/windows/keyboard-shortcuts-in-windows).
- Італійська: [Файловий провідник](https://support.microsoft.com/it-it/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/it-it/accessibility/windows/keyboard-shortcuts-in-windows).
- Іспанська: [Файловий провідник](https://support.microsoft.com/es-es/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/es-es/accessibility/windows/keyboard-shortcuts-in-windows).
- Португальська (Португалія): [Файловий провідник](https://support.microsoft.com/pt-pt/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/pt-pt/accessibility/windows/keyboard-shortcuts-in-windows).
- Грецька: [Файловий провідник](https://support.microsoft.com/el-gr/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/el-gr/accessibility/windows/keyboard-shortcuts-in-windows).
- Польська: [Файловий провідник](https://support.microsoft.com/pl-pl/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/pl-pl/accessibility/windows/keyboard-shortcuts-in-windows).
- Нідерландська: [Файловий провідник](https://support.microsoft.com/nl-nl/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/nl-nl/accessibility/windows/keyboard-shortcuts-in-windows).
- Українська: [Файловий провідник](https://support.microsoft.com/uk-ua/windows/experience/fileexplorer/file-explorer-in-windows), [сполучення клавіш](https://support.microsoft.com/uk-ua/accessibility/windows/keyboard-shortcuts-in-windows).
- [Посібники зі стилю локалізації Microsoft](https://learn.microsoft.com/en-us/globalization/reference/microsoft-style-guides).
- [Обробники піктограм Windows португальською](https://learn.microsoft.com/pt-pt/windows/win32/shell/how-to-create-icon-handlers).
- [CC0 public-domain terminology in Greek](https://creativecommons.org/publicdomain/zero/1.0/deed.el).
