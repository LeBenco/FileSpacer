# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
$temporaryRoot=Join-Path ([IO.Path]::GetTempPath()) ('FileSpacer-processes-'+[Guid]::NewGuid().ToString('N'))
$previousRun=$env:FS_REGRESSION_RUN; $previousTemp=$env:TEMP; $previousLocal=$env:LOCALAPPDATA
$processes=@()
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
    Write-Host 'PASS: all process exits are checked and available diagnostics survive failures.'
} finally {
    $env:FS_REGRESSION_RUN=$previousRun; $env:TEMP=$previousTemp; $env:LOCALAPPDATA=$previousLocal
    foreach ($applicationProcess in $processes) { $applicationProcess.Dispose() }
    if (Test-Path -LiteralPath $temporaryRoot) { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force }
}
