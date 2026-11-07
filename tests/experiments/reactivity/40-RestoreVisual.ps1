# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$baseline=Build-NativeTest -Name RestoreAllBaselineTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','..\build\restore-all-reference\ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
$candidate=Build-NativeTest -Name RestoreAllTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 5000
$cache=Join-Path $RunDirectory 'cache';[void][IO.Directory]::CreateDirectory($cache)
try {
 $code=Invoke-NativeProgram $candidate @('--build-cache',$cache) (Join-Path $RunDirectory 'discovery.log') (Join-Path $RunDirectory 'discovery-errors.log')
 if($code-ne0){throw ('Discovery preparation failed: '+$code)}
 $failures=@()
 foreach($arm in @(@('baseline',$baseline,'fs-visible'),@('candidate',$candidate,'fs-visible'),@('explorer',$candidate,'explorer-visible'))) {
  $output=Join-Path $RunDirectory $arm[0];[void][IO.Directory]::CreateDirectory($output)
  $code=Invoke-NativeProgram $arm[1] @($output,$fixture,$cache,$arm[2]) (Join-Path $output 'output.log') (Join-Path $output 'errors.log')
  if($code-ne0){$failures+=($arm[0]+' exit='+$code);continue}
  $rows=@(Get-ChildItem -LiteralPath $output -Filter '*-presentation.csv'|ForEach-Object {Import-Csv $_.FullName})
  $expected=if($arm[0]-eq'explorer'){8}else{9}
  if($rows.Count-ne$expected-or@($rows|Where-Object {$_.quality -notin @('OBSERVED','COALESCED_UPPER_BOUND')}).Count){$failures+=($arm[0]+' incomplete or invalid visual observations')}
 }
 if($failures.Count){throw ('Presentation investigation failures: '+($failures-join '; '))}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}