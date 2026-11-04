<a id="running-tests-in-a-hyper-v-client"></a>

# Tests uitvoeren in een Hyper-V-client

De [regressielauncher](../regression/README.md) voert gekozen tests uit in een aparte Windows-client terwijl de ontwikkelhost bruikbaar blijft.

```cmd
tests.cmd -VM AgentWin10
tests.cmd -VM AgentWin10 -Category Internal -Pattern "26-*"
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Zonder `-VM` draaien tests lokaal. Een VM-naam verwijst naar een bestaande configuratie; de launcher maakt geen VM en valt niet terug op lokale UI.

<a id="prepare-the-client-once"></a>

## Client eenmalig voorbereiden

Gebruik een aparte VM en testaccount met wachtwoord. De VM moet draaien met aangemelde, ontgrendelde grafische sessie. Sluit VMConnect zonder afmelden of afsluiten. Tests gebruiken een beperkt token. De controller kan een ontkoppelde sessie aan de gastconsole koppelen, maar Windows niet ontgrendelen.

Configureer een nieuwe client op een afgesproken moment vanuit de hoofdmap:

```cmd
prepare-client-hyperv.cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName"
```

Configuratie vraagt UAC en gastreferenties en bewaart die met Windows DPAPI versleuteld buiten de repository. Gewone runs hergebruiken dit zonder vragen. Zet geen wachtwoorden in scripts of rapporten.

De beveiligde controller bijwerken met behoud van referenties:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName" -UpdateController
```

Installeer niet voor elke test opnieuw. Een protocol-/scriptverschil faalt expliciet en vereist deze bewuste update.

<a id="host-and-guest-responsibilities"></a>

## Verantwoordelijkheden van host en gast

Selectie gaat vooraf aan voorbereiding. De host bouwt toepassing, native tests en C#-helpers en verpakt alleen gekozen tests/afhankelijkheden. De gast controleert SHA256 en voorbereidingsargumenten vóór dezelfde scripts/asserties. Een ontbrekende of gewijzigde helper faalt; geen gastcompilatie. Visual Studio, MSVC en SDK zijn geen clientvereisten.

De verhoogde hosttaak draait alleen geïnstalleerde beveiligde controllerscripts. `NoConsoleRunner.exe` start zonder console en bewaart stdout, stderr en afsluitcode. Checkoutscenario’s draaien alleen in de gast, niet verhoogd op de host. Transport gebruikt PowerShell Direct en een interactieve gastworker; invoer gaat alleen naar het gastbureaublad.

Installeer testspecifieke vereisten in de gast. Everything op de host volstaat niet voor een VM-test. Schermen, DPI, providers en externe apps moeten aan de testvereisten voldoen.

<a id="reports-and-diagnosis"></a>

## Rapporten en diagnose

Rapporten worden automatisch verzameld onder `build/regression/vm-<id>/`:

| Pad | Inhoud |
| --- | --- |
| `output/regression/results.json`, `summary.json` | Testresultaten en totaal. |
| `output/regression/` | Testlogs, afsluitingen en bewijs. |
| `output/result.json` | Deploymentidentiteit, sessie en herstel. |
| `connection.json`, `smoke.json` | Transport-/sessieresultaten; smoke.json is de transportnaam. |
| `host-controller.stdout.log`, `host-controller.stderr.log` | Uitvoer van de hostcontroller. |

`-RunDirectory` kopieert alles naar een andere ongebruikte bestemming. Bewaar alle afsluitcodes: test, runner, gasttaak en hosttaak. Totale deadlines houden rekening met aantal tests en `-TimeoutSeconds`. Time-out, ontbrekend rapport, mislukt herstel of niet-nul worker is geen succes ondanks enkele PASS-rapporten.

`test-agentwin10.cmd` test transport met een echte Shell-map, Alt-rechts-formaatwijziging en normaal sluiten, niet de hele suite. `connect-agentwin10.cmd` is voor handmatige verbinding met Windows-authenticatie, geen onbeheerde invoer.

Bureaubladsessiekeuze kan zonder VM-verbinding worden gecontroleerd:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Test-DesktopSession.ps1
```

Test de controllerselectie: meerdere Verkenner-processen in één gastsessie vormen één bureaublad; nul of meerdere verschillende sessies blijven blokkeren.

Referenties: [PowerShell Direct](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/powershell-direct),
[beveiliging van interactieve taken](https://learn.microsoft.com/en-us/windows/win32/taskschd/security-contexts-for-running-tasks),
[DPAPI-referentie-export](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/export-clixml?view=powershell-5.1).
