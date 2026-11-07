# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This private causal probe belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name SelectionInternalsTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','ShellViewSelection.cpp','ShellSelectionDiscovery.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Oleacc.lib')
if((Get-FileHash -LiteralPath (Join-Path $env:windir 'System32\shell32.dll') -Algorithm SHA256).Hash-ne'936C33A8283196A070AA6F844A65A29A8F99F760EAE0016754C1CBF17D62BDB7'){throw 'Guest Shell image changed; private causal probe is disabled.'}
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
try{
    for($rep=0;$rep-lt3;++$rep){
        $strategies=if($rep%2){@('private-bypass','batch')}else{@('batch','private-bypass')}
        foreach($strategy in $strategies){Invoke-TestProgram $test @($RunDirectory,$fixture,'extra-large',$strategy,'6',[string]$rep)}
    }
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
