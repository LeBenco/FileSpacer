# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [ValidateSet('SelfTest','Details','List','Placement')][string]$Scenario=$(if($env:FILESPACER_VISUAL_SCENARIO){$env:FILESPACER_VISUAL_SCENARIO}else{'SelfTest'}))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name VisualTimingTests -CaseDirectory $PSScriptRoot -Standard c++14 -Resources -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Advapi32.lib','Dbghelp.lib','Psapi.lib','Comctl32.lib','Gdi32.lib','D3D11.lib','Dxgi.lib','Bcrypt.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot $(if($Scenario-eq'SelfTest'){0}elseif($Scenario-eq'Placement'){100}else{5000})
$operation=@{SelfTest='self-test';Details='details';List='list';Placement='api'}[$Scenario]
try{Invoke-TestProgram $test @($RunDirectory,$fixture,$operation)}
finally{
    try{if(Test-Path -LiteralPath (Join-Path $RunDirectory 'desktop-backup.bin')){Invoke-TestProgram $test @($RunDirectory,'recover','api')}}
    finally{Remove-ReactivityFixture $ProjectRoot $fixture}
}
