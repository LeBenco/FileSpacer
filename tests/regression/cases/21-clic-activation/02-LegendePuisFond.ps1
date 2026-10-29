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
Add-Type -Path (Join-Path $inputRoot 'NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
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
# A verified click on the caption: Windows reports WA_CLICKACTIVE, then delivers
# WM_NCLBUTTONDOWN instead of a client button message to the view.
function Activate-ByCaption([IntPtr]$Window) {
    $point=[FileSpacerRegression.Native]::ActivateByCaptionClick($Window)
    Write-Host "STEP: caption click sent to HWND=$Window at ($($point.X),$($point.Y))"
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $Window } 'foreground after the caption click' 0 5
}
try {
    Initialize-Ui 'activation-caption'
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
    $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -eq 1) 'Require the validated native Shell item container.'
    $list=$lists[0]
    Assert-Ui ((Get-UiPreference 'KeepSelectionOnActivate') -eq 1) 'The fixture preference must enable selection preservation.'

    Write-Host 'STEP: caption click on the inactive window'
    Deactivate-Window
    Activate-ByCaption $window
    Assert-UiSelected $window 'AnchorA;AnchorB'
    Write-Host 'PASS: the caption click activates the window and leaves its selection alone.'

    Write-Host 'STEP: background click in the window activated by its caption'
    $bounds=[FileSpacerRegression.Native]::ClientBounds($list)
    $background=New-Object FileSpacerRegression.MousePoint
    $background.X=[int]($bounds.Left+$bounds.Width/2);$background.Y=[int]($bounds.Top+3*$bounds.Height/4)
    [FileSpacerRegression.NativeTargets]::Background($list,$background.X,$background.Y)
    Click-At $window $background
    Assert-UiSelected $window ''
    Write-Host 'PASS: the first background click after a caption activation keeps the native deselection.'

    Write-Host 'STEP: background click on the inactive window still preserves the selection'
    Use-UiView $window {param($view) $view.Select('AnchorA'); $view.Select('AnchorB',$true)}
    Assert-UiSelected $window 'AnchorA;AnchorB'
    Deactivate-Window
    [FileSpacerRegression.NativeTargets]::Background($list,$background.X,$background.Y)
    Click-At $window $background
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $window } 'background click activates the inactive window' 0 5
    Assert-UiSelected $window 'AnchorA;AnchorB'
    Write-Host 'PASS: the activating background click is still absorbed after the caption scenario.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
