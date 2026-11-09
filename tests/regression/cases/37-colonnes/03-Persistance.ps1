# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')

function Assert-ColumnMenu([IntPtr]$Window,[bool]$Present,[bool]$Checked=$false) {
    [void](Open-PreviewMenu $Window)
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
    $entriesFound=@($popup.Entries | Where-Object { $_.Command -eq (Get-UiId 'IDM_FILL_COLUMNS') })
    Assert-Ui (($entriesFound.Count -eq 1) -eq $Present) 'Column command visibility does not match the global preference.'
    if($Present) { Assert-Ui ((($entriesFound[0].State -band 8) -ne 0) -eq $Checked) 'Column command has the wrong saved check state.' }
    [FileSpacerViewMenuTests.MenuUi]::Escape($Window)
    Wait-PreviewMenuClosed $Window
}
function Set-ColumnsGeneral([IntPtr]$Window,[bool]$Enabled) {
    [void](Show-UiSettings $Window)
    $page=Set-UiPage 1
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_ENABLED' $Enabled
    Press-UiSettings 3
    Wait-Ui { (Get-UiPreference 'DetailsColumnsEnabled') -eq [int]$Enabled } 'global columns preference stored'
}
function Toggle-ColumnMenu([IntPtr]$Window) {
    [void](Open-PreviewMenu $Window)
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
    $index=-1
    for($i=0;$i -lt $popup.Entries.Count;++$i){if($popup.Entries[$i].Command -eq (Get-UiId 'IDM_FILL_COLUMNS')){$index=$i}}
    Assert-Ui ($index -ge 0) 'Column command missing from the native menu.'
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$index,$Window)
    Wait-PreviewMenuClosed $Window
}
$failure=$null
try {
    Initialize-Ui 'column-settings' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    Set-UiPreference 'DetailsColumnsDeferred' 1
    $first=Join-Path $script:UiRoot 'First';$second=Join-Path $script:UiRoot 'Second'
    [void][IO.Directory]::CreateDirectory($first);[void][IO.Directory]::CreateDirectory($second)
    1..5|ForEach-Object{[IO.File]::WriteAllText((Join-Path $first ('item-'+$_+'.txt')),'columns')}
    $window=Open-UiFolder $first 5
    Use-UiView $window {param($view) $view.Mode=4};Wait-UiViewMode $window 4
    Assert-ColumnMenu $window $false
    [void][FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr](Get-UiId 'IDM_FILL_COLUMNS'),[IntPtr]::Zero)
    Wait-ModernColumns $window
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_ENABLED' $true
    Press-UiSettings 5
    Assert-Ui ((Get-UiPreference 'DetailsColumnsEnabled') -eq 0) 'Cancel saved the changed preference.'
    Resize-ColumnsWindow $window 960
    Set-ColumnsGeneral $window $true
    [void](Wait-Columns $window)
    Assert-ColumnMenu $window $true $true
    Close-UiFolder $window
    $window=Open-UiFolder $first 5
    [void](Wait-Columns $window)
    $other=Open-UiFolder $second 0
    Use-UiView $other {param($view) $view.Mode=4};Wait-UiViewMode $other 4
    [void](Wait-Columns $other)
    Toggle-ColumnMenu $other
    Wait-ModernColumns $other
    Close-UiFolder $other
    $other=Open-UiFolder $second 0
    Wait-ModernColumns $other
    Assert-ColumnMenu $other $true $false
    Set-ColumnsGeneral $window $false
    Wait-ModernColumns $window
    Assert-ColumnMenu $window $false
    Close-UiFolder $window
    $window=Open-UiFolder $first 5
    Wait-ModernColumns $window
    Resize-ColumnsWindow $window 1120
    Set-ColumnsGeneral $window $true
    [void](Wait-Columns $window)
    Assert-ColumnMenu $window $true $true
    Assert-ColumnMenu $other $true $false
    Toggle-ColumnMenu $window
    Wait-ModernColumns $window
    Close-UiFolder $window
    $window=Open-UiFolder $first 5
    Wait-ModernColumns $window
    Resize-ColumnsWindow $window 1280
    Toggle-ColumnMenu $window
    [void](Wait-Columns $window)
    Close-UiFolder $window
    $window=Open-UiFolder $first 5
    [void](Wait-Columns $window)
    Write-Host 'PASS: Cancel/OK, hidden and guarded command, automatic fit on global/folder enable after adding space, default-on folders, independent persisted false/true choices and global off/reopen/on without erasure.'
} catch {$failure=$_;throw} finally {Stop-Ui -Failure $failure}
