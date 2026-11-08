# Test-Category: UI
param([string]$ProjectRoot)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'Native transaction investigation belongs to the dedicated VM.'}
$hash=(Get-FileHash -LiteralPath (Join-Path $env:windir 'System32\shell32.dll') -Algorithm SHA256).Hash
if($hash-ne'936C33A8283196A070AA6F844A65A29A8F99F760EAE0016754C1CBF17D62BDB7'){throw 'The transaction ABI differs from the exact inspected Shell image.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name NativePositionTransactionTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Oleacc.lib','Dwmapi.lib','Gdi32.lib','Propsys.lib','Advapi32.lib','Dbghelp.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
$layout='icons'
$publicStyle='current'
$iconSize=48
$cache=Join-Path $RunDirectory 'cache'
[void][IO.Directory]::CreateDirectory($cache)
try {
 $records=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin')
 if($records.Count-ne1){throw 'Seed the existing runtime-generated selection descriptor for read-only host identification.'}
 $code=Invoke-NativeProgram $test @($fixture,$records[0].FullName,$RunDirectory,$layout,$publicStyle,[string]$iconSize) (Join-Path $RunDirectory 'native-wrapper-output.log') (Join-Path $RunDirectory 'native-errors.log')
 if($code-ne0){throw ('Native transaction investigation failed: '+$code)}
 if(@(Select-String -LiteralPath (Join-Path $RunDirectory 'native-output.log') -Pattern '^(QUALIFIED_LAYOUT|READY_FOR_FULL|REJECT) mode=').Count-ne1){throw 'Incomplete native transaction investigation.'}
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
