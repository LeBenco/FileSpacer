# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'shell-context-menus'
    $folder=Join-Path $script:UiRoot 'Menus';[void][IO.Directory]::CreateDirectory($folder)
    foreach ($name in @('First','Second')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $window=Open-UiFolder $folder 2
    foreach ($selected in @('First','')) {
        Focus-ShellFixture $window 'First'
        if (!$selected) { Use-UiView $window {param($view) $view.Deselect('First')} }
        Assert-UiSelected $window $selected
        [FileSpacerRegression.ShellIntegration]::Shortcut($window,0x79,$false,$true)
        try {
            Wait-Ui { [FileSpacerRegression.ShellIntegration]::MenuActive($window) } 'real Shift+F10 context menu'
        } finally { [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false) }
        Wait-Ui { ![FileSpacerRegression.ShellIntegration]::MenuActive($window) } 'Escape cancels the menu'
        Assert-SearchNames $window @('First','Second');Assert-UiSelected $window $selected
        Assert-Ui ([FileSpacerRegression.Native]::IsWindow($window)) 'Context menu cancellation closed the window.'
    }
    Write-Host 'PASS: native context menus open with and without selection, then cancel without changing contents or selection. Menu contents and theming require visual review.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
