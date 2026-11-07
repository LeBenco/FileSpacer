# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$candidate=Build-NativeTest -Name LayoutPathTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 5000
$cache=Join-Path $RunDirectory 'cache';[void][IO.Directory]::CreateDirectory($cache)
try {
 $code=Invoke-NativeProgram $candidate @('--build-cache',$cache) (Join-Path $RunDirectory 'discovery.log') (Join-Path $RunDirectory 'discovery-errors.log')
 if($code-ne0){throw ('Discovery preparation failed: '+$code)}
 $code=Invoke-NativeProgram $candidate @($RunDirectory,$fixture,$cache) (Join-Path $RunDirectory 'native-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Layout path investigation failed: '+$code)}
 $log=Join-Path $RunDirectory 'native-output.log'
 if(@(Select-String -LiteralPath $log -Pattern '^TIMING name=.* operation=restore ').Count-ne21-or@(Select-String -LiteralPath $log -Pattern '^PROTOTYPE .* exact_order=1 retained=1$').Count-ne3){throw 'Incomplete path or array checks.'}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
