# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
$testFailure=$null
function Point([int]$X,[int]$Y) { $p=New-Object FileSpacerRegression.MousePoint; $p.X=$X; $p.Y=$Y; return $p }
function Same-Bounds($A,$B) { return $A.Left -eq $B.Left -and $A.Top -eq $B.Top -and $A.Width -eq $B.Width -and $A.Height -eq $B.Height }
function Background([IntPtr]$Window) {
    $view=Use-UiView $Window {param($v) $v.Window}
    return [FileSpacerRegression.AltDragInput]::BackgroundPoint($view)
}
function Native-Menu([IntPtr]$Window) {
    Activate-Ui $Window
    [FileSpacerRegression.AltDragInput]::BeginConfigured($Window,(Background $Window),2,0)
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui {[FileSpacerRegression.AltDragInput]::MenuActive($Window)} 'native right click opened its real Shell menu' 0 10
    [FileSpacerRegression.AltDragInput]::Escape()
    Wait-Ui {![FileSpacerRegression.AltDragInput]::MenuActive($Window)} 'native context menu closed'
}
function Menu-Key([IntPtr]$Window) {
    [FileSpacerRegression.Native]::Keys($Window,0x79,$false,$false) # F10: native accelerator table
    Wait-Ui {[FileSpacerRegression.AltDragInput]::MenuActive($Window)} 'F10 opened its native menu' 0 10
    [FileSpacerRegression.AltDragInput]::Escape()
    Wait-Ui {![FileSpacerRegression.AltDragInput]::MenuActive($Window)} 'F10 menu closed'
}
try {
    Initialize-Ui 'secondary-shortcut'
    Set-UiPreference 'WindowShortcuts' 0x01001228 # middle/no modifier move; resize disabled
    $windows=@()
    foreach($name in @('A','B')) {
        $folder=Join-Path $script:UiRoot $name
        [void](New-Item -ItemType Directory -Path (Join-Path $folder 'AnchorA') -Force)
        [void](New-Item -ItemType Directory -Path (Join-Path $folder 'OtherC') -Force)
        $windows+=Open-UiFolder $folder 2
    }
    $first=$windows[0]; $second=$windows[1]
    $work=[FileSpacerRegression.Native]::WorkArea($first)
    for($i=0;$i -lt 2;++$i) {
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($windows[$i],[IntPtr]::Zero,($work.Left+20+460*$i),($work.Top+30),400,320,0x14)) 'Place isolated Shell windows side by side.'
    }
    $menuWatch=[Diagnostics.Stopwatch]::StartNew(); Native-Menu $second; $menuWatch.Stop()
    $search=Get-UiSearchEdit $second
    [FileSpacerRegression.Native]::Keys($second,0x46,$true,$false)
    Wait-Ui {[FileSpacerRegression.Native]::FocusedWindowInOwningThread($second) -eq $search} 'Ctrl+F works before the gesture'
    Menu-Key $second
    foreach($window in $windows) { Use-UiView $window {param($v) $v.Focus(); $v.Select('AnchorA')}; Assert-UiSelected $window 'AnchorA' }
    Activate-Ui $first
    $origin=Background $first; $outside=Background $second
    $before=[FileSpacerRegression.Native]::Bounds($first); $otherBefore=[FileSpacerRegression.Native]::Bounds($second)
    [FileSpacerRegression.AltDragInput]::BeginConfigured($first,$origin,1,0)
    Wait-Ui {[FileSpacerRegression.AltDragInput]::Captured($first)} 'middle gesture captured'
    [FileSpacerRegression.AltDragInput]::Move((Point ($origin.X+40) ($origin.Y+30)))
    Wait-Ui {!(Same-Bounds $before ([FileSpacerRegression.Native]::Bounds($first)))} 'middle gesture moved the real Shell window'
    [FileSpacerRegression.AltDragInput]::PressSecondaryRight()
    [FileSpacerRegression.AltDragInput]::ReleasePrimary()
    Wait-Ui {Assert-Ui ([FileSpacerRegression.AltDragInput]::Captured($first)) 'Secondary down requires finishing capture.'; return $true} 'primary release leaves only finishing capture' 150 5
    $committed=[FileSpacerRegression.Native]::Bounds($first)
    [FileSpacerRegression.AltDragInput]::Move($outside)
    Assert-Ui ([FileSpacerRegression.AltDragInput]::Captured($first)) 'Capture follows the consumed down over the other process.'
    [FileSpacerRegression.AltDragInput]::ReleaseSecondaryRight()
    Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($first)} 'secondary release ends capture'
    $observation=[Math]::Max(2000,[Math]::Min(10000,[int](2*$menuWatch.ElapsedMilliseconds+500)))
    Wait-Ui {
        foreach($window in $windows) { Assert-Ui (![FileSpacerRegression.AltDragInput]::MenuActive($window)) 'An orphan secondary up opened a real Shell menu.'; Assert-UiSelected $window 'AnchorA' }
        Assert-Ui (Same-Bounds $committed ([FileSpacerRegression.Native]::Bounds($first))) 'Finishing capture changed committed geometry.'
        Assert-Ui (Same-Bounds $otherBefore ([FileSpacerRegression.Native]::Bounds($second))) 'Secondary up changed the other window.'
        return $true
    } 'Shell menu/selection/geometry remain stable after secondary up' $observation 15
    $search=Get-UiSearchEdit $first
    [FileSpacerRegression.Native]::Keys($first,0x46,$true,$false)
    Wait-Ui {[FileSpacerRegression.Native]::FocusedWindowInOwningThread($first) -eq $search} 'Ctrl+F works after finishing capture without reactivation'
    Menu-Key $first
    Native-Menu $first
    Write-Host 'PASS: real Shell cross-process secondary up, retained capture, unchanged selection/final geometry and native right clicks before/after.'
} catch { $testFailure=$_; throw }
finally {
    try { [FileSpacerRegression.AltDragInput]::End() }
    finally { Stop-Ui -Failure $testFailure }
}
