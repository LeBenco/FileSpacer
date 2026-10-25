param(
    [string]$ProjectRoot,
    [Parameter(Mandatory=$true)][ValidateSet('Mode','Unselected','Inactive','ReleaseAlt','Escape','AltClick','PlainClick','PlainRectangle','DoubleAlt','DoubleAltMotion','Header','Rename','Toolbar','Search','QuickAccess','MultipleIcon','Cell','ReleaseEarly','AltBackgroundClick','MenuButton','PlainDouble','Scrollbar','PlainDrag')][string]$Case,
    [uint32]$ViewMode=0,[string]$ViewName='', [int]$IconSize=-1,[bool]$FullNamesOnSelection=$true
)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $PSScriptRoot 'NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
if($Case -in @('DoubleAlt','DoubleAltMotion','QuickAccess')) { Add-Type -Path (Join-Path $PSScriptRoot 'WindowEvidence.cs') }
$testFailure=$null; $window=[IntPtr]::Zero; $properties=[IntPtr]::Zero
$phase='initialization'; $list=[IntPtr]::Zero; $other=[IntPtr]::Zero
$state=@{Status='RUNNING'; Case=$Case; Phase=$phase; Error=$null; Before=$null; After=$null;
    ViewWindow=$null; Mode=$null; IconSize=$null; RequestedMode=$ViewMode; RequestedFullNames=$FullNamesOnSelection;
    StoredFullNames=$null; Children=$null; SelectionBefore=$null; SelectionAfter=$null;
    InputBeforeRelease=$null; InputAfterRelease=$null; Evidence=@{}}

$script:diagnosticProcessId=[uint32]0
$script:targetProcessId=[uint32]0
$quickAccessWindowsBefore=$null
function Save-AltWindowEvidence([string]$Stage) {
    if($Case -notin @('DoubleAlt','DoubleAltMotion','QuickAccess') -or $window -eq [IntPtr]::Zero) { return }
    try {
        if($script:diagnosticProcessId -eq 0) {
            [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$script:diagnosticProcessId)
        }
        $snapshot=[FileSpacerRegression.WindowEvidence]::Capture($window,$script:diagnosticProcessId,'OtherC')
        $snapshot | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN ('windows-'+$Stage+'.json')) -Encoding UTF8
    } catch { Write-Warning ('Cannot collect window evidence: '+$_.Exception.Message) }
}

function Point-At([int]$X,[int]$Y) { $p=New-Object FileSpacerRegression.MousePoint; $p.X=$X; $p.Y=$Y; return $p }
function Assert-Fixed($Baseline) {
    $r=[FileSpacerRegression.Native]::Bounds($window)
    Assert-Ui ($r.Left -eq $Baseline.Left -and $r.Top -eq $Baseline.Top -and $r.Width -eq $Baseline.Width -and $r.Height -eq $Baseline.Height) 'Window geometry changed during a gesture which must not move it.'
}
function Observe-Fixed($Baseline) {
    Wait-Ui {
        Assert-Ui (![FileSpacerRegression.AltDragInput]::Moving($window)) 'An excluded/native Shell gesture entered a window move loop.'
        Assert-Fixed $Baseline
        return $true
    } 'unchanged window geometry and no native move loop' 200 5
}
function Assert-Moved($Baseline) {
    Wait-Ui {
        $r=[FileSpacerRegression.Native]::Bounds($window)
        $r.Left -gt $Baseline.Left -and $r.Top -gt $Baseline.Top -and $r.Width -eq $Baseline.Width -and $r.Height -eq $Baseline.Height
    } 'real window displacement right/down without resizing' 200 5
}
function Wait-Idle {
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'mouse capture and native move ended' 0 5
}
function Background-Point {
    $r=[FileSpacerRegression.Native]::ClientBounds($viewWindow)
    Assert-Ui ($r.Width -ge 200 -and $r.Height -ge 160) 'The Shell view is too small for the validated gesture.'
    $p=Point-At ([int]($r.Left+$r.Width/2)) ([int]($r.Top+3*$r.Height/4))
    if($list -ne [IntPtr]::Zero) { [FileSpacerRegression.NativeTargets]::Background($list,$p.X,$p.Y) }
    return $p
}
function Item-Point([string]$Name,[bool]$Selected,[bool]$Cell=$false) {
    Assert-Ui ($list -ne [IntPtr]::Zero) 'This item-specific case requires the native ListView observed in the validated configuration.'
    $index=Use-UiView $window {param($view) $view.IndexOf($Name)}
    $native=[FileSpacerRegression.NativeTargets]::Item($list,$index,$itemCount,$Selected,$Cell)
    $state.Evidence.Target=@{Name=$Name; Index=$index; X=$native.X; Y=$native.Y; Cell=$Cell; Selected=$Selected}
    return Point-At $native.X $native.Y
}
function Control-Point([IntPtr]$Control) {
    $r=[FileSpacerRegression.Native]::ClientBounds($Control)
    $native=[FileSpacerRegression.NativeTargets]::Control($Control,$r.Left,$r.Top,$r.Right,$r.Bottom)
    return Point-At $native.X $native.Y
}
function Toolbar-Point([string]$Command) {
    $id=Get-UiId $Command
    $bars=@([FileSpacerRegression.Native]::Children($window,'ToolbarWindow32') | Where-Object {
        [FileSpacerRegression.Native]::IsWindowVisible($_) -and [FileSpacerRegression.Native]::Message($_,0x419,$id,0) -ge 0
    })
    Assert-Ui ($bars.Count -eq 1) "Expected one visible toolbar for $Command."
    $native=[FileSpacerRegression.NativeTargets]::Toolbar($bars[0],$id)
    return Point-At $native.X $native.Y
}
function Begin-Gesture($Point,[bool]$Alt=$true,[bool]$Double=$false) {
    $state.Evidence.StartPoint=@{X=$Point.X; Y=$Point.Y; Alt=$Alt; Double=$Double}
    [FileSpacerRegression.AltDragInput]::Begin($window,$Point,$Alt,$Double)
    [FileSpacerRegression.AltDragInput]::VerifyHeld($Alt)
}
function Start-Move($Point,[bool]$ReleaseEarly=$false) {
    Begin-Gesture $Point
    if($ReleaseEarly) {
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'Alt press reached DragDetect before modifier release' 0 5
        [FileSpacerRegression.AltDragInput]::ReleaseAlt()
        [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
    }
    [FileSpacerRegression.AltDragInput]::Move((Point-At ($Point.X+32) ($Point.Y+24)))
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Moving($window) } 'native window move started' 0 5
}
function Finish-Move($Point,[bool]$Alt=$true) {
    [FileSpacerRegression.AltDragInput]::VerifyHeld($Alt)
    [FileSpacerRegression.AltDragInput]::Move((Point-At ($Point.X+96) ($Point.Y+72)))
    Start-Sleep -Milliseconds 100
    $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Idle
    Assert-Moved $before
}
function Test-AltProperties([IntPtr]$Dialog) {
    $dialogProcessId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Dialog,[ref]$dialogProcessId)
    if($dialogProcessId -ne $script:targetProcessId -or
            [FileSpacerRegression.Native]::Class($Dialog) -ne '#32770' -or
            ![FileSpacerRegression.Native]::IsWindowVisible($Dialog) -or
            [FileSpacerRegression.Native]::Text($Dialog) -notlike '*OtherC*' -or
            @([FileSpacerRegression.Native]::Children($Dialog,'SysTabControl32')).Count -ne 1) { return $false }
    # The Shell may own this sheet through an intermediate window/thread.
    # Prove the concrete target using its process, name and exact parent path.
    $texts=@([FileSpacerRegression.Native]::Children($Dialog,'Edit') | ForEach-Object { [FileSpacerRegression.Native]::Text($_) })
    return ($texts -contains 'OtherC') -and @($texts | Where-Object {
        [string]::Equals($_,$folder,[StringComparison]::OrdinalIgnoreCase)
    }).Count -eq 1
}
function Assert-NoNewQuickAccessWindow {
    if($Case -ne 'QuickAccess') { return }
    $newWindows=@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,$script:targetProcessId) | Where-Object { $quickAccessWindowsBefore -notcontains $_.ToInt64() })
    $state.Evidence.UnexpectedWindows=@($newWindows | ForEach-Object { @{Handle=$_.ToInt64(); Title=[FileSpacerRegression.Native]::Text($_)} })
    # Track only new windows of the already verified Debug test process. Their
    # closure must not turn an unintended navigation into a successful test.
    foreach($createdWindow in $newWindows) {
        Register-UiWindowProcess $createdWindow
        if(!$script:UiWindows.Contains($createdWindow)) { [void]$script:UiWindows.Add($createdWindow) }
    }
    Assert-Ui ($newWindows.Count -eq 0) 'The excluded quick-access gesture unexpectedly opened a folder window.'
}
function Close-Properties {
    if($script:properties -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:properties)) {
        Assert-Ui (Test-AltProperties $script:properties) 'Properties dialog no longer matches the tested process, item name and parent path.'
        Assert-Ui ([FileSpacerRegression.Native]::PostMessage($script:properties,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot close verified Properties dialog.'
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($script:properties) } 'verified Properties closed' 0 5
        $script:properties=[IntPtr]::Zero
    }
}
try {
    Initialize-Ui ('alt-'+$Case+'-'+$ViewName) -UseApplicationDefaults
    # This case inspects the source after an ordinary double click. Keep it
    # open explicitly; default source closure is covered by navigation group 12.
    if($Case -eq 'PlainDouble') { Set-UiPreference 'KeepSourceWindowOpen' 1 }
    if($Case -eq 'Mode') { Set-UiPreference 'FullNamesOnSelection' ([int]$FullNamesOnSelection) }
    $folder=Join-Path $script:UiRoot 'GestureFixture'
    [void](New-Item -ItemType Directory -Path $folder)
    $names=@('AnchorA','AnchorB','OtherC')
    if($Case -eq 'Mode') { $names=@() }
    if($Case -eq 'PlainRectangle') { $names=@('AnchorA') }
    if($Case -eq 'Scrollbar') { $names=@(0..79 | ForEach-Object { 'Scroll{0:D3}' -f $_ }) }
    foreach($name in $names) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $name)) }
    $itemCount=$names.Count; $expectedNames=(@($names | Sort-Object) -join ';')
    $window=Open-UiFolder $folder $itemCount
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$script:targetProcessId)
    Assert-Ui ($script:targetProcessId -ne 0) 'Cannot identify the tested window process.'
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 800 -and $work.Height -ge 600) 'At least 800x600 physical pixels of work area are required.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the test window.'
    Activate-Ui $window
    if($Case -eq 'Mode') {
        $phase='display mode setup'
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize($ViewMode,$IconSize)}
        Wait-Ui { Use-UiView $window {param($view) $view.Mode -eq $ViewMode -and ($IconSize -lt 0 -or $view.IconSize -eq $IconSize)} } 'requested mode and icon size applied' 300 5
        Assert-Ui ((Get-UiPreference 'FullNamesOnSelection') -eq [int]$FullNamesOnSelection) 'Requested appearance preference was not applied.'
    }
    $viewWindow=Use-UiView $window {param($view) $view.Window}
    $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -le 1) 'Ambiguous native ListView.'
    if($lists.Count -eq 1) { $list=$lists[0] }
    $state.ViewWindow=$viewWindow.ToInt64()
    $state.Mode=Use-UiView $window {param($view) $view.Mode}
    $state.IconSize=Use-UiView $window {param($view) $view.IconSize}
    $state.StoredFullNames=Get-UiPreference 'FullNamesOnSelection'
    $state.Children=@([FileSpacerRegression.Native]::DescribeChildren($viewWindow))
    $expectedSelection=''
    if($itemCount -gt 0 -and $Case -ne 'PlainRectangle') {
        Use-UiView $window {param($view) $view.Focus(); $view.Select($names[0])}
        $expectedSelection=$names[0]
        if($Case -in @('MultipleIcon','AltBackgroundClick')) {
            Use-UiView $window {param($view) $view.Select('AnchorB',$true)}
            $expectedSelection='AnchorA;AnchorB'
        }
    }
    if($Case -eq 'PlainRectangle') { Use-UiView $window {param($view) $view.Focus(); $view.Deselect('AnchorA')} }
    Assert-UiSelected $window $expectedSelection
    $state.SelectionBefore=@(Use-UiView $window {param($view) $view.Selected()})
    $phase='target location'
    switch($Case) {
        {$_ -in @('Unselected','Inactive','AltClick','PlainClick','DoubleAlt','DoubleAltMotion','PlainDouble')} { $point=Item-Point 'OtherC' $false }
        {$_ -in @('MultipleIcon','PlainDrag')} { $point=Item-Point 'AnchorA' $true }
        'Cell' {
            Assert-Ui ($state.Mode -eq 4) 'The initial display mode must be Details for the subitem-cell case.'
            $point=Item-Point 'OtherC' $false $true
        }
        'Header' { $point=Control-Point (Get-UiControl $viewWindow 'SysHeader32') }
        'Rename' {
            [FileSpacerRegression.Native]::Keys($window,0x71,$false,$false)
            Wait-Ui { @([FileSpacerRegression.Native]::Children($viewWindow,'Edit') | Where-Object { [FileSpacerRegression.Native]::IsWindowVisible($_) }).Count -eq 1 } 'native rename edit visible' 0 5
            $rename=Get-UiControl $viewWindow 'Edit'
            Assert-Ui ([FileSpacerRegression.Native]::Text($rename) -eq 'AnchorA') 'Rename edit is not editing the fixture item.'
            $point=Control-Point $rename
        }
        'Toolbar' { $point=Toolbar-Point 'IDM_REFRESH' }
        'QuickAccess' { $point=Toolbar-Point 'IDM_QUICK_ACCESS' }
        'MenuButton' { $point=Toolbar-Point 'IDM_CONTEXT_MENU' }
        'Search' { $search=Get-UiSearchEdit $window; $searchText=[FileSpacerRegression.Native]::Text($search); $point=Control-Point $search }
        'Scrollbar' {
            Assert-Ui ($list -ne [IntPtr]::Zero) 'The scrollbar case requires the validated native ListView.'
            $state.Evidence.TopIndexBefore=[FileSpacerRegression.Native]::Message($list,0x1027,0,0)
            $native=[FileSpacerRegression.NativeTargets]::VerticalThumb($list); $point=Point-At $native.X $native.Y
        }
        default { $point=Background-Point }
    }
    if($Case -eq 'PlainRectangle') { $icon=Item-Point 'AnchorA' $false }
    if($Case -eq 'PlainDrag') { $drop=Item-Point 'OtherC' $false }
    if($Case -eq 'Inactive') {
        $otherFolder=Join-Path $script:UiRoot 'OtherWindow'; [void](New-Item -ItemType Directory -Path $otherFolder)
        $other=Open-UiFolder $otherFolder 0
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($other,[IntPtr]::Zero,($work.Right-310),($work.Bottom-210),300,200,0x14)) 'Cannot place the other window.'
        Activate-Ui $other
        Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow() -eq $other) 'Test window is not initially inactive.'
        Assert-UiSelected $window $expectedSelection
        $otherBefore=[FileSpacerRegression.Native]::Bounds($other)
        # Re-read the same named item after deactivation; do not activate its view.
        $point=Item-Point 'OtherC' $false
    }
    $before=[FileSpacerRegression.Native]::Bounds($window); $state.Before=$before
    Write-Host ('VIEW: '+($state | ConvertTo-Json -Depth 6 -Compress))
    if($Case -eq 'QuickAccess') { $quickAccessWindowsBefore=@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,$script:targetProcessId) | ForEach-Object { $_.ToInt64() }) }
    Save-AltWindowEvidence 'before-gesture'
    $phase='gesture'
    switch($Case) {
        {$_ -in @('Mode','Unselected','Inactive','MultipleIcon','Cell','ReleaseEarly')} {
            Start-Move $point ($Case -eq 'ReleaseEarly')
            Finish-Move $point ($Case -ne 'ReleaseEarly')
            if($Case -eq 'Mode') {
                # The empty gesture must succeed before any item locator is used.
                Assert-Ui ((Use-UiView $window {param($view) $view.Count}) -eq 0) 'Empty-folder stage changed the contents.'
                Assert-UiSelected $window ''
                $state.Evidence.EmptyStage=@{Before=$before; After=[FileSpacerRegression.Native]::Bounds($window); StartPoint=$state.Evidence.StartPoint}
                $phase='mode selected-icon setup'
                [void](New-Item -ItemType Directory -Path (Join-Path $folder 'AnchorA'))
                Wait-Ui { Use-UiView $window {param($view) $view.Count -eq 1 -and ($view.Names() -join ';') -eq 'AnchorA'} } 'new fixture item appears in the same view' 300 5
                # Restore only fixture placement between two already checked
                # gestures, to keep both away from work-area edges.
                Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the second-stage fixture.'
                Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA')}
                Assert-UiSelected $window 'AnchorA'
                $viewWindow=Use-UiView $window {param($view) $view.Window}
                $list=Get-UiControl $viewWindow 'SysListView32'
                $itemCount=1; $expectedNames='AnchorA'; $expectedSelection='AnchorA'
                $state.SelectionBefore=@(Use-UiView $window {param($view) $view.Selected()})
                $point=Item-Point 'AnchorA' $true
                $before=[FileSpacerRegression.Native]::Bounds($window); $state.Before=$before
                Assert-Ui ((Use-UiView $window {param($view) $view.Mode}) -eq $ViewMode) 'The requested mode changed before the selected-icon stage.'
                $phase='mode selected-icon gesture'
                Start-Move $point
                Finish-Move $point
            }
            if($Case -eq 'Inactive') {
                Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow() -eq $window) 'Alt-drag did not activate the target window.'
                $otherAfter=[FileSpacerRegression.Native]::Bounds($other)
                Assert-Ui ($otherAfter.Left -eq $otherBefore.Left -and $otherAfter.Top -eq $otherBefore.Top -and $otherAfter.Width -eq $otherBefore.Width -and $otherAfter.Height -eq $otherBefore.Height) 'The other window moved.'
            }
        }
        'ReleaseAlt' {
            Start-Move $point
            $script:motionSignature=''
            Wait-Ui {
                $r=[FileSpacerRegression.Native]::Bounds($window); $s="$($r.Left),$($r.Top),$($r.Width),$($r.Height)"
                $same=$s -eq $script:motionSignature; $script:motionSignature=$s; return $same
            } 'native geometry settled before releasing Alt' 200 5
            $atRelease=[FileSpacerRegression.Native]::Bounds($window); $state.Evidence.AtAltRelease=$atRelease
            [FileSpacerRegression.AltDragInput]::ReleaseAlt()
            Finish-Move $point $false
            # Positive movement from the original position alone is insufficient.
            Assert-Moved $atRelease
        }
        'Escape' {
            Start-Move $point
            [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
            [FileSpacerRegression.AltDragInput]::Move((Point-At ($point.X+96) ($point.Y+72)))
            Assert-Moved $before
            $state.Evidence.BeforeEscape=[FileSpacerRegression.Native]::Bounds($window)
            [FileSpacerRegression.AltDragInput]::Escape()
            Wait-Idle
            [FileSpacerRegression.AltDragInput]::End()
            Observe-Fixed $before
        }
        {$_ -in @('AltClick','AltBackgroundClick','PlainClick')} {
            Begin-Gesture $point ($Case -ne 'PlainClick')
            [FileSpacerRegression.AltDragInput]::End()
            Wait-Idle; Observe-Fixed $before
            if($Case -eq 'PlainClick') { $expectedSelection='OtherC' }
        }
        'PlainRectangle' {
            Begin-Gesture $point $false
            Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'native background selection captured' 0 5
            Observe-Fixed $before
            [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
            [FileSpacerRegression.AltDragInput]::Move($icon)
            Observe-Fixed $before
            [FileSpacerRegression.AltDragInput]::End()
            Wait-Idle; Observe-Fixed $before
            $expectedSelection='AnchorA'
        }
        {$_ -in @('DoubleAlt','DoubleAltMotion')} {
            # Native Explorer selects the pointed item before opening Properties.
            # The disabled-window-shortcuts reference is checked in 24/03.
            $expectedSelection='OtherC'
            Begin-Gesture $point $true $true
            Wait-Ui {
                $found=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$script:targetProcessId) | Where-Object { Test-AltProperties $_ })
                if($found.Count -eq 1) { $script:properties=$found[0]; return $true }; return $false
            } 'Properties for the exact fixture path with second press still held' 0 10
            $state.Evidence.Properties=$properties.ToInt64()
            if($Case -eq 'DoubleAltMotion') {
                [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
                [FileSpacerRegression.AltDragInput]::Move((Point-At ($point.X+96) ($point.Y+72)))
            }
            Observe-Fixed $before
            [FileSpacerRegression.AltDragInput]::End()
            Close-Properties
            Wait-Idle; Observe-Fixed $before
        }
        'PlainDrag' {
            Begin-Gesture $point $false
            Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'normal item press captured' 0 5
            Observe-Fixed $before
            [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
            [FileSpacerRegression.AltDragInput]::Move((Point-At ($point.X+32) ($point.Y+24)))
            Observe-Fixed $before
            [FileSpacerRegression.AltDragInput]::Move($drop)
            Start-Sleep -Milliseconds 100
            [FileSpacerRegression.AltDragInput]::End()
            Wait-Idle; Observe-Fixed $before
            Wait-Ui {
                (Test-Path -LiteralPath (Join-Path $folder 'OtherC\AnchorA') -PathType Container) -and
                    !(Test-Path -LiteralPath (Join-Path $folder 'AnchorA'))
            } 'normal Shell drop moved AnchorA into OtherC on disk' 200 10
            # The source item was really moved away; Windows owns the resulting
            # selection. Its identities/count, rather than a restored selection,
            # are the final assertions for this ordinary file operation.
            $expectedSelection=$null; $expectedNames='AnchorB;OtherC'; $itemCount=2
        }
        'PlainDouble' {
            Begin-Gesture $point $false $true
            [FileSpacerRegression.AltDragInput]::End()
            $opened=Track-UiFolder (Join-Path $folder 'OtherC')
            Assert-Ui ($opened -ne $window) 'Normal double click did not open the child folder in its own window.'
            Wait-Idle; Observe-Fixed $before
            $expectedSelection='OtherC'
        }
        default {
            Begin-Gesture $point
            Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) -or [FileSpacerRegression.NativeTargets]::MenuActive($window) } 'native excluded control received the press' 0 5
            Observe-Fixed $before
            [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
            if($Case -eq 'Scrollbar') { $destination=Point-At $point.X ($point.Y+72) }
            elseif($Case -eq 'QuickAccess') {
                # A live popup can contain real user folders. Move to an exposed
                # HTCAPTION point, not across its entries, and do not click there.
                $safe=[FileSpacerRegression.NativeTargets]::Caption($window)
                $destination=Point-At $safe.X $safe.Y
                $state.Evidence.MenuSafeDestination=@{X=$safe.X; Y=$safe.Y}
            }
            else { $destination=Point-At ($point.X+96) ($point.Y+72) }
            [FileSpacerRegression.AltDragInput]::Move($destination)
            Observe-Fixed $before
            # Cancel a menu before release so no command is accidentally invoked.
            if($Case -eq 'QuickAccess') {
                $state.Evidence.InputBeforeMenuCancel=[FileSpacerRegression.AltDragInput]::Describe($window)
                # The gesture has already been checked. Cancel its popup through
                # the documented owner message, independently of keyboard input
                # while the original mouse press is still held (WM_CANCELMODE).
                if([FileSpacerRegression.NativeTargets]::MenuActive($window)) { [void][FileSpacerRegression.Native]::Message($window,0x1f,0,0) }
                Wait-Ui { ![FileSpacerRegression.NativeTargets]::MenuActive($window) } 'quick-access popup cancelled before button release' 0 5
                [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
                $state.Evidence.InputAfterMenuCancel=[FileSpacerRegression.AltDragInput]::Describe($window)
            }
            elseif([FileSpacerRegression.NativeTargets]::MenuActive($window)) { [FileSpacerRegression.AltDragInput]::Escape() }
            $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
            [FileSpacerRegression.AltDragInput]::End()
            if([FileSpacerRegression.NativeTargets]::MenuActive($window)) { [FileSpacerRegression.AltDragInput]::Escape() }
            Wait-Ui { ![FileSpacerRegression.NativeTargets]::MenuActive($window) } 'native menu interaction ended' 0 5
            Wait-Idle; Observe-Fixed $before
            if($Case -eq 'Rename') {
                Assert-Ui ([FileSpacerRegression.Native]::IsWindowVisible($rename)) 'Alt-drag unexpectedly closed the rename edit.'
                Assert-Ui ([FileSpacerRegression.Native]::Text($rename) -eq 'AnchorA') 'Alt-drag changed the item name.'
                [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
                Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($rename) -or ![FileSpacerRegression.Native]::IsWindowVisible($rename) } 'rename cancelled without renaming' 0 5
            }
            if($Case -eq 'Scrollbar') {
                Wait-Ui { [FileSpacerRegression.Native]::Message($list,0x1027,0,0) -gt $state.Evidence.TopIndexBefore } 'native scrollbar scrolled the list without moving the window' 200 5
            }
            if($Case -eq 'Search') { Assert-Ui ([FileSpacerRegression.Native]::Text($search) -eq $searchText) 'Alt-drag changed the search query.' }
        }
    }
    if($Case -eq 'QuickAccess') { Assert-NoNewQuickAccessWindow }
    Save-AltWindowEvidence 'after-gesture'
    $phase='final state'
    $state.After=[FileSpacerRegression.Native]::Bounds($window)
    $state.InputAfterRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
    if($null -ne $expectedSelection) { Assert-UiSelected $window $expectedSelection }
    $state.SelectionAfter=@(Use-UiView $window {param($view) $view.Selected()})
    Wait-Ui {
        Use-UiView $window {param($view) $view.Count -eq $itemCount -and ($view.Names() -join ';') -eq $expectedNames}
    } 'expected folder count and item identities after the gesture' 200 5
    Assert-Ui ((Use-UiView $window {param($view) $view.Mode}) -eq $state.Mode) 'Display mode changed during the gesture.'
    Assert-Ui ((Use-UiView $window {param($view) $view.IconSize}) -eq $state.IconSize) 'Icon size changed during the gesture.'
    $state.Status='PASS'; $state.Phase='complete'
    $state | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'alt-gesture.json') -Encoding UTF8
    Write-Host "PASS: Alt gesture case=$Case, mode=$($state.Mode), selection=[$expectedSelection]."
} catch {
    $testFailure=$_; $state.Status='FAIL'; $state.Phase=$phase; $state.Error=$testFailure.Exception.Message
    Save-AltWindowEvidence 'failure-held'
    try {
        if($window -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($window)) {
            $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
            [FileSpacerRegression.AltDragInput]::End()
            $state.After=[FileSpacerRegression.Native]::Bounds($window)
            $state.InputAfterRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
            if([FileSpacerRegression.AltDragInput]::Idle($window) -and $properties -eq [IntPtr]::Zero) {
                $state.SelectionAfter=@(Use-UiView $window {param($view) $view.Selected()})
            }
        }
        $state | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'alt-gesture.json') -Encoding UTF8
    } catch { Write-Warning ('Cannot collect Alt gesture diagnostics: '+$_.Exception.Message) }
    throw $testFailure
} finally {
    $cleanupFailure=$null
    try { [FileSpacerRegression.AltDragInput]::End() } catch { $cleanupFailure=$_ }
    if($Case -eq 'QuickAccess' -and $null -ne $quickAccessWindowsBefore) {
        try { Assert-NoNewQuickAccessWindow } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } else { Write-Warning $_.Exception.Message } }
    }
    Save-AltWindowEvidence 'before-cleanup'
    try { Close-Properties } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } else { Write-Warning $_.Exception.Message } }
    try {
        if($window -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($window) -and [FileSpacerRegression.NativeTargets]::MenuActive($window)) {
            Assert-Ui ([FileSpacerRegression.NativeTargets]::GetAncestor([FileSpacerRegression.Native]::GetForegroundWindow(),3) -eq $window) 'Cannot cancel a menu owned by a background window.'
            [FileSpacerRegression.AltDragInput]::Escape()
        }
    } catch { if(!$cleanupFailure) { $cleanupFailure=$_ } else { Write-Warning $_.Exception.Message } }
    try { Stop-Ui -Failure $testFailure }
    catch { if(!$cleanupFailure) { $cleanupFailure=$_ } else { Write-Warning $_.Exception.Message } }
    Save-AltWindowEvidence 'after-cleanup'
    if($cleanupFailure) {
        if($testFailure) { Write-Warning ('Cleanup after failure: '+$cleanupFailure.Exception.Message) }
        else { throw $cleanupFailure }
    }
}
