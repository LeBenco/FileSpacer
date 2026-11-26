. (Join-Path $PSScriptRoot 'GridDrag.ps1')

function Get-SpacingViewMenu($Window) {
    return [FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,(Get-UiId 'IDM_RESET_BOTH_GAPS'))
}

function Assert-SpacingMenuState($Window,[bool]$Enabled) {
    $menu=Get-SpacingViewMenu $Window
    foreach ($id in @('IDM_ICON_GAP_HORIZONTAL','IDM_ICON_GAP_VERTICAL')) {
        $row=@($menu.Entries | Where-Object Command -eq (Get-UiId $id))
        Assert-Ui ($row.Count -eq 1 -and $row[0].SubMenu -eq [IntPtr]::Zero) "$id opens an independent menu"
        Assert-Ui ((($row[0].State -band 3) -eq 0) -eq $Enabled) "availability of $id in the applied view"
    }
}

function Assert-SpacingResetState($Window,$Slider,[bool]$AxisEnabled) {
    $command=$Slider.Child.Entries[1].Command
    $child=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,$command)
    Assert-Ui ($child -and $child.Window -eq $Slider.Child.Window) 'same axis submenu while checking its reset'
    Assert-Ui ((($child.Entries[1].State -band 3) -eq 0) -eq $AxisEnabled) 'availability of the displayed axis reset'
}

function Assert-CombinedSpacingResetState($Menu,[bool]$Enabled) {
    $both=$Menu.Entries | Where-Object Command -eq (Get-UiId 'IDM_RESET_BOTH_GAPS')
    Assert-Ui ($both -and ((($both.State -band 3) -eq 0) -eq $Enabled)) 'availability of the combined reset'
}

function Choose-SpacingCommand($Window,$Id,$ExistingMenu) {
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $menu=Get-SpacingViewMenu $Window
    if ($ExistingMenu) { Assert-Ui ($menu.Window -eq $ExistingMenu) 'original View menu still open' }
    if ($Id -ne (Get-UiId 'IDM_RESET_BOTH_GAPS')) {
        $axis=if ($Id -eq (Get-UiId 'IDM_RESET_VERTICAL_GAP')) {'VERTICAL'} else {'HORIZONTAL'}
        $slider=Open-SpacingSlider $Window ([int]($axis -eq 'VERTICAL')) $menu.Window
        $menu=$slider.Child
    }
    $i=[array]::IndexOf($menu.Entries.Command,[uint32]$Id)
    Assert-Ui ($i -ge 0 -and ($menu.Entries[$i].State -band 3) -eq 0 -and $menu.Entries[$i].Label) 'enabled named spacing command'
    [FileSpacerViewMenuTests.MenuUi]::Choose($menu,$i,$Window)
    if ($Id -ne (Get-UiId 'IDM_RESET_BOTH_GAPS')) {
        $normalPosition=[FileSpacerRegression.Native]::Message($slider.Track,0x402,0,0)-20
        Wait-Ui { [FileSpacerRegression.Native]::Message($slider.Track,0x400,0,0) -eq $normalPosition } 'reset applies 100% and keeps the axis open'
        Close-SpacingOutside $Window $slider
    }
    return $menu.Entries[$i].Label
}

function Open-SpacingSlider([IntPtr]$Window,[int]$Axis,$ExistingMenu) {
    $id=if ($Axis) {'IDM_ICON_GAP_VERTICAL'} else {'IDM_ICON_GAP_HORIZONTAL'}
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $menu=Get-SpacingViewMenu $Window
    if ($ExistingMenu) { Assert-Ui ($menu.Window -eq $ExistingMenu) 'select an axis in the reopened View menu' }
    $row=[array]::IndexOf($menu.Entries.Command,[uint32](Get-UiId $id))
    Assert-Ui ($row -ge 0 -and ($menu.Entries[$row].State -band 3) -eq 0) 'spacing entry enabled'
    $resetId=Get-UiId $(if ($Axis) {'IDM_RESET_VERTICAL_GAP'} else {'IDM_RESET_HORIZONTAL_GAP'})
    $viewBounds=[FileSpacerRegression.Native]::Bounds($menu.Window)
    $entry=$menu.Entries[$row].Bounds
    $cursor=[Drawing.Point]::new([int][Math]::Truncate(($entry.Left+$entry.Right)/2),
        [int][Math]::Truncate(($entry.Top+$entry.Bottom)/2))
    $workArea=[FileSpacerRegression.Native]::WorkArea($menu.Window)
    [FileSpacerViewMenuTests.MenuUi]::Choose($menu,$row,$Window)
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
    $axisBounds=[FileSpacerRegression.Native]::Bounds($found.Child.Window)
    $left=[Math]::Max($workArea.Left,[Math]::Min($workArea.Right-$axisBounds.Width,$cursor.X-$axisBounds.Width/2))
    $top=[Math]::Max($workArea.Top,[Math]::Min($workArea.Bottom-$axisBounds.Height,$cursor.Y-$axisBounds.Height/2))
    $tolerance=[Math]::Ceiling(4*[FileSpacerRegression.Native]::GetDpiForWindow($Window)/96.0)
    Assert-Ui ([Math]::Abs($axisBounds.Left-$left) -le $tolerance -and [Math]::Abs($axisBounds.Top-$top) -le $tolerance) (
        "axis popup centered at $cursor within the work area; expected ($left, $top), got ("+
        $axisBounds.Left+', '+$axisBounds.Top+')')
    $bounds=[FileSpacerRegression.Native]::Bounds($found.Popup)
    $slot=$found.Child.Entries[0].Bounds
    Assert-Ui ($bounds.Top -eq $slot.Top -and $bounds.Bottom -eq $slot.Bottom -and
        $bounds.Left -eq $slot.Left -and $bounds.Right -eq $slot.Right) 'the whole slider fits inside its menu row'
    $reset=$found.Child.Entries[1]
    Assert-Ui ($found.Child.Entries.Count -eq 2 -and $reset.Command -eq $resetId -and
        $bounds.Left -eq $reset.Bounds.Left -and $bounds.Right -eq $reset.Bounds.Right) 'slider directly above the reset in the same column'
    if ($env:FILESPACER_TEST_NILESOFT -eq '1') {
        Assert-Ui (($reset.Type -band 0x100) -ne 0) 'Nilesoft draws the reset in the independent menu'
    }
    Assert-Ui (!(Get-SpacingViewMenu $Window)) 'the menu of the selected axis replaced View'
    $found.ViewBounds=$viewBounds
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

function Close-SpacingOutside([IntPtr]$Window,$Slider,[switch]$KeepViewMenu) {
    [FileSpacerViewMenuTests.MenuUi]::Outside($Window)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($Slider.Popup) } 'outside click dismisses the popup'
    $found=@{}
    Wait-Ui { $found.Menu=Get-SpacingViewMenu $Window; $null -ne $found.Menu } 'outside click reopens View'
    $bounds=[FileSpacerRegression.Native]::Bounds($found.Menu.Window)
    Assert-Ui ($bounds.Left -eq $Slider.ViewBounds.Left -and $bounds.Top -eq $Slider.ViewBounds.Top) 'View reopened at its original anchor'
    if ($KeepViewMenu) {return $found.Menu}
    [FileSpacerViewMenuTests.MenuUi]::Escape($Window)
    Wait-PreviewMenuClosed $Window
}

# Sample the reset row throughout the gesture, in addition to the HWND checks.
function Get-SpacingMenuInk($Menu) {
    $r=$Menu.Entries[1].Bounds
    $bitmap=New-Object Drawing.Bitmap(($r.Right-$r.Left-8),($r.Bottom-$r.Top-4))
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    $ink=New-Object 'Collections.Generic.HashSet[int]'
    try {
        $graphics.CopyFromScreen($r.Left+4,$r.Top+2,0,0,$bitmap.Size)
        for ($y=0;$y -lt $bitmap.Height;$y++) {
            $previous=$bitmap.GetPixel(0,$y).G
            for ($x=1;$x -lt $bitmap.Width;$x++) {
                $value=$bitmap.GetPixel($x,$y).G
                # The themed reset text can be antialiased over a dark translucent background.
                if ([Math]::Abs([int]$value-$previous) -gt 20) { [void]$ink.Add($y*$bitmap.Width+$x) }
                $previous=$value
            }
        }
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
    return ,$ink
}
