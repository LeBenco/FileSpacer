# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,[switch]$ReorderOnly)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This transition qualification belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name ArrangementTransitionsTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','ShellViewSelection.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Oleacc.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot $(if($ReorderOnly){20}else{5000})
try{$reorderFixture=New-ReactivityFixture $ProjectRoot 20;try{$arguments=@($RunDirectory,$fixture,$reorderFixture);if($ReorderOnly){$arguments+='reorder-only'};Invoke-TestProgram $test $arguments}finally{Remove-ReactivityFixture $ProjectRoot $reorderFixture}}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
