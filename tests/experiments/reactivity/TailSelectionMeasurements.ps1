# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $ProjectRoot 'tests/regression/common/Native.ps1')
$source=Join-Path $ProjectRoot 'tests/experiments/reactivity'
$exe=Build-NativeTest -Name TailSelectionMeasurements -CaseDirectory $source -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Oleacc.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $source 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 0
$cache=if($env:FS_GESTURE_CACHE){[IO.Path]::GetFullPath($env:FS_GESTURE_CACHE)}else{Join-Path $ProjectRoot 'build/remaining-selection/cache'}
$job=$null
try {
    for($index=0;$index-lt5000;++$index){[IO.File]::WriteAllText((Join-Path $fixture ('File{0:D5}.txt'-f$index)),'fixture')}
    $job=Start-RecordedProcess -Executable $exe -Arguments (Join-ProcessArguments @($RunDirectory,$fixture,$cache)) -OutputPath (Join-Path $RunDirectory 'tail.log') -ErrorPath (Join-Path $RunDirectory 'tail.errors.log')
    if(!$job.Process.WaitForExit(240000)){throw 'Measurement helper exceeded 240 seconds; no FileSpacer process was stopped.'}
    $code=Complete-RecordedProcess $job
    if($code-ne0){throw ('Measurement helper failed: '+$code)}
    $log=Get-Content -LiteralPath (Join-Path $RunDirectory 'tail.log')
    $arrayRows=if($env:FS_TAIL_EXPLORER_TILES_ONLY-eq'1'){3}else{30}
    $clickRows=if($env:FS_TAIL_EXPLORER_TILES_ONLY-eq'1'){3}else{6}
    if(@($log|Where-Object {$_-match'^ARRAY '}).Count-ne$arrayRows-or@($log|Where-Object {$_-match'^CLICK '}).Count-ne$clickRows){throw 'Incomplete final measurements.'}
}finally {
    if($null-ne$job){if(!$job.Process.HasExited){$job.Process.Kill();$job.Process.WaitForExit()};$job.Process.Dispose()}
    Remove-ReactivityFixture $ProjectRoot $fixture
}
