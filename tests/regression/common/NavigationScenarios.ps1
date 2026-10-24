function Invoke-SpatialPolicy([int]$KeepDefault,[bool]$Control) {
    $testFailure=$null
    try {
        Initialize-Ui 'navigation-policy'
        Set-UiPreference 'KeepSourceWindowOpen' $keepDefault
        $a=Join-Path $script:UiRoot ('A-'+$keepDefault+'-'+[int]$control)
        $b=Join-Path $a ('B-'+[int]$control)
        [void](New-Item -ItemType Directory -Path $b -Force)
        foreach ($path in @((Join-Path $a 'keep.txt'),(Join-Path $a 'other.txt'),(Join-Path $b 'child.txt'))) {
            [IO.File]::WriteAllText($path,'fixture')
        }
        $source=Open-UiFolder $a 3
        Activate-Ui $source
        Use-UiView $source { param($view) $view.Select([IO.Path]::GetFileName($b)); $view.Focus() }
        Assert-UiSelected $source ([IO.Path]::GetFileName($b))
        Write-Host "STEP: Enter, KeepSourceWindowOpen=$keepDefault, Ctrl=$control"
        [FileSpacerRegression.Native]::Keys($source,0x0d,$control,$false)
        $child=Track-UiFolder $b
        Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $child } 'child activated by Enter'
        $keep=([bool]$keepDefault -xor $control)
        Wait-Ui { [FileSpacerRegression.Native]::IsWindow($source) -eq $keep } "Enter source kept = $keep" 300
        if ($keep) { Close-UiFolder $source } # Check navigation into a newly created parent in all four combinations.
        Write-Host "STEP: Alt+Up, KeepSourceWindowOpen=$keepDefault, Ctrl=$control"
        [FileSpacerRegression.Native]::Keys($child,0x26,$control,$true)
        $parent=Track-UiFolder $a
        Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $parent } 'parent activated by Alt+Up'
        Wait-Ui { [FileSpacerRegression.Native]::IsWindow($child) -eq $keep } "Alt+Up source kept = $keep" 300
        Assert-UiSelected $parent ''
        Write-Host "PASS: Enter / Alt+Up, KeepSourceWindowOpen=$keepDefault, Ctrl=$control; no automatic selection in the new parent and unchanged closure policy."
        Close-UiFolder $child; Close-UiFolder $parent
    } catch { $testFailure=$_; throw }
    finally { Stop-Ui -Failure $testFailure }
}

function Invoke-ExistingParent() {
    $testFailure=$null
    try {
        Initialize-Ui 'navigation-existing'
        Set-UiPreference 'KeepSourceWindowOpen' 1
        $a=Join-Path $script:UiRoot 'A-existing'
        $b=Join-Path $a 'B-existing'
        [void](New-Item -ItemType Directory -Path $b -Force)
        [IO.File]::WriteAllText((Join-Path $a 'keep.txt'),'fixture')
        [IO.File]::WriteAllText((Join-Path $a 'other.txt'),'fixture')
        $parent=Open-UiFolder $a 3; $child=Open-UiFolder $b 0
        Initialize-UiParentChildWindows $parent $child
        Activate-Ui $parent
        Use-UiView $parent { param($view) $view.Mode=1 }
        Wait-Ui { (Use-UiView $parent {param($view) $view.Mode}) -eq 1 } 'native icon view' 300
        Use-UiView $parent {param($view) $view.Select('keep.txt')}
        Assert-UiSelected $parent 'keep.txt'
        $positions=Get-UiStableLayout $parent 3 'before'
        Activate-Ui $child
        [FileSpacerRegression.Native]::Keys($child,0x26,$false,$true)
        Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $parent } 'existing parent activated'
        Assert-Ui (@(Find-UiFolder $a).Count -eq 1) 'Navigation duplicated the existing parent.'
        Assert-UiSelected $parent 'keep.txt'
        $reactivatedPositions=Get-UiStableLayout $parent 3 'after'
        Assert-UiLayoutUnchanged $positions $reactivatedPositions
        Write-Host 'PASS: existing parent activated without duplicate, selection replacement or icon movement.'
    } catch { $testFailure=$_; throw }
    finally { Stop-Ui -Failure $testFailure }
}
