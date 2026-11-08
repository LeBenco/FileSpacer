# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$candidate=Build-NativeTest -Name PublicFallbackTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 5000
try {
 $code=Invoke-NativeProgram $candidate @($RunDirectory,$fixture) (Join-Path $RunDirectory 'native-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Public fallback investigation failed: '+$code)}
 $rows=@(Import-Csv (Join-Path $RunDirectory 'public-fallback.csv'))
 if($rows.Count-ne48-or@($rows|Where-Object private_calls -ne '0').Count){throw 'Incomplete public-only fallback matrix.'}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
