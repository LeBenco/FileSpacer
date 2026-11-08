# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$candidate=Build-NativeTest -Name ScrollStateTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 5000
try {
 $code=Invoke-NativeProgram $candidate @($fixture,(Join-Path $RunDirectory 'native-progress.log')) (Join-Path $RunDirectory 'native-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Viewport investigation failed: '+$code)}
 if(@(Select-String -LiteralPath (Join-Path $RunDirectory 'native-progress.log') -Pattern '^SCROLL .* exact=1 selected=1 focus=7$').Count-ne36){throw 'Incomplete viewport matrix.'}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
