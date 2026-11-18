# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
$failure=$null
function Select-ContentMode([int]$Index,[uint32]$Mode) {
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$Index,$window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window $Mode
}
function Assert-ContentRows([uint32]$Mode,[string]$Name) {
    Wait-Ui {
        $body=Use-UiView $window {param($view) $view.Window}
        @([FileSpacerRegression.Native]::Children($body,'DirectUIHWND')).Count -gt 0
    } ($Name+' uses the native ordered renderer') 150
    $null=Open-PreviewMenu $window
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    $placement=@($popup.Entries|Where-Object {$_.Command -eq (Get-UiId 'IDM_ICON_PLACEMENT_MENU')})
    Assert-Ui ($placement.Count-eq1-and($placement[0].State-band3)-ne0) ('Placement choices must be disabled in '+$Name+'.')
    $entry=@($popup.Entries|Where-Object {$_.Command -eq (Get-UiId 'IDM_REALIGN_ICONS')})
    Assert-Ui ($entry.Count-eq1-and($entry[0].State-band3)-ne0) ('Realign icons must be disabled in '+$Name+'.')
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window $Mode
}
try {
    Initialize-Ui 'content-placement' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 6
    $window=Open-UiFolder $folder 6
    # Use the real menu so its profile handoff finishes before seeding positions.
    Select-ContentMode 2 1
    Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
    Use-UiView $window {
        param($view)
        $names=@($view.Names()|Sort-Object)
        $view.Position($names[0],43,37)
        $view.Position($names[2],410,235)
        $view.Select($names[0])
        $view.Select($names[2],$true)
    }
    $initial=Read-PreviewState $window
    $restoredPositions=Get-PreviewRestoredPositions $window $initial.Positions
    $initial|ConvertTo-Json -Depth 4|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'initial-state.json') -Encoding UTF8
    # Tiles take no part in icon placement either: same ordered renderer and guarantees.
    foreach($ordered in @(@{Index=7;Mode=8;Name='Content'},@{Index=6;Mode=6;Name='Tiles'})) {
        Select-ContentMode $ordered.Index $ordered.Mode
        Assert-ContentRows $ordered.Mode $ordered.Name
        Close-UiFolder $window
        $window=Open-UiFolder $folder 6
        Wait-UiViewMode $window $ordered.Mode
        Assert-ContentRows $ordered.Mode $ordered.Name
        Select-ContentMode 2 1
        Wait-Ui {(Read-PreviewState $window).Positions -eq $restoredPositions} ($ordered.Name+' and reopening preserve the saved free icon layout through integer DPI storage') 150
    }
    Write-Host 'PASS: Content and Tiles use native ordered rows, disable placement and realignment, survive reopening and preserve the free icon layout.'
} catch {
    $failure=$_
    $observed=try{Use-UiView $window {param($view) @{Positions=$view.Positions();Mode=$view.Mode;Auto=$view.AutoArrange;Grid=$view.SnapToGrid}}}catch{$null}
    @{Message=$_.Exception.Message;Stack=$_.ScriptStackTrace;Initial=$initial;RestoredPositions=$restoredPositions;Observed=$observed}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'first-error.json') -Encoding UTF8
    throw
}
finally {Stop-Ui -Failure $failure}
