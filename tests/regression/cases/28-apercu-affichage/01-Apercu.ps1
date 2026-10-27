# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
$failure=$null
try {
    Initialize-Ui 'view-preview' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 300
    $window=Open-UiFolder $folder 300
    Reset-PreviewMode $window
    $modes=Get-PreviewModes $window
    Assert-Ui ($null -eq (Get-UiPreference 'ViewMenuPreviewEnabled')) 'Default-on test requires an absent stored value.'
    $initial=Read-PreviewMode $window
    $state=Set-PreviewState $window
    $state|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'initial-state.json') -Encoding UTF8
    foreach($finish in @('escape','outside')) {
        $popup=Open-PreviewMenu $window
        [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0);Wait-PreviewMode $window $modes[0]
        [FileSpacerViewMenuTests.MenuUi]::Hover($popup,3);Wait-PreviewMode $window $modes[3]
        [FileSpacerViewMenuTests.MenuUi]::Leave($popup,$window);Wait-PreviewMode $window $initial
        Assert-PreviewState $window $state
        [FileSpacerViewMenuTests.MenuUi]::Hover($popup,4);Wait-PreviewMode $window $modes[4]
        if($finish -eq 'escape'){[FileSpacerViewMenuTests.MenuUi]::Escape($window)}else{[FileSpacerViewMenuTests.MenuUi]::Outside($window)}
        Wait-PreviewMenuClosed $window
        Wait-PreviewMode $window $initial
        Assert-PreviewState $window $state
    }
    Write-Host 'PASS: default-on hover changes modes; leaving, Escape and outside click restore all original state.'
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,5);Wait-PreviewMode $window $modes[5]
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,5,$window)
    Wait-PreviewMenuClosed $window;Wait-PreviewMode $window $modes[5]
    $committed=Read-PreviewState $window
    Assert-Ui ($committed.Selected -eq $state.Selected -and $committed.Focus -eq $state.Focus) 'Committing a mode lost the selection or focus.'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $modes[5]
    Write-Host 'PASS: clicking a preview commits it and the chosen mode survives reopening.'
    Reset-PreviewMode $window
    $state=Set-PreviewState $window
    $state|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'before-owner-close.json') -Encoding UTF8
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0);Wait-PreviewMode $window $modes[0]
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $initial
    Read-PreviewState $window|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'after-owner-reopen.json') -Encoding UTF8
    Wait-Ui {(Read-PreviewState $window).Positions -eq $state.Positions} 'original persisted icon positions after owner close' 250
    Write-Host 'PASS: closing the owner during a preview preserves the original persisted presentation.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
