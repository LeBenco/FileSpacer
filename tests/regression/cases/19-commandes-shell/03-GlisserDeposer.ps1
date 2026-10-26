# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
$testFailure=$null;$controlHeld=$false;$first=[IntPtr]::Zero;$second=[IntPtr]::Zero
try {
    Initialize-Ui 'shell-native-drag'
    $source=Join-Path $script:UiRoot 'Drag-source';$destination=Join-Path $script:UiRoot 'Drag-target'
    foreach ($folder in @($source,$destination)) { [void][IO.Directory]::CreateDirectory($folder) }
    [void][IO.Directory]::CreateDirectory((Join-Path $destination 'AnchorA'))
    foreach ($name in @('Drag-move','Drag-copy')) { [IO.File]::WriteAllText((Join-Path $source $name),('payload-'+$name)) }
    $first=Open-UiFolder $source 2;$second=Open-UiFolder $destination 1
    $work=[FileSpacerRegression.Native]::WorkArea($first)
    Assert-Ui ($work.Width -ge 960 -and $work.Height -ge 560) 'Drag fixture requires a 960x560 work area for two nonoverlapping windows.'
    $width=[int](($work.Width-60)/2);$height=[Math]::Min(480,$work.Height-60)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($first,[IntPtr]::Zero,($work.Left+20),($work.Top+20),$width,$height,0x14)) 'Cannot place the source.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($second,[IntPtr]::Zero,($work.Left+40+$width),($work.Top+20),$width,$height,0x14)) 'Cannot place the destination.'
    foreach ($name in @('Drag-move','Drag-copy')) {
        Focus-ShellFixture $first $name
        $sourceView=Use-UiView $first {param($view) $view.Window}
        $destinationView=Use-UiView $second {param($view) $view.Window}
        # Details rows can be visible yet expose no UIA clickable point.
        # Reuse the physical gesture bench's verified icon hit test instead.
        $lists=@([FileSpacerRegression.Native]::Children($sourceView,'SysListView32'))
        Assert-Ui ($lists.Count -eq 1) 'Require the validated native Shell item container.'
        $itemIndex=Use-UiView $first {param($view) $view.IndexOf($name)}
        $itemCount=Use-UiView $first {param($view) $view.Count}
        $native=[FileSpacerRegression.NativeTargets]::Item($lists[0],$itemIndex,$itemCount,$true,$false)
        $from=New-Object FileSpacerRegression.MousePoint; $from.X=$native.X; $from.Y=$native.Y
        $destinationLists=@([FileSpacerRegression.Native]::Children($destinationView,'SysListView32'))
        Assert-Ui ($destinationLists.Count -eq 1) 'Require the validated native destination container.'
        $bounds=[FileSpacerRegression.Native]::ClientBounds($destinationLists[0])
        $to=New-Object FileSpacerRegression.MousePoint
        $to.X=[int]($bounds.Left+$bounds.Width/2);$to.Y=[int]($bounds.Top+3*$bounds.Height/4)
        [FileSpacerRegression.NativeTargets]::Background($destinationLists[0],$to.X,$to.Y)
        try {
            [FileSpacerRegression.AltDragInput]::Begin($first,$from,$false,$false)
            [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
            if ($name -eq 'Drag-copy') {
                $controlHeld=$true;[FileSpacerRegression.ShellIntegration]::Control($true)
                Wait-Ui { [FileSpacerRegression.ShellIntegration]::ControlDown } 'Windows reports Ctrl down before drag motion'
            }
            for ($step=1;$step -le 12;$step++) {
                $point=New-Object FileSpacerRegression.MousePoint
                $point.X=[int]($from.X+($to.X-$from.X)*$step/12);$point.Y=[int]($from.Y+($to.Y-$from.Y)*$step/12)
                [FileSpacerRegression.AltDragInput]::Move($point)
                Start-Sleep -Milliseconds 35
            }
            # Allow the real OLE drag loop to observe the destination before release.
            Start-Sleep -Milliseconds 200
            if ($controlHeld) { Assert-Ui ([FileSpacerRegression.ShellIntegration]::ControlDown) 'Ctrl was released before the mouse button.' }
            [FileSpacerRegression.AltDragInput]::End()
            # SendInput queues the release; retain Ctrl until the observed drop has completed.
            Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($first) } 'source leaves the drag capture'
            $target=Join-Path $destination $name
            Wait-Ui { Test-Path -LiteralPath $target } ('native mouse drop creates '+$name)
            Wait-Ui { try { [IO.File]::ReadAllText($target) -ceq ('payload-'+$name) } catch [IO.IOException] { $false } } 'drag/drop produces the exact bytes'
            if ($name -eq 'Drag-move') { Wait-Ui { !(Test-Path -LiteralPath (Join-Path $source $name)) } 'same-volume default drag moves the source' }
            else {
                Assert-Ui ([FileSpacerRegression.ShellIntegration]::ControlDown) 'Ctrl was released before drop completion.'
                Assert-Ui (Test-Path -LiteralPath (Join-Path $source $name)) 'Ctrl+drag removed the source.'
                Assert-Ui ([IO.File]::ReadAllText((Join-Path $source $name)) -ceq ('payload-'+$name)) 'Ctrl+drag changed the source bytes.'
            }
        } finally { [FileSpacerRegression.AltDragInput]::End();if ($controlHeld) { [FileSpacerRegression.ShellIntegration]::Control($false);$controlHeld=$false } }
        if ($name -eq 'Drag-copy') { Assert-Ui (Test-Path -LiteralPath (Join-Path $source $name)) 'Ctrl+drag removed the source after releasing Ctrl.' }
    }
    Assert-SearchNames $first @('Drag-copy');Assert-SearchNames $second @('AnchorA','Drag-copy','Drag-move')
    Write-Host 'PASS: physical native drag/drop moves, Ctrl+drag copies, bytes and both views agree; targets verified by native Shell hit testing.'
} catch {
    $testFailure=$_
    try {
        [FileSpacerRegression.AltDragInput]::End()
        if ($controlHeld) { [FileSpacerRegression.ShellIntegration]::Control($false);$controlHeld=$false }
        $evidence=@{Error=$testFailure.Exception.Message;Windows=@()}
        foreach ($candidate in @($first,$second)) {
            if ($candidate -eq [IntPtr]::Zero -or ![FileSpacerRegression.Native]::IsWindow($candidate)) { continue }
            $entry=@{Window=$candidate.ToInt64();Title=[FileSpacerRegression.Native]::Text($candidate)}
            try {
                $entry.Names=@(Use-UiView $candidate {param($view) $view.Names()})
                $entry.Selection=@(Use-UiView $candidate {param($view) $view.Selected()})
                $viewWindow=Use-UiView $candidate {param($view) $view.Window}
                $entry.Accessibility=[FileSpacerRegression.AltDragInput]::DescribeView($viewWindow)
            } catch { $entry.DiagnosticError=$_.Exception.Message }
            $evidence.Windows+=@($entry)
        }
        Save-ShellCoverage 'drag-failure.json' $evidence
    } catch { Write-Warning ('Cannot collect drag diagnostics: '+$_.Exception.Message) }
    throw $testFailure
}
finally { try { [FileSpacerRegression.AltDragInput]::End();if ($controlHeld) { [FileSpacerRegression.ShellIntegration]::Control($false) } } finally { Stop-Ui -Failure $testFailure } }
