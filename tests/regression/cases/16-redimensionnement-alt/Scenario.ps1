param([string]$ProjectRoot,
    [Parameter(Mandatory=$true)][ValidateSet('Empty','Background','Unselected','Minimum','Escape','Cancel','Inactive','ReleaseEarly','AltClick','PlainContext','Header','Rename','Toolbar','Search','DoubleRight','Outside','Persistence','Icons')][string]$Case)
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
    $nativeMenuAllowed=$Case -in @('Header','Rename','Toolbar','Search')
    Wait-Ui {
        # An excluded control can open its native menu after button-up, even
        # during the final idle check. Handle it at every polling point.
        # Actual resize cases still fail if a menu is unexpectedly opened.
        if($nativeMenuAllowed) { Stop-NativeMenu }
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
    $names=@(); if($Case -ne 'Empty') { $names=@('AnchorA','AnchorB','OtherC') }
    foreach($name in $names) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $name)) }
    $window=Open-UiFolder $folder $names.Count
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 800 -and $work.Height -ge 600) 'At least 800x600 physical pixels are required.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the test window.'
    Activate-Ui $window
    if($Case -eq 'Header') { Use-UiView $window {param($v) $v.SetViewModeAndIconSize(4,-1)}; Wait-Ui { Use-UiView $window {param($v) $v.Mode -eq 4} } 'Details mode ready' 200 5 }
    if($Case -eq 'Icons') { Use-UiView $window {param($v) $v.SetViewModeAndIconSize(1,48)}; Wait-Ui { Use-UiView $window {param($v) $v.Mode -eq 1} } 'Icons mode ready' 200 5 }
    $viewWindow=Use-UiView $window {param($v) $v.Window}
    $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -eq 1) 'Exactly one classic ListView is required.'; $list=$lists[0]
    $state.Mode=Use-UiView $window {param($v) $v.Mode}
    $selection=''
    if($names.Count) {
        Use-UiView $window {param($v) $v.Focus(); $v.Select('AnchorA'); $v.Select('AnchorB',$true)}
        $selection='AnchorA;AnchorB'
    }
    Assert-UiSelected $window $selection
    $state.SelectionBefore=@(Use-UiView $window {param($v) $v.Selected()})
    $r=[FileSpacerRegression.Native]::ClientBounds($list)
    $point=Point-At ([int]($r.Left+$r.Width/2)) ([int]($r.Top+3*$r.Height/4))
    if($Case -eq 'Minimum') { $point=Point-At ($r.Right-25) ($r.Bottom-25) }
    [FileSpacerRegression.NativeTargets]::Background($list,$point.X,$point.Y)
    if($Case -in @('Unselected','Inactive','PlainContext','DoubleRight','Icons')) {
        $index=Use-UiView $window {param($v) $v.IndexOf('OtherC')}
        $p=[FileSpacerRegression.NativeTargets]::Item($list,$index,$names.Count,$false,$false)
        $point=Point-At $p.X $p.Y
    }
    if($Case -in @('Header','Rename','Search','Toolbar')) {
        if($Case -eq 'Header') { $control=Get-UiControl $viewWindow 'SysHeader32' }
        if($Case -eq 'Rename') {
            Use-UiView $window {param($v) $v.Select('AnchorA')}; $selection='AnchorA'
            [FileSpacerRegression.Native]::Keys($window,0x71,$false,$false)
            Wait-Ui { @([FileSpacerRegression.Native]::Children($viewWindow,'Edit') | Where-Object { [FileSpacerRegression.Native]::IsWindowVisible($_) }).Count -eq 1 } 'rename edit visible' 0 5
            $control=Get-UiControl $viewWindow 'Edit'
        }
        if($Case -eq 'Search') { $control=Get-UiSearchEdit $window }
        if($Case -eq 'Toolbar') {
            $id=Get-UiId 'IDM_REFRESH'
            $bars=@([FileSpacerRegression.Native]::Children($window,'ToolbarWindow32') | Where-Object { [FileSpacerRegression.Native]::IsWindowVisible($_) -and [FileSpacerRegression.Native]::Message($_,0x419,$id,0) -ge 0 })
            Assert-Ui ($bars.Count -eq 1) 'Exactly one refresh toolbar is required.'
            $p=[FileSpacerRegression.NativeTargets]::Toolbar($bars[0],$id); $point=Point-At $p.X $p.Y
        } else {
            $c=[FileSpacerRegression.Native]::ClientBounds($control)
            $p=[FileSpacerRegression.NativeTargets]::Control($control,$c.Left,$c.Top,$c.Right,$c.Bottom)
            $point=Point-At $p.X $p.Y
        }
    }
    if($Case -eq 'Inactive') {
        $otherFolder=Join-Path $script:UiRoot 'OtherWindow'; [void](New-Item -ItemType Directory -Path $otherFolder)
        $other=Open-UiFolder $otherFolder 0
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($other,[IntPtr]::Zero,($work.Right-310),($work.Bottom-210),300,200,0x14)) 'Cannot place the other window.'
        Activate-Ui $other
        Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow() -eq $other) 'The resized window must initially be inactive.'
        Assert-UiSelected $window $selection
    }
    $before=[FileSpacerRegression.Native]::Bounds($window); $state.Before=$before
    $phase='Alt then right press'
    $alt=$Case -ne 'PlainContext'
    [FileSpacerRegression.AltDragInput]::BeginRight($window,$point,$alt,($Case -eq 'DoubleRight'))
    [FileSpacerRegression.AltDragInput]::VerifyHeld($alt)
    if($Case -eq 'PlainContext') {
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Ui { [FileSpacerRegression.NativeTargets]::MenuActive($window) } 'native right-click context menu opened' 0 5
        Assert-Fixed $before
        Assert-UiSelected $window 'OtherC'
        $phase='native context menu cancellation'
        Stop-NativeMenu
    } elseif($Case -in @('Header','Rename','Toolbar','Search')) {
        $destination=Point-At ($point.X+40) ($point.Y+25)
        if($Case -eq 'Rename') {
            # Stay inside the editor: a right release over the list is a context-menu
            # request for the selection, which ends the rename (standard Shell behavior).
            $c=[FileSpacerRegression.Native]::ClientBounds($control)
            $x=[Math]::Min($point.X+40,$c.Right-4); $y=[Math]::Min($point.Y+25,$c.Bottom-4)
            Assert-Ui (($x-$point.X) -gt [FileSpacerRegression.AltDragInput]::GetSystemMetrics(68)) 'The rename editor is too narrow for a drag inside it.'
            $p=[FileSpacerRegression.NativeTargets]::Control($control,($x-1),($y-1),($x+1),($y+1))
            $destination=Point-At $p.X $p.Y
        }
        [FileSpacerRegression.AltDragInput]::Move($destination)
        Wait-Ui { Assert-Fixed $before; return $true } 'excluded control kept window geometry' 200 5
        Stop-NativeMenu
        $phase='excluded control release and native menu cancellation'
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Idle
        if($Case -eq 'Rename') {
            Assert-Ui ([FileSpacerRegression.Native]::IsWindowVisible($control)) 'Alt-right drag unexpectedly closed the rename edit.'
            Assert-Ui ([FileSpacerRegression.Native]::Text($control) -eq 'AnchorA') 'Alt-right drag changed the item name.'
            $phase='rename cancellation'
            # Same input and completion check as the validated left-drag case.
            [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
            Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($control) -or ![FileSpacerRegression.Native]::IsWindowVisible($control) } 'rename cancelled without renaming' 0 5
            Assert-Ui (((Use-UiView $window {param($v) $v.Names()}) | Sort-Object) -join ';' -eq (($names | Sort-Object) -join ';')) 'The gesture renamed an item.'
        }
        Assert-Fixed $before
    } else {
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'right press captured before any drag motion' 0 5
        if($Case -eq 'AltClick') {
            Assert-Fixed $before
            [FileSpacerRegression.AltDragInput]::End(); Wait-Idle; Assert-Fixed $before
        } else {
            if($Case -eq 'ReleaseEarly') { [FileSpacerRegression.AltDragInput]::ReleaseAlt(); [FileSpacerRegression.AltDragInput]::VerifyHeld($false) }
            $phase='resizing with right held'
            $dx=[Math]::Max(96,[Math]::Abs([FileSpacerRegression.AltDragInput]::GetSystemMetrics(68))+16)
            $dy=[Math]::Max(72,[Math]::Abs([FileSpacerRegression.AltDragInput]::GetSystemMetrics(69))+16)
            $destination=Point-At ($point.X+$dx) ($point.Y+$dy)
            if($Case -eq 'Minimum') { $destination=Point-At ($work.Left+2) ($work.Top+2) }
            if($Case -eq 'Outside') { $destination=Point-At ($before.Right+35) ($before.Top+30) }
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
            if($Case -in @('Escape','Cancel')) {
                if($Case -eq 'Escape') { [FileSpacerRegression.AltDragInput]::Escape() }
                else { [void][FileSpacerRegression.Native]::Message($window,0x1f,0,0) } # WM_CANCELMODE
                Wait-Idle; Assert-Fixed $before
            }
            [FileSpacerRegression.AltDragInput]::End(); Wait-Idle
            if($Case -in @('Escape','Cancel')) { Assert-Fixed $before }
            elseif($Case -ne 'Minimum') { Assert-Resized ($before.Width+$destination.X-$point.X) ($before.Height+$destination.Y-$point.Y) }
        }
        Wait-Ui { ![FileSpacerRegression.AltDragInput]::ResizeCursor() } 'resize cursor restored after gesture completion' 100 5
        Assert-UiSelected $window $selection
        Assert-Ui (![FileSpacerRegression.NativeTargets]::MenuActive($window)) 'Alt-right release opened a context menu.'
        if($Case -eq 'Inactive') { Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow() -eq $window) 'The resized window was not activated.' }
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
