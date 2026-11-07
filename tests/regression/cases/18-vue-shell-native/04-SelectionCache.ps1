# Test-Category: UI
# Hidden real Shell views; no input or desktop capture. All data stays in RunDirectory.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$cacheTest=Build-NativeTest -Name SelectionCacheTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib')
$matrix=Build-NativeTest -Name ProductionSelectionTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib')
$cache=Join-Path $RunDirectory 'selection-cache'
[void][IO.Directory]::CreateDirectory($cache)
& $cacheTest --fallback $cache > (Join-Path $RunDirectory 'missing-cache.log')
if($LASTEXITCODE-ne0){throw 'Missing cache did not select public fallback.'}
& $cacheTest --live $cache > (Join-Path $RunDirectory 'cache-lifecycle.log') 2> (Join-Path $RunDirectory 'cache-lifecycle-errors.log')
if($LASTEXITCODE-ne0){throw ('Native discovery/qualification/cache lifecycle failed: '+$LASTEXITCODE)}
$data=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin' -File)
if($data.Count-ne1-or$data[0].Length-ne140){throw 'No unique qualified cache.'}
$timestamp=$data[0].LastWriteTimeUtc
& $cacheTest --build-cache $cache > (Join-Path $RunDirectory 'reuse-cache.log')
if($LASTEXITCODE-ne0-or(Get-Item -LiteralPath $data[0].FullName).LastWriteTimeUtc-ne$timestamp){throw 'An unchanged DLL recomputed the qualified cache.'}
$negative=Join-Path $RunDirectory 'saved-public';[void][IO.Directory]::CreateDirectory($negative)
[IO.File]::WriteAllText((Join-Path $negative ($data[0].Name+'.public')),'FSPUBLIC01unsupported DLL capability')
& $cacheTest --public-persist $negative > (Join-Path $RunDirectory 'saved-public.log')
if($LASTEXITCODE-ne0-or@(Get-ChildItem -LiteralPath $negative -Filter '*.attempt').Count){throw 'Saved public decision caused rediscovery.'}
$backoff=Join-Path $RunDirectory 'transient-failure';[void][IO.Directory]::CreateDirectory($backoff)
[IO.File]::WriteAllText((Join-Path $backoff ([IO.Path]::GetFileNameWithoutExtension($data[0].Name)+'.attempt')),'network failure')
& $cacheTest --public-persist $backoff > (Join-Path $RunDirectory 'transient-backoff.log')
if($LASTEXITCODE-ne0-or(Test-Path -LiteralPath (Join-Path $backoff 'symbols'))){throw 'Transient error backoff did not preserve public operations.'}
$shared=Join-Path $RunDirectory 'shared-discovery';[void][IO.Directory]::CreateDirectory($shared)
Copy-Item -LiteralPath (Join-Path $cache 'symbols') -Destination $shared -Recurse
$processes=@()
try {
    foreach($index in @(1,2)){
        $process=Start-Process -FilePath $cacheTest -ArgumentList @('--live',('"'+$shared+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $RunDirectory ('shared-'+$index+'.log')) -RedirectStandardError (Join-Path $RunDirectory ('shared-'+$index+'-errors.log'))
        $null=$process.Handle # Retain the original process handle, including after a fast exit.
        $processes+=$process
    }
    foreach($process in $processes){if(!$process.WaitForExit(180000)){throw 'Concurrent discovery timed out.'};$process.WaitForExit();if($process.ExitCode-ne0){throw ('Concurrent discovery failed: '+$process.ExitCode)}}
}finally{foreach($process in $processes){if(!$process.HasExited){$process.Kill();$process.WaitForExit()};$process.Dispose()}}
$bad=Join-Path $RunDirectory 'bad-cache';[void][IO.Directory]::CreateDirectory($bad)
$bytes=[IO.File]::ReadAllBytes($data[0].FullName)
foreach($fault in @(@{Name='identity';Offset=12},@{Name='callback';Offset=76},@{Name='layout';Offset=116},@{Name='procedure';Offset=132})){
    $mutated=[byte[]]$bytes.Clone();$mutated[$fault.Offset]=$mutated[$fault.Offset]-bxor1
    [IO.File]::WriteAllBytes((Join-Path $bad $data[0].Name),$mutated)
    & $cacheTest --fallback $bad > (Join-Path $RunDirectory ('fallback-'+$fault.Name+'.log'))
    if($LASTEXITCODE-ne0){throw ('Invalid cache accepted: '+$fault.Name)}
}
. (Join-Path $ProjectRoot 'tests\experiments\reactivity\Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 5000
try {
    $env:FS_SELECTION_PRODUCT_CACHE=$cache;$env:FS_SELECTION_HEADLESS='1'
    Remove-Item Env:\FS_SELECTION_FIXTURE_64,Env:\FS_SELECTION_SMOKE,Env:\FS_SELECTION_ONLY_MEDIUM -ErrorAction SilentlyContinue
    $output=Join-Path $RunDirectory 'production-selection'
    [void][IO.Directory]::CreateDirectory($output)
    & $matrix $output $fixture --allow-host > (Join-Path $RunDirectory 'production-selection.log') 2> (Join-Path $RunDirectory 'production-selection-errors.log')
    if($LASTEXITCODE-ne0){throw ('Product selection matrix failed: '+$LASTEXITCODE)}
    $rows=@(Import-Csv -LiteralPath (Join-Path $output 'clear-results.csv'))
    $candidate=@($rows|Where-Object {$_.arm-eq'candidate'-and[int]$_.selected_count-gt0})
    if($rows.Count-ne405-or$candidate.Count-ne108-or@($candidate|Where-Object private_applied -ne '1').Count){throw 'Product acceleration coverage incomplete.'}
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
