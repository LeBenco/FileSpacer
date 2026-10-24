# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'shell-virtual-namespaces'
    foreach ($parsingName in @('shell:::{20D04FE0-3AEA-1069-A2D8-08002B30309D}')) {
        $window=Open-ShellNamespace $parsingName
        $expected=(Use-UiView $window {param($view) $view.ParsingNames($false)}) -join ';'
        Assert-Ui ($expected.Length -gt 0) 'Virtual namespace exposes no parsing identities.'
        Activate-Ui $window
        Use-UiView $window {param($view) $view.SelectIndex(0);$view.Focus()}
        $selected=(Use-UiView $window {param($view) $view.ParsingNames($true)}) -join ';'
        Assert-Ui ($selected.Length -gt 0) 'Virtual selection exposes no identity.'
        [FileSpacerRegression.Native]::Keys($window,0x74,$false,$false)
        Wait-Ui { ((Use-UiView $window {param($view) $view.ParsingNames($false)}) -join ';') -ceq $expected } 'same virtual identities after refresh' 500
        Close-UiFolder $window
        Wait-Ui { @($script:UiProcesses | Where-Object {!$_.HasExited}).Count -eq 0 } 'namespace processes exit before reopening'
        $window=Open-ShellNamespace $parsingName
        Wait-Ui { ((Use-UiView $window {param($view) $view.ParsingNames($false)}) -join ';') -ceq $expected } 'virtual namespace reopens with the same identities' 500
        Close-UiFolder $window
        Wait-Ui { @($script:UiProcesses | Where-Object {!$_.HasExited}).Count -eq 0 } 'reopened namespace processes exit'
    }
    Write-Host 'PASS: This PC enumerates, selects, refreshes and reopens through parsing identities. External Control Panel opening is checked by 05-PanneauExterne.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
