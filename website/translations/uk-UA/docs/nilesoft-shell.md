<a id="nilesoft-shell-configuration-reference"></a>

# Довідник конфігурації Nilesoft Shell

Огляд — у [посібнику користувача Nilesoft Shell](user-guide.md#nilesoft-shell).

[Nilesoft Shell](https://nilesoft.org/) дає змогу застосовувати теми й налаштування меню до FileSpacer через `shell.nss` та імпортовані файли конфігурації. Для цієї необов’язкової інтеграції рекомендовано адаптувати конфігурацію до вигляду FileSpacer. Приклади охоплюють додавання, змінення й видалення пунктів, умови для FileSpacer і рекомендовані прямі кути.

<a id="scope-system-menu-rules"></a>

## Обмеження правил системних меню

Для правил Провідника Windows, робочого стола або панелі завдань додайте `process.is_explorer &&` до наявної умови `where`. Щоб зберегти дію в інших програмах, але виключити FileSpacer, використовуйте `process.name != 'FileSpacer' &&`. Застосуйте фільтр до загальних правил `menu`, `item`, `modify` і `remove`, зокрема в імпортованих файлах.

<a id="apply-themes-to-filespacer"></a>

## Застосування тем до FileSpacer

Дозвольте FileSpacer у `settings.exclude.where` і збережіть імпорт теми. Для звичайної умови виключення:

```nss
settings {
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
import 'imports/theme.nss'
```

Властивості теми можуть використовувати вирази для окремих процесів: наприклад, `dark = if(process.name == 'FileSpacer', true, sys.dark)` у `theme { ... }` вибирає темний режим для FileSpacer і системну настройку для інших програм.

Коли використовується FileSpacer замість Файлового провідника, зміни теми діють для нових меню після скасування реєстрації й повторної реєстрації Nilesoft Shell без перезапуску Провідника.

<a id="target-a-specific-menu"></a>

## Вибір конкретного меню

Оголосіть ці змінні перед імпортами, що їх використовують:

```nss
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
$filespacer_context = (filespacer && !filespacer_title && !filespacer_toolbar)
```

Використовуйте відповідні властивості `where` і `type` у правилах:

| Меню | `where` | `type` |
| --- | --- | --- |
| Розкривне меню заголовка | `filespacer_title` | `'*'` |
| Панель інструментів **…** | `filespacer_toolbar` | `'*'` |
| Вибрані файли або папки | `filespacer_context` | `'file\|dir'` |
| Тло папки | `filespacer_context` | `'back'` |

Використовуйте `mode='multiple'` для правил, доступних незалежно від вибору. `process.name` не містить `.exe`; `window.parent.name` визначає клас вікна.

<a id="target-an-existing-menu-entry"></a>

## Вибір наявного пункту меню

Загальний механізм Nilesoft `modify(find=...)` знаходить пункт за написом. Поєднуйте його з `where`, щоб обмежити зміну певним меню; `|` розділяє альтернативні написи, а `*` є символом узагальнення:

```nss
modify(type='*' mode='multiple' where=filespacer_title
       find='Copy path|Copier le chemin|Копіювати шлях' title='Копіювати шлях папки')
```

Див. [довідник конфігурації](https://nilesoft.org/docs/configuration/) Nilesoft.

<a id="recommended-menu-appearance"></a>

## Рекомендований вигляд меню

Для меню заголовка й панелі інструментів рекомендовано прямі кути, що відповідають кнопкам FileSpacer. Використовуючи наведені змінні, додайте цю настройку до наявного блоку `theme { ... }`:

```nss
border.radius = if(
    process.name == 'FileSpacer' &&
    (
        window.parent.name == 'FileSpacer Item Window' ||
        window.parent.name == 'FileSpacer Toolbar Host'
    ),
    0,
    default
)
```

Якщо вже використовується власний радіус, замініть `default` його попереднім значенням.

<a id="use-filespacer-actions"></a>

## Використання дій FileSpacer

Використовуйте [`window.command`](https://nilesoft.org/docs/functions/window) з одним із наведених ID для додавання дії FileSpacer, навіть якщо її немає у власному складі меню. Обмежте правило меню заголовка умовою `filespacer_title`. Звичайні вимоги дії зберігаються: наприклад, відновлення заблокованого розташування потребує наявного блокування.

```nss
item(type='*' mode='multiple' where=filespacer_title
     title='Відновити заблоковане розташування' cmd=window.command(1123))
```

| Дія | ID |
| --- | ---: |
| Відновити заблоковане розташування | 1123 |
| Замінити еталонне розташування | 1124 |
| Забути розташування цієї папки | 1125 |
| Закрити вікна вкладених папок | 1126 |
| Закрити інші вікна | 1127 |
| Припасувати вікно до вмісту | 1128 |
| Зберегти / відкрити групу папок | 1129 |
