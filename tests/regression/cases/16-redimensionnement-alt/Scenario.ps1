param([string]$ProjectRoot,
    [Parameter(Mandatory=$true)][ValidateSet('Background','Minimum','Escape','PlainContext','Persistence','Icons')][string]$Case)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$inputRoot=Join-Path $PSScriptRoot '..\15-deplacement-alt'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $inputRoot 'MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $inputRoot 'NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
$testFailure=$null; $window=[IntPtr]::Zero; $phase='initialization'
$state=@{Status='RUNNING'; Case=$Case; Error=$null; Phase=$phase; Before=$null; After=$null;
    Mode=$null; SelectionBefore=$null; SelectionAfter=$null; InputBeforeRelease=$null; InputAfterRelease=$null; CursorDuringResize=$null;
    AssertionsPassed=$false; CleanupError=$null; MenuCleanup=@()}
function Point-At([int]$X,[int]$Y) { $p=New-Object FileSpacerRegression.MousePoint; $p.X=$X; $p.Y=$Y; return $p }
function Assert-Fixed($r) {
    $a=[FileSpacerRegression.Native]::Bounds($window)
    Assert-Ui ($a.Left -eq $r.Left -and $a.Top -eq $r.Top -and $a.Width -eq $r.Width -and $a.Height -eq $r.Height) 'Window geometry changed unexpectedly.'
}
function Assert-Resized([int]$Width,[int]$Height) {
    Wait-Ui {
        $a=[FileSpacerRegression.Native]::Bounds($window)
        $a.Left -eq $before.Left -and $a.Top -eq $before.Top -and $a.Width -eq $Width -and $a.Height -eq $Height
    } 'exact size from pointer delta, with fixed top-left corner' 150 5
}
function Wait-Idle {
    Wait-Ui {
        [FileSpacerRegression.AltDragInput]::Idle($window) -and ![FileSpacerRegression.NativeTargets]::MenuActive($window)
    } 'capture released, sizing idle and native menu loop ended' 200 5
}
function Stop-NativeMenu {
    if(![FileSpacerRegression.NativeTargets]::MenuActive($window)) { return }
    Wait-Ui {
        ![FileSpacerRegression.NativeTargets]::MenuActive($window) -or [FileSpacerRegression.NativeTargets]::MenuOwner($window) -ne [IntPtr]::Zero
    } 'active native menu owner identified' 0 5
    $owner=[FileSpacerRegression.NativeTargets]::MenuOwner($window)
    if($owner -ne [IntPtr]::Zero) {
        $evidence=@{Owner=$owner.ToInt64(); OwnerClass=[FileSpacerRegression.Native]::Class($owner);
            Before=[FileSpacerRegression.AltDragInput]::Describe($window); After=$null}
        $state.MenuCleanup+=@($evidence)
        # Cleanup after assertions: address the actual owner of Shell/edit menus,
        # which need not be the top-level FileSpacer window. Do not select a verb.
        [void][FileSpacerRegression.Native]::Message($owner,0x1f,0,0) # WM_CANCELMODE
        Wait-Ui { ![FileSpacerRegression.NativeTargets]::MenuActive($window) } 'native menu cancellation completed' 200 5
        $evidence.After=[FileSpacerRegression.AltDragInput]::Describe($window)
    }
}
try {
    Initialize-Ui ('alt-resize-'+$Case) -UseApplicationDefaults
    $folder=Join-Path $script:UiRoot 'ResizeFixture'; [void](New-Item -ItemType Directory -Path $folder)
    $names=@('AnchorA','AnchorB','OtherC')
    foreach($name in $names) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $name)) }
    $window=Open-UiFolder $folder $names.Count
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 800 -and $work.Height -ge 600) 'At least 800x600 physical pixels are required.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the test window.'
    Activate-Ui $window
    if($Case -eq 'Icons') { Use-UiView $window {param($v) $v.SetViewModeAndIconSize(1,48)}; Wait-UiViewMode $window 1 48 }
    $viewWindow=Use-UiView $window {param($v) $v.Window}
    $state.Mode=Use-UiView $window {param($v) $v.Mode}
    $selection=''
    if($names.Count) {
        Use-UiView $window {param($v) $v.Focus(); $v.Select('AnchorA'); $v.Select('AnchorB',$true)}
        $selection='AnchorA;AnchorB'
    }
    Assert-UiSelected $window $selection
    $state.SelectionBefore=@(Use-UiView $window {param($v) $v.Selected()})
    $point=[FileSpacerRegression.AltDragInput]::BackgroundPoint($viewWindow,($Case -eq 'Minimum'))
    if($Case -in @('PlainContext','Icons')) {
        $point=[FileSpacerRegression.AltDragInput]::ItemPoint($viewWindow,'OtherC')
    }
    $before=[FileSpacerRegression.Native]::Bounds($window); $state.Before=$before
    $phase='Alt then right press'
    $alt=$Case -ne 'PlainContext'
    [FileSpacerRegression.AltDragInput]::BeginRight($window,$point,$alt,$false)
    [FileSpacerRegression.AltDragInput]::VerifyHeld($alt)
    if($Case -eq 'PlainContext') {
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Ui { [FileSpacerRegression.NativeTargets]::MenuActive($window) } 'native right-click context menu opened' 0 5
        Assert-Fixed $before
        Assert-UiSelected $window 'OtherC'
        $phase='native context menu cancellation'
        Stop-NativeMenu
    } else {
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'right press captured before any drag motion' 0 5
        $phase='resizing with right held'
        $dx=[Math]::Max(96,[Math]::Abs([FileSpacerRegression.AltDragInput]::GetSystemMetrics(68))+16)
        $dy=[Math]::Max(72,[Math]::Abs([FileSpacerRegression.AltDragInput]::GetSystemMetrics(69))+16)
        $destination=Point-At ($point.X+$dx) ($point.Y+$dy)
        if($Case -eq 'Minimum') { $destination=Point-At ($work.Left+2) ($work.Top+2) }
        [FileSpacerRegression.AltDragInput]::Move($destination)
        if($Case -eq 'Minimum') {
            $minWidth=[FileSpacerRegression.AltDragInput]::GetSystemMetrics(34)
            $minHeight=[FileSpacerRegression.AltDragInput]::GetSystemMetrics(35)
            Wait-Ui {
                $a=[FileSpacerRegression.Native]::Bounds($window)
                $a.Left -eq $before.Left -and $a.Top -eq $before.Top -and $a.Width -lt $before.Width -and $a.Height -lt $before.Height -and $a.Width -ge $minWidth -and $a.Height -ge $minHeight
            } 'shrinking respects Windows minimum tracking dimensions' 150 5
        } else { Assert-Resized ($before.Width+$destination.X-$point.X) ($before.Height+$destination.Y-$point.Y) }
        Wait-Ui { [FileSpacerRegression.AltDragInput]::ResizeCursor() } 'system IDC_SIZENWSE cursor during resize' 100 5
        $state.CursorDuringResize=$true
        Assert-UiSelected $window $selection
        Assert-Ui (![FileSpacerRegression.NativeTargets]::MenuActive($window)) 'An Alt-right gesture opened a context menu.'
        $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
        if($Case -eq 'Escape') {
            [FileSpacerRegression.AltDragInput]::Escape()
            Wait-Idle; Assert-Fixed $before
        }
        [FileSpacerRegression.AltDragInput]::End(); Wait-Idle
        if($Case -eq 'Escape') { Assert-Fixed $before }
        elseif($Case -ne 'Minimum') { Assert-Resized ($before.Width+$destination.X-$point.X) ($before.Height+$destination.Y-$point.Y) }
        Wait-Ui { ![FileSpacerRegression.AltDragInput]::ResizeCursor() } 'resize cursor restored after gesture completion' 100 5
        Assert-UiSelected $window $selection
        Assert-Ui (![FileSpacerRegression.NativeTargets]::MenuActive($window)) 'Alt-right release opened a context menu.'
        if($Case -eq 'Persistence') {
            $saved=[FileSpacerRegression.Native]::Bounds($window)
            Close-UiFolder $window
            $window=Open-UiFolder $folder $names.Count
            Wait-Ui { Assert-Fixed $saved; return $true } 'resized geometry restored on reopen' 200 5
        }
    }
    $phase='final interaction idle'
    Wait-Idle
    $state.InputAfterRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
    $state.After=[FileSpacerRegression.Native]::Bounds($window)
    $state.SelectionAfter=@(Use-UiView $window {param($v) $v.Selected()})
    Assert-Ui ((Use-UiView $window {param($v) $v.Mode}) -eq $state.Mode) 'The view mode changed during the gesture.'
    Assert-Ui ((Use-UiView $window {param($v) $v.Count}) -eq $names.Count) 'The gesture changed folder contents.'
    $state.AssertionsPassed=$true
} catch {
    $testFailure=$_; $state.Status='FAIL'; $state.Error=$_.Exception.Message; $state.Phase=$phase
    try { if($window -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($window)) { $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window); $state.After=[FileSpacerRegression.Native]::Bounds($window) } } catch { Write-Warning $_.Exception.Message }
    throw $testFailure
} finally {
    $cleanupFailure=$null
    try {
        [FileSpacerRegression.AltDragInput]::End()
        if($window -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($window)) {
            Stop-NativeMenu
            $state.InputBeforeCleanup=[FileSpacerRegression.AltDragInput]::Describe($window)
        }
    } catch { $cleanupFailure=$_ }
    try { Stop-Ui -Failure $testFailure } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } }
    if($cleanupFailure) {
        $state.CleanupError=$cleanupFailure.Exception.Message
        if(!$testFailure) { $state.Status='FAIL'; $state.Error=$state.CleanupError; $state.Phase='application cleanup' }
    } elseif(!$testFailure) { $state.Status='PASS'; $state.Phase='complete' }
    try { $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'resize.json') -Encoding UTF8 } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } }
    if($cleanupFailure) { if($testFailure) { Write-Warning ('Cleanup: '+$cleanupFailure.Exception.Message) } else { throw $cleanupFailure } }
    if(!$testFailure) { Write-Host ('PASS: Alt-right resize / '+$Case+'; application exit verified') }
}
