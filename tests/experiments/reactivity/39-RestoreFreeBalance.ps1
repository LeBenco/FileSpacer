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
 for($cycle=0;$cycle-lt3;++$cycle) {
  $arms=if($cycle%2){@(@('candidate',$candidate),@('baseline',$baseline))}else{@(@('baseline',$baseline),@('candidate',$candidate))}
  foreach($arm in $arms) {
   $output=Join-Path $RunDirectory ($arm[0]+'-'+$cycle);[void][IO.Directory]::CreateDirectory($output)
   $code=Invoke-NativeProgram $arm[1] @($output,$fixture,$cache,'fs-free') (Join-Path $output 'output.log') (Join-Path $output 'errors.log')
   if($code-ne0){$failures+=($arm[0]+' cycle='+$cycle+' exit='+$code)}
   elseif(@(Import-Csv -LiteralPath (Join-Path $output 'restore-all.csv')).Count-ne6){throw 'Incomplete free-placement matrix.'}
  }
 }
 if($failures.Count){throw ('Free-placement investigation failures: '+($failures-join '; '))}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}