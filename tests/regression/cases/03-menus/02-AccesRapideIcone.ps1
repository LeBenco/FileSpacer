# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null;$window=[IntPtr]::Zero
try {
    Initialize-Ui 'quick-access-shell-icon'
    $folder=Join-Path $script:UiRoot 'QuickAccessIcon';[void][IO.Directory]::CreateDirectory($folder)
    $window=Open-UiFolder $folder 0
    $command=Get-UiId 'IDM_QUICK_ACCESS'
    $toolbars=@([FileSpacerRegression.Native]::Children($window,'ToolbarWindow32') | Where-Object {
        [FileSpacerRegression.Native]::Message($_,0x419,$command,0) -ge 0
    })
    Assert-Ui ($toolbars.Count -eq 1) 'Expected exactly one Quick Access toolbar.'
    $toolbar=$toolbars[0]
    Wait-Ui { [FileSpacerRegression.Native]::Message($toolbar,0x431,0,0) -ne 0 } 'Shell image list replaces the fallback Quick Access glyph'
    Assert-Ui ([FileSpacerRegression.Native]::Message($toolbar,0x42c,$command,0) -eq 0) 'The Quick Access button does not use its Shell image.'
    Save-ShellScreenshot $window 'quick-access-icon.png'
    Write-Host 'PASS: the Quick Access button uses its loaded Shell image list instead of the fallback font glyph.'
} catch {
    $testFailure=$_
    if($window -ne [IntPtr]::Zero) { try { Save-ShellScreenshot $window 'quick-access-icon-failure.png' -WithoutActivation } catch { Write-Warning $_.Exception.Message } }
    throw $testFailure
} finally { Stop-Ui -Failure $testFailure }
