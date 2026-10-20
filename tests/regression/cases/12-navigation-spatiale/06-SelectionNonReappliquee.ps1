# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'navigation-selection-once'
    Set-UiPreference 'KeepSourceWindowOpen' 0
    $a=Join-Path $script:UiRoot 'A-selection-once'
    $b=Join-Path $a 'B-selection-once'
    [void](New-Item -ItemType Directory -Path $b -Force)
    [IO.File]::WriteAllText((Join-Path $a 'keep.txt'),'fixture')
    [IO.File]::WriteAllText((Join-Path $a 'other.txt'),'fixture')
    $child=Open-UiFolder $b 0
    Activate-Ui $child
    [FileSpacerRegression.Native]::Keys($child,0x26,$false,$true)
    $parent=Track-UiFolder $a
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $parent } 'fresh parent activated by Alt+Up'
    Assert-UiSelected $parent 'B-selection-once'

    Write-Host 'STEP: user clears the initial selection, then refreshes the view'
    Use-UiView $parent { param($view) $view.Focus() }
    [FileSpacerRegression.Native]::Keys($parent,0x1b,$false,$false)
    Assert-UiSelected $parent ''
    [FileSpacerRegression.Native]::Keys($parent,0x74,$false,$false)
    Wait-Ui { (Use-UiView $parent { param($view) $view.Count }) -eq 3 } 'parent contents after refresh' 1000
    Assert-UiSelected $parent ''
    Write-Host 'PASS: initial origin selection is not reapplied after user deselection and refresh.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
