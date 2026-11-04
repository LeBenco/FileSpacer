# Running tests in a Hyper-V client

The [regression launcher](../regression/README.md) can run the selected cases in
a dedicated Windows client while the development host remains usable.

```cmd
tests.cmd -VM AgentWin10
tests.cmd -VM AgentWin10 -Category Internal -Pattern "26-*"
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Without `-VM`, tests run locally. A VM name refers to an existing configured VM;
the launcher never creates one or falls back to local UI execution.

## Prepare the client once

Use a dedicated VM and a test account with a password. The VM must be running
with that account's graphical session signed in and unlocked. Close VMConnect
without signing out or shutting down the guest. Cases run under a limited token
in that session. The controller can reconnect a disconnected session to the
guest console; it cannot unlock Windows.

At an agreed time, configure a new client from the repository root:

```cmd
prepare-client-hyperv.cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName"
```

Setup requests UAC approval and the guest credentials. It stores the credentials
encrypted with Windows DPAPI outside the repository. Ordinary runs reuse this
configuration without prompts. Do not put passwords in scripts or reports.

To update the protected controller while retaining its credentials:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Install-HyperVRunner.ps1 -VMName "VMName" -UpdateController
```

Do not reinstall it for each test. A protocol or protected-script mismatch
fails explicitly and requires this deliberate update.

## Host and guest responsibilities

Selection happens before preparation. The host compiles the application,
native tests and C# helpers, then packages only the chosen cases and their
dependencies. The guest checks executable/assembly SHA256 hashes and preparation
arguments before running the same scripts and assertions as the local suite.
A missing or changed helper fails; there is no guest compilation. Visual Studio,
MSVC and the Windows SDK are not client prerequisites.

The elevated host task executes only its installed protected controller scripts.
`NoConsoleRunner.exe` starts that controller without a console and captures its
stdout, stderr and exit code. Checkout scenarios execute only in the guest, not
as elevated host scripts. The transport uses PowerShell Direct and an interactive
guest worker; input is sent to the guest desktop only.

Install any scenario-specific prerequisites in the guest. Everything on the host
does not satisfy an Everything case in the VM. Display layout, DPI, Shell
providers and external applications must match the case's requirements.

## Reports and diagnosis

Reports are collected automatically under `build/regression/vm-<id>/`:

| Path | Contents |
| --- | --- |
| `output/regression/results.json`, `summary.json` | Case results and aggregate outcome. |
| `output/regression/` | Case logs, application exits and captured evidence. |
| `output/result.json` | Guest deployment identity, session and restoration checks. |
| `connection.json`, `smoke.json` | Transport/session results; `smoke.json` is the transport's filename. |
| `host-controller.stdout.log`, `host-controller.stderr.log` | Host controller output. |

`-RunDirectory` requests a complete copy at another unused destination. Preserve
all exit codes: case, runner, guest task and host task. Overall deadlines account
for the selected case count and `-TimeoutSeconds`. A timeout, missing report,
failed restoration or nonzero worker exit is not a successful run, even if some
case reports say PASS.

For transport diagnosis, `test-agentwin10.cmd` exercises a real Shell folder,
Alt-right resizing and normal closure. It does not run the full regression
catalogue. `connect-agentwin10.cmd` is for manual connection with Windows
authentication, not unattended test input.

Desktop-session selection can be checked without a VM connection:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\client\Test-DesktopSession.ps1
```

It tests the controller's selection logic: multiple Explorer processes in one
guest-user session still identify one desktop; zero or multiple distinct
sessions remain blocking conditions.

References: [PowerShell Direct](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/powershell-direct),
[interactive task security](https://learn.microsoft.com/en-us/windows/win32/taskschd/security-contexts-for-running-tasks),
[DPAPI credential export](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/export-clixml?view=powershell-5.1).
