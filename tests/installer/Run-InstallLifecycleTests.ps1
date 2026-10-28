# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
# Run the real NSIS sections with a harmless payload, fixture shortcuts and
# GUID-prefixed registry keys. Never install the application into a user profile.
[CmdletBinding()]
param(
    [string]$MakeNSIS = "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
    [Parameter(Mandatory=$true)][string]$RunDirectory
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$namespaceId = [guid]::NewGuid().ToString('N')
$registryPrefix = 'Software\FileSpacerInstallerTests\' + $namespaceId + '\'
$registryRelative = 'Software\FileSpacerInstallerTests\' + $namespaceId
$shortcutDirectory = Join-Path $RunDirectory 'fixture shortcuts'
[void][IO.Directory]::CreateDirectory($shortcutDirectory)
$payload = Join-Path $RunDirectory 'Payload.exe'
Add-Type -TypeDefinition @'
using System.Reflection;
[assembly: AssemblyFileVersion("1.1.10.0")]
[assembly: AssemblyInformationalVersion("1.1.10")]
public static class InstallerTestPayload {
    public static void Main(string[] args) {
        if (args.Length == 2 && args[0] == "/hold") {
            System.IO.File.WriteAllText(args[1], "ready");
            System.Threading.Thread.Sleep(10000);
        }
    }
}
'@ -OutputAssembly $payload -OutputType WindowsApplication
$variant = [IO.File]::ReadAllText((Join-Path $root 'installer\install.nsi'))
if (!$variant.Contains('RequestExecutionLevel user')) { throw 'Installer must declare user execution level.' }
$variant = $variant.Replace('Software\', $registryPrefix)
$variant = $variant.Replace('$SMPROGRAMS', $shortcutDirectory)
$providerFixture = Join-Path $RunDirectory 'owned provider cache'
$variant = $variant.Replace('$LOCALAPPDATA\FileSpacer\ShellView.{4FDD0CE1-6FD0-441C-AD2D-6DD056610DE3}', $providerFixture)
$variant = $variant.Replace('..\build\release\FileSpacer.exe', $payload)
$variant = $variant.Replace('File "'+$payload+'"','File /oname=FileSpacer.exe "'+$payload+'"')
$shortcutHelper = Join-Path $RunDirectory 'FixtureShortcutAppID.nsh'
$shortcutCode = [IO.File]::ReadAllText((Join-Path $root 'installer\ShortcutAppID.nsh')).Replace('$SMPROGRAMS',$shortcutDirectory)
[IO.File]::WriteAllText($shortcutHelper,$shortcutCode,[Text.UTF8Encoding]::new($true))
$variant = $variant.Replace('!include ShortcutAppID.nsh','!include "'+$shortcutHelper+'"')
$installer = Join-Path $RunDirectory 'LifecycleInstaller.exe'
$variant = $variant.Replace('OutFile "..\build\FileSpacer-install.exe"', 'OutFile "'+$installer+'"')
$variant = '!cd "' + (Join-Path $root 'installer') + '"' + "`r`n" + $variant
$variantPath = Join-Path $RunDirectory 'LifecycleInstaller.nsi'
[IO.File]::WriteAllText($variantPath,$variant,[Text.UTF8Encoding]::new($true))
& $MakeNSIS /NOCONFIG /WX /V2 $variantPath
if ($LASTEXITCODE) { throw 'Lifecycle fixture compilation failed.' }
function Run-FixtureProcess([string]$path,[string]$arguments,[int]$expected=0) {
    $process=Start-Process -FilePath $path -ArgumentList $arguments -WindowStyle Hidden -PassThru
    try {
        if (!$process.WaitForExit(30000)) { throw "Installer fixture timed out: $path" }
        if ($process.ExitCode -ne $expected) { throw "Installer fixture returned $($process.ExitCode), expected $expected ($arguments)" }
    } finally { $process.Dispose() }
}
function Check-Profile([string]$directory,[string]$mode,[int]$selected) {
    $profile=[IO.File]::ReadAllText((Join-Path $directory 'install-info.ini'))
    foreach($entry in @("Mode=$mode","StartMenu=$selected","ContextMenu=$selected",'DefaultFileManager=0')) {
        if($profile -notmatch ('(?m)^'+[regex]::Escape($entry)+'\r?$')) { throw "Wrong installed metadata: $entry" }
    }
}
$testError=$null
$cleanupErrors=New-Object 'System.Collections.Generic.List[string]'
try {
    # Machine installation needs the dedicated client VM.
    $mode='CurrentUser'
    $directory=Join-Path $RunDirectory ('installed fixture '+$mode)
    $hive=[Microsoft.Win32.Registry]::CurrentUser
    Run-FixtureProcess $installer ("/S /$mode /D=$directory")
    if(!(Test-Path -LiteralPath (Join-Path $directory 'FileSpacer.exe'))) { throw 'Payload not installed.' }
    if(!(Test-Path -LiteralPath (Join-Path $directory 'FileSpacerFolder.dll'))) { throw 'Mixed-sort provider DLL not installed.' }
    Check-Profile $directory $mode 1
    $installed=$hive.OpenSubKey($registryPrefix+'FileSpacer')
    try {
        if(!$installed -or $installed.GetValue('Install_Dir') -cne $directory -or $installed.GetValue('InstallMode') -cne $mode) { throw 'Installation registration was written under the wrong scope or directory.' }
    } finally { if($installed) { $installed.Dispose() } }
    if(!(Test-Path -LiteralPath (Join-Path $shortcutDirectory 'FileSpacer.lnk'))) { throw 'Start Menu choice was not applied.' }
    $command=$hive.OpenSubKey($registryPrefix+'Classes\Directory\shell\filespacer\command')
    try { if(!$command -or $command.GetValue('') -notlike ('*'+$directory+'*')) { throw 'Context command was not installed under the fixture namespace.' } }
    finally { if($command) { $command.Dispose() } }
    # A running fixture must be detected before an upgrade writes anything.
    # The inert payload exits by itself; no FileSpacer process is terminated.
    $ready=Join-Path $RunDirectory 'holder-ready.txt'
    $heldExecutable=Join-Path $directory 'FileSpacer.exe'
    $heldBytes=[Convert]::ToBase64String([IO.File]::ReadAllBytes($heldExecutable))
    $holder=Start-Process -FilePath $heldExecutable -ArgumentList ('/hold "'+$ready+'"') -WindowStyle Hidden -PassThru
    try {
        $deadline=[DateTime]::UtcNow.AddSeconds(5)
        while(!(Test-Path -LiteralPath $ready) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 25 }
        if(!(Test-Path -LiteralPath $ready)) { throw 'The running-file fixture did not become ready.' }
        Run-FixtureProcess $installer ("/S /$mode /D=$directory") 2
        if([Convert]::ToBase64String([IO.File]::ReadAllBytes($heldExecutable)) -cne $heldBytes) { throw 'The blocked upgrade changed its running executable.' }
        Check-Profile $directory $mode 1
    } finally {
        $holderExited=$holder.WaitForExit(15000)
        $holder.Dispose()
        if(!$holderExited) { $cleanupErrors.Add('The inert holder did not exit naturally.') }
    }
    # Reproduce an upgrade from a setup where both optional components were
    # unchecked. Existing shortcut/verb must actually be removed on upgrade.
    $profile="[Installation]`r`nMode=$mode`r`nLanguage=1036`r`n[Components]`r`nStartMenu=0`r`nContextMenu=0`r`nDefaultFileManager=0`r`n"
    [IO.File]::WriteAllText((Join-Path $directory 'install-info.ini'),$profile,[Text.Encoding]::Unicode)
    Run-FixtureProcess $installer ("/S /$mode /D=$directory")
    Check-Profile $directory $mode 0
    if(Test-Path -LiteralPath (Join-Path $shortcutDirectory 'FileSpacer.lnk')) { throw 'Unchecked shortcut survived the upgrade.' }
    $command=$hive.OpenSubKey($registryPrefix+'Classes\Directory\shell\filespacer')
    if($command) { $command.Dispose(); throw 'Unchecked context command survived the upgrade.' }
    $userFile=Join-Path $directory 'keep-me.txt'
    [IO.File]::WriteAllText($userFile,'Fixture user data to preserve.')
    # Exercise provider ownership cleanup only in this run's redirected namespace.
    $providerKey=$registryPrefix+'Classes\CLSID\{4FDD0CE1-6FD0-441C-AD2D-6DD056610DE3}'
    $provider=$hive.CreateSubKey($providerKey+'\InprocServer32')
    try { $provider.SetValue('',(Join-Path $directory 'FileSpacerFolder.dll')) } finally { $provider.Dispose() }
    [void][IO.Directory]::CreateDirectory($providerFixture)
    Run-FixtureProcess (Join-Path $directory 'uninstall.exe') '/S'
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    while([DateTime]::UtcNow -lt $deadline -and ((Test-Path -LiteralPath (Join-Path $directory 'FileSpacer.exe')) -or (Test-Path -LiteralPath (Join-Path $directory 'uninstall.exe')))) { Start-Sleep -Milliseconds 50 }
    foreach($owned in 'FileSpacer.exe','FileSpacerFolder.dll','uninstall.exe','install-info.ini') {
        if(Test-Path -LiteralPath (Join-Path $directory $owned)) { throw "Owned file remained after uninstallation: $owned" }
    }
    $provider=$hive.OpenSubKey($providerKey)
    if($provider) { $provider.Dispose(); throw 'Owned provider registration remained after uninstallation.' }
    if(Test-Path -LiteralPath $providerFixture) { throw 'Owned provider cache remained after uninstallation.' }
    if(!(Test-Path -LiteralPath $userFile) -or [IO.File]::ReadAllText($userFile) -cne 'Fixture user data to preserve.') { throw 'Uninstallation removed or changed an unowned file.' }
    $installed=$hive.OpenSubKey($registryPrefix+'FileSpacer')
    if($installed) { $installed.Dispose(); throw 'Installation registration remained after uninstallation.' }
    Write-Host "PASS: $mode fixture installation, running-process protection, silent upgrade, component removal and native uninstallation preserving unowned data."
} catch { $testError=$_ }
finally {
    # Delete only the fresh GUID namespaces owned by this run, even on failure.
    if($namespaceId -notmatch '^[a-f0-9]{32}$' -or $registryRelative -cne ('Software\FileSpacerInstallerTests\'+$namespaceId)) { throw 'Invalid registry cleanup namespace.' }
    foreach($hive in @([Microsoft.Win32.Registry]::CurrentUser)) {
        try { $hive.DeleteSubKeyTree($registryRelative,$false) } catch { $cleanupErrors.Add($_.Exception.Message) }
    }
}
if($testError) {
    foreach($cleanupError in $cleanupErrors) { Write-Host "Cleanup failure: $cleanupError" }
    throw $testError
}
if($cleanupErrors.Count) { throw ('Registry cleanup failed: '+($cleanupErrors -join '; ')) }
Write-Host 'Coverage: real all-users lifecycle and UAC consent need the dedicated client VM and were not run here.'
Write-Host 'Coverage: application payload is inert; registry keys and shortcuts are redirected to this test run.'
