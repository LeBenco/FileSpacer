# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
$failure=$null
try {
    Initialize-Ui 'column-release' -UseApplicationDefaults -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot 'Details'
    [void][IO.Directory]::CreateDirectory($folder)
    1..100|ForEach-Object{[IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns')}
    $window=Open-UiFolder $folder 100
    Use-UiView $window {param($view) $view.Mode=4};Wait-UiViewMode $window 4
    $before=Wait-Columns $window
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    foreach($name in @('IDC_DETAILS_COLUMNS_ENABLED','IDC_DETAILS_COLUMNS_DEFERRED')) {
        $control=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId $name))
        Assert-Ui ([FileSpacerRegression.Native]::Message($control,0xf0,0,0) -eq 1) 'All column options must default on.'
    }
    Press-UiSettings 5
    Activate-Ui $window
    $before=Wait-Columns $window
    $start=New-Object FileSpacerRegression.MousePoint
    Save-ColumnWindow $window 'native-guide-before'
    $start.X=[int]$before.Headers[0].Right-1;$start.Y=[int]($before.Headers[0].Top+$before.Headers[0].Height/2)
    $point=New-Object FileSpacerRegression.MousePoint;$point.Y=$start.Y
    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
    foreach($delta in @(12,24)) {
        $point.X=$start.X+$delta
        [FileSpacerRegression.AltDragInput]::Move($point)
        Start-Sleep -Milliseconds 600
        [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
        $during=Read-Columns $window
        Assert-Ui (($during.Widths -join ',') -eq ($before.Widths -join ',')) 'A pause with the button held refreshed a column.'
        Save-ColumnWindow $window ('native-guide-'+$delta)
    }
    [FileSpacerRegression.AltDragInput]::End()
    $after=Wait-Columns $window
    Assert-Ui ($after.Widths[0] -eq $before.Widths[0]+24 -and $after.Widths[1] -eq $before.Widths[1]-24) 'Release did not apply exactly one adjacent-column adjustment.'
    $start.X=[int]$after.Headers[0].Right-1
    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
    $point.X=$start.X+12
    [FileSpacerRegression.AltDragInput]::Move($point)
    Start-Sleep -Milliseconds 300
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Start-Sleep -Milliseconds 100
    [FileSpacerRegression.AltDragInput]::End()
    $cancelled=Wait-Columns $window
    Assert-Ui (($cancelled.Widths -join ',') -eq ($after.Widths -join ',')) 'Escape committed a cancelled divider drag.'
    Write-Host 'PASS: defaults on, automatic initial full width, two stationary pauses while held without resizing, single update on release, adjacent compensation and Escape cancellation.'
} catch {$failure=$_;throw} finally {
    try {[FileSpacerRegression.AltDragInput]::End()} finally {Stop-Ui -Failure $failure}
}
