# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'settings-activation'
    $folder=Join-Path $script:UiRoot 'SettingsActivation'
    [void](New-Item -ItemType Directory -Path $folder)
    $window=Open-UiFolder $folder 0
    # This is a prerequisite of the foreground scenario, never silently bypassed.
    Activate-Ui $window
    foreach ($attempt in @(1,2)) {
        Write-Host "STEP: preferences opened from foreground owner, attempt=$attempt"
        Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow() -eq $window) 'Owner lost foreground before the settings command.'
        $dialog=Show-UiSettings $window
        # Do not call Activate/SetForegroundWindow on the dialog: FileSpacer owns this behavior.
        try {
            Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $dialog } 'settings dialog becomes foreground' 200 5
        } catch {
            Write-Host "DIAG: owner=$window, dialog=$dialog, active=$([FileSpacerRegression.Native]::ActiveWindowInOwningThread($dialog)), foreground=$([FileSpacerRegression.Native]::GetForegroundWindow())"
            throw
        }
        Press-UiSettings 5
        Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $window } 'foreground restored to settings owner after Cancel' 200 5
    }
    Write-Host 'PASS: preferences activate on opening and restore their owner on closing, including a second opening.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
