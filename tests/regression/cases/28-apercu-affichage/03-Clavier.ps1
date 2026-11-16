# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
$failure=$null
try {
    Initialize-Ui 'view-preview-keyboard' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 300
    $window=Open-UiFolder $folder 300
    Reset-PreviewMode $window
    $modes=Get-PreviewModes $window
    $initial=Read-PreviewMode $window
    $state=Set-PreviewState $window
    $popup=Open-PreviewMenu $window
    $modeCount=$popup.Entries.Count
    $all=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    Assert-Ui ($null -ne $all -and $all.Entries.Count -gt $modeCount) 'The native View menu has no entry after the display modes.'
    $all.Entries|ForEach-Object {[pscustomobject]@{Command=$_.Command;Label=$_.Label;State=$_.State}}|ConvertTo-Json -Depth 3|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'menu-entries.json') -Encoding UTF8
    Assert-Ui ($all.Entries[$modeCount].Command-eq(Get-UiId 'IDM_ICON_PLACEMENT_MENU')) 'Placement must follow the display modes.'
    # Down highlights the entries in order; the menu skips its separators.
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x28,4);Wait-PreviewMode $window $modes[3]
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x28,$modeCount-3);Wait-PreviewMode $window $initial
    Assert-PreviewState $window $state
    Write-Host 'PASS: keyboard highlight previews a mode and leaves it on an entry outside the modes.'
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x0D,1)
    Wait-Ui {$null-ne[FileSpacerViewMenuTests.MenuUi]::Active($window,$true,(Get-UiId 'IDM_FREE_PLACEMENT_ICONS'))} 'Enter opens Placement submenu'
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x24,1) # Home selects Automatic.
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x0D,1)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window $initial
    # Automatic placement may legitimately move the icons.
    Wait-Ui {$now=Read-PreviewState $window; $now.Selected -eq $state.Selected -and $now.Focus -eq $state.Focus} 'original selection and focus after the chosen entry' 250
    Wait-Ui {Use-UiView $window {param($view) $view.AutoArrange-and!$view.SnapToGrid}} 'keyboard choice applies Automatic only'
    Assert-PlacementChoice 'IDM_AUTO_ARRANGE_ICONS'
    Wait-PreviewMode $window $initial
    Write-Host 'PASS: keyboard navigation enters Placement, runs its exclusive Automatic choice and keeps the original display mode.'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $initial
    Write-Host 'PASS: the original mode, not the keyboard preview, survives reopening.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
