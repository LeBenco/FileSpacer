# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
# Harmless NSIS probes: no installation, registry writes or host UAC prompt.
[CmdletBinding()]
param(
    [string]$MakeNSIS = "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
    [Parameter(Mandatory=$true)][string]$RunDirectory
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$source = [IO.File]::ReadAllText((Join-Path $root 'installer\install.nsi'))
function Block([string]$pattern) {
    $match = [regex]::Match($source, $pattern, 'Singleline')
    if (!$match.Success) { throw "Missing NSIS block: $pattern" }
    $match.Value
}
function Compile-Probe([string]$name, [string]$body) {
    $body = $body.Replace('@RUN@', $RunDirectory).Replace('@NAME@', $name)
    $path = Join-Path $RunDirectory ($name + '.nsi')
    [IO.File]::WriteAllText($path, $body, [Text.UTF8Encoding]::new($true))
    & $MakeNSIS /NOCONFIG /WX /V2 $path
    if ($LASTEXITCODE) { throw "NSIS compilation failed: $name" }
}
function Run-Probe([string]$name, [string]$arguments, [int]$expected) {
    $process = Start-Process -FilePath (Join-Path $RunDirectory ($name + '.exe')) -ArgumentList $arguments -WindowStyle Hidden -PassThru
    try {
        if (!$process.WaitForExit(30000)) {
            $process.Kill() # This probe owns no FileSpacer process or user data.
            throw "NSIS probe timed out: $name"
        }
        if ($process.ExitCode -ne $expected) { throw "$name returned $($process.ExitCode), expected $expected ($arguments)" }
    } finally { $process.Dispose() }
}

# Exercise the production ShellExecuteExW/wait/exit-code implementation with
# 'open' instead of 'runas'. Actual consent on the secure desktop is not tested.
$native = [IO.File]::ReadAllText((Join-Path $root 'installer\Elevation.nsh'))
$native = $native.Replace('!insertmacro ELEVATION_FUNCTION "un."', '')
$native = $native.Replace('"runas"', '"open"').Replace('$EXEPATH', '$probeProgram')
$nativeProbe = @'
Unicode True
Name "Native launch probe"
OutFile "@RUN@\@NAME@.exe"
RequestExecutionLevel user
SilentInstall silent
!include LogicLib.nsh
!include FileFunc.nsh
Var elevatedParameters
Var elevatedResult
Var probeProgram
@NATIVE@
Section
SectionEnd
Function .onInit
    ${GetParameters} $0
    ClearErrors
    ${GetOptionsS} $0 "/Child=" $1
    ${IfNot} ${Errors}
        SetErrorLevel $1
        Quit
    ${EndIf}
    StrCpy $probeProgram $EXEPATH
    StrCpy $0 "register preserved"
    StrCpy $R0 "extended register preserved"
    StrCpy $elevatedParameters "/S /Child=0"
    Call RunElevated
    ${If} $elevatedResult != 0
        SetErrorLevel 10
        Quit
    ${EndIf}
    StrCpy $elevatedParameters "/S /Child=17"
    Call RunElevated
    ${If} $elevatedResult != 17
        SetErrorLevel 11
        Quit
    ${EndIf}
    StrCpy $elevatedParameters "/S /Child=1223"
    Call RunElevated
    ${If} $elevatedResult != 1223
        SetErrorLevel 12
        Quit
    ${EndIf}
    StrCpy $probeProgram "$EXEDIR\missing-probe.exe"
    Call RunElevated
    ${If} $elevatedResult != 2
        SetErrorLevel 13
        Quit
    ${EndIf}
    ${If} $0 != "register preserved"
    ${OrIf} $R0 != "extended register preserved"
        SetErrorLevel 14
        Quit
    ${EndIf}
    SetErrorLevel 0
    Quit
FunctionEnd
'@
Compile-Probe 'NativeLaunchProbe' ($nativeProbe.Replace('@NATIVE@', $native))
Run-Probe 'NativeLaunchProbe' '/S' 0

$logic = @(
    (Block 'Function \.onSelChange\r?\n.*?FunctionEnd'),
    (Block '!macro RESTORE_COMPONENT .*?!macroend'),
    (Block 'Function RestoreInstallChoices\r?\n.*?FunctionEnd'),
    (Block 'Function RestoreInstallLanguage\r?\n.*?FunctionEnd'),
    (Block 'Function SetInstallScope\r?\n.*?FunctionEnd'),
    (Block '!macro WORKER_COMPONENT .*?!macroend'),
    (Block 'Function InitializeInstall\r?\n.*?FunctionEnd'),
    (Block 'Function InstallForAllUsers\r?\n.*?FunctionEnd')
) -join "`r`n"
$parent = [regex]::Match($source, 'Call CheckClosed[^\r\n]*\r?\n(?<branch>.*?)(?=    ClearErrors)', 'Singleline').Groups['branch'].Value
if (!$parent.Contains('Call InstallForAllUsers')) { throw 'Missing machine-install delegation.' }
$errors = (@($source -split '\r?\n' | Where-Object { $_ -match '^LangString ERROR_MACHINE ' }) -join "`r`n")
$bootstrap = @'
Unicode True
Name "Installer bootstrap probe"
OutFile "@RUN@\@NAME@.exe"
RequestExecutionLevel user
SilentInstall silent
InstallDir ""
!include LogicLib.nsh
!include FileFunc.nsh
!include Sections.nsh
!define LANG_ENGLISH 1033
!define LANG_FRENCH 1036
LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"
LoadLanguageFile "${NSISDIR}\Contrib\Language files\French.nlf"
@ERRORS@
Var installMode
Var parameters
Var requestedDirectory
Var machineWorker
Var delegated
Var choicesScope
Var upgrading
Var elevatedParameters
Var elevatedResult
Var fakeResult
Section "Start" SecStart
SectionEnd
Section "Context" SecContext
SectionEnd
Section /o "Default" SecDefault
SectionEnd
@LOGIC@
Function RunElevated
    StrCpy $elevatedResult $fakeResult ; Capture the command, never start an elevated installer.
FunctionEnd
Function ApplyMachineParent
@PARENT@
install_done:
FunctionEnd
Function .onInit
    SetRegView 64
    ${GetParameters} $0
    StrCpy $fakeResult 0
    ClearErrors
    ${GetOptionsS} $0 "/Result=" $1
    ${IfNot} ${Errors}
        StrCpy $fakeResult $1
    ${EndIf}
    Call InitializeInstall
    Call ApplyMachineParent
    FileOpen $9 "@RUN@\@NAME@.txt" w
    FileWriteWord $9 0xFEFF
    FileWriteUTF16LE $9 'Mode=$installMode$\r$\nDirectory=$INSTDIR$\r$\nLanguage=$LANGUAGE$\r$\nWorker=$machineWorker$\r$\n'
    SectionGetFlags ${SecStart} $0
    IntOp $0 $0 & ${SF_SELECTED}
    FileWriteUTF16LE $9 'Start=$0$\r$\n'
    SectionGetFlags ${SecContext} $0
    IntOp $0 $0 & ${SF_SELECTED}
    FileWriteUTF16LE $9 'Context=$0$\r$\n'
    SectionGetFlags ${SecDefault} $0
    IntOp $0 $0 & ${SF_SELECTED}
    FileWriteUTF16LE $9 'Default=$0$\r$\nParameters=$elevatedParameters$\r$\n'
    FileClose $9
    SetErrorLevel 0
    Quit
FunctionEnd
'@
$bootstrap = $bootstrap.Replace('@LOGIC@',$logic).Replace('@PARENT@',$parent).Replace('@ERRORS@',$errors)
Compile-Probe 'BootstrapProbe' $bootstrap
function Read-Report([string]$name) {
    $report = @{}
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $RunDirectory ($name+'.txt')), [Text.Encoding]::Unicode)) {
        $parts = $line -split '=',2
        if ($parts.Count -eq 2) { $report[$parts[0]] = $parts[1] }
    }
    $report
}
$fixture = Join-Path $RunDirectory 'fixture with spaces'
[void][IO.Directory]::CreateDirectory($fixture)
$profile = "[Installation]`r`nMode=CurrentUser`r`nLanguage=1036`r`n[Components]`r`nStartMenu=0`r`nContextMenu=1`r`nDefaultFileManager=1`r`n"
[IO.File]::WriteAllText((Join-Path $fixture 'install-info.ini'),$profile,[Text.Encoding]::Unicode)
Run-Probe 'BootstrapProbe' ('/CurrentUser /D='+$fixture) 0
$report = Read-Report 'BootstrapProbe'
if ($report.Mode -cne 'CurrentUser' -or $report.Directory -cne $fixture -or $report.Language -ne '1036' -or $report.Start -ne '0' -or $report.Context -ne '1' -or $report.Default -ne '1' -or $report.Parameters) { throw 'Per-user bootstrap did not preserve the profile or requested directory.' }
Run-Probe 'BootstrapProbe' ('/AllUsers /D='+$fixture) 0
$report = Read-Report 'BootstrapProbe'
$expected = "/S /AllUsers /MachineWorker /StartMenu=1 /ContextMenu=1 /DefaultFileManager=0 /Language=1036 /D=$fixture"
if ($report.Mode -cne 'AllUsers' -or $report.Parameters -cne $expected) { throw 'Elevated command did not preserve the selected scope, directory or language.' }
Run-Probe 'BootstrapProbe' ('/AllUsers /Result=1223 /D='+$fixture) 1
Run-Probe 'BootstrapProbe' ('/AllUsers /Result=7 /D='+$fixture) 7

# The real effective-token check above is retained in BootstrapProbe. A separate
# probe stubs only that token query to test worker options without host elevation.
$stubbed = [regex]::Replace($bootstrap, '        UserInfo::GetAccountType\r?\n        Pop \$0', { '        StrCpy $0 "Admin"' })
if ($stubbed -eq $bootstrap) { throw 'Worker token test boundary not found.' }
Compile-Probe 'WorkerOptionsProbe' $stubbed
foreach ($start in 0,1) {
    foreach ($pair in @(@(0,0),@(1,0),@(1,1))) {
        $options = "/S /AllUsers /MachineWorker /StartMenu=$start /ContextMenu=$($pair[0]) /DefaultFileManager=$($pair[1]) /Language=1033 /D=$fixture"
        Run-Probe 'WorkerOptionsProbe' $options 0
        $report = Read-Report 'WorkerOptionsProbe'
        if ($report.Worker -ne '1' -or $report.Mode -cne 'AllUsers' -or $report.Directory -cne $fixture -or $report.Language -ne '1033' -or $report.Start -ne "$start" -or $report.Context -ne "$($pair[0])" -or $report.Default -ne "$($pair[1])") { throw 'Worker options were lost or applied under the wrong scope.' }
    }
}
Run-Probe 'WorkerOptionsProbe' ('/AllUsers /MachineWorker /StartMenu=2 /ContextMenu=1 /DefaultFileManager=0 /Language=1033 /D='+$fixture) 2
Run-Probe 'WorkerOptionsProbe' ('/AllUsers /MachineWorker /StartMenu=1 /ContextMenu=1 /DefaultFileManager=0 /Language=9999 /D='+$fixture) 2
if (!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Run-Probe 'BootstrapProbe' ('/AllUsers /MachineWorker /StartMenu=1 /ContextMenu=1 /DefaultFileManager=0 /Language=1033 /D='+$fixture) 2
    Write-Host 'PASS: the real effective-token check rejects an unelevated machine worker.'
} else {
    Write-Host 'Coverage: unelevated-token rejection is skipped when this test process is already elevated.'
}
Write-Host 'PASS: native process exit codes and launch failure; bootstrap/profile restoration; worker argument transport; cancellation/failure propagation; invalid options.'
Write-Host 'Coverage: worker option parsing uses a token stub; native launcher uses open; no real UAC consent or full installation was exercised.'
