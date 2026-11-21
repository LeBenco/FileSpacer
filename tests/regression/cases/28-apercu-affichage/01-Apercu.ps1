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
    [void][FileSpacerViewMenuTests.MenuUi]::Open($window,(Get-UiCommandToolbar $window),(Get-UiId 'IDM_CONTEXT_MENU'),(Get-UiId 'IDM_SETTINGS'))
    Assert-Ui ([FileSpacerViewMenuTests.MenuUi]::OwnerParentClass($window) -eq 'FileSpacer Toolbar Host') 'Application menu must keep the toolbar theme scope.'
    Assert-Ui ([FileSpacerViewMenuTests.MenuUi]::OwnerParentTitle($window) -eq '') 'Application menu must not acquire the View-only marker.'
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    $popup=Open-PreviewMenu $window
    $ownerClass=[FileSpacerViewMenuTests.MenuUi]::OwnerParentClass($window)
    Assert-Ui ($ownerClass -eq 'FileSpacer Toolbar Host') 'View menu must share the toolbar theme scope.'
    Assert-Ui ([FileSpacerViewMenuTests.MenuUi]::OwnerParentTitle($window) -eq 'FileSpacer View Menu') 'View menu must expose its own stable marker.'
    $allEntries=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true,(Get-UiId 'IDM_TOGGLE_THUMBNAILS'))
    $thumbnails=@($allEntries.Entries|Where-Object {$_.Command -eq (Get-UiId 'IDM_TOGGLE_THUMBNAILS')})
    Assert-Ui ($thumbnails.Count -eq 1 -and $thumbnails[0].SeparatorBefore) 'Thumbnails must be separated from the view modes above.'
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x28,1) # Down highlights the first mode.
    Wait-PreviewMode $window $modes[0]
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x0D,1)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window $modes[0]
    Reset-PreviewMode $window
    Write-Host 'PASS: View uses the toolbar theme scope; keyboard highlighting previews and Enter commits the mode.'
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
    # Details has no icon coordinates in the modern native renderer.
    # This assertion concerns the selection and focus, which both renderers expose.
    $committed=Use-UiView $window {param($view) @{Selected=(@($view.Selected())|Sort-Object) -join ';';Focus=$view.Focused()}}
    Assert-Ui ($committed.Selected -eq $state.Selected -and $committed.Focus -eq $state.Focus) 'Committing a mode lost the selection or focus.'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $modes[5]
    Write-Host 'PASS: clicking a preview commits it and the chosen mode survives reopening.'
    Reset-PreviewMode $window
    $state=Set-PreviewState $window
    $state|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'before-owner-close.json') -Encoding UTF8
    $restoredPositions=Get-PreviewRestoredPositions $window $state.Positions
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0);Wait-PreviewMode $window $modes[0]
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $initial
    Read-PreviewState $window|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'after-owner-reopen.json') -Encoding UTF8
    Wait-Ui {(Read-PreviewState $window).Positions -eq $restoredPositions} 'original persisted icon positions after owner close through integer DPI storage' 250
    Write-Host 'PASS: closing the owner during a preview preserves the original persisted presentation.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
