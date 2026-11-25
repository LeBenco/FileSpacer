. (Join-Path $PSScriptRoot 'GridDrag.ps1')

function Get-SpacingViewMenu($Window) {
    return [FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,(Get-UiId 'IDM_RESET_SPACING_MENU'))
}

function Get-SpacingResetEntry($Menu) {
    $entry=@($Menu.Entries | Where-Object Command -eq (Get-UiId 'IDM_RESET_SPACING_MENU'))
    Assert-Ui ($entry.Count -eq 1 -and $entry[0].SubMenu -ne [IntPtr]::Zero) 'spacing reset submenu in View'
    return $entry[0]
}

function Assert-SpacingMenuState($Window,[bool]$Enabled) {
    $menu=Get-SpacingViewMenu $Window
    foreach ($id in @('IDM_ICON_GAP_HORIZONTAL','IDM_ICON_GAP_VERTICAL')) {
        $row=@($menu.Entries | Where-Object Command -eq (Get-UiId $id))
        Assert-Ui ($row.Count -eq 1 -and $row[0].SubMenu -eq [IntPtr]::Zero) "$id directly in View"
        Assert-Ui ((($row[0].State -band 3) -eq 0) -eq $Enabled) "availability of $id in the applied view"
    }
}

function Assert-SpacingResetState($Menu,[bool]$Horizontal,[bool]$Vertical) {
    $entry=Get-SpacingResetEntry $Menu
    $expected=@{IDM_RESET_HORIZONTAL_GAP=$Horizontal; IDM_RESET_VERTICAL_GAP=$Vertical; IDM_RESET_BOTH_GAPS=($Horizontal -or $Vertical)}
    foreach ($id in $expected.Keys) {
        $state=[FileSpacerViewMenuTests.MenuUi]::CommandState($entry.SubMenu,(Get-UiId $id))
        Assert-Ui ((($state -band 3) -eq 0) -eq $expected[$id]) "availability of $id in the reset submenu"
    }
}

function Choose-SpacingCommand($Window,$Id,$ExistingMenu) {
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $menu=Get-SpacingViewMenu $Window
    if ($ExistingMenu) { Assert-Ui ($menu.Window -eq $ExistingMenu) 'original View menu still open' }
    if ($Id -in @((Get-UiId 'IDM_RESET_HORIZONTAL_GAP'),(Get-UiId 'IDM_RESET_VERTICAL_GAP'),(Get-UiId 'IDM_RESET_BOTH_GAPS'))) {
        $entry=Get-SpacingResetEntry $menu
        [FileSpacerViewMenuTests.MenuUi]::Move(($entry.Bounds.Left+$entry.Bounds.Right)/2,($entry.Bounds.Top+$entry.Bounds.Bottom)/2)
        $found=@{}
        Wait-Ui {$found.Menu=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,$Id); $null -ne $found.Menu} 'reset choices opened'
        $menu=$found.Menu
        Assert-Ui ($menu.Entries.Count -eq 3) 'three reset choices'
    }
    $i=[array]::IndexOf($menu.Entries.Command,[uint32]$Id)
    Assert-Ui ($i -ge 0 -and ($menu.Entries[$i].State -band 3) -eq 0 -and $menu.Entries[$i].Label) 'enabled named spacing command'
    [FileSpacerViewMenuTests.MenuUi]::Choose($menu,$i,$Window)
    return $menu.Entries[$i].Label
}

function Open-SpacingSlider([IntPtr]$Window,[int]$Axis,$ExistingMenu) {
    $id=if ($Axis) {'IDM_ICON_GAP_VERTICAL'} else {'IDM_ICON_GAP_HORIZONTAL'}
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $menu=Get-SpacingViewMenu $Window
    if ($ExistingMenu) { Assert-Ui ($menu.Window -eq $ExistingMenu) 'same menu before hovering over the other axis' }
    $row=[array]::IndexOf($menu.Entries.Command,[uint32](Get-UiId $id))
    Assert-Ui ($row -ge 0 -and ($menu.Entries[$row].State -band 3) -eq 0) 'spacing entry enabled'
    $popupName=$menu.Entries[$row].Label
    [FileSpacerViewMenuTests.MenuUi]::Hover($menu,$row)
    $pidValue = [uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$pidValue)
    $found = @{}
    Wait-Ui {
        foreach ($popup in [FileSpacerRegression.Native]::Top('Static',$popupName,$pidValue)) {
            $tracks = [FileSpacerRegression.Native]::Children($popup,'msctls_trackbar32')
            if ($tracks.Length -eq 1 -and [FileSpacerRegression.Native]::IsWindowVisible($popup)) {
                $found.Popup = $popup
                $found.Track = $tracks[0]
                return $true
            }
        }
        return $false
    } 'hover alone opens the native trackbar'
    $bounds=[FileSpacerRegression.Native]::Bounds($found.Popup)
    $entry=$menu.Entries[$row].Bounds
    $middle=[int](($entry.Top+$entry.Bottom)/2)
    Assert-Ui (($bounds.Left -eq $entry.Right -or $bounds.Right -eq $entry.Left) -and
        $bounds.Top -le $middle -and $bounds.Bottom -gt $middle) 'slider beside its own menu row'
    $root=Get-SpacingViewMenu $Window
    Assert-Ui ($root.Window -eq $menu.Window) 'opening a slider preserves the active View menu'
    $found.Menu=$root.Window
    return $found
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
