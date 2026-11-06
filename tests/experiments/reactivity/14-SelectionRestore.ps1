# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This experiment belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name SelectionStrategiesTests -CaseDirectory $PSScriptRoot -Standard c++14 -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','ShellViewSelection.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
try{Invoke-TestProgram $test @($RunDirectory,$fixture,'restore')}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
