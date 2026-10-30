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
        if (args.Length == 3 && args[0] == "/hold") {
            using (var release = System.Threading.EventWaitHandle.OpenExisting(args[2])) {
                System.IO.File.WriteAllText(args[1], "ready");
                if (!release.WaitOne(60000)) System.Environment.Exit(3);
            }
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
# Exercise actual nsExec launch/nonzero failures without changing the host's PowerShell.
if([regex]::Matches($variant,'(?m)^    nsExec::ExecToLog ').Count -ne 2){throw 'Expected both production documentation cleanup commands.'}
$variant = [regex]::Replace($variant, '(?m)^    (nsExec::ExecToLog .*)\r?$', {
    param($match)
    @'
    ${If} ${FileExists} "$INSTDIR\missing-powershell.fixture"
        nsExec::ExecToLog '"$INSTDIR\absent-powershell.exe"'
    ${ElseIf} ${FileExists} "$INSTDIR\script-failure.fixture"
        nsExec::ExecToLog '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -Command "exit 126"'
    ${Else}
        @COMMAND@
    ${EndIf}
'@.Replace('@COMMAND@',$match.Groups[1].Value)
})
# Record the real detail/warning branches even in silent mode, where NSIS skips MessageBox.
$detail='DetailPrint "Documentation cleanup failed; preserving the remaining help files and inventory."'
$warning='MessageBox MB_OK|MB_ICONEXCLAMATION $(WARNING_UNINSTALL_DOCUMENTATION) /SD IDOK'
foreach($entry in @(@($detail,'detail'),@($warning,'final'))) {
    if(!$variant.Contains($entry[0])) { throw ('Missing documentation failure notification: '+$entry[1]) }
    $record=@'
FileOpen $R8 "@RUN@\documentation-warning.log" a
    FileSeek $R8 0 END
    FileWriteUTF16LE $R8 "@KIND@: $(WARNING_UNINSTALL_DOCUMENTATION)$\r$\n"
    FileClose $R8
    @ORIGINAL@
'@.Replace('@RUN@',$RunDirectory).Replace('@KIND@',$entry[1]).Replace('@ORIGINAL@',$entry[0])
    $variant=$variant.Replace($entry[0],$record)
}
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
    $resultFile=Join-Path $RunDirectory 'result with spaces.ini'
    Run-FixtureProcess $installer ("/S /$mode /UpdateResult=`"$resultFile`" /D=$directory")
    $result=[IO.File]::ReadAllText($resultFile,[Text.Encoding]::Unicode)
    foreach($entry in @('Complete=1',"Mode=$mode",("Executable="+(Join-Path $directory 'FileSpacer.exe')))) {
        if($result -notmatch ('(?m)^'+[regex]::Escape($entry)+'\r?$')) { throw "Wrong setup result: $entry" }
    }
    Remove-Item -LiteralPath $resultFile
    if(!(Test-Path -LiteralPath (Join-Path $directory 'FileSpacer.exe'))) { throw 'Payload not installed.' }
    if(!(Test-Path -LiteralPath (Join-Path $directory 'FileSpacerFolder.dll'))) { throw 'Mixed-sort provider DLL not installed.' }
    $documentation=Join-Path $directory 'help'
    $manifest=Get-Content -LiteralPath (Join-Path $root 'build\offline-docs\build-info.json') -Raw | ConvertFrom-Json
    foreach($language in $manifest.languages) {
        $page=Join-Path $documentation ($language.path+'/index.html')
        if(!(Test-Path -LiteralPath $page -PathType Leaf)) { throw "Missing installed documentation: $($language.code)" }
    }
    foreach($entry in $manifest.files.PSObject.Properties) {
        $installedFile=Join-Path $documentation $entry.Name
        if((Get-FileHash -LiteralPath $installedFile -Algorithm SHA256).Hash -ine $entry.Value) { throw "Installed documentation differs: $($entry.Name)" }
    }
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
    # Release the inert payload after the check; installer startup can take
    # longer than a fixed hold delay. No FileSpacer process is terminated.
    $ready=Join-Path $RunDirectory 'holder-ready.txt'
    $heldExecutable=Join-Path $directory 'FileSpacer.exe'
    $heldBytes=[Convert]::ToBase64String([IO.File]::ReadAllBytes($heldExecutable))
    $releaseName='Local\FileSpacerInstallerTests.'+[guid]::NewGuid().ToString('N')
    $release=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$releaseName)
    $holder=$null
    try {
        $holder=Start-Process -FilePath $heldExecutable -ArgumentList ('/hold "'+$ready+'" '+$releaseName) -WindowStyle Hidden -PassThru
        $deadline=[DateTime]::UtcNow.AddSeconds(5)
        while(!(Test-Path -LiteralPath $ready) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 25 }
        if(!(Test-Path -LiteralPath $ready)) { throw 'The running-file fixture did not become ready.' }
        Run-FixtureProcess $installer ("/S /$mode /UpdateResult=`"$resultFile`" /D=$directory") 2
        if(Test-Path -LiteralPath $resultFile){throw 'Failed setup reported a completed installation.'}
        if([Convert]::ToBase64String([IO.File]::ReadAllBytes($heldExecutable)) -cne $heldBytes) { throw 'The blocked upgrade changed its running executable.' }
        Check-Profile $directory $mode 1
    } finally {
        [void]$release.Set()
        if($holder) {
            $holderExited=$holder.WaitForExit(15000)
            if(!$holderExited -or $holder.ExitCode -ne 0) { $cleanupErrors.Add('The inert holder did not exit after release.') }
            $holder.Dispose()
        }
        $release.Dispose()
    }
    # Reproduce an upgrade from a setup where both optional components were
    # unchecked. Existing shortcut/verb must actually be removed on upgrade.
    $profile="[Installation]`r`nMode=$mode`r`nLanguage=1036`r`n[Components]`r`nStartMenu=0`r`nContextMenu=0`r`nDefaultFileManager=0`r`n"
    [IO.File]::WriteAllText((Join-Path $directory 'install-info.ini'),$profile,[Text.Encoding]::Unicode)
    [IO.File]::WriteAllText((Join-Path $documentation 'index.html'),'Old documentation to replace.')
    $userDocumentation=Join-Path $documentation 'keep-my-notes.txt'
    [IO.File]::WriteAllText($userDocumentation,'Unowned help notes.')
    # An older package owned assets absent from the replacement package.
    $obsolete=Join-Path $documentation 'retired\old.css'
    $modified=Join-Path $documentation 'retired\edited.css'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $obsolete))
    [IO.File]::WriteAllText($obsolete,'Old managed asset.')
    [IO.File]::WriteAllText($modified,'Original managed asset.')
    $previous=Get-Content -LiteralPath (Join-Path $documentation 'build-info.json') -Raw|ConvertFrom-Json
    foreach($file in @($obsolete,$modified)) {
        $relative=$file.Substring($documentation.Length+1).Replace('\','/')
        $previous.files|Add-Member -NotePropertyName $relative -NotePropertyValue (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    }
    $previous|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $documentation 'build-info.json') -Encoding UTF8
    [IO.File]::WriteAllText($modified,'User-edited obsolete asset.')
    Run-FixtureProcess $installer ("/S /$mode /D=$directory")
    if(Test-Path -LiteralPath $obsolete){throw 'An obsolete managed documentation asset survived upgrade.'}
    if([IO.File]::ReadAllText($modified) -cne 'User-edited obsolete asset.'){throw 'Upgrade changed a modified obsolete asset.'}
    if((Get-FileHash -LiteralPath (Join-Path $documentation 'index.html') -Algorithm SHA256).Hash -ine $manifest.files.'index.html') { throw 'Documentation was not replaced on upgrade.' }
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
    # Run the actual uninstaller from outside its installation directory, with
    # _?= last and unquoted to suppress NSIS's additional temporary launcher.
    # Waiting for that real process also verifies its exit code after help cleanup.
    $uninstaller=Join-Path $RunDirectory 'LifecycleUninstall.exe'
    Copy-Item -LiteralPath (Join-Path $directory 'uninstall.exe') -Destination $uninstaller
    Run-FixtureProcess $uninstaller ('/S _?='+$directory)
    foreach($owned in 'FileSpacer.exe','FileSpacerFolder.dll','uninstall.exe','install-info.ini') {
        if(Test-Path -LiteralPath (Join-Path $directory $owned)) { throw "Owned file remained after uninstallation: $owned" }
    }
    foreach($owned in @($manifest.files.PSObject.Properties.Name)+@('build-info.json')) {
        if(Test-Path -LiteralPath (Join-Path $documentation $owned)) { throw "Owned documentation remained after uninstallation: $owned" }
    }
    if(!(Test-Path -LiteralPath $userDocumentation) -or [IO.File]::ReadAllText($userDocumentation) -cne 'Unowned help notes.') { throw 'Uninstallation removed or changed unowned help notes.' }
    if([IO.File]::ReadAllText($modified) -cne 'User-edited obsolete asset.'){throw 'Uninstallation changed a preserved obsolete asset.'}
    $provider=$hive.OpenSubKey($providerKey)
    if($provider) { $provider.Dispose(); throw 'Owned provider registration remained after uninstallation.' }
    if(Test-Path -LiteralPath $providerFixture) { throw 'Owned provider cache remained after uninstallation.' }
    if(!(Test-Path -LiteralPath $userFile) -or [IO.File]::ReadAllText($userFile) -cne 'Fixture user data to preserve.') { throw 'Uninstallation removed or changed an unowned file.' }
    $installed=$hive.OpenSubKey($registryPrefix+'FileSpacer')
    if($installed) { $installed.Dispose(); throw 'Installation registration remained after uninstallation.' }
    Write-Host "PASS: $mode fixture installation, running-process protection, silent upgrade, component removal and native uninstallation preserving unowned data."

    foreach($scenario in @('invalid-inventory','missing-powershell','script-failure')) {
        $directory=Join-Path $RunDirectory $scenario
        $documentation=Join-Path $directory 'help'
        $warningLog=Join-Path $RunDirectory 'documentation-warning.log'
        if(Test-Path -LiteralPath $warningLog){Remove-Item -LiteralPath $warningLog}
        Run-FixtureProcess $installer ("/S /CurrentUser /D=$directory")
        $inventory=Join-Path $documentation 'build-info.json'
        if($scenario -eq 'invalid-inventory') {
            [IO.File]::WriteAllText($inventory,'{"files":{"../outside.txt":"'+('0'*64)+'"}}')
        } else {
            [IO.File]::WriteAllText((Join-Path $directory ($scenario+'.fixture')),'Inject cleanup failure only in this fixture.')
        }
        [IO.File]::WriteAllText((Join-Path $documentation 'unowned.txt'),'Keep this help note.')
        # An extra byte distinguishes the old payload from the replacement: a
        # failed Upgrade must leave every installed byte unchanged before copy.
        $payloadPath=Join-Path $directory 'FileSpacer.exe'
        $stream=[IO.File]::OpenWrite($payloadPath)
        try {$stream.Seek(0,[IO.SeekOrigin]::End)|Out-Null;$stream.WriteByte(0)}finally{$stream.Dispose()}
        $before=@{}
        foreach($file in Get-ChildItem -LiteralPath $directory -Recurse -File -Force) {
            $before[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
        Run-FixtureProcess $installer ("/S /CurrentUser /UpdateResult=`"$resultFile`" /D=$directory") 2
        if(Test-Path -LiteralPath $resultFile){throw "Failed $scenario upgrade reported success."}
        if(@(Get-ChildItem -LiteralPath $directory -Recurse -File -Force).Count -ne $before.Count){throw "Failed $scenario upgrade changed installed files."}
        foreach($file in $before.Keys) {
            if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -cne $before[$file]) {
                throw "Failed $scenario upgrade changed $file."
            }
        }
        if(Test-Path -LiteralPath $warningLog){throw 'Upgrade used the nonfatal uninstall warning branch.'}
        Copy-Item -LiteralPath (Join-Path $directory 'uninstall.exe') -Destination $uninstaller -Force
        Run-FixtureProcess $uninstaller ('/S _?='+$directory)
        foreach($owned in @('FileSpacer.exe','FileSpacerFolder.dll','uninstall.exe','install-info.ini','LICENSE.txt','LockedList-LICENSE.txt',
            'Lucide-LICENSE.txt','SQLite-NOTICE.txt','THIRD_PARTY_NOTICES.md','privacy-policy.md')) {
            if(Test-Path -LiteralPath (Join-Path $directory $owned)){throw "Cleanup failure blocked program removal: $scenario / $owned"}
        }
        foreach($file in @($before.Keys | Where-Object {$_.StartsWith($documentation+'\',[StringComparison]::OrdinalIgnoreCase)})) {
            if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -cne $before[$file]) {
                throw "Cleanup failure changed preserved help: $scenario / $file"
            }
        }
        if(!(Test-Path -LiteralPath $inventory)){throw 'Cleanup failure removed build-info.json.'}
        $notification=[IO.File]::ReadAllText($warningLog,[Text.Encoding]::Unicode)
        foreach($kind in @('detail','final')) {
            if($notification -notmatch ('(?m)^'+$kind+': .*build-info\.json.*'+[regex]::Escape($directory)+'\\help')) {
                throw "Missing $kind documentation warning for $scenario."
            }
        }
        if(Test-Path -LiteralPath (Join-Path $shortcutDirectory 'FileSpacer.lnk')){throw 'Cleanup failure left the program shortcut.'}
        foreach($key in @('FileSpacer','Microsoft\Windows\CurrentVersion\Uninstall\FileSpacer','Microsoft\Windows\CurrentVersion\App Paths\FileSpacer.exe',
            'Classes\Directory\shell\filespacer')) {
            $remaining=$hive.OpenSubKey($registryPrefix+$key)
            if($remaining){$remaining.Dispose();throw "Cleanup failure left program registration: $key"}
        }
        Write-Host "PASS: $scenario blocks Upgrade before copy; uninstall removes the program, preserves help/inventory and reports both detail and final warnings."
    }
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
