# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This investigation belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name SelectionInternalsTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','ShellViewSelection.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Oleacc.lib')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot
try{
    foreach($strategy in @('batch','batch-deselect','selection-array','selection-array-reverse')){
        Invoke-TestProgram $test @($RunDirectory,$fixture,'extra-large',$strategy,'6','0')
    }
    for($rep=0;$rep-lt3;++$rep){
        foreach($view in @('extra-large','large','medium','small','tiles','content')){
            $profiles=if($rep%2){@(0,6)}else{@(6,0)}
            foreach($profile in $profiles){Invoke-TestProgram $test @($RunDirectory,$fixture,$view,'batch',[string]$profile,[string]$rep)}
        }
    }
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
