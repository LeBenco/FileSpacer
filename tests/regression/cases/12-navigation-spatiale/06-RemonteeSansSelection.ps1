# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'navigation-no-auto-selection'
    Set-UiPreference 'KeepSourceWindowOpen' 0
    $a=Join-Path $script:UiRoot 'A-no-auto-selection'
    $b=Join-Path $a 'B-origin'
    $c=Join-Path $a 'C-user-choice'
    [void](New-Item -ItemType Directory -Path $b -Force)
    [void](New-Item -ItemType Directory -Path $c -Force)
    $child=Open-UiFolder $b 0
    Activate-Ui $child
    [FileSpacerRegression.Native]::Keys($child,0x26,$false,$true)
    $parent=Track-UiFolder $a
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $parent } 'fresh parent activated by Alt+Up'
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($child) } 'origin window closed by the navigation policy' 300
    Assert-UiSelected $parent ''

    [FileSpacerRegression.Native]::Keys($parent,0x74,$false,$false)
    Wait-Ui { (Use-UiView $parent { param($view) $view.Count }) -eq 2 } 'parent contents after refresh' 1000
    Assert-UiSelected $parent ''
    # Extend through the public Shell API, without clearing a potential unwanted
    # origin selection. This checks Ctrl-style selection semantics, not a mouse gesture.
    Use-UiView $parent { param($view) $view.Select('C-user-choice',$true); $view.Focus() }
    Assert-UiSelected $parent 'C-user-choice'
    [FileSpacerRegression.Native]::Keys($parent,0x0d,$true,$false)
    $chosen=Track-UiFolder $c
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $chosen } 'chosen folder activated by Ctrl+Enter'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($parent)) 'Ctrl+Enter closed the parent unexpectedly.'
    Assert-Ui (@(Find-UiFolder $b).Count -eq 0) 'The origin folder was opened again without being chosen.'
    Assert-Ui (@(Find-UiFolder $c).Count -eq 1) 'The chosen folder was duplicated.'
    Assert-UiSelected $parent 'C-user-choice'
    Write-Host 'PASS: ascent and refresh add no selection; extending selection and Ctrl+Enter open only the chosen folder.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
