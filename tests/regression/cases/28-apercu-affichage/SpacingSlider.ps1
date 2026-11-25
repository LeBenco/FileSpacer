. (Join-Path $PSScriptRoot 'GridDrag.ps1')

function Get-SpacingSubmenu($Window) {
    $parent=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,(Get-UiId 'IDM_ICON_SPACING_MENU'))
    $entry=@($parent.Entries | Where-Object Command -eq (Get-UiId 'IDM_ICON_SPACING_MENU'))
    Assert-Ui ($entry.Count -eq 1 -and $entry[0].SubMenu -ne [IntPtr]::Zero) 'stable icon spacing submenu'
    return $entry[0]
}

function Assert-SpacingMenuState($Window,[bool]$Enabled) {
    $entry=Get-SpacingSubmenu $Window
    foreach ($id in @('IDM_ICON_GAP_HORIZONTAL','IDM_ICON_GAP_VERTICAL')) {
        $state=[FileSpacerViewMenuTests.MenuUi]::CommandState($entry.SubMenu,(Get-UiId $id))
        Assert-Ui ((($state -band 3) -eq 0) -eq $Enabled) "availability of $id in the applied view"
    }
}

function Choose-SpacingCommand($Window,$Id,$ExistingMenu) {
    if (!$ExistingMenu) { $null=Open-PreviewMenu $Window }
    $entry=Get-SpacingSubmenu $Window
    if (!$ExistingMenu) {
        [FileSpacerViewMenuTests.MenuUi]::Move(($entry.Bounds.Left+$entry.Bounds.Right)/2,($entry.Bounds.Top+$entry.Bounds.Bottom)/2)
    }
    $found=@{}
    Wait-Ui {$found.Menu=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,$Id); $null -ne $found.Menu} 'spacing choices opened'
    if ($ExistingMenu) { Assert-Ui ($found.Menu.Window -eq $ExistingMenu) 'original submenu still open for second axis' }
    $entries=$found.Menu.Entries
    $i=[array]::IndexOf($entries.Command,[uint32]$Id)
    Assert-Ui ($i -ge 0 -and ($entries[$i].State -band 3) -eq 0 -and $entries[$i].Label) 'enabled named spacing command'
    [FileSpacerViewMenuTests.MenuUi]::Choose($found.Menu,$i,$Window)
    return $entries[$i].Label
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
    $root=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,(Get-UiId 'IDM_ICON_SPACING_MENU'))
    $child=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true,(Get-UiId $id))
    Assert-Ui ($null -ne $root -and $null -ne $child) 'opening a slider preserves the active View menu and its submenu'
    $found.Root=$root.Window
    $found.Menu=$child.Window
    return $found
}

function Read-Spacing([IntPtr]$Window) {
    return Use-UiView $Window {param($v) $v.Spacing}
}

function Close-SpacingOutside([IntPtr]$Window,$Slider) {
    [FileSpacerViewMenuTests.MenuUi]::Outside($Window)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($Slider.Popup) } 'outside click dismisses the popup'
}
