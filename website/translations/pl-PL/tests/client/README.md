<a id="running-tests-in-a-hyper-v-client"></a>

# Testy w kliencie Hyper-V

[Runner regresji](../regression/README.md) uruchamia wybrane przypadki w dedykowanym Windows, pozostawiając hosta do pracy.

```cmd
tests.cmd -VM AgentWin10
tests.cmd -VM AgentWin10 -Category Internal -Pattern "26-*"
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Bez `-VM` testy działają lokalnie. Nazwa wskazuje istniejącą skonfigurowaną maszynę; runner nigdy jej nie tworzy ani nie wraca do lokalnego UI.

<a id="prepare-the-client-once"></a>

## Jednorazowe przygotowanie klienta

Używaj dedykowanej VM i konta testowego z hasłem. Maszyna musi działać z zalogowaną i odblokowaną sesją graficzną. Zamknij VMConnect bez wylogowania i wyłączenia gościa. Testy działają z ograniczonym tokenem. Kontroler może ponownie przyłączyć sesję do konsoli, ale nie odblokować Windows.

W uzgodnionym czasie skonfiguruj klienta z katalogu głównego:

```cmd
prepare-client-hyperv.cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName"
```

Instalacja pyta o UAC i poświadczenia gościa. Zapisuje je zaszyfrowane przez Windows DPAPI poza repozytorium. Zwykłe uruchomienia używają konfiguracji bez pytań. Nie umieszczaj haseł w skryptach i raportach.

Aktualizacja chronionego kontrolera z zachowaniem poświadczeń:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName" -UpdateController
```

Nie instaluj go ponownie przy każdym teście. Niezgodność protokołu lub chronionych skryptów jawnie kończy się błędem i wymaga świadomej aktualizacji.

<a id="host-and-guest-responsibilities"></a>

## Odpowiedzialności hosta i gościa

Wybór poprzedza przygotowanie. Host kompiluje aplikację, testy natywne i pomocniki C#, pakując tylko wybrane przypadki i zależności. Gość sprawdza SHA256 programów/zestawów i argumenty przygotowania przed tymi samymi skryptami i asercjami. Brak lub zmiana pomocnika to błąd; brak kompilacji gościa. Visual Studio, MSVC i SDK nie są wymaganiami klienta.

Podwyższone zadanie hosta wykonuje tylko zainstalowane chronione skrypty kontrolera. `NoConsoleRunner.exe` uruchamia go bez konsoli i zapisuje stdout, stderr i kod. Scenariusze drzewa działają tylko w gościu, nie jako podwyższone skrypty hosta. Transport używa PowerShell Direct i interaktywnego pomocnika; wejście trafia tylko do pulpitu gościa.

Zainstaluj wymagania scenariusza w gościu. Everything na hoście nie wystarcza dla VM. Ekrany, DPI, dostawcy powłoki i aplikacje muszą odpowiadać wymaganiom przypadku.

<a id="reports-and-diagnosis"></a>

## Raporty i diagnostyka

Raporty są automatycznie zbierane w `build/regression/vm-<id>/`:

| Ścieżka | Zawartość |
| --- | --- |
| `output/regression/results.json`, `summary.json` | Wyniki przypadków i podsumowanie. |
| `output/regression/` | Dzienniki, wyjścia i dowody. |
| `output/result.json` | Tożsamość wdrożenia, sesja i przywrócenie. |
| `connection.json`, `smoke.json` | Wyniki transportu/sesji; smoke.json jest nazwą transportu. |
| `host-controller.stdout.log`, `host-controller.stderr.log` | Wyjście kontrolera hosta. |

`-RunDirectory` kopiuje całość do innej wolnej lokalizacji. Zachowaj wszystkie kody wyjścia: przypadku, runnera, zadania gościa i hosta. Całkowity limit uwzględnia liczbę przypadków i `-TimeoutSeconds`. Timeout, brak raportu, nieudane przywrócenie lub niezerowy kod pomocnika nie są sukcesem mimo częściowych PASS.

`test-agentwin10.cmd` diagnozuje transport przez prawdziwy folder Shell, zmianę rozmiaru Alt+prawy i zwykłe zamknięcie; nie uruchamia pełnego katalogu. `connect-agentwin10.cmd` służy do ręcznego połączenia z uwierzytelnianiem Windows, nie bezobsługowego wejścia.

Wybór sesji pulpitu można sprawdzić bez połączenia VM:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Test-DesktopSession.ps1
```

Testuje wybór kontrolera: wiele procesów Eksploratora w jednej sesji gościa oznacza jeden pulpit; brak lub wiele różnych sesji pozostają blokadą.

Źródła: [PowerShell Direct](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/powershell-direct),
[zabezpieczenia zadań interaktywnych](https://learn.microsoft.com/en-us/windows/win32/taskschd/security-contexts-for-running-tasks),
[eksport poświadczeń DPAPI](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/export-clixml?view=powershell-5.1).
