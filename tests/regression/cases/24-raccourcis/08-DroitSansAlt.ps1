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
function Point([int]$X,[int]$Y) { $point=New-Object FileSpacerRegression.MousePoint; $point.X=$X; $point.Y=$Y; return $point }
function Assert-Fixed($Before) {
    $after=[FileSpacerRegression.Native]::Bounds($window)
    Assert-Ui ($Before.Left -eq $after.Left -and $Before.Top -eq $after.Top -and $Before.Width -eq $after.Width -and $Before.Height -eq $after.Height) 'Window geometry changed unexpectedly.'
}
function Stop-NativeMenu {
    Wait-Ui { [FileSpacerRegression.NativeTargets]::MenuOwner($window) -ne [IntPtr]::Zero } 'native menu owner identified' 0 5
    # Cancel through the actual owner of the Shell menu, without choosing a verb.
    [void][FileSpacerRegression.Native]::Message([FileSpacerRegression.NativeTargets]::MenuOwner($window),0x1f,0,0) # WM_CANCELMODE
    Wait-Ui { ![FileSpacerRegression.NativeTargets]::MenuActive($window) } 'native menu cancelled' 200 5
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'capture released after the menu' 200 5
}
# One view: without Alt, a right drag from the background resizes; a right
# click there, and any right press on an item, stay the Shell's.
function Test-View([string]$Name) {
    $viewWindow=Use-UiView $window {param($view) $view.Window}
    Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA')}
    Assert-UiSelected $window 'AnchorA'
    $background=[FileSpacerRegression.AltDragInput]::BackgroundPoint($viewWindow)
    $before=[FileSpacerRegression.Native]::Bounds($window)
    # A click without movement: the native background menu and deselection.
    [FileSpacerRegression.AltDragInput]::BeginRight($window,$background,$false,$false)
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } ($Name+': background press captured') 0 5
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { [FileSpacerRegression.NativeTargets]::MenuActive($window) } ($Name+': native background menu opened by a click without movement') 0 5
    Assert-Fixed $before
    Wait-Ui { (@(Use-UiView $window {param($view) $view.Selected()}).Count) -eq 0 } ($Name+': background click cleared the selection') 100 5
    Stop-NativeMenu
    # A drag: exact resize, no menu, selection untouched.
    Use-UiView $window {param($view) $view.Select('AnchorA')}
    Assert-UiSelected $window 'AnchorA'
    [FileSpacerRegression.AltDragInput]::BeginRight($window,$background,$false,$false)
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } ($Name+': background drag captured') 0 5
    [FileSpacerRegression.AltDragInput]::Move((Point ($background.X+96) ($background.Y+72)))
    Wait-Ui {
        $after=[FileSpacerRegression.Native]::Bounds($window)
        $after.Left -eq $before.Left -and $after.Top -eq $before.Top -and $after.Width -eq $before.Width+96 -and $after.Height -eq $before.Height+72
    } ($Name+': exact size from the pointer delta') 150 5
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } ($Name+': capture released') 200 5
    Assert-UiSelected $window 'AnchorA'
    Assert-Ui (![FileSpacerRegression.NativeTargets]::MenuActive($window)) ($Name+': the resize opened a menu.')
    # A press on an item never starts the gesture: native selection and menu.
    $resized=[FileSpacerRegression.Native]::Bounds($window)
    $item=[FileSpacerRegression.AltDragInput]::ItemPoint($viewWindow,'OtherC')
    [FileSpacerRegression.AltDragInput]::BeginRight($window,$item,$false,$false)
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { [FileSpacerRegression.NativeTargets]::MenuActive($window) } ($Name+': native item context menu opened') 0 5
    Assert-Fixed $resized
    Assert-UiSelected $window 'OtherC'
    Stop-NativeMenu
    Assert-Ui (![FileSpacerRegression.NativeTargets]::MenuActive($window)) ($Name+': a menu stayed open.')
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,$before.Left,$before.Top,$before.Width,$before.Height,0x14)) 'Cannot restore the fixture size.'
    Write-Host ('PASS: '+$Name+'; background click keeps the native menu and deselection, background drag resizes, item press stays native.')
}
try {
    Initialize-Ui 'right-without-alt'
    # Move: Alt + left. Resize: right button, no modifier.
    Set-UiPreference 'WindowShortcuts' 0x01003022
    $folder=Join-Path $script:UiRoot 'Fixture'; [void](New-Item -ItemType Directory -Path $folder)
    foreach ($item in @('AnchorA','OtherC')) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $item)) }
    $window=Open-UiFolder $folder 2
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the fixture window.'
    Activate-Ui $window
    # The ordered renderer of the fixture's default Details view, then the
    # classic icon list: each recognizes the background and receives the
    # returned click through different means.
    Wait-Ui {
        $body=Use-UiView $window {param($view) $view.Window}
        @([FileSpacerRegression.Native]::Children($body,'DirectUIHWND')).Count -gt 0
    } 'ordered renderer ready' 150
    Test-View 'ordered view'
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,48)}
    Wait-UiViewMode $window 1 48
    Wait-Ui {
        $body=Use-UiView $window {param($view) $view.Window}
        @([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)}).Count -eq 1
    } 'classic icon list ready' 150
    Test-View 'icon view'
    Write-Host 'PASS: right button without Alt resizes by dragging from the background only, in the ordered and classic views.'
} catch { $testFailure=$_; throw }
finally {
    try {
        [FileSpacerRegression.AltDragInput]::End()
        # Never leave a native menu loop open behind a failed assertion.
        if($window -is [IntPtr] -and [FileSpacerRegression.Native]::IsWindow($window) -and [FileSpacerRegression.NativeTargets]::MenuActive($window)) {
            $owner=[FileSpacerRegression.NativeTargets]::MenuOwner($window)
            if($owner -ne [IntPtr]::Zero) { [void][FileSpacerRegression.Native]::Message($owner,0x1f,0,0) }
        }
    }
    finally { Stop-Ui -Failure $testFailure }
}
