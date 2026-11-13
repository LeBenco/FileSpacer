# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryUi.cs')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$taskReferences=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'StorageTaskDialog.cs') -ReferencedAssemblies $taskReferences
function Storage-Dialog([int]$ProcessId) {
    return @([MutexRecoveryNative]::Find($ProcessId,'#32770') | Where-Object {
        [StorageTaskDialog]::HasButton($_,1650) -and [StorageTaskDialog]::HasButton($_,1651)
    })
}
$failure=$null;$application=$null
$database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
$damage='not a database'
try {
    Initialize-Ui 'storage-single-dialog' -FixtureParent $env:FS_REGRESSION_RUN
    # The second window is opened from the first one: both belong to one process.
    Set-UiPreference 'KeepSourceWindowOpen' 1
    $first=Join-Path $script:UiRoot 'first';$second=Join-Path $first 'second'
    [void][IO.Directory]::CreateDirectory($second)
    [void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
    [IO.File]::WriteAllText($database,$damage,[Text.Encoding]::ASCII)
    $application=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$first+'"') -OutputDirectory $env:FS_REGRESSION_RUN
    [void]$script:UiProcesses.Add($application)
    Wait-Ui { @(Storage-Dialog $application.Id).Count -eq 1 } 'storage error of the outage'
    $dialog=(Storage-Dialog $application.Id)[0]
    Assert-Ui ([MutexRecoveryNative]::PostMessage($dialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot dismiss the storage error.'
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($dialog) } 'storage error dismissed'
    $windowA=Track-UiFolder $first
    # A second window of the same process meets the same outage: it is not said again.
    Activate-Ui $windowA
    Use-UiView $windowA {param($view) $view.Focus(); $view.Select('second')}
    Assert-UiSelected $windowA 'second'
    [FileSpacerRegression.Native]::Keys($windowA,0x0d,$false,$false)
    $windowB=Track-UiFolder $second
    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($windowB,[ref]$ownerId)
    Assert-Ui ($ownerId -eq $application.Id) 'Precondition: the second window belongs to another process.'
    Wait-Ui { @(Storage-Dialog $application.Id).Count -eq 0 } 'no second storage error for the second window' 1500
    Close-UiFolder $windowB
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($windowB) } 'second window closed'
    Wait-Ui { @(Storage-Dialog $application.Id).Count -eq 0 } 'no storage error when a window closes during the outage' 1500
    Close-UiFolder $windowA
    Wait-Ui { $application.HasExited } 'process exit without a further storage error'
    Assert-Ui ([IO.File]::ReadAllText($database) -eq $damage) 'The damaged database was changed without a confirmed reset.'
    Write-Host 'PASS: one storage error for the outage, none for the second window nor for the closures.'
} catch {$failure=$_;throw}
finally {
    try {
        if($application -and !$application.HasExited) {
            foreach($left in @([MutexRecoveryNative]::Find($application.Id,'#32770'))) { [void][MutexRecoveryNative]::PostMessage($left,0x10,[IntPtr]::Zero,[IntPtr]::Zero) }
        }
        if((Test-Path -LiteralPath $database) -and [IO.File]::ReadAllText($database) -eq $damage) { Remove-Item -LiteralPath $database -Force }
    } finally { Stop-Ui -Failure $failure }
}
