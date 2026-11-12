# Test-Category: UI
# Hidden real Shell views; no input or desktop capture. All data stays in RunDirectory.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,[ValidateSet(129,5000)][int]$ItemCount=129)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$cacheTest=Build-NativeTest -Name SelectionCacheTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib')
$matrix=Build-NativeTest -Name ProductionSelectionTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib')
$cache=Join-Path $RunDirectory 'selection-cache'
[void][IO.Directory]::CreateDirectory($cache)
$code=Invoke-NativeProgram $cacheTest @('--fallback',$cache) (Join-Path $RunDirectory 'missing-cache.log') (Join-Path $RunDirectory 'missing-cache-errors.log')
if($code-ne0){throw 'Missing cache did not select public fallback.'}
$code=Invoke-NativeProgram $cacheTest @('--live',$cache) (Join-Path $RunDirectory 'cache-lifecycle.log') (Join-Path $RunDirectory 'cache-lifecycle-errors.log')
if($code-ne0-and$code-ne50){throw ('Native discovery/qualification/cache lifecycle failed: '+$code)}
$privateSupported=$code-eq0
$data=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin' -File)
if($privateSupported){
if($data.Count-ne1-or$data[0].Length-ne164){throw 'No unique qualified cache.'}
$timestamp=$data[0].LastWriteTimeUtc
$code=Invoke-NativeProgram $cacheTest @('--build-cache',$cache) (Join-Path $RunDirectory 'reuse-cache.log') (Join-Path $RunDirectory 'reuse-cache-errors.log')
if($code-ne0-or(Get-Item -LiteralPath $data[0].FullName).LastWriteTimeUtc-ne$timestamp){throw 'An unchanged DLL recomputed the qualified cache.'}
$negative=Join-Path $RunDirectory 'saved-public';[void][IO.Directory]::CreateDirectory($negative)
[IO.File]::WriteAllText((Join-Path $negative ($data[0].Name+'.public')),'FSPUBLIC01unsupported DLL capability')
$code=Invoke-NativeProgram $cacheTest @('--public-persist',$negative) (Join-Path $RunDirectory 'saved-public.log') (Join-Path $RunDirectory 'saved-public-errors.log')
if($code-ne0-or@(Get-ChildItem -LiteralPath $negative -Filter '*.attempt').Count){throw 'Saved public decision caused rediscovery.'}
$backoff=Join-Path $RunDirectory 'transient-failure';[void][IO.Directory]::CreateDirectory($backoff)
[IO.File]::WriteAllText((Join-Path $backoff ([IO.Path]::GetFileNameWithoutExtension($data[0].Name)+'.attempt')),'network failure')
$code=Invoke-NativeProgram $cacheTest @('--public-persist',$backoff) (Join-Path $RunDirectory 'transient-backoff.log') (Join-Path $RunDirectory 'transient-backoff-errors.log')
if($code-ne0-or(Test-Path -LiteralPath (Join-Path $backoff 'symbols'))){throw 'Transient error backoff did not preserve public operations.'}
$shared=Join-Path $RunDirectory 'shared-discovery';[void][IO.Directory]::CreateDirectory($shared)
Copy-Item -LiteralPath (Join-Path $cache 'symbols') -Destination $shared -Recurse
$processes=@()
try {
    foreach($index in @(1,2)){
        $job=Start-RecordedProcess -Executable $cacheTest -Arguments (Join-ProcessArguments @('--live',$shared)) -OutputPath (Join-Path $RunDirectory ('shared-'+$index+'.log')) -ErrorPath (Join-Path $RunDirectory ('shared-'+$index+'-errors.log'))
        $processes+=$job
    }
    foreach($job in $processes){if(!$job.Process.WaitForExit(180000)){throw 'Concurrent discovery timed out.'};$code=Complete-RecordedProcess $job;if($code-ne0){throw ('Concurrent discovery failed: '+$code)}}
}finally{foreach($job in $processes){if(!$job.Process.HasExited){$job.Process.Kill();$job.Process.WaitForExit()};$job.Process.Dispose()}}
$bad=Join-Path $RunDirectory 'bad-cache';[void][IO.Directory]::CreateDirectory($bad)
$bytes=[IO.File]::ReadAllBytes($data[0].FullName)
$faults=@(@{Name='schema';Offset=7},@{Name='identity';Offset=12},@{Name='callback';Offset=76},@{Name='layout';Offset=116},@{Name='procedure';Offset=132})
if(([BitConverter]::ToUInt32($bytes,132)-band4)-ne0){$faults+=@(@{Name='inline-clear';Offset=36},@{Name='inline-notify';Offset=156},@{Name='inline-erase';Offset=160})}
foreach($fault in $faults){
    $mutated=[byte[]]$bytes.Clone();$mutated[$fault.Offset]=$mutated[$fault.Offset]-bxor1
    [IO.File]::WriteAllBytes((Join-Path $bad $data[0].Name),$mutated)
    $code=Invoke-NativeProgram $cacheTest @('--fallback',$bad) (Join-Path $RunDirectory ('fallback-'+$fault.Name+'.log')) (Join-Path $RunDirectory ('fallback-'+$fault.Name+'-errors.log'))
    if($code-ne0){throw ('Invalid cache accepted: '+$fault.Name)}
}
}else{
    $negative=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin.public' -File)
    if($data.Count-or$negative.Count-ne1-or!(Get-Content $negative[0].FullName -Raw).StartsWith('FSPUBLIC01')){throw 'Private discovery failed without a saved structural fallback decision.'}
    $timestamp=$negative[0].LastWriteTimeUtc
    $code=Invoke-NativeProgram $cacheTest @('--public-persist',$cache) (Join-Path $RunDirectory 'unsupported-public.log') (Join-Path $RunDirectory 'unsupported-public-errors.log')
    if($code-ne0-or(Get-Item $negative[0].FullName).LastWriteTimeUtc-ne$timestamp-or@(Get-ChildItem -LiteralPath $cache -Filter '*.attempt').Count){throw 'Saved structural fallback was not reused without rediscovery.'}
}
. (Join-Path $PSScriptRoot '..\..\..\experiments\reactivity\Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot $ItemCount
try {
    $env:FS_SELECTION_PRODUCT_CACHE=$cache;$env:FS_SELECTION_HEADLESS='1'
    Remove-Item Env:\FS_SELECTION_FIXTURE_64,Env:\FS_SELECTION_FIXTURE_129,Env:\FS_SELECTION_SMOKE,Env:\FS_SELECTION_ONLY_MEDIUM,Env:\FS_SELECTION_ALL_ONLY -ErrorAction SilentlyContinue
    if($ItemCount -eq 129){$env:FS_SELECTION_FIXTURE_129='1'}
    $output=Join-Path $RunDirectory 'production-selection'
    [void][IO.Directory]::CreateDirectory($output)
    $code=Invoke-NativeProgram $matrix @($output,$fixture,'--allow-host') (Join-Path $RunDirectory 'production-selection.log') (Join-Path $RunDirectory 'production-selection-errors.log')
    if($code-ne0){throw ('Product selection matrix failed: '+$code)}
    $rows=@(Import-Csv -LiteralPath (Join-Path $output 'clear-results.csv'))
    $candidate=@($rows|Where-Object {$_.arm-eq'candidate'-and[int]$_.selected_count-gt0})
    $expectedApplied=if($privateSupported){'1'}else{'0'}
    $expectedRows=135
    $expectedCandidates=36
    if($rows.Count-ne$expectedRows-or$candidate.Count-ne$expectedCandidates-or@($candidate|Where-Object private_applied -ne $expectedApplied).Count){throw 'Product selection backend coverage incomplete.'}
    if(!$privateSupported){Write-Host ('PASS: saved structural fallback reused; all '+$expectedRows+' real Shell selection cases verified through public APIs. Private acceleration not exercised.')}
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
