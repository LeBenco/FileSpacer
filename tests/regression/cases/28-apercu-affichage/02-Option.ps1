# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
$failure=$null
try {
    Initialize-Ui 'view-preview-setting' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 100
    $window=Open-UiFolder $folder 100
    Reset-PreviewMode $window
    $modes=Get-PreviewModes $window
    $initial=Read-PreviewMode $window
    $sheet=Show-UiSettings $window;$page=Set-UiPage 1
    $control=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_VIEW_MENU_PREVIEW_ENABLED'))
    Assert-Ui ($control -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::Message($control,0xf0,0,0) -eq 1) 'Preview must be enabled by default in Display settings.'
    $label=[FileSpacerRegression.Native]::Text($control) -replace '&',''
    Assert-Ui ($label -in @('Preview display modes in the View menu',"Prévisualiser les modes d’affichage dans le menu Affichage")) 'Preview label does not use the expected localized View menu name.'
    $groups=@([FileSpacerRegression.Native]::Children($page,'Button')|Where-Object{[FileSpacerRegression.Native]::Text($_) -in @('View menu','Menu Affichage')})
    Assert-Ui ($groups.Count -eq 1) 'Preview option needs its own localized group.'
    $box=[FileSpacerRegression.Native]::Bounds($groups[0]);$check=[FileSpacerRegression.Native]::Bounds($control)
    Assert-Ui ($check.Left -gt $box.Left -and $check.Right -lt $box.Right -and $check.Top -gt $box.Top -and $check.Bottom -lt $box.Bottom) 'Preview checkbox extends outside its group.'
    $edit=[FileSpacerRegression.Native]::Bounds([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_FOLDER_WINDOW_WIDTH')))
    Assert-Ui ($edit.Top -gt $box.Bottom) 'Preview group overlaps the default window dimensions.'
    Save-PreviewSettingsImage $sheet
    Set-UiCheck $page 'IDC_VIEW_MENU_PREVIEW_ENABLED' $false
    Press-UiSettings 5
    Assert-Ui ($null -eq (Get-UiPreference 'ViewMenuPreviewEnabled')) 'Cancel saved the preview option.'
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0);Wait-PreviewMode $window $modes[0]
    [FileSpacerViewMenuTests.MenuUi]::Escape($window);Wait-PreviewMenuClosed $window;Wait-PreviewMode $window $initial
    $sheet=Show-UiSettings $window;$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_VIEW_MENU_PREVIEW_ENABLED' $false
    Press-UiSettings 4
    Wait-Ui {(Get-UiPreference 'ViewMenuPreviewEnabled') -eq 0} 'Apply stores disabled preview'
    Press-UiSettings 5
    $popup=Assert-PreviewDisabled $window $initial
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,0,$window)
    Wait-PreviewMenuClosed $window;Wait-PreviewMode $window $modes[0]
    Close-UiFolder $window
    $window=Open-UiFolder $folder 100
    Wait-PreviewMode $window $modes[0]
    $sheet=Show-UiSettings $window;$page=Set-UiPage 1
    $control=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_VIEW_MENU_PREVIEW_ENABLED'))
    Assert-Ui ([FileSpacerRegression.Native]::Message($control,0xf0,0,0) -eq 0) 'Disabled preview did not survive reopening.'
    Set-UiCheck $page 'IDC_VIEW_MENU_PREVIEW_ENABLED' $true
    Press-UiSettings 3
    Wait-Ui {(Get-UiPreference 'ViewMenuPreviewEnabled') -eq 1} 'OK stores enabled preview'
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,4);Wait-PreviewMode $window $modes[4]
    [FileSpacerViewMenuTests.MenuUi]::Escape($window);Wait-PreviewMenuClosed $window;Wait-PreviewMode $window $modes[0]
    Write-Host 'PASS: dedicated default-on option; Cancel, Apply, disabled hover with working clicks, persistence and immediate re-enabling.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
