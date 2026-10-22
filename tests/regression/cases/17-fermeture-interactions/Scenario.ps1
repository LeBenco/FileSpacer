param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$helpers=Join-Path $PSScriptRoot '..\15-deplacement-alt'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $helpers 'MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $helpers 'NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
Add-Type -Path (Join-Path $helpers 'WindowEvidence.cs')
$testFailure=$null; $window=[IntPtr]::Zero; $applicationProcess=$null
$applicationProcessId=[uint32]0; $applicationThreadId=[uint32]0
$phase='initialization'
$state=@{Status='RUNNING'; Error=$null; Phase=$phase; SetupPassed=$false;
    CloseRoute='Alt+F4 through SendInput';
    CloseUsesKeyboardInput=$true; CloseInputCount=0; WindowClosedBeforeCleanup=$false;
    ProcessExitedBeforeCleanup=$false; ProcessExitCode=$null; CleanupError=$null;
    Steps=@(); Observations=@(); Binary=$null; Before=$null}
function Point-At([int]$X,[int]$Y) { $p=New-Object FileSpacerRegression.MousePoint; $p.X=$X; $p.Y=$Y; return $p }
function Menu-IsOpen {
    if(![FileSpacerRegression.Native]::IsWindow($window)) { return $false }
    try { return [FileSpacerRegression.NativeTargets]::MenuActive($window) }
    catch { if(![FileSpacerRegression.Native]::IsWindow($window)) { return $false }; throw }
}
function Save-Evidence([string]$Stage) {
    # Observe without querying the COM view after opening the native menu.
    $entry=@{Stage=$Stage; Utc=[DateTime]::UtcNow.ToString('o'); WindowAlive=[FileSpacerRegression.Native]::IsWindow($window);
        ProcessExited=$null; Thread=$null; ObservationError=$null}
    try {
        if($null -ne $applicationProcess) { $entry.ProcessExited=$applicationProcess.HasExited }
        if($applicationThreadId -ne 0 -and $entry.ProcessExited -ne $true) {
            $entry.Thread=[FileSpacerRegression.NativeTargets]::DescribeThread($applicationThreadId)
        }
        if($applicationProcessId -ne 0) {
            [FileSpacerRegression.WindowEvidence]::Capture($window,$applicationProcessId,'CloseFixture') |
                ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN ('windows-'+$Stage+'.json')) -Encoding UTF8
        }
    } catch { $entry.ObservationError=$_.Exception.Message }
    $state.Observations+=@($entry)
}
function Press-Close {
    try {
        $state.Steps+=@{Action='Alt+F4 through SendInput'; Utc=[DateTime]::UtcNow.ToString('o')}
        [FileSpacerRegression.Native]::Keys($window,0x73,$false,$true)
        $state.CloseInputCount++
    } catch {
        # The first shortcut may finish closing the window while a second is
        # being prepared. A surviving window's input failure is never hidden.
        if([FileSpacerRegression.Native]::IsWindow($window)) { throw }
    }
}

try {
    Initialize-Ui 'close-context-menu' -UseApplicationDefaults
    $folder=Join-Path $script:UiRoot 'CloseFixture'; [void](New-Item -ItemType Directory -Path $folder)
    foreach($name in @('AnchorA','AnchorB','OtherC')) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $name)) }
    $window=Open-UiFolder $folder 3
    $applicationThreadId=[FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$applicationProcessId)
    $applicationProcess=@($script:UiProcesses | Where-Object {$_.Id -eq $applicationProcessId})[0]
    $state.Binary=@{Path=$script:UiExecutable; SHA256=(Get-FileHash -LiteralPath $script:UiExecutable -Algorithm SHA256).Hash}
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 800 -and $work.Height -ge 600) 'At least 800x600 pixels of work area are required.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the test window.'
    Activate-Ui $window
    Use-UiView $window {param($v) $v.Focus(); $v.Select('AnchorA'); $v.Select('AnchorB',$true)}
    Assert-UiSelected $window 'AnchorA;AnchorB'
    $viewWindow=Use-UiView $window {param($v) $v.Window}
    $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -eq 1) 'Exactly one native ListView is required.'; $list=$lists[0]
    $index=Use-UiView $window {param($v) $v.IndexOf('OtherC')}
    $nativeItem=[FileSpacerRegression.NativeTargets]::Item($list,$index,3,$false,$false)
    $itemPoint=Point-At $nativeItem.X $nativeItem.Y
    $before=[FileSpacerRegression.Native]::Bounds($window); $state.Before=$before
    $phase='opening item context menu with the right button'
    [FileSpacerRegression.AltDragInput]::BeginRight($window,$itemPoint,$false,$false)
    [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { Menu-IsOpen } 'native context menu active' 100 5
    Save-Evidence 'interaction-active'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($window)) 'The window disappeared before the chosen close action.'
    $state.SetupPassed=$true
    $phase='close request before any cleanup'
    Save-Evidence 'before-close'
    $menuBeforeClose=Menu-IsOpen
    Press-Close
    if($menuBeforeClose) {
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($window) -or !(Menu-IsOpen) } 'first user action closed the window or dismissed the popup' 100 5
        Save-Evidence 'after-first-close-input'
        if([FileSpacerRegression.Native]::IsWindow($window)) {
            Assert-Ui (!(Menu-IsOpen)) 'The first action did not dismiss the menu.'
            $state.Steps+=@{Action='popup dismissed; one further Alt+F4'; Utc=[DateTime]::UtcNow.ToString('o')}
            # This is a second deliberate Alt+F4 after popup dismissal,
            # never a repeated WM_CLOSE or an unbounded retry-until-success.
            Press-Close
        }
    }
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($window) } 'window closed through the selected route' 0 10
    Assert-Ui ($state.CloseInputCount -gt 0) 'No complete close input was sent; this run cannot establish user-close behavior.'
    $state.WindowClosedBeforeCleanup=$true
    Save-Evidence 'window-closed'
    $phase='process exit before any cleanup'
    $state.ProcessExitedBeforeCleanup=$applicationProcess.WaitForExit(10000)
    if($state.ProcessExitedBeforeCleanup) { $state.ProcessExitCode=$applicationProcess.ExitCode }
    Save-Evidence 'exit-check'
    Assert-Ui $state.ProcessExitedBeforeCleanup 'The window closed, but FileSpacer stayed alive before any cleanup intervention.'
    Assert-Ui ($state.ProcessExitCode -eq 0) ('FileSpacer exited abnormally: '+$state.ProcessExitCode)
    $state.Status='OBSERVED_PASS'; $state.Phase='normal exit observed'
} catch {
    $testFailure=$_; $state.Status='FAIL'; $state.Phase=$phase; $state.Error=$_.Exception.Message
    Save-Evidence 'failure-before-cleanup'
    throw $testFailure
} finally {
    # Persist the actual result BEFORE any WM_CLOSE sent by Stop-Ui. Recovery
    # cleanup can never turn a failed user interaction/exit into a pass.
    $cleanupFailure=$null
    try { $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'close-interaction.json') -Encoding UTF8 } catch { $cleanupFailure=$_ }
    try { [FileSpacerRegression.AltDragInput]::End() } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } }
    try { Stop-Ui -Failure $testFailure } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } }
    if($cleanupFailure) {
        $state.CleanupError=$cleanupFailure.Exception.Message
        if(!$testFailure) { $state.Status='FAIL'; $state.Phase='cleanup'; $state.Error=$state.CleanupError }
    } elseif(!$testFailure) { $state.Status='PASS'; $state.Phase='complete' }
    try { $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'close-interaction.json') -Encoding UTF8 } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } }
    if($cleanupFailure) { if($testFailure) { Write-Warning $cleanupFailure.Exception.Message } else { throw $cleanupFailure } }
    if(!$testFailure) { Write-Host 'PASS: native item menu then Alt+F4; real process exit observed before cleanup' }
}
