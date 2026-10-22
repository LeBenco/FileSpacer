# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'MouseInput.cs') -ReferencedAssemblies $references
$testFailure=$null
$window=[IntPtr]::Zero
$phase='initialization'
$state=@{Status='RUNNING'; Error=$null; Phase=$phase; Before=$null; After=$null;
    Mode=$null; ViewWindow=$null; Children=$null; StoredFullNames=$null;
    InputBeforeRelease=$null; InputAfterRelease=$null; SelectionBefore=$null; SelectionAfter=$null}

function Point-At([int]$X,[int]$Y) {
    $point=New-Object FileSpacerRegression.MousePoint; $point.X=$X; $point.Y=$Y; return $point
}
try {
    # Keep the application's initial display mode and appearance defaults.
    # Two selected items and one unselected item; reuse case 07's background gesture.
    # No view-mode setter or UIA target lookup.
    Initialize-Ui 'alt-multiple-selection' -UseApplicationDefaults
    $folder=Join-Path $script:UiRoot 'MultipleSelectionAltDrag'
    [void](New-Item -ItemType Directory -Path $folder)
    foreach($name in @('AnchorA','AnchorB','OtherC')) {
        [void](New-Item -ItemType Directory -Path (Join-Path $folder $name))
    }
    $window=Open-UiFolder $folder 3
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 800 -and $work.Height -ge 600) 'The test requires at least 800x600 physical pixels of work area.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the test window away from screen edges.'
    Activate-Ui $window
    $viewWindow=Use-UiView $window {param($view) $view.Window}
    $state.Mode=Use-UiView $window {param($view) $view.Mode}
    $state.ViewWindow=$viewWindow.ToInt64()
    $state.Children=@([FileSpacerRegression.Native]::DescribeChildren($viewWindow))
    $state.StoredFullNames=Get-UiPreference 'FullNamesOnSelection'
    $phase='multiple selection setup'
    Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA'); $view.Select('AnchorB',$true)}
    Assert-UiSelected $window 'AnchorA;AnchorB'
    $state.SelectionBefore=@(Use-UiView $window {param($view) $view.Selected()})
    $phase='background location'
    $rect=[FileSpacerRegression.Native]::ClientBounds($viewWindow)
    Assert-Ui ($rect.Width -ge 200 -and $rect.Height -ge 160) 'The Shell view is too small for the background gesture.'
    # Lower middle of the public Shell view: below the header, away from bars.
    $point=Point-At ([int]($rect.Left+$rect.Width/2)) ([int]($rect.Top+3*$rect.Height/4))
    $before=[FileSpacerRegression.Native]::Bounds($window)
    $state.Before=$before
    Write-Host ('VIEW: '+($state | ConvertTo-Json -Depth 5 -Compress))

    $phase='Alt and left press'
    [FileSpacerRegression.AltDragInput]::Begin($window,$point,$true,$false)
    [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
    $phase='native move start'
    [FileSpacerRegression.AltDragInput]::Move((Point-At ($point.X+32) ($point.Y+24)))
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Moving($window) } 'native move from the populated folder background started' 0 5
    [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
    [FileSpacerRegression.AltDragInput]::Move((Point-At ($point.X+96) ($point.Y+72)))
    # Pace the last motion before release; assertions below await actual results.
    Start-Sleep -Milliseconds 100
    $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
    [FileSpacerRegression.AltDragInput]::End()
    $phase='final geometry'
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'window move and capture ended' 0 5
    Wait-Ui {
        $after=[FileSpacerRegression.Native]::Bounds($window)
        $after.Left -gt $before.Left -and $after.Top -gt $before.Top -and
            $after.Width -eq $before.Width -and $after.Height -eq $before.Height
    } 'folder window moved right and down without resizing' 200 5
    $state.After=[FileSpacerRegression.Native]::Bounds($window)
    $state.InputAfterRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
    Assert-Ui ((Use-UiView $window {param($view) $view.Count}) -eq 3) 'The test folder no longer contains exactly three items.'
    Assert-Ui ((Use-UiView $window {param($view) $view.Names() -join ';'}) -eq 'AnchorA;AnchorB;OtherC') 'The test folder items changed during the gesture.'
    Assert-UiSelected $window 'AnchorA;AnchorB'
    $state.SelectionAfter=@(Use-UiView $window {param($view) $view.Selected()})
    Assert-Ui ((Use-UiView $window {param($view) $view.Mode}) -eq $state.Mode) 'The display mode changed during the gesture.'
    $state.Status='PASS'; $state.Phase='complete'
    $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'multiple-selection-drag.json') -Encoding UTF8
    Write-Host "PASS: one background Alt-drag preserved AnchorA/AnchorB, left OtherC unselected and moved the folder window by ($($state.After.Left-$before.Left),$($state.After.Top-$before.Top)) without resizing or changing display mode."
} catch {
    $testFailure=$_
    $state.Status='FAIL'; $state.Error=$testFailure.Exception.Message; $state.Phase=$phase
    try {
        if($window -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($window)) {
            # This snapshot precedes cleanup: it records held keys on failure.
            $state.InputBeforeRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
            Write-Host ('INPUT before cleanup: '+$state.InputBeforeRelease)
            [FileSpacerRegression.AltDragInput]::End()
            $state.InputAfterRelease=[FileSpacerRegression.AltDragInput]::Describe($window)
            $state.After=[FileSpacerRegression.Native]::Bounds($window)
            if([FileSpacerRegression.AltDragInput]::Idle($window)) {
                $state.SelectionAfter=@(Use-UiView $window {param($view) $view.Selected()})
            }
        }
        $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'multiple-selection-drag.json') -Encoding UTF8
    } catch { Write-Warning ('Cannot collect multiple-selection drag diagnostics: '+$_.Exception.Message) }
    throw $testFailure
} finally {
    $cleanupFailure=$null
    try { [FileSpacerRegression.AltDragInput]::End() } catch { $cleanupFailure=$_ }
    try { Stop-Ui -Failure $testFailure }
    catch { if(!$cleanupFailure) { $cleanupFailure=$_ } else { Write-Warning $_.Exception.Message } }
    if($cleanupFailure) {
        if($testFailure) { Write-Warning ('Cleanup after failure: '+$cleanupFailure.Exception.Message) }
        else { throw $cleanupFailure }
    }
}
