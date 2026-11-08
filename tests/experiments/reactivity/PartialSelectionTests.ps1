# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $ProjectRoot 'tests/regression/common/Native.ps1')
$source=Join-Path $ProjectRoot 'tests/experiments/reactivity'
$exe=Build-NativeTest -Name PartialSelectionTests -CaseDirectory $source -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Oleacc.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib')
. (Join-Path $source 'Fixture.ps1')
$fixture=New-ReactivityFixture $ProjectRoot 0
$cache=Join-Path $ProjectRoot 'build/remaining-selection/cache'
$job=$null
try {
    for($index=0;$index-lt5000;++$index){[IO.File]::WriteAllText((Join-Path $fixture ('File{0:D5}.txt'-f$index)),'fixture')}
    $job=Start-RecordedProcess -Executable $exe -Arguments (Join-ProcessArguments @($RunDirectory,$fixture,$cache)) -OutputPath (Join-Path $RunDirectory 'tail.log') -ErrorPath (Join-Path $RunDirectory 'tail.errors.log')
    if(!$job.Process.WaitForExit(600000)){throw 'Measurement helper exceeded 600 seconds; no FileSpacer process was stopped.'}
    $code=Complete-RecordedProcess $job
    if($code-ne0){throw ('Measurement helper failed: '+$code)}
    $log=Get-Content -LiteralPath (Join-Path $RunDirectory 'tail.log')
    $expected=if($env:FS_PARTIAL_MEASURE_ONLY-eq'1'){24}else{80}
    if(@($log|Where-Object {$_-match'^PARTIAL '}).Count-ne$expected){throw 'Incomplete partial-selection validation.'}
    if(@($log|Where-Object {$_-match'^SAFETY '}).Count-ne4){throw 'Incomplete partial-selection safety checks.'}
}finally {
    if($null-ne$job){if(!$job.Process.HasExited){$job.Process.Kill();$job.Process.WaitForExit()};$job.Process.Dispose()}
    Remove-ReactivityFixture $ProjectRoot $fixture
}
