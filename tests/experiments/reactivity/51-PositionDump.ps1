# Test-Category: UI
param([string]$ProjectRoot)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'Position dump investigation belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name PositionDumpTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Oleacc.lib','Dwmapi.lib','Gdi32.lib','Propsys.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
try {
 $code=Invoke-NativeProgram $test @($fixture,$RunDirectory) (Join-Path $RunDirectory 'native-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Position dump investigation failed: '+$code)}
 $log=Join-Path $RunDirectory 'native-output.log'
 $qualified=@(Select-String -LiteralPath $log -Pattern '^DUMP .* identities=1$').Count
 $unqualified=@(Select-String -LiteralPath $log -Pattern '^UNQUALIFIED .* path=stream reason=stream_not_used ').Count
 if($qualified+$unqualified-ne4-or@(Select-String -LiteralPath $log -Pattern '^DUMP .* path=bag .* identities=1$').Count-ne2-or@(Select-String -LiteralPath $log -Pattern '^STREAM ').Count-ne2-or@(Select-String -LiteralPath $log -Pattern '^BAG_CONTROL .* setting_reloaded=1 ').Count-ne2-or@(Select-String -LiteralPath $log -Pattern '^BAG_CONTRAST .* read_back_details=1$').Count-ne2){throw 'Incomplete dump investigation.'}
 if($unqualified){Write-Host ('Stream qualification incomplete for '+$unqualified+' modes; unused streams prove no persistence behavior.')}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
