param(
    [string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [ValidateSet('All','Internal','UI')][string]$Category='All',
    [string]$Pattern='*',
    [ValidateRange(30,3600)][int]$TimeoutSeconds=300,
    [string]$RunDirectory='',
    [string]$VM='',
    [switch]$Prepared
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
. (Join-Path $PSScriptRoot 'common\Process.ps1')
. (Join-Path $PSScriptRoot 'common\Results.ps1')
if ($env:OS -ne 'Windows_NT' -or ![Environment]::Is64BitProcess) { throw 'Windows x64 is required.' }
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$caseRoot=Join-Path $PSScriptRoot 'cases'
$preparedSelection=$null
if ($Prepared) { $preparedSelection=Get-Content -LiteralPath (Join-Path $ProjectRoot 'selection.json') -Raw | ConvertFrom-Json }
$tests=@(Get-ChildItem -LiteralPath $caseRoot -Recurse -File -Filter '*.ps1' | Where-Object { $_.Name -match '^\d{2}-.+\.ps1$' } | Sort-Object FullName)
$selected=@()
foreach ($test in $tests) {
    $relative=$test.FullName.Substring($caseRoot.Length+1)
    if ($Prepared -and $relative -notin $preparedSelection.Cases) { continue }
    if ($relative -notlike $Pattern) { continue }
    $first=Get-Content -LiteralPath $test.FullName -TotalCount 1
    if ($first -notmatch '^# Test-Category: (Internal|UI)$') { throw "Missing test category: $relative" }
    $kind=$Matches[1]
    if ($Category -eq 'All' -or $Category -eq $kind) { $selected+=@{ File=$test; Relative=$relative; Category=$kind } }
}
if (!$selected.Count) { throw 'No tests matched the requested category/pattern.' }
if ($Prepared -and $selected.Count -ne @($preparedSelection.Cases).Count) { throw 'Prepared selection differs from the selected guest cases.' }
if ($Prepared) { $TimeoutSeconds=[int]$preparedSelection.TimeoutSeconds }
if ($VM) {
    if ($Prepared) { throw 'A prepared guest run cannot dispatch another VM.' }
    . (Join-Path $PSScriptRoot 'common\Prepare-Client.ps1')
    Invoke-ClientRegression -VM $VM -Selected $selected -TimeoutSeconds $TimeoutSeconds -RunDirectory $RunDirectory
    exit $LASTEXITCODE
}
if ($Prepared) { . (Join-Path $PSScriptRoot 'common\Prepared.ps1') }
elseif (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Run tests.cmd or use an x64 MSVC developer prompt.' }
$ui=@($selected | Where-Object { $_.Category -eq 'UI' }).Count -gt 0
if ($ui -and ![Environment]::UserInteractive) { throw 'UI tests require an interactive Windows desktop.' }
if ($ui -and @(Get-Process -Name FileSpacer -ErrorAction SilentlyContinue).Count) { throw 'Close existing FileSpacer processes before running UI tests.' }
if ($ui -and !(Test-Path -LiteralPath (Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'))) { throw 'Build the Debug application with build.cmd debug.' }

if ($RunDirectory) {
    $run=[IO.Path]::GetFullPath($RunDirectory)
    if (Test-Path -LiteralPath $run) { throw 'The requested result directory already exists; previous results will not be overwritten.' }
} else { $run=Join-Path $ProjectRoot ('build\regression\'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N')) }
[void][IO.Directory]::CreateDirectory($run)
$results=New-Object Collections.Generic.List[object]
$testKey='Software\FileSpacer\Test'
$database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
$backup=Join-Path $run 'saved-test-state'
$registryPrepared=$false; $dbPrepared=$false; $dbComplete=$false; $fatal=$null
$savedRegistry=$null; $movedFiles=New-Object Collections.Generic.List[string]
$originalFiles=@{}
$desktopActivity=$null

function Read-RegistryTree([Microsoft.Win32.RegistryKey]$Key) {
    $values=@(); $children=@()
    foreach ($name in $Key.GetValueNames()) {
        $values+=@{Name=$name; Kind=$Key.GetValueKind($name); Value=$Key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}
    }
    foreach ($name in $Key.GetSubKeyNames()) {
        $child=$Key.OpenSubKey($name)
        try { $children+=@{Name=$name; Tree=(Read-RegistryTree $child)} } finally { $child.Dispose() }
    }
    return @{Values=$values; Children=$children}
}
function Write-RegistryTree([Microsoft.Win32.RegistryKey]$Key,$Tree) {
    foreach ($entry in $Tree.Values) { $Key.SetValue($entry.Name,$entry.Value,$entry.Kind) }
    foreach ($entry in $Tree.Children) {
        $child=$Key.CreateSubKey($entry.Name)
        try { Write-RegistryTree $child $entry.Tree } finally { $child.Dispose() }
    }
}
function Initialize-TestPreferences {
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($testKey,$false)
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($testKey)
    try {
        foreach ($name in @('UpdateCheckEnabled','AdminWarning')) { $key.SetValue($name,0,[Microsoft.Win32.RegistryValueKind]::DWord) }
        $key.SetValue('LastOpenedVersion',2147483647,[Microsoft.Win32.RegistryValueKind]::DWord)
        $key.SetValue('KeepSourceWindowOpen',1,[Microsoft.Win32.RegistryValueKind]::DWord)
    } finally { $key.Dispose() }
}
function Stop-TestApplication {
    # Preflight requires no existing FileSpacer. Clean up only the exact Debug
    # executable under test, including its crash-recovery helper processes.
    $image=Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
    # A helper waits on its parent's exit handle. The main process can have
    # exited successfully before Windows schedules that helper's final exit.
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
        $owned=@(Get-Process -Name FileSpacer -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $image })
        if (!$owned.Count) { return }
        if ([DateTime]::UtcNow -ge $deadline) { break }
        Start-Sleep -Milliseconds 100
    } while ($true)
    # Record persistent leftovers before requesting their closure.
    $diagnostics=@()
    foreach ($process in $owned) {
        $entry=[ordered]@{ProcessId=$process.Id; Image=$image; ParentProcessId=$null; CommandLine=$null; QueryError=$null}
        try {
            $details=Get-CimInstance -ClassName Win32_Process -Filter ('ProcessId='+$process.Id) -ErrorAction Stop
            if ($details) { $entry.ParentProcessId=$details.ParentProcessId; $entry.CommandLine=$details.CommandLine }
        } catch { $entry.QueryError=$_.Exception.Message }
        $diagnostics+=[pscustomobject]$entry
    }
    $diagnosticRoot=$run
    if ($env:FS_REGRESSION_RUN) { $diagnosticRoot=$env:FS_REGRESSION_RUN }
    try {
        ConvertTo-Json -InputObject $diagnostics -Depth 4 | Set-Content -LiteralPath (Join-Path $diagnosticRoot 'leftover-processes.json') -Encoding UTF8
    } catch { Write-Warning ('Cannot record leftover processes: '+$_.Exception.Message) }
    $cleanupFailures=New-Object Collections.Generic.List[string]
    foreach ($process in $owned) {
        try {
            if (!$process.HasExited) {
                [void]$process.CloseMainWindow()
                if (!$process.WaitForExit(5000)) { $process.Kill(); [void]$process.WaitForExit(5000) }
            }
        } catch { $cleanupFailures.Add($_.Exception.Message) }
        finally { $process.Dispose() }
    }
    $reason="Test left $($owned.Count) FileSpacer process(es) running after the exit grace period; cleanup attempted."
    if ($cleanupFailures.Count) { $reason+=' '+($cleanupFailures -join '; ') }
    throw $reason
}
try {
    if ($ui) {
        Add-Type -Path (Join-Path $PSScriptRoot 'common\DesktopActivity.cs')
        $desktopActivity=New-Object FileSpacerRegression.DesktopActivity
        @{OriginalScreenSaverActive=$desktopActivity.OriginalScreenSaverActive; Suspended=$true} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'desktop-activity.json') -Encoding UTF8
        [void][IO.Directory]::CreateDirectory($backup)
        $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($testKey)
        if ($key) {
            try { $savedRegistry=Read-RegistryTree $key } finally { $key.Dispose() }
            $savedRegistry | Export-Clixml -LiteralPath (Join-Path $backup 'registry.xml')
        }
        $registryPrepared=$true
        Initialize-TestPreferences
        [void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
        foreach ($suffix in @('','-wal','-shm')) { $originalFiles[$suffix]=Test-Path -LiteralPath ($database+$suffix) }
        $dbPrepared=$true
        foreach ($suffix in @('','-wal','-shm')) {
            $file=$database+$suffix
            if ($originalFiles[$suffix]) {
                Move-Item -LiteralPath $file -Destination (Join-Path $backup ([IO.Path]::GetFileName($file)))
                $movedFiles.Add($suffix)
            }
        }
        $dbComplete=$true
    }
    foreach ($entry in $selected) {
        $relative=$entry.Relative
        Write-Host "RUN  $relative"
        @{Expected=$selected.Count;Completed=$results.Count;Current=$relative} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $run 'progress.json') -Encoding UTF8
        $relativeDirectory=Join-Path ([IO.Path]::GetDirectoryName($relative)) ([IO.Path]::GetFileNameWithoutExtension($relative))
        $testRun=Join-Path $run $relativeDirectory
        [void][IO.Directory]::CreateDirectory($testRun)
        $env:FS_REGRESSION_RUN=$testRun
        $log=Join-Path $testRun 'output.log'; $errors=Join-Path $testRun 'errors.log'
        $started=Get-Date; $exitCode=-1; $status='FAIL'; $detail=''; $cleanupDetail=''; $collectionDetail=''
        if ($entry.Category -eq 'UI') {
            # Cases are independent. Retaining previous fixtures' taskbar identities
            # makes each Shell CommitList rewrite an ever-growing set of old lists.
            $jumpLists=$testKey+'\JumpLists'
            $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($jumpLists)
            $previousIdentities=0
            try { if ($key) { $previousIdentities=$key.SubKeyCount } }
            finally { if ($key) { $key.Dispose() } }
            # A preceding case may legitimately apply other gestures or enable
            # Everything. Each UI case gets the same preferences as a lone run.
            Initialize-TestPreferences
            @{RemovedJumpListIdentities=$previousIdentities;Key=$jumpLists;PreferencesReset=$true} | ConvertTo-Json |
                Set-Content -LiteralPath (Join-Path $testRun 'case-fixture.json') -Encoding UTF8
        }
        # Separate PowerShell processes prevent Add-Type and global collisions.
        $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$entry.File.FullName+'" -ProjectRoot "'+$ProjectRoot+'"'
        if ($Prepared) {
            $env:FS_REGRESSION_CASE=$relative
            $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $PSScriptRoot 'common\Prepared-Case.ps1')+'" -ProjectRoot "'+$ProjectRoot+'" -Case "'+$relative+'"'
        }
        $job=Start-RecordedProcess -Executable (Join-Path $PSHOME 'powershell.exe') -Arguments $arguments -OutputPath $log -ErrorPath $errors
        $process=$job.Process
        try {
            if (!$process.WaitForExit($TimeoutSeconds*1000)) {
                & taskkill.exe /PID $process.Id /T /F | Out-Host
                if (!$process.WaitForExit(5000)) { $process.Kill(); $process.WaitForExit() }
                $detail='Timeout'
            } else {
                $exitCode=[int]$process.ExitCode
                if ($exitCode -eq 0) { $status='PASS' }
            }
        } catch { $status='FAIL'; $detail=$_.Exception.Message }
        if ($entry.Category -eq 'UI') {
            try { Stop-TestApplication } catch { $status='FAIL'; $cleanupDetail=$_.Exception.Message }
            $trackingPath=Join-Path $testRun 'application-processes.json'
            if (Test-Path -LiteralPath $trackingPath) {
                try {
                    foreach ($tracked in @((Get-Content -LiteralPath $trackingPath -Raw | ConvertFrom-Json))) {
                        Save-TestApplicationDiagnostics $tracked.ProcessId ([datetime]$tracked.StartedUtc)
                    }
                } catch { Write-Warning ('Cannot collect tracked application diagnostics: '+$_.Exception.Message) }
            }
            if ($status -eq 'FAIL') {
                $state=Join-Path $testRun 'application-state'
                [void][IO.Directory]::CreateDirectory($state)
                foreach ($suffix in @('','-wal','-shm')) {
                    if (Test-Path -LiteralPath ($database+$suffix)) { Copy-Item -LiteralPath ($database+$suffix) -Destination $state }
                }
            }
        }
        try {
            $exitCode=Complete-RecordedProcess $job
        } catch { $status='FAIL'; $collectionDetail=$_.Exception.Message }
        finally { $process.Dispose() }
        $cleanupDetail=(@($cleanupDetail,$collectionDetail) | Where-Object { $_ }) -join '; '
        $outcome=Get-RegressionOutcome $status $exitCode $detail $errors $testRun $cleanupDetail
        $status=$outcome.Status; $detail=$outcome.Detail
        $seconds=((Get-Date)-$started).TotalSeconds
        $results.Add([pscustomobject]@{Test=$relative; Category=$entry.Category; Status=$status; ExitCode=$exitCode; Seconds=[Math]::Round($seconds,2); Detail=$detail; Log=$log; ErrorLog=$errors})
        ConvertTo-Json -InputObject $results -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'results.json') -Encoding UTF8
        Write-Host "$status $relative ($([Math]::Round($seconds,1)) s)"
        if ($detail) { Write-Warning "$relative : $detail" }
        if ($entry.Category -eq 'UI') {
            $image=Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
            $remaining=@(Get-Process -Name FileSpacer -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $image })
            if ($remaining.Count) {
                throw "$relative : cannot isolate the next test; $($remaining.Count) FileSpacer process(es) still running."
            }
        }
    }
} catch { $fatal=$_.Exception.Message; Write-Warning $fatal }
finally {
    Remove-Item Env:\FS_REGRESSION_RUN -ErrorAction SilentlyContinue
    if ($ui) {
        try { Stop-TestApplication } catch { if (!$fatal) { $fatal=$_.Exception.Message } }
        if ($dbPrepared) {
            foreach ($suffix in @('','-wal','-shm')) {
                try {
                    $file=$database+$suffix
                    $saved=Join-Path $backup ([IO.Path]::GetFileName($file))
                    if ($dbComplete -or $movedFiles.Contains($suffix)) {
                        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
                    }
                    if ($movedFiles.Contains($suffix)) { Move-Item -LiteralPath $saved -Destination $file }
                } catch { $fatal="Restore test database: $($_.Exception.Message)"; Write-Warning $fatal }
            }
        }
        if ($registryPrepared) {
            try {
                [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($testKey,$false)
                if ($savedRegistry) {
                    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($testKey)
                    try { Write-RegistryTree $key $savedRegistry } finally { $key.Dispose() }
                }
            } catch { $fatal="Restore test registry: $($_.Exception.Message)"; Write-Warning $fatal }
        }
        if ($desktopActivity) {
            try {
                $desktopActivity.Dispose()
                @{OriginalScreenSaverActive=$desktopActivity.OriginalScreenSaverActive; Restored=$true} |
                    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'desktop-activity.json') -Encoding UTF8
            } catch {
                $restoreError="Restore desktop activity: $($_.Exception.Message)"
                if ($fatal) { $fatal+='; '+$restoreError } else { $fatal=$restoreError }
                Write-Warning $restoreError
            }
        }
    }
}
$summary=Get-RegressionSummary @($results.ToArray()) $selected.Count $fatal
ConvertTo-Json -InputObject $results -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'results.json') -Encoding UTF8
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'summary.json') -Encoding UTF8
Show-RegressionSummary @($results.ToArray()) $summary $run
if ($summary.Status -ne 'PASS') { exit 1 }
exit 0
