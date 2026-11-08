# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $ProjectRoot 'tests/regression/common/Native.ps1')
$source=Join-Path $ProjectRoot 'tests/experiments/reactivity'
$exe=Build-NativeTest -Name PartialSelectionTests -CaseDirectory $source -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Oleacc.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $source 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 0
$cache=if($env:FS_GESTURE_CACHE){[IO.Path]::GetFullPath($env:FS_GESTURE_CACHE)}else{Join-Path $ProjectRoot 'build/remaining-selection/cache'}
$job=$null
try {
    for($index=0;$index-lt5000;++$index){[IO.File]::WriteAllText((Join-Path $fixture ('File{0:D5}.txt'-f$index)),'fixture')}
    $job=Start-RecordedProcess -Executable $exe -Arguments (Join-ProcessArguments @($RunDirectory,$fixture,$cache)) -OutputPath (Join-Path $RunDirectory 'tail.log') -ErrorPath (Join-Path $RunDirectory 'tail.errors.log')
    if(!$job.Process.WaitForExit(600000)){throw 'Measurement helper exceeded 600 seconds; no FileSpacer process was stopped.'}
    $code=Complete-RecordedProcess $job
    if($code-ne0){throw ('Measurement helper failed: '+$code)}
    $log=Get-Content -LiteralPath (Join-Path $RunDirectory 'tail.log')
    $expected=New-Object 'Collections.Generic.HashSet[string]'
    $safety=New-Object 'Collections.Generic.HashSet[string]'
    $measure=$env:FS_PARTIAL_MEASURE_ONLY-eq'1'
    foreach($mode in @('medium','small','large','extra-large')){
        [void]$safety.Add($mode)
        foreach($pattern in 0..5){foreach($focus in @(7,4000,4999)){
            if($measure-and($pattern-gt1-or$focus-ne7)){continue}
            $repetitions=if($measure-or($pattern-eq1-and$focus-eq7)){3}else{1}
            for($rep=0;$rep-lt$repetitions;++$rep){[void]$expected.Add(('{0}|{1}|{2}|{3}'-f$mode,$pattern,$focus,$rep))}
        }}
    }
    foreach($line in $log){
        if($line-match'^PARTIAL '){
            if($line-notmatch'^PARTIAL mode=(\S+) pattern=(\d+) focus=(\d+) rep=(\d+) count=\d+ native_ms=[\d.]+ integrated_ms=[\d.]+ order=1 attributes=1 data_object=1 retained=1 focus=1$'){
                throw 'Malformed partial-selection result.'
            }
            if(!$expected.Remove(('{0}|{1}|{2}|{3}'-f$matches[1],$matches[2],$matches[3],$matches[4]))){throw 'Duplicate or unexpected partial-selection result.'}
        }elseif($line-match'^SAFETY '){
            if($line-notmatch'^SAFETY mode=(\S+) small_subset=1 cancellation=1 same_count_mutation=1 late_focus_change=1 tied_positions=1 arranged=1 positions=1$'){
                throw 'Malformed partial-selection safety result.'
            }
            if(!$safety.Remove($matches[1])){throw 'Duplicate or unexpected safety result.'}
        }
    }
    if($expected.Count-or$safety.Count){throw 'Incomplete partial-selection validation.'}
}finally {
    if($null-ne$job){if(!$job.Process.HasExited){$job.Process.Kill();$job.Process.WaitForExit()};$job.Process.Dispose()}
    Remove-ReactivityFixture $ProjectRoot $fixture
}
