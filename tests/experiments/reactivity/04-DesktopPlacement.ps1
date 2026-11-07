# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [ValidateSet('api','profile')][string]$Operation='api')
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This desktop experiment belongs exclusively to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name DesktopPlacementTests -CaseDirectory $PSScriptRoot -Standard c++14 -Resources -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Advapi32.lib','Dbghelp.lib','Psapi.lib','Comctl32.lib','Gdi32.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 100
try{Invoke-TestProgram $test @($RunDirectory,$fixture,$Operation)}
finally{
    try{if(Test-Path -LiteralPath (Join-Path $RunDirectory 'desktop-backup.bin')){Invoke-TestProgram $test @($RunDirectory,'recover')}}
    finally{Remove-ReactivityFixture $ProjectRoot $fixture}
}
