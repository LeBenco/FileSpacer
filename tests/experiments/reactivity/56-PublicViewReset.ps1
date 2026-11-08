# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path,
    [ValidateSet('all','medium','tiles')][string]$View='all',[ValidateRange(1,3)][int]$Repetitions=3)
$ErrorActionPreference='Stop'
if(Get-Process FileSpacer -ErrorAction SilentlyContinue){throw 'Close FileSpacer before building. No user process was stopped.'}
. (Join-Path $ProjectRoot 'tests\regression\common\Native.ps1')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$exe=Build-NativeTest -Name PublicViewResetTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','ShellViewSettings.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
$fixture=New-ReactivityFixture $ProjectRoot 5000
try {
    $job=Start-RecordedProcess -Executable $exe -Arguments (Join-ProcessArguments @($RunDirectory,$fixture,$View,[string]$Repetitions)) -OutputPath (Join-Path $RunDirectory 'reset.log') -ErrorPath (Join-Path $RunDirectory 'reset.errors.log')
    try {
        if(!$job.Process.WaitForExit(600000)) {
            $job.Process.Kill();$job.Process.WaitForExit();[void](Complete-RecordedProcess $job)
            throw 'Public reset timed out; only this owned helper was stopped; partial evidence retained.'
        }
        if((Complete-RecordedProcess $job)-ne0){throw ('Public reset failed; see '+$RunDirectory)}
    }finally{$job.Process.Dispose()}
    $rows=@(Import-Csv (Join-Path $RunDirectory 'public-reset.csv'))
    $expected=if($View-eq'all'){4*$Repetitions}else{2*$Repetitions}
    if($rows.Count-ne$expected-or@($rows|Where-Object correct -ne '1').Count){throw 'Incomplete or incorrect public reset matrix.'}
    Write-Host ('PASS '+$rows.Count+' public clear/recreation workflows; output='+$RunDirectory)
}finally{Remove-ReactivityFixture $ProjectRoot $fixture}
