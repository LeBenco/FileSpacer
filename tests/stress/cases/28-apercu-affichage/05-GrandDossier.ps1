# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\..\regression\cases\28-apercu-affichage\ViewMenu.ps1')
$failure=$null
try {
    Initialize-Ui 'view-preview-large' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 5000
    $window=Open-UiFolder $folder 5000
    Reset-PreviewMode $window
    $modes=Get-PreviewModes $window
    $initial=Read-PreviewMode $window
    $state=Set-PreviewState $window
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0)
    Wait-PreviewMode $window $modes[0]
    [FileSpacerViewMenuTests.MenuUi]::Leave($popup,$window)
    Wait-PreviewMode $window $initial
    # Close as soon as the original mode is visible, without waiting for its
    # progressive coordinate restoration. The final state must remain exact.
    $watch=[Diagnostics.Stopwatch]::StartNew()
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    Assert-PreviewState $window $state
    $watch.Stop()
    Write-Host ('PASS: large-preview cancellation completed in {0} ms including automation and full state readback; all 5000 positions, selection and focus match.' -f $watch.ElapsedMilliseconds)
    Close-UiFolder $window
    $window=Open-UiFolder $folder 5000
    Wait-PreviewMode $window $initial
    Wait-Ui {(Read-PreviewState $window).Positions -eq $state.Positions} '5000 persisted positions after early preview cancellation' 250
    Write-Host 'PASS: all 5000 free positions survive reopening after early cancellation.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
