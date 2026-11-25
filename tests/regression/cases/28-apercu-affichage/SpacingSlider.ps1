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
    $popupName=Choose-SpacingCommand $Window (Get-UiId $id) $ExistingMenu
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
    } 'native trackbar popup'
    $root=Get-SpacingViewMenu $Window
    Assert-Ui ($null -ne $root) 'opening a slider preserves the active View menu'
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
