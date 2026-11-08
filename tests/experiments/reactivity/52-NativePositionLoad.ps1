# Test-Category: UI
param([string]$ProjectRoot)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'Native legacy layout investigation belongs to the dedicated VM.'}
# Test-only parser qualification, not a release version/profile catalogue.
$hash=(Get-FileHash -LiteralPath (Join-Path $env:windir 'System32\shell32.dll') -Algorithm SHA256).Hash
if($hash-ne'936C33A8283196A070AA6F844A65A29A8F99F760EAE0016754C1CBF17D62BDB7'){throw 'The loaded Shell file differs from the exact parser inspected for this experiment.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name NativePositionLoadTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Oleacc.lib','Dwmapi.lib','Gdi32.lib','Propsys.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
$initialDesktop=$false
try {
 $arguments=@($fixture,$RunDirectory)
 if($initialDesktop){$arguments+='desktop'}
 $code=Invoke-NativeProgram $test $arguments (Join-Path $RunDirectory 'native-wrapper-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Native position load investigation failed: '+$code)}
 $log=Join-Path $RunDirectory 'native-output.log'
 if(@(Select-String -LiteralPath $log -Pattern '^(QUALIFIED|REJECT) mode=').Count-ne2){throw 'Incomplete native loader investigation.'}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
