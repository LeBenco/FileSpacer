# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\28-apercu-affichage\ViewMenu.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'WheelInput.cs')
$testFailure=$null
# One wheel message with Ctrl held, then the mode and size it must lead to.
function Turn([int]$Delta,[uint32]$Mode,[int]$Size) {
    $bounds=Use-UiView $window {param($view) $view.Bounds}
    [FileSpacerRegression.WheelInput]::Turn($bounds,$Delta)
    Wait-UiViewMode $window $Mode $Size
}
function Assert-Placement([string]$Step) {
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()})-eq$script:placed} "Free icon positions retained after $Step" 150
}
function Grid-Cells {
    Use-UiView $window {param($view)
        $spacing=$view.Spacing;$cells=@{}
        foreach($name in $view.Names()) {
            $point=($view.Position($name)-split'=',2)[1]-split','
            $cells[$name]=('{0},{1}'-f[Math]::Floor([int]$point[0]/$spacing.X),[Math]::Floor([int]$point[1]/$spacing.Y))
        }
        return $cells
    }
}
function Assert-GridCells($Expected) {
    $actual=Grid-Cells
    foreach($name in $Expected.Keys) {Assert-Ui ($actual[$name]-eq$Expected[$name]) ('Zoom changed an occupied grid cell or a hole: '+$name)}
}
function Choose-Zoom([int]$Index,[int]$Size) {
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$Index,$window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window 1 $Size
}
try {
    Initialize-Ui 'wheel-zoom'
    $folder=Join-Path $script:UiRoot 'Zoom'
    [void](New-Item -ItemType Directory -Path $folder)
    foreach ($name in @('a','b','c','d','e','f')) { [IO.File]::WriteAllText((Join-Path $folder ($name+'.txt')),'x') }
    $window=Open-UiFolder $folder 6
    Activate-Ui $window
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,48)}
    Wait-UiViewMode $window 1 48
    Use-UiView $window {param($view) $view.AutoArrange=$false}
    Use-UiView $window {param($view) $view.Position('c.txt',420,260); $view.Position('e.txt',500,40)}
    # Seed representable coordinates: zoom restores the same integer-DPI snapshot
    # as persistence; arbitrary physical pixels can round once above 100%.
    $seed=Get-PreviewRestoredPositions $window (Use-UiView $window {param($view) $view.Positions()})
    Use-UiView $window {param($view)
        foreach($entry in($seed-split';')){$pair=$entry-split'=',2;$point=$pair[1]-split',';$view.Position($pair[0],[int]$point[0],[int]$point[1])}
    }
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()})-eq$seed} 'representable zoom fixture positions settled' 150
    $script:placed=Use-UiView $window {param($view) $view.Positions()}
    [FileSpacerRegression.WheelInput]::Control($true)
    try {
        # Explorer's own steps around Medium icons: 44, 47, 50; never a preset jump.
        Turn 120 1 50
        Assert-Placement 'one notch up'
        Turn (-120) 1 47
        # A partial notch is a whole step, as in Explorer.
        Turn (-60) 1 44
        Assert-Placement 'two steps down'
        # Windows merges the notches received during a change into one message:
        # a message of three notches is three steps, applied by one change.
        Turn 360 1 54
        Assert-Placement 'three notches in one message'
        # Below the smallest icons, the wheel moves on to the modes.
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize(2,16)}
        Wait-UiViewMode $window 2 16
        Turn (-120) 3 (-1)
    } finally { [FileSpacerRegression.WheelInput]::Control($false) }
    Write-Host 'PASS: Ctrl+wheel follows the Explorer steps, one per notch, and keeps free icon positions.'
    Choose-Zoom 2 48
    Choose-Placement 'IDM_KEEP_ICONS_ALIGNED'
    $cells=Grid-Cells
    # The fixture includes displaced icons and holes, not just a packed row.
    Assert-Ui (@($cells.Values|Where-Object {$_-notlike'*,0'}).Count-gt0) 'Grid fixture must occupy more than one row.'
    $gridBeforePreview=Use-UiView $window {param($view) $view.Positions()}
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0)
    Wait-UiViewMode $window 1 256
    Assert-GridCells $cells
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window 1 48
    $gridAfterPreview=Use-UiView $window {param($view) $view.Positions()}
    @{Before=$gridBeforePreview;After=$gridAfterPreview}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-preview-cancel.json') -Encoding UTF8
    Assert-Ui ($gridAfterPreview-eq$gridBeforePreview) 'Cancelling a grid preview changed icon coordinates.'
    [FileSpacerRegression.WheelInput]::Control($true)
    try {Turn 12000 1 256} finally {[FileSpacerRegression.WheelInput]::Control($false)}
    Assert-GridCells $cells
    Choose-Zoom 1 96
    Assert-GridCells $cells
    Choose-Zoom 0 256
    Assert-GridCells $cells
    $gridSpacing=Use-UiView $window {param($view) $view.Spacing}
    $gridPositions=Use-UiView $window {param($view) $view.Positions()}
    Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
    Assert-Ui ((Use-UiView $window {param($view) $view.Positions()})-eq$gridPositions) 'Changing from grid to free placement replayed an older arrangement.'
    Choose-Placement 'IDM_AUTO_ARRANGE_ICONS'
    $autoSpacing=Use-UiView $window {param($view) $view.Spacing}
    Assert-Ui ($gridSpacing.X-eq$autoSpacing.X-and$gridSpacing.Y-eq$autoSpacing.Y) 'Grid and automatic placement use different native spacing at the same zoom.'
    Write-Host 'PASS: grid zoom preserves occupied cells and holes, uses automatic placement spacing, and enters free placement without a jump.'

} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
