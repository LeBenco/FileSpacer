# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'rename-input-guard'
    $folder=Join-Path $script:UiRoot 'Folder';[void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'Untouched'),'guard-payload')
    $window=Open-UiFolder $folder 1
    $viewWindow=Use-UiView $window { param($view) $view.Window }
    $edit=Get-UiSearchEdit $window
    Activate-Ui $window
    [FileSpacerRegression.Native]::Keys($window,0x46,$true,$false)
    Wait-Ui { [FileSpacerRegression.Native]::FocusedWindowInOwningThread($window) -eq $edit } 'real search edit receives keyboard focus'
    Assert-Ui ([FileSpacerRegression.ShellIntegration]::EditFocused($window)) 'Search control is not an Edit in the target frame.'
    Assert-Ui (![FileSpacerRegression.ShellIntegration]::RenameFocused($window,$viewWindow)) 'Search Edit was mistaken for the Shell rename editor.'
    $before=[FileSpacerRegression.Native]::Text($edit)
    $rejected=$false
    try { [FileSpacerRegression.ShellIntegration]::ReplaceRenameText($window,$viewWindow,'Must-not-be-typed') }
    catch {
        if ($_.Exception.InnerException -isnot [InvalidOperationException] -or
            $_.Exception.InnerException.Message -cne 'No focused in-place rename editor in the target Shell view.') { throw }
        $rejected=$true
    }
    Assert-Ui $rejected 'Rename input was accepted while the search edit had focus.'
    Assert-Ui ([FileSpacerRegression.Native]::Text($edit) -ceq $before) 'Rejected rename input changed the search field.'
    Assert-SearchNames $window @('Untouched')
    Assert-Ui ([IO.File]::ReadAllText((Join-Path $folder 'Untouched')) -ceq 'guard-payload') 'Rejected input changed fixture data.'
    Write-Host 'PASS: the real search Edit is rejected by rename readiness and input guards without changing text or fixture data.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
