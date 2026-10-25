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
function Configure([int]$Action,[int]$Modifiers,[int]$Button) {
    $move=2; $resize=18 # retain the disabled actions' default combinations
    $binding=32 -bor $Modifiers -bor ($Button -shl 3)
    if ($Action -eq 0) {
        $move=$binding; $resize=50
        if ($binding -eq $resize) { $resize=42 }
    } elseif ($Action -eq 1) {
        $resize=$binding; $move=34
        if ($binding -eq $move) { $move=42 }
    }
    $value=0x01000000 -bor $move -bor ($resize -shl 8)
    Set-UiPreference 'WindowShortcuts' $value
    return $value
}
function Fixed($Before,$After) {
    return $Before.Left -eq $After.Left -and $Before.Top -eq $After.Top -and $Before.Width -eq $After.Width -and $Before.Height -eq $After.Height
}
function Target([IntPtr]$Window,[bool]$Item) {
    $viewWindow=Use-UiView $Window {param($view) $view.Window}
    $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -eq 1) 'Require the validated native Shell item container.'
    if ($Item) {
        $itemIndex=Use-UiView $Window {param($view) $view.IndexOf('OtherC')}
        $position=[FileSpacerRegression.NativeTargets]::Item($lists[0],$itemIndex,2,$false,$false)
        return Point $position.X $position.Y
    }
    $bounds=[FileSpacerRegression.Native]::ClientBounds($lists[0])
    $point=Point ([int]($bounds.Left+$bounds.Width/2)) ([int]($bounds.Top+3*$bounds.Height/4))
    [FileSpacerRegression.NativeTargets]::Background($lists[0],$point.X,$point.Y)
    return $point
}
try {
    Initialize-Ui 'shortcut-matrix'
    [void](Configure -1 0 0)
    $windows=@(); $folders=@()
    foreach ($name in @('A','B')) {
        $folder=Join-Path $script:UiRoot $name; $folders+=$folder
        [void](New-Item -ItemType Directory -Path $folder)
        foreach ($item in @('AnchorA','OtherC')) { [void](New-Item -ItemType Directory -Path (Join-Path $folder $item)) }
        $window=Open-UiFolder $folder 2; $windows+=$window
        Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA')}
        Assert-UiSelected $window 'AnchorA'
    }
    # The complete application's modal property sheet hosts the same production page.
    $beforePreference=Get-UiPreference 'WindowShortcuts'
    [void](Show-UiSettings $windows[0]); $page=Set-UiPage 2
    Set-UiCheck $page 'IDC_SHORTCUT_ENABLE_MOVE' $true
    $editors=@([FileSpacerRegression.Native]::Children($page,'#32770'))
    Assert-Ui ($editors.Count -eq 1 -and ![FileSpacerRegression.Native]::IsWindowVisible($editors[0])) 'Header activation must leave the shared editor collapsed.'
    Assert-Ui ((Get-UiPreference 'WindowShortcuts') -eq $beforePreference) 'Header activation saved before Apply.'
    Press-UiSettings 5
    Assert-Ui ((Get-UiPreference 'WindowShortcuts') -eq $beforePreference) 'Cancel saved pending shortcuts.'
    [void](Show-UiSettings $windows[0]); $page=Set-UiPage 2
    Set-UiCheck $page 'IDC_SHORTCUT_ENABLE_MOVE' $true
    Set-UiCheck $page 'IDC_SHORTCUT_ENABLE_RESIZE' $true
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'WindowShortcuts') -eq 0x01003222 } 'real property sheet Apply stored both shortcuts'
    Press-UiSettings 5
    Assert-Ui ((Get-UiPreference 'WindowShortcuts') -eq 0x01003222) 'Cancel after Apply reverted saved shortcuts.'
    $scenarios=0
    for ($action=0; $action -lt 2; ++$action) {
        for ($modifiers=0; $modifiers -lt 8; ++$modifiers) {
            for ($button=0; $button -lt 3; ++$button) {
                if ($button -ne 1 -and !($modifiers -band 2)) { continue }
                [void](Configure $action $modifiers $button)
                for ($index=0; $index -lt 2; ++$index) {
                    $window=$windows[$index]; $other=$windows[1-$index]
                    Activate-Ui $window
                    $work=[FileSpacerRegression.Native]::WorkArea($window)
                    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+45),($work.Top+45),550,350,0x14)) 'Place matrix window.'
                    $viewWindow=Use-UiView $window {param($view) $view.Window}
                    $point=Target $window ($index -eq 1)
                    Assert-UiSelected $window 'AnchorA'
                    $before=[FileSpacerRegression.Native]::Bounds($window); $otherBefore=[FileSpacerRegression.Native]::Bounds($other)
                    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$point,$button,$modifiers)
                    Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'configured gesture captured' 0 5
                    [FileSpacerRegression.AltDragInput]::Move((Point ($point.X+32) ($point.Y+24)))
                    if ($action -eq 0 -and $button -eq 0) {
                        Wait-Ui { [FileSpacerRegression.AltDragInput]::Moving($window) } 'native Windows move loop started' 0 5
                    } else {
                        Wait-Ui { !(Fixed $before ([FileSpacerRegression.Native]::Bounds($window))) } 'configured gesture crossed drag threshold' 0 5
                    }
                    [FileSpacerRegression.AltDragInput]::Move((Point ($point.X+80) ($point.Y+56)))
                    [FileSpacerRegression.AltDragInput]::End()
                    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'configured gesture released' 0 5
                    Wait-Ui {
                        $after=[FileSpacerRegression.Native]::Bounds($window)
                        if ($action -eq 0) { return $after.Left -gt $before.Left -and $after.Top -gt $before.Top -and $after.Width -eq $before.Width -and $after.Height -eq $before.Height }
                        return $after.Left -eq $before.Left -and $after.Top -eq $before.Top -and $after.Width -gt $before.Width -and $after.Height -gt $before.Height
                    } 'correct movement versus resize' 120 5
                    Assert-Ui (Fixed $otherBefore ([FileSpacerRegression.Native]::Bounds($other))) 'Gesture changed the other process window.'
                    Assert-UiSelected $window 'AnchorA'
                    ++$scenarios
                    Write-Host "PASS: action=$action modifiers=$modifiers button=$button process/window=$index; geometry and selection verified"
                }
            }
        }
    }
    Assert-Ui ($scenarios -eq 64) 'All 32 valid action/binding choices require both background and unselected-item coverage across two processes.'
    [void](Configure -1 0 0)
    $window=$windows[0]; Activate-Ui $window
    $point=Target $window $false
    $before=[FileSpacerRegression.Native]::Bounds($window)
    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$point,0,2)
    [FileSpacerRegression.AltDragInput]::Move((Point ($point.X+80) ($point.Y+56)))
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'disabled gesture leaves no capture' 0 5
    Assert-Ui (Fixed $before ([FileSpacerRegression.Native]::Bounds($window))) 'Disabled action still moved/resized the window.'
    Write-Host 'PASS: 64 real Shell gesture scenarios; live cross-process settings, native selection, disabled action, modal tab/Cancel.'
} catch { $testFailure=$_; throw }
finally {
    try { [FileSpacerRegression.AltDragInput]::End() }
    finally { Stop-Ui -Failure $testFailure }
}
