# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'WheelInput.cs')
$testFailure=$null
function Scale([int]$Value) { return [int][Math]::Round($Value*$script:dpi/96.0,[MidpointRounding]::AwayFromZero) }
# One wheel message with Ctrl held, then the mode and size it must lead to.
function Turn([int]$Delta,[uint32]$Mode,[int]$Size) {
    $bounds=Use-UiView $window {param($view) $view.Bounds}
    [FileSpacerRegression.WheelInput]::Turn($bounds,$Delta)
    Wait-UiViewMode $window $Mode $Size
}
function Assert-Placement([string]$Step) {
    $now=Use-UiView $window {param($view) $view.Positions()}
    Assert-Ui ($now -eq $script:placed) "Free icon positions changed after $Step : $now"
}
try {
    Initialize-Ui 'wheel-zoom'
    $folder=Join-Path $script:UiRoot 'Zoom'
    [void](New-Item -ItemType Directory -Path $folder)
    foreach ($name in @('a','b','c','d','e','f')) { [IO.File]::WriteAllText((Join-Path $folder ($name+'.txt')),'x') }
    $window=Open-UiFolder $folder 6
    Activate-Ui $window
    $script:dpi=[FileSpacerRegression.Native]::GetDpiForWindow($window)
    Assert-Ui ($script:dpi -gt 0) 'Cannot obtain the actual window DPI.'
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,(Scale 48))}
    Wait-UiViewMode $window 1 (Scale 48)
    Use-UiView $window {param($view) $view.AutoArrange=$false}
    Use-UiView $window {param($view) $view.Position('c.txt',420,260); $view.Position('e.txt',500,40)}
    $script:placed=Use-UiView $window {param($view) $view.Positions()}
    [FileSpacerRegression.WheelInput]::Control($true)
    try {
        # Explorer's own steps around Medium icons: 44, 47, 50; never a preset jump.
        Turn 120 1 (Scale 50)
        Assert-Placement 'one notch up'
        Turn (-120) 1 (Scale 47)
        # A partial notch is a whole step, as in Explorer.
        Turn (-60) 1 (Scale 44)
        Assert-Placement 'two steps down'
        # Windows merges the notches received during a change into one message:
        # a message of three notches is three steps, applied by one change.
        Turn 360 1 (Scale 54)
        Assert-Placement 'three notches in one message'
        # Below the smallest icons, the wheel moves on to the modes.
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize(2,(Scale 16))}
        Wait-UiViewMode $window 2 (Scale 16)
        Turn (-120) 3 (-1)
    } finally { [FileSpacerRegression.WheelInput]::Control($false) }
    Write-Host 'PASS: Ctrl+wheel follows the Explorer steps, one per notch, and keeps free icon positions.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
