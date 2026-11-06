# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,[switch]$HandoffOnly)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'The independent transfer prototype belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name OrderTransferTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Oleacc.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$strategies=if($HandoffOnly){@('handoff')}else{@('fresh','toggle','handoff')}
foreach($persistence in @('no-persist','persist')){foreach($strategy in $strategies){
    $fixture=New-ReactivityFixture $ProjectRoot
    try{Invoke-TestProgram $test @($fixture,$strategy,$persistence)}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
}}
