. (Join-Path $PSScriptRoot 'GridDrag.ps1')

function Get-SpacingViewMenu($Window) {
    return [FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,(Get-UiId 'IDM_RESET_BOTH_GAPS'))
}

function Assert-SpacingMenuState($Window,[bool]$Enabled) {
    $menu=Get-SpacingViewMenu $Window
    foreach ($id in @('IDM_ICON_GAP_HORIZONTAL','IDM_ICON_GAP_VERTICAL')) {
        $row=@($menu.Entries | Where-Object Command -eq (Get-UiId $id))
        Assert-Ui ($row.Count -eq 1 -and $row[0].SubMenu -ne [IntPtr]::Zero) "$id opens its own submenu"
        Assert-Ui ((($row[0].State -band 3) -eq 0) -eq $Enabled) "availability of $id in the applied view"
    }
}

function Assert-SpacingResetState($Window,$Slider,[bool]$AxisEnabled,[bool]$BothEnabled) {
    # A menu renderer may populate a submenu only when it is displayed.
    $command=$Slider.Child.Entries[1].Command
    $child=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,$command)
    Assert-Ui ($child -and $child.Window -eq $Slider.Child.Window) 'same axis submenu while checking its reset'
    Assert-Ui ((($child.Entries[1].State -band 3) -eq 0) -eq $AxisEnabled) 'availability of the displayed axis reset'
    $Menu=Get-SpacingViewMenu $Window
    $both=$Menu.Entries | Where-Object Command -eq (Get-UiId 'IDM_RESET_BOTH_GAPS')
    Assert-Ui ($Menu.Window -eq $Slider.Menu -and $both -and ((($both.State -band 3) -eq 0) -eq $BothEnabled)) 'availability of the combined reset'
}

function Choose-SpacingCommand($Window,$Id,$ExistingMenu) {
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $menu=Get-SpacingViewMenu $Window
    if ($ExistingMenu) { Assert-Ui ($menu.Window -eq $ExistingMenu) 'original View menu still open' }
    if ($Id -ne (Get-UiId 'IDM_RESET_BOTH_GAPS')) {
        $axis=if ($Id -eq (Get-UiId 'IDM_RESET_VERTICAL_GAP')) {'VERTICAL'} else {'HORIZONTAL'}
        $entry=$menu.Entries | Where-Object Command -eq (Get-UiId "IDM_ICON_GAP_$axis")
        [FileSpacerViewMenuTests.MenuUi]::Hover($menu,[array]::IndexOf($menu.Entries,$entry))
        $found=@{}
        Wait-Ui {$found.Menu=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,$Id); $null -ne $found.Menu} 'axis submenu opened'
        $menu=$found.Menu
    }
    $i=[array]::IndexOf($menu.Entries.Command,[uint32]$Id)
    Assert-Ui ($i -ge 0 -and ($menu.Entries[$i].State -band 3) -eq 0 -and $menu.Entries[$i].Label) 'enabled named spacing command'
    [FileSpacerViewMenuTests.MenuUi]::Choose($menu,$i,$Window)
    return $menu.Entries[$i].Label
}

function Open-SpacingSlider([IntPtr]$Window,[int]$Axis,$ExistingMenu,[switch]$ClickParent) {
    $id=if ($Axis) {'IDM_ICON_GAP_VERTICAL'} else {'IDM_ICON_GAP_HORIZONTAL'}
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $menu=Get-SpacingViewMenu $Window
    if ($ExistingMenu) { Assert-Ui ($menu.Window -eq $ExistingMenu) 'same menu before hovering over the other axis' }
    $row=[array]::IndexOf($menu.Entries.Command,[uint32](Get-UiId $id))
    Assert-Ui ($row -ge 0 -and ($menu.Entries[$row].State -band 3) -eq 0) 'spacing entry enabled'
    $resetId=Get-UiId $(if ($Axis) {'IDM_RESET_VERTICAL_GAP'} else {'IDM_RESET_HORIZONTAL_GAP'})
    if ($ClickParent) {[FileSpacerViewMenuTests.MenuUi]::Choose($menu,$row,$Window)}
    else {[FileSpacerViewMenuTests.MenuUi]::Hover($menu,$row)}
    $pidValue = [uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$pidValue)
    $found = @{}
    Wait-Ui {
        $found.Child=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,$resetId)
        if (!$found.Child) {return $false}
        foreach ($popup in [FileSpacerRegression.Native]::Top('Static',[NullString]::Value,$pidValue)) {
            $tracks = [FileSpacerRegression.Native]::Children($popup,'msctls_trackbar32')
            if ($tracks.Length -eq 1 -and [FileSpacerRegression.Native]::IsWindowVisible($popup) -and
                [Math]::Abs([FileSpacerRegression.Native]::Bounds($popup).Bottom - $found.Child.Entries[1].Bounds.Top) -le 2) {
                $found.Popup = $popup
                $found.Track = $tracks[0]
                return $true
            }
        }
        return $false
    } 'axis submenu exposes its native trackbar'
    $bounds=[FileSpacerRegression.Native]::Bounds($found.Popup)
    $slot=$found.Child.Entries[0].Bounds
    Assert-Ui ($bounds.Top -eq $slot.Top -and $bounds.Bottom -eq $slot.Bottom -and
        $bounds.Left -eq $slot.Left -and $bounds.Right -eq $slot.Right) 'the whole slider fits inside its menu row'
    $reset=$found.Child.Entries[1]
    Assert-Ui ($found.Child.Entries.Count -eq 2 -and $reset.Command -eq $resetId -and
        $bounds.Left -eq $reset.Bounds.Left -and $bounds.Right -eq $reset.Bounds.Right) 'slider directly above the reset in the same column'
    $root=Get-SpacingViewMenu $Window
    Assert-Ui ($root.Window -eq $menu.Window) 'opening a slider preserves the active View menu'
    $found.Menu=$root.Window
    return $found
}

# GetSpacing returns the displayed pitch. When the DPI is fractional, multiple
# integer Shell reference pitches can map to this same ListView spacing at 100%.
# Project each possible reference through the requested step and the same DPI
# rounding. Require an exactly representable result at both ends of the scale.
function Test-SpacingPitch([int]$Standard,[int]$Step,[int]$Dpi,[int]$Actual) {
    if ($Dpi -eq 96) {return $Actual -eq [Math]::Round($Standard*$Step/10.0,[MidpointRounding]::AwayFromZero)}
    $margin=[int][Math]::Ceiling($Dpi/192.0)
    foreach ($reference in (($Standard-$margin)..($Standard+$margin))) {
        $units=[Math]::Round($reference*96.0/$Dpi,[MidpointRounding]::AwayFromZero)
        if ([Math]::Round($units*$Dpi/96.0,[MidpointRounding]::AwayFromZero) -ne $Standard) {continue}
        $scaled=[Math]::Round($reference*$Step/10.0,[MidpointRounding]::AwayFromZero)
        $units=[Math]::Round($scaled*96.0/$Dpi,[MidpointRounding]::AwayFromZero)
        if ([Math]::Round($units*$Dpi/96.0,[MidpointRounding]::AwayFromZero) -eq $Actual) {return $true}
    }
    return $false
}

function Read-Spacing([IntPtr]$Window) {
    return Use-UiView $Window {param($v) $v.Spacing}
}

function Close-SpacingOutside([IntPtr]$Window,$Slider) {
    [FileSpacerViewMenuTests.MenuUi]::Outside($Window)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($Slider.Popup) } 'outside click dismisses the popup'
}

# Sample text edges on a different View row. HWND visibility alone cannot tell
# whether the menu is actually drawn on the user's screen.
function Get-SpacingMenuInk($Menu) {
    $r=$Menu.Entries[0].Bounds
    $bitmap=New-Object Drawing.Bitmap(($r.Right-$r.Left-8),($r.Bottom-$r.Top-4))
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    $ink=New-Object 'Collections.Generic.HashSet[int]'
    try {
        $graphics.CopyFromScreen($r.Left+4,$r.Top+2,0,0,$bitmap.Size)
        for ($y=0;$y -lt $bitmap.Height;$y++) {
            $previous=$bitmap.GetPixel(0,$y).G
            for ($x=1;$x -lt $bitmap.Width;$x++) {
                $value=$bitmap.GetPixel($x,$y).G
                if ([Math]::Abs([int]$value-$previous) -gt 48) { [void]$ink.Add($y*$bitmap.Width+$x) }
                $previous=$value
            }
        }
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
    return ,$ink
}
