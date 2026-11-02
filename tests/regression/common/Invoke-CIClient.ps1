# Run prepared UI tests with the interactive user's limited token, like the VM.
param([Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$RunDirectory,
    [switch]$Worker,[int]$SessionId=-1)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
if($env:GITHUB_ACTIONS-ne'true' -and !$Worker){throw 'CI dispatch is reserved for GitHub Actions.'}
$Package=[IO.Path]::GetFullPath($Package)
$RunDirectory=[IO.Path]::GetFullPath($RunDirectory)
$resultPath=Join-Path $RunDirectory 'worker-result.json'
if($Worker){
    $record=[ordered]@{Status='FAIL';SessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId;Administrator=$null;RunnerExitCode=$null;Error=$null}
    $job=$null;$dragOriginal=$false;$dragPrepared=$false
    try {
        $principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
        $record.Administrator=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
        if($record.Administrator -or $record.SessionId-ne$SessionId -or ![Environment]::UserInteractive){throw 'Expected the same interactive desktop with a limited token.'}
        $env:TEMP=Join-Path $RunDirectory 'temp';$env:TMP=$env:TEMP
        [void][IO.Directory]::CreateDirectory($env:TEMP)
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CIDesktop {
    [DllImport("user32.dll", EntryPoint="SystemParametersInfoW", SetLastError=true)]
    public static extern bool Read(uint action,uint parameter,out bool value,uint flags);
    [DllImport("user32.dll", EntryPoint="SystemParametersInfoW", SetLastError=true)]
    public static extern bool Write(uint action,uint parameter,IntPtr value,uint flags);
}
'@
        $selection=Get-Content (Join-Path $Package 'selection.json') -Raw|ConvertFrom-Json
        $ui=@($selection.Cases|Where-Object {
            (Get-Content (Join-Path $Package ('tests\regression\cases\'+$_)) -TotalCount 1) -eq '# Test-Category: UI'
        }).Count-gt0
        if($ui){
            if(![CIDesktop]::Read(0x26,0,[ref]$dragOriginal,0)){throw 'Cannot read full-window drag setting.'}
            $dragPrepared=$true
            if(![CIDesktop]::Write(0x25,1,[IntPtr]::Zero,2)){throw 'Cannot enable full-window dragging on the CI desktop.'}
        }
        $packageFiles=Get-Content (Join-Path $Package 'files.json') -Raw | ConvertFrom-Json
        foreach($entry in $packageFiles){
            $file=[IO.Path]::GetFullPath((Join-Path $Package $entry.Path))
            if(!$file.StartsWith($Package+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash $file -Algorithm SHA256).Hash-ne$entry.Sha256){throw 'Prepared package path/hash mismatch.'}
        }
        . (Join-Path $Package 'tests\regression\common\Process.ps1')
        $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $Package 'tests\regression\Run-Regression.ps1')+'" -ProjectRoot "'+$Package+'" -Prepared -RunDirectory "'+(Join-Path $RunDirectory 'output')+'"'
        $job=Start-RecordedProcess -Executable (Join-Path $PSHOME 'powershell.exe') -Arguments $arguments -OutputPath (Join-Path $RunDirectory 'runner.stdout.log') -ErrorPath (Join-Path $RunDirectory 'runner.stderr.log')
        $job.Process.WaitForExit()
        $record.RunnerExitCode=Complete-RecordedProcess $job
        if($record.RunnerExitCode-eq0){$record.Status='PASS'}
    } catch {$record.Error=$_.Exception.Message}
    finally {
        if($job){$job.Process.Dispose()}
        if($dragPrepared -and ![CIDesktop]::Write(0x25,[uint32]$dragOriginal,[IntPtr]::Zero,2)){$record.Status='FAIL';$record.Error='Cannot restore full-window drag setting.'}
        $record|ConvertTo-Json|Set-Content $resultPath -Encoding UTF8
    }
    if($record.Status-ne'PASS'){exit 1};exit 0
}
# Build the launcher outside the prepared package; it only starts the worker.
Add-Type -Path (Join-Path $PSScriptRoot 'CIRestrictedProcess.cs')
$session=[Diagnostics.Process]::GetCurrentProcess().SessionId
$arguments='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+$PSCommandPath+'" -Worker -Package "'+$Package+'" -RunDirectory "'+$RunDirectory+'" -SessionId '+$session
$exitCode=$null
$launchStarted=Get-Date
try {
    $exitCode=[FileSpacerRegression.CIRestrictedProcess]::Run((Join-Path $PSHOME 'powershell.exe'),$arguments,$Package,169*60*1000,$true)
    if(!(Test-Path $resultPath)){throw ('CI worker produced no result; exit={0} (0x{0:X8})' -f $exitCode)}
    $record=Get-Content $resultPath -Raw|ConvertFrom-Json
    if($exitCode-ne0 -or $record.Status-ne'PASS' -or $record.RunnerExitCode-ne0 -or $record.Administrator -or $record.SessionId-ne$session){throw ('CI worker failed: '+($record|ConvertTo-Json -Compress))}
} catch {
    $launchError=$_;$missingResult=!(Test-Path $resultPath)
    if(Test-Path $resultPath){$record=Get-Content $resultPath -Raw|ConvertFrom-Json}
    else {$record=[pscustomobject]@{Status='FAIL';SessionId=$null;Administrator=$null;RunnerExitCode=$exitCode;Error=$null}}
    $record.Status='FAIL';$record.Error=$launchError.Exception.Message
    $record|ConvertTo-Json|Set-Content $resultPath -Encoding UTF8
    if($missingResult){
        try {
            $events=@(Get-WinEvent -FilterHashtable @{LogName='Application';StartTime=$launchStarted;Level=1,2,3} -ErrorAction SilentlyContinue | Select-Object -First 5)
            foreach($event in $events){
                $message=$event.Message -replace '\s+',' ';if($message.Length-gt400){$message=$message.Substring(0,400)+'...'}
                Write-Host ('Windows event {0}/{1}: {2}' -f $event.ProviderName,$event.Id,$message)
            }
        } catch {Write-Host ('Windows event diagnostics unavailable: '+$_.Exception.Message)}
    }
    throw $launchError
} finally {
    foreach($name in @('runner.stdout.log','runner.stderr.log')){
        $log=Join-Path $RunDirectory $name
        if(Test-Path $log){Get-Content $log -Tail 55|Write-Host}
    }
}
exit 0
