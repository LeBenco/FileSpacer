# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$inputRoot=Join-Path $PSScriptRoot '..\15-deplacement-alt'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $inputRoot 'MouseInput.cs') -ReferencedAssemblies $references
$testFailure=$null
# A real left click through SendInput; the helper spaces independent clicks by the
# Windows double-click time and verifies that the point is not covered.
function Click-At([IntPtr]$Window,$Point) {
    [FileSpacerRegression.AltDragInput]::Begin($Window,$Point,$false,$false)
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($Window) } 'click released without capture' 0 5
}
function Deactivate-Window {
    Activate-Ui $other
    Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow() -eq $other) 'The test window is not inactive.'
}
try {
    Initialize-Ui 'activation-click'
    $folder=Join-Path $script:UiRoot 'Activation'
    [void](New-Item -ItemType Directory -Path $folder)
    foreach ($name in @('AnchorA','AnchorB','OtherC')) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $name)) }
    $otherFolder=Join-Path $script:UiRoot 'Other'
    [void](New-Item -ItemType Directory -Path $otherFolder)
    $window=Open-UiFolder $folder 3
    $other=Open-UiFolder $otherFolder 0
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 960 -and $work.Height -ge 560) 'Two nonoverlapping windows need a 960x560 work area.'
    $width=[int](($work.Width-60)/2); $height=[Math]::Min(480,$work.Height-60)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+20),($work.Top+20),$width,$height,0x14)) 'Cannot place the test window.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($other,[IntPtr]::Zero,($work.Left+40+$width),($work.Top+20),$width,$height,0x14)) 'Cannot place the other window.'
    Activate-Ui $window
    Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA'); $view.Select('AnchorB',$true)}
    Assert-UiSelected $window 'AnchorA;AnchorB'
    $viewWindow=Use-UiView $window {param($view) $view.Window}

    Write-Host 'STEP: preference enabled; background click on the inactive window'
    Assert-Ui ((Get-UiPreference 'KeepSelectionOnActivate') -eq 1) 'The fixture preference must enable selection preservation.'
    Deactivate-Window
    $background=[FileSpacerRegression.AltDragInput]::BackgroundPoint($viewWindow)
    Click-At $window $background
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $window } 'background click activates the inactive window' 0 5
    Assert-UiSelected $window 'AnchorA;AnchorB'
    Write-Host 'PASS: the first background click activates the window and preserves its selection.'

    Write-Host 'STEP: same background click in the now active window'
    Click-At $window $background
    Assert-UiSelected $window ''
    Write-Host 'PASS: a background click in the active window keeps the native deselection.'

    Write-Host 'STEP: item click on the inactive window'
    Use-UiView $window {param($view) $view.Select('AnchorA')}
    Assert-UiSelected $window 'AnchorA'
    Deactivate-Window
    $item=[FileSpacerRegression.AltDragInput]::ItemPoint($viewWindow,'OtherC')
    Click-At $window $item
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $window } 'item click activates the inactive window' 0 5
    Assert-UiSelected $window 'OtherC'
    Write-Host 'PASS: an item click in an inactive window selects that item normally.'

    Write-Host 'STEP: preference disabled; background click on the inactive window'
    Set-UiPreference 'KeepSelectionOnActivate' 0
    Deactivate-Window
    $background=[FileSpacerRegression.AltDragInput]::BackgroundPoint($viewWindow)
    Click-At $window $background
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $window } 'background click activates with the preference disabled' 0 5
    Assert-UiSelected $window ''
    Write-Host 'PASS: with the preference disabled, the activation click deselects natively.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
