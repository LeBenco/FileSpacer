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
function Assert-ContentRows {
    Wait-Ui {
        $body=Use-UiView $window {param($view) $view.Window}
        @([FileSpacerRegression.Native]::Children($body,'DirectUIHWND')).Count -gt 0
    } 'Content uses the native ordered renderer' 150
    $null=Open-PreviewMenu $window
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    $entry=@($popup.Entries|Where-Object {$_.Command -eq (Get-UiId 'IDM_REALIGN_ICONS')})
    Assert-Ui ($entry.Count-eq1-and($entry[0].State-band3)-ne0) 'Realign icons must be disabled in Content.'
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window 8
}
try {
    Initialize-Ui 'content-placement' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 6
    $window=Open-UiFolder $folder 6
    # Use the real menu so its profile handoff finishes before seeding positions.
    Select-ContentMode 2 1
    Use-UiView $window {param($view) $view.AutoArrange=$false}
    Wait-Ui {Use-UiView $window {param($view) $view.Mode-eq1-and!$view.AutoArrange}} 'free icon view ready before seeding' 150
    Use-UiView $window {
        param($view)
        $names=@($view.Names()|Sort-Object)
        $view.SnapToGrid=$false
        $view.Position($names[0],43,37)
        $view.Position($names[2],410,235)
        $view.Select($names[0])
        $view.Select($names[2],$true)
    }
    $initial=Read-PreviewState $window
    Select-ContentMode 7 8
    Assert-ContentRows
    Close-UiFolder $window
    $window=Open-UiFolder $folder 6
    Wait-UiViewMode $window 8
    Assert-ContentRows
    Select-ContentMode 2 1
    Wait-Ui {(Read-PreviewState $window).Positions -eq $initial.Positions} 'Content and reopening preserve the saved free icon layout' 150
    Write-Host 'PASS: Content uses native ordered rows, disables realignment, survives reopening and preserves the free icon layout.'
} catch {
    $failure=$_
    @{Message=$_.Exception.Message;Stack=$_.ScriptStackTrace}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'first-error.json') -Encoding UTF8
    throw
}
finally {Stop-Ui -Failure $failure}
