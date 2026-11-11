# Test-Category: UI
# Real Shell views (hidden by default), isolated fixtures/cache, no input injection.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,[switch]$Presented,[ValidateSet(129,5000)][int]$ItemCount=129)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$cacheTest=Build-NativeTest -Name SelectionCacheTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib')
$matrix=Build-NativeTest -Name ProductionSelectionTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib')
$cache=Join-Path $RunDirectory 'selection-cache';[void][IO.Directory]::CreateDirectory($cache)
$code=Invoke-NativeProgram $cacheTest @('--live',$cache) (Join-Path $RunDirectory 'discovery.log') (Join-Path $RunDirectory 'discovery-errors.log')
if($code-ne0-and$code-ne50){throw 'Global-selection discovery/qualification failed.'}
$privateSupported=$code-eq0
$records=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin' -File)
if($privateSupported-and($records.Count-ne1-or$records[0].Length-ne160)){throw 'Qualified selection descriptor absent.'}
$fallback=Join-Path $RunDirectory 'without-select';[void][IO.Directory]::CreateDirectory($fallback)
if($privateSupported){
$bytes=[IO.File]::ReadAllBytes($records[0].FullName)
[Array]::Clear($bytes,140,20)
[IO.File]::WriteAllBytes((Join-Path $fallback $records[0].Name),$bytes)
}else{
 $negative=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin.public' -File)
 if($records.Count-or$negative.Count-ne1-or!(Get-Content $negative[0].FullName -Raw).StartsWith('FSPUBLIC01')){throw 'Unsupported discovery did not persist its structural fallback decision.'}
 $code=Invoke-NativeProgram $cacheTest @('--public-persist',$cache) (Join-Path $RunDirectory 'public-reuse.log') (Join-Path $RunDirectory 'public-reuse-errors.log')
 if($code-ne0-or@(Get-ChildItem -LiteralPath $cache -Filter '*.attempt').Count){throw 'Persisted public fallback was not reused.'}
}
. (Join-Path $PSScriptRoot '..\..\..\experiments\reactivity\Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot $ItemCount
try {
 $env:FS_SELECTION_ALL_ONLY='1';$env:FS_SELECTION_HEADLESS='1'
 if($Presented){Remove-Item Env:\FS_SELECTION_HEADLESS -ErrorAction SilentlyContinue}
 Remove-Item Env:\FS_SELECTION_FIXTURE_64,Env:\FS_SELECTION_FIXTURE_129,Env:\FS_SELECTION_SMOKE,Env:\FS_SELECTION_ONLY_MEDIUM -ErrorAction SilentlyContinue
 if($ItemCount -eq 129){$env:FS_SELECTION_FIXTURE_129='1'}
 $arms=@(if($privateSupported){@(@('qualified',$cache),@('missing-select-capability',$fallback))}else{,@('unsupported-private',$cache)})
 foreach($arm in $arms){
  $output=Join-Path $RunDirectory $arm[0];[void][IO.Directory]::CreateDirectory($output)
  $env:FS_SELECTION_PRODUCT_CACHE=$arm[1]
  $log=Join-Path $output 'selection.log'
  $code=Invoke-NativeProgram $matrix @($output,$fixture,'--allow-host') $log (Join-Path $output 'errors.log')
  if($code-ne0){throw ('Global-selection matrix failed: '+$arm[0])}
  if(@(Select-String -LiteralPath $log -Pattern '^SELECT_ALL ').Count-ne9){throw 'Global-selection representation coverage incomplete.'}
  if(@(Select-String -LiteralPath $log -Pattern '^PARTIAL_ALL ').Count-ne27){throw 'Partial-to-all representation coverage incomplete.'}
  if(!$privateSupported-and@(Select-String -LiteralPath $log -Pattern 'private_applied=1').Count){throw 'Unsupported private path was used.'}
 }
 if(!$privateSupported){Write-Host 'PASS: structurally unsupported private path; persisted public fallback verified in all nine real Shell representations. Private acceleration not exercised.'}
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
