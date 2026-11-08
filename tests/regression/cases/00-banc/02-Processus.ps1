# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
$temporaryRoot=Join-Path $ProjectRoot ('build\processes-'+[Guid]::NewGuid().ToString('N'))
$previousRun=$env:FS_REGRESSION_RUN; $previousTemp=$env:TEMP; $previousLocal=$env:LOCALAPPDATA
$processes=@()
$timeoutJob=$null;$timeoutChild=$null
try {
    [void][IO.Directory]::CreateDirectory($temporaryRoot)
    $env:FS_REGRESSION_RUN=$temporaryRoot
    $env:TEMP=$temporaryRoot
    $env:LOCALAPPDATA=$temporaryRoot
    $hostExecutable=Join-Path $PSHOME $(if ($env:OS -eq 'Windows_NT') {'powershell.exe'} else {'pwsh'})
    foreach ($code in @(0,7,9)) {
        if ($code -eq 0) {
            $processes+=Start-TestApplication -Executable $hostExecutable -Arguments ('-NoProfile -Command "exit '+$code+'"')
        } else {
            # Both pipes exceed their buffers: sequential reads would deadlock.
            $arguments='-NoProfile -Command "[Console]::Out.Write((''stdout-'+$code+''' * 20000)); [Console]::Error.Write((''stderr-'+$code+''' * 20000)); exit '+$code+'"'
            $processes+=Start-TestApplication -Executable $hostExecutable -Arguments $arguments -OutputDirectory $temporaryRoot
        }
    }
    if (('0x{0:X8}' -f [int]-1073740791) -ne '0xC0000409' -or ('0x{0:X8}' -f [int]-1073741819) -ne '0xC0000005') {
        throw 'Crash exit-code formatting is incorrect.'
    }
    $applicationIds=@($processes | ForEach-Object { $_.Id })
    $traceName='FileSpacer-selection-initialisation-'+$applicationIds[1]+'.log'
    [IO.File]::WriteAllText((Join-Path $temporaryRoot $traceName),'trace fixture')
    $searchName='FileSpacer-search-'+$applicationIds[1]+'.log'
    [IO.File]::WriteAllText((Join-Path $temporaryRoot $searchName),'search fixture')
    $recoveryRoot=Join-Path $temporaryRoot 'FileSpacer'
    [void][IO.Directory]::CreateDirectory($recoveryRoot)
    $reportName='recovery-test-'+$applicationIds[2]+'.txt'
    [IO.File]::WriteAllText((Join-Path $recoveryRoot $reportName),'recovery fixture')
    $staleName='recovery-test-'+$applicationIds[0]+'.txt'
    $stalePath=Join-Path $recoveryRoot $staleName
    [IO.File]::WriteAllText($stalePath,'stale fixture')
    [IO.File]::SetLastWriteTimeUtc($stalePath,[DateTime]::UtcNow.AddDays(-1))
    $failure=$null
    try { Complete-TestApplications $processes }
    catch { $failure=$_.Exception.Message }
    if (!$failure -or !$failure.Contains('0x00000007') -or !$failure.Contains('0x00000009')) {
        throw 'Not all application exit failures were reported.'
    }
    $records=@((Get-Content -LiteralPath (Join-Path $temporaryRoot 'application-exits.json') -Raw | ConvertFrom-Json))
    if ($records.Count -ne 3 -or ($records.ExitCode -join ',') -ne '0,7,9') { throw 'Missing application exit records.' }
    foreach ($index in @(1,2)) {
        $code=@(0,7,9)[$index]
        $prefix=Join-Path $temporaryRoot ('application-'+$applicationIds[$index])
        if ([IO.File]::ReadAllText($prefix+'.stdout.log') -cne (('stdout-'+$code)*20000)) { throw 'Application stdout was lost or truncated.' }
        if ([IO.File]::ReadAllText($prefix+'.stderr.log') -cne (('stderr-'+$code)*20000)) { throw 'Application stderr was lost or truncated.' }
    }
    if (!(Test-Path -LiteralPath (Join-Path $temporaryRoot ('application-traces\'+$traceName)))) { throw 'Missing application trace.' }
    if (!(Test-Path -LiteralPath (Join-Path $temporaryRoot ('application-traces\'+$searchName)))) { throw 'Missing name-search trace.' }
    if (!(Test-Path -LiteralPath (Join-Path $temporaryRoot ('application-recovery\'+$reportName)))) { throw 'Missing application recovery report.' }
    if (Test-Path -LiteralPath (Join-Path $temporaryRoot ('application-recovery\'+$staleName))) { throw 'A stale recovery report was collected.' }
    $tracked=@((Get-Content -LiteralPath (Join-Path $temporaryRoot 'application-processes.json') -Raw | ConvertFrom-Json))
    if ($tracked.Count -ne 3) { throw 'Not all started processes were tracked.' }
    $childScript=Join-Path $temporaryRoot 'child.ps1'
    $parentScript=Join-Path $temporaryRoot 'parent.ps1'
    [IO.File]::WriteAllText($childScript,@'
param([string]$Root)
[IO.File]::WriteAllText((Join-Path $Root 'child.ready'),'ready')
while(!(Test-Path -LiteralPath (Join-Path $Root 'release.child'))){Start-Sleep -Milliseconds 50}
exit 0
'@)
    [IO.File]::WriteAllText($parentScript,@'
param([string]$Root)
$child=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList ('-NoProfile -File "'+(Join-Path $Root 'child.ps1')+'" -Root "'+$Root+'"') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $Root 'child.stdout') -RedirectStandardError (Join-Path $Root 'child.stderr')
[IO.File]::WriteAllText((Join-Path $Root 'child.id'),[string]$child.Id)
while(!(Test-Path -LiteralPath (Join-Path $Root 'release.parent'))){Start-Sleep -Milliseconds 50}
'@)
    $timeoutJob=Start-RecordedProcess -Executable $hostExecutable -Arguments (Join-ProcessArguments @('-NoProfile','-File',$parentScript,'-Root',$temporaryRoot)) -OutputPath (Join-Path $temporaryRoot 'parent.stdout') -ErrorPath (Join-Path $temporaryRoot 'parent.stderr')
    $readyDeadline=[DateTime]::UtcNow.AddSeconds(10)
    while(!(Test-Path -LiteralPath (Join-Path $temporaryRoot 'child.ready'))-or!(Test-Path -LiteralPath (Join-Path $temporaryRoot 'child.id'))){
        if($timeoutJob.Process.HasExited-or[DateTime]::UtcNow-ge$readyDeadline){throw 'Timeout fixture did not create its child.'}
        Start-Sleep -Milliseconds 50
    }
    $timeoutChild=Get-Process -Id ([int][IO.File]::ReadAllText((Join-Path $temporaryRoot 'child.id')))
    $null=$timeoutChild.Handle
    Stop-TestCaseProcess $timeoutJob.Process
    if($timeoutChild.HasExited){throw 'Stopping the scenario also terminated its application child.'}
    [IO.File]::WriteAllText((Join-Path $temporaryRoot 'release.child'),'exit normally')
    if(!$timeoutChild.WaitForExit(10000)-or$timeoutChild.ExitCode-ne0){throw 'The preserved application child did not exit normally.'}
    if((Complete-RecordedProcess $timeoutJob)-eq0){throw 'The timed-out scenario unexpectedly succeeded.'}
    Write-Host 'PASS: timeout stops only the scenario; its application child survives and exits normally.'
    Write-Host 'PASS: all process exits are checked and available diagnostics survive failures.'
} finally {
    foreach($name in @('release.child','release.parent')){if(Test-Path -LiteralPath $temporaryRoot){[IO.File]::WriteAllText((Join-Path $temporaryRoot $name),'exit normally')}}
    foreach($owned in @($timeoutChild,$(if($timeoutJob){$timeoutJob.Process}else{$null}))){if($null-ne$owned){[void]$owned.WaitForExit(10000);$owned.Dispose()}}
    $env:FS_REGRESSION_RUN=$previousRun; $env:TEMP=$previousTemp; $env:LOCALAPPDATA=$previousLocal
    foreach ($applicationProcess in $processes) { $applicationProcess.Dispose() }
    if (Test-Path -LiteralPath $temporaryRoot) {
        $allowed=[IO.Path]::GetFullPath((Join-Path $ProjectRoot 'build')).TrimEnd('\')+'\'
        $target=[IO.Path]::GetFullPath($temporaryRoot)
        if(!$target.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)-or((Get-Item -LiteralPath $target).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw 'Unsafe process fixture cleanup path.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
