# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This experiment belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name ExplorerObservationTests -CaseDirectory $PSScriptRoot -Standard c++14 -Resources -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Advapi32.lib','Dbghelp.lib','Psapi.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
try{Invoke-TestProgram $test @($RunDirectory,$fixture,'details')}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
