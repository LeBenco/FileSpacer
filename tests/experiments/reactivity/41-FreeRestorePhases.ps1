# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$candidate=Build-NativeTest -Name FreeRestorePhaseTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','..\build\restore-free-phases\ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 5000
$cache=Join-Path $RunDirectory 'cache';[void][IO.Directory]::CreateDirectory($cache)
try {
 $code=Invoke-NativeProgram $candidate @('--build-cache',$cache) (Join-Path $RunDirectory 'discovery.log') (Join-Path $RunDirectory 'discovery-errors.log')
 if($code-ne0){throw ('Discovery preparation failed: '+$code)}
 $code=Invoke-NativeProgram $candidate @($RunDirectory,$fixture,$cache,'fs-free') (Join-Path $RunDirectory 'native-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Native phase investigation failed: '+$code)}
 $rows=@(Import-Csv -LiteralPath (Join-Path $RunDirectory 'restore-all.csv'))
 if($rows.Count-ne6-or@($rows|Where-Object selected -ne '5000').Count){throw 'Incomplete free restoration matrix.'}
 $qualified=@($rows|Where-Object arm -eq 'qualified')
 if($qualified.Count-ne3-or@($qualified|Where-Object private_all_calls -ne '1').Count){throw 'Qualified global selection was not exercised.'}
 if(@(Select-String -LiteralPath (Join-Path $RunDirectory 'native-output.log') -Pattern '^PHASE name=end ').Count-ne6){throw 'Incomplete phase markers.'}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
