# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
# Compile and run the production NSIS choice logic without installing anything.
[CmdletBinding()]
param([string]$MakeNSIS = "${env:ProgramFiles(x86)}\NSIS\makensis.exe")
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$run = Join-Path $root ('build\installer-tests\' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($run)
$source = [IO.File]::ReadAllText((Join-Path $root 'installer\install.nsi'))
function Extract([string]$pattern) {
    $match = [regex]::Match($source, $pattern, 'Singleline')
    if (!$match.Success) { throw "Missing production NSIS block: $pattern" }
    return $match.Value
}
$logic = @(
    (Extract 'Function \.onSelChange\r?\n.*?FunctionEnd'),
    (Extract '!macro RESTORE_COMPONENT .*?!macroend'),
    (Extract 'Function RestoreInstallChoices\r?\n.*?FunctionEnd'),
    (Extract 'Function RestoreInstallLanguage\r?\n.*?FunctionEnd'),
    (Extract '!macro SAVE_COMPONENT .*?!macroend')
) -join "`r`n"
$guard = [regex]::Match($source, 'Section /o "\$\(SEC_DEFAULT\)" SecDefault.*?(?<guard>    \$\{If\} \$machineWorker.*?)(?=    ClearErrors)', 'Singleline').Groups['guard'].Value
if (!$guard) { throw 'Missing silent-upgrade association guard.' }
$harness = @'
Unicode True
Name "FileSpacer installer tests"
OutFile "@RUN@\InstallerTests.exe"
RequestExecutionLevel user
SilentInstall silent
!include LogicLib.nsh
!include Sections.nsh
!define LANG_ENGLISH 1033
!define LANG_FRENCH 1036
Var installMode
Var choicesScope
Var upgrading
Var machineWorker
Var defaultWritten
Var checks
Section "Start" SecStart
SectionEnd
Section "Context" SecContext
SectionEnd
Section /o "Default" SecDefault
SectionEnd
@LOGIC@
Function RestoreForTest
    Call RestoreInstallChoices
    Call RestoreInstallLanguage
FunctionEnd
Function ApplyDefault
    StrCpy $defaultWritten 0
@GUARD@
    StrCpy $defaultWritten 1
default_done:
FunctionEnd
!macro ASSERT ACTUAL EXPECTED NAME
    ${If} ${ACTUAL} != ${EXPECTED}
        FileWrite $9 'FAIL: ${NAME}: ${ACTUAL} != ${EXPECTED}$\r$\n'
        FileClose $9
        SetErrorLevel 1
        Quit
    ${EndIf}
    IntOp $checks $checks + 1
!macroend
!macro CHECK_SELECTION SECTION EXPECTED NAME
    SectionGetFlags ${SECTION} $0
    IntOp $0 $0 & ${SF_SELECTED}
    !insertmacro ASSERT $0 ${EXPECTED} "${NAME}"
!macroend
!macro ROUNDTRIP START CONTEXT DEFAULT
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "StartMenu" "${START}"
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "ContextMenu" "${CONTEXT}"
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "DefaultFileManager" "${DEFAULT}"
    StrCpy $choicesScope ""
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecStart} ${START} "restored Start"
    !insertmacro CHECK_SELECTION ${SecContext} ${CONTEXT} "restored Context"
    !insertmacro CHECK_SELECTION ${SecDefault} ${DEFAULT} "restored Default"
    DeleteINISec "$INSTDIR\install-info.ini" "Components"
    !insertmacro SAVE_COMPONENT ${SecStart} "StartMenu"
    !insertmacro SAVE_COMPONENT ${SecContext} "ContextMenu"
    !insertmacro SAVE_COMPONENT ${SecDefault} "DefaultFileManager"
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Components" "StartMenu"
    !insertmacro ASSERT $0 ${START} "saved Start"
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Components" "ContextMenu"
    !insertmacro ASSERT $0 ${CONTEXT} "saved Context"
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Components" "DefaultFileManager"
    !insertmacro ASSERT $0 ${DEFAULT} "saved Default"
!macroend
Function .onInit
    SetRegView 64
    FileOpen $9 "@RUN@\result.txt" w
    StrCpy $checks 0
    StrCpy $INSTDIR "@RUN@\fixture"
    CreateDirectory "$INSTDIR"
    StrCpy $installMode "CurrentUser"
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecStart} 1 "fresh Start"
    !insertmacro CHECK_SELECTION ${SecContext} 1 "fresh Context"
    !insertmacro CHECK_SELECTION ${SecDefault} 0 "fresh Default"
    !insertmacro ASSERT $upgrading 0 "fresh mode"
    Call ApplyDefault
    !insertmacro ASSERT $defaultWritten 1 "fresh explicit default"
    StrCpy $1 0
next_scope:
    ${If} $installMode == "AllUsers"
        SetShellVarContext all
    ${Else}
        SetShellVarContext current
    ${EndIf}
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Mode" $installMode
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Language" "1036"
    !insertmacro ROUNDTRIP 0 0 0
    !insertmacro ROUNDTRIP 0 1 0
    !insertmacro ROUNDTRIP 0 1 1
    !insertmacro ROUNDTRIP 1 0 0
    !insertmacro ROUNDTRIP 1 1 0
    !insertmacro ROUNDTRIP 1 1 1
    !insertmacro ASSERT $LANGUAGE 1036 "saved language"
    !insertmacro ASSERT $upgrading 1 "upgrade detected"
    Call ApplyDefault
    !insertmacro ASSERT $defaultWritten 0 "silent upgrade preserves associations"
    ; Revisiting a page must keep checkbox edits made in this run.
    !insertmacro UnselectSection ${SecStart}
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecStart} 0 "pending edits retained"
    ; Inconsistent saved metadata must still respect the component dependency.
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "ContextMenu" "0"
    StrCpy $choicesScope ""
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecContext} 1 "default requires context"
    IntOp $1 $1 + 1
    ${If} $1 == 1
        StrCpy $installMode "AllUsers"
        Goto next_scope
    ${EndIf}
    ; Metadata for another scope cannot leak into a fresh installation.
    StrCpy $installMode "CurrentUser"
    SetShellVarContext current
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecStart} 1 "other scope Start"
    !insertmacro CHECK_SELECTION ${SecContext} 1 "other scope Context"
    !insertmacro CHECK_SELECTION ${SecDefault} 0 "other scope Default"
    !insertmacro ASSERT $upgrading 0 "other scope mode"
    ; Legacy profiles infer existing components, using read-only Shell state.
    DeleteINISec "$INSTDIR\install-info.ini" "Components"
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Mode" $installMode
    StrCpy $2 0
    ${If} ${FileExists} "$SMPROGRAMS\FileSpacer.lnk"
        StrCpy $2 1
    ${EndIf}
    StrCpy $3 0
    ReadRegStr $0 SHCTX "Software\Classes\Directory\shell\filespacer\command" ""
    ${If} $0 != ""
        StrCpy $3 1
    ${EndIf}
    StrCpy $4 0
    ReadRegStr $0 HKCR "Directory\shell" ""
    ${If} $0 == "filespacer"
        StrCpy $3 1
        StrCpy $4 1
    ${EndIf}
    StrCpy $choicesScope ""
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecStart} $2 "legacy Start"
    !insertmacro CHECK_SELECTION ${SecContext} $3 "legacy Context"
    !insertmacro CHECK_SELECTION ${SecDefault} $4 "legacy Default"
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "StartMenu" "invalid"
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "ContextMenu" "2"
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "DefaultFileManager" "-1"
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Language" "invalid"
    StrCpy $LANGUAGE 1033
    StrCpy $choicesScope ""
    Call RestoreForTest
    !insertmacro CHECK_SELECTION ${SecStart} $2 "invalid Start keeps migration"
    !insertmacro CHECK_SELECTION ${SecContext} $3 "invalid Context keeps migration"
    !insertmacro CHECK_SELECTION ${SecDefault} $4 "invalid Default keeps migration"
    !insertmacro ASSERT $LANGUAGE 1033 "invalid language ignored"
    ; The same saved choice is actionable when a user reviews an interactive upgrade.
    SetSilent normal
    Call ApplyDefault
    !insertmacro ASSERT $defaultWritten 1 "interactive explicit default"
    StrCpy $machineWorker 1
    Call ApplyDefault
    !insertmacro ASSERT $defaultWritten 0 "machine worker leaves HKCU to parent"
    FileWrite $9 'PASS: $checks NSIS assertions; fixture only, no registry writes.$\r$\n'
    FileClose $9
    SetErrorLevel 0
    Quit
FunctionEnd
'@
$harness = $harness.Replace('@RUN@', $run).Replace('@LOGIC@', $logic).Replace('@GUARD@', $guard)
$testSource = Join-Path $run 'InstallerTests.nsi'
[IO.File]::WriteAllText($testSource, $harness, [Text.UTF8Encoding]::new($true))
& $MakeNSIS /NOCONFIG /WX /V2 $testSource
if ($LASTEXITCODE) { throw "NSIS test compilation failed: $LASTEXITCODE" }
$testProcess = Start-Process -FilePath (Join-Path $run 'InstallerTests.exe') -WindowStyle Hidden -Wait -PassThru
$result = Get-Content -LiteralPath (Join-Path $run 'result.txt') -Raw
Write-Host $result.Trim()
Write-Host "Retained report: $run"
if ($testProcess.ExitCode -ne 0 -or $result -notmatch '^PASS:') { throw 'NSIS installer choice tests failed.' }
& (Join-Path $PSScriptRoot 'Run-ElevationTests.ps1') -MakeNSIS $MakeNSIS -RunDirectory $run
& (Join-Path $PSScriptRoot 'Run-DocumentationInventoryTests.ps1') -RunDirectory (Join-Path $run 'documentation-inventory')
& (Join-Path $PSScriptRoot 'Run-InstallLifecycleTests.ps1') -MakeNSIS $MakeNSIS -RunDirectory $run
