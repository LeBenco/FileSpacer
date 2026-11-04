<a id="running-tests-in-a-hyper-v-client"></a>

# Тести в клієнті Hyper-V

[Засіб регресії](../regression/README.md) запускає вибрані тести у виділеному Windows-клієнті, залишаючи хост доступним для роботи.

```cmd
tests.cmd -VM AgentWin10
tests.cmd -VM AgentWin10 -Category Internal -Pattern "26-*"
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Без `-VM` тести локальні. Ім’я вказує на наявну налаштовану ВМ; засіб не створює її й не переходить до локального UI.

<a id="prepare-the-client-once"></a>

## Одноразова підготовка клієнта

Використовуйте виділену ВМ і тестовий обліковий запис із паролем. ВМ працює з відкритим і розблокованим графічним сеансом. Закрийте VMConnect без виходу із сеансу чи вимкнення. Тести працюють з обмеженим токеном. Контролер може приєднати від’єднаний сеанс до консолі, але не розблокувати Windows.

В узгоджений час налаштуйте клієнт із кореня:

```cmd
prepare-client-hyperv.cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName"
```

Налаштування запитує UAC й облікові дані гостя та зберігає їх зашифрованими Windows DPAPI поза репозиторієм. Звичайні запуски використовують конфігурацію без запитів. Не записуйте паролі до сценаріїв чи звітів.

Оновлення захищеного контролера зі збереженням облікових даних:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName" -UpdateController
```

Не перевстановлюйте для кожного тесту. Невідповідність протоколу/захищених сценаріїв явно завершується помилкою й потребує свідомого оновлення.

<a id="host-and-guest-responsibilities"></a>

## Обов’язки хоста й гостя

Вибір передує підготовці. Хост збирає програму, нативні тести й C#-засоби, пакуючи лише вибрані тести/залежності. Гість перевіряє SHA256 програм/збірок та аргументи підготовки перед тими самими сценаріями/твердженнями. Відсутній чи змінений засіб — помилка; збирання в гості немає. Visual Studio, MSVC й SDK клієнту не потрібні.

Підвищене завдання хоста виконує лише інстальовані захищені сценарії контролера. `NoConsoleRunner.exe` запускає без консолі й записує stdout, stderr та код. Сценарії дерева виконуються лише в гості, не як підвищені сценарії хоста. Транспорт — PowerShell Direct та інтерактивний робочий процес; введення лише на стіл гостя.

Інсталюйте передумови сценарію в гості. Everything на хості не задовольняє тест у ВМ. Дисплеї, DPI, постачальники й зовнішні програми мають відповідати тесту.

<a id="reports-and-diagnosis"></a>

## Звіти й діагностика

Звіти автоматично збираються в `build/regression/vm-<id>/`:

| Шлях | Вміст |
| --- | --- |
| `output/regression/results.json`, `summary.json` | Результати й підсумок. |
| `output/regression/` | Журнали, виходи й докази. |
| `output/result.json` | Ідентичність розгортання, сеанс і відновлення. |
| `connection.json`, `smoke.json` | Результати транспорту/сеансу; smoke.json — назва транспорту. |
| `host-controller.stdout.log`, `host-controller.stderr.log` | Вивід контролера хоста. |

`-RunDirectory` копіює все до іншого вільного місця. Зберігайте всі коди: тесту, засобу, завдань гостя й хоста. Загальний строк враховує кількість тестів і `-TimeoutSeconds`. Тайм-аут, відсутній звіт, невдале відновлення чи ненульовий код не є успіхом попри окремі PASS.

`test-agentwin10.cmd` перевіряє транспорт справжньою папкою Shell, зміною розміру Alt+права й звичайним закриттям, не весь набір. `connect-agentwin10.cmd` — для ручного з’єднання з автентифікацією Windows, не автоматичного введення.

Вибір сеансу можна перевірити без з’єднання ВМ:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Test-DesktopSession.ps1
```

Перевіряє вибір контролера: кілька процесів Провідника в одному сеансі гостя — один стіл; нуль або кілька різних сеансів блокують запуск.

Джерела: [PowerShell Direct](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/powershell-direct),
[безпека інтерактивних завдань](https://learn.microsoft.com/en-us/windows/win32/taskschd/security-contexts-for-running-tasks),
[експорт облікових даних DPAPI](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/export-clixml?view=powershell-5.1).
