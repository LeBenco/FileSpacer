# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This private instrumentation belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name SelectionInternalsTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','ShellViewSelection.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Oleacc.lib')
# The RVA and neutral-prefix breakpoint belong only to this exact captured image.
if((Get-FileHash -LiteralPath (Join-Path $env:windir 'System32\shell32.dll') -Algorithm SHA256).Hash-ne'936C33A8283196A070AA6F844A65A29A8F99F760EAE0016754C1CBF17D62BDB7'){throw 'Guest Shell image changed; private comparison probe is disabled.'}
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
try{
    foreach($count in @(100,200,500)){
        foreach($profile in @(6,0)){Invoke-TestProgram $test @($RunDirectory,$fixture,'extra-large','comparison-counts',[string]$profile,[string]$count)}
    }
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
