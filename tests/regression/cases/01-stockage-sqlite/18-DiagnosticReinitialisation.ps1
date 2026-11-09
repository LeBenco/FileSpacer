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
function Click-Task([IntPtr]$Dialog,[int]$Button) {
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($Dialog)) ('Task dialog target is invalid: '+$Dialog+', button '+$Button)
    Assert-Ui ([MutexRecoveryNative]::PostMessage($Dialog,0x466,[IntPtr]$Button,[IntPtr]::Zero)) 'Task dialog command failed.'
}
$failure=$null;$application=$null;$fileUser=$null
$newExplorerWindows=New-Object Collections.ArrayList
try {
    Initialize-Ui 'storage-error-assistance' -FixtureParent $RunDirectory
    $folder=Join-Path $script:UiRoot 'folder'
    [void][IO.Directory]::CreateDirectory($folder)
    $database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
    [void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
    [IO.File]::WriteAllText($database,'not a database',[Text.Encoding]::ASCII)
    $hash=(Get-FileHash -LiteralPath $database -Algorithm SHA256).Hash
    $before=@(Get-ChildItem -LiteralPath (Split-Path $database -Parent) -Directory -Filter 'folder-state-test.sqlite3.backup-*' | ForEach-Object {$_.FullName})
    $application=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$folder+'"') -OutputDirectory $RunDirectory
    [void]$script:UiProcesses.Add($application)
    Wait-Ui { @(Storage-Dialog $application.Id).Count -eq 1 } 'identified storage error'
    $dialog=(Storage-Dialog $application.Id)[0]
    Assert-Ui ([StorageTaskDialog]::HasButton($dialog,1652)) 'Damaged database has no confirmed reset action.'
    $fileUser=[IO.File]::Open($database,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
    $clipboardSequence=[MutexRecoveryNative]::GetClipboardSequenceNumber()
    Click-Task $dialog 1651
    Wait-Ui { $text=[MutexRecoveryNative]::ClipboardText(); [MutexRecoveryNative]::GetClipboardSequenceNumber() -ne $clipboardSequence -and $text -and $text.Contains($database) } 'newly copied diagnostic with exact database path'
    $diagnostic=[MutexRecoveryNative]::ClipboardText()
    $diagnostic | Set-Content -LiteralPath (Join-Path $RunDirectory 'copied-diagnostic.txt') -Encoding UTF8
    Assert-Ui ($diagnostic -match '\b26\b' -and $diagnostic -match 'not a database') 'Diagnostic lost the original SQLite error.'
    Assert-Ui ($diagnostic.Contains('PID '+$PID)) 'Restart Manager did not identify the controlled file user.'
    Assert-Ui ($diagnostic -notmatch '%[1-6]!') 'Unexpanded resource format in diagnostic.'
    $fileUser.Dispose();$fileUser=$null
    $existingExplorer=@($script:UiShell.Windows() | ForEach-Object {try{[long]$_.HWND}catch{}})
    Click-Task $dialog 1650
    Wait-Ui {
        foreach($explorer in @($script:UiShell.Windows())){
            try {
                if($explorer.Document.Folder.Self.Path -eq (Split-Path $database -Parent)){
                    $handle=[IntPtr][long]$explorer.HWND
                    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($handle,[ref]$ownerId)
                    if((Get-Process -Id $ownerId).ProcessName -ne 'explorer'){continue}
                    if([long]$handle -notin $existingExplorer -and !$newExplorerWindows.Contains($explorer)){
                        [void]$newExplorerWindows.Add($explorer)
                    }
                    return $true
                }
            } catch {}
        }
        return $false
    } 'exact data folder opened by Windows Explorer rather than FileSpacer'
    Click-Task $dialog 1652
    $confirmation=[IntPtr]::Zero
    Wait-Ui {
        $found=@([MutexRecoveryNative]::Find($application.Id,'#32770') | Where-Object {
            [StorageTaskDialog]::HasButton($_,6) -and [StorageTaskDialog]::HasButton($_,7)
        })
        if($found.Count -eq 1){$script:confirmation=$found[0];return $true};return $false
    } 'destructive confirmation'
    Write-Host ('STEP: cancel confirmation HWND='+$script:confirmation)
    Click-Task $script:confirmation 7
    Wait-Ui {
        @([MutexRecoveryNative]::Find($application.Id,'#32770') | Where-Object {
            [StorageTaskDialog]::HasButton($_,6) -and [StorageTaskDialog]::HasButton($_,7)
        }).Count -eq 0
    } 'cancelled reset'
    Assert-Ui ((Get-FileHash -LiteralPath $database -Algorithm SHA256).Hash -eq $hash) 'Cancelling reset changed the database.'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($dialog)) 'Cancelling reset dismissed the error instead of preserving assistance.'
    Click-Task $dialog 1652
    Wait-Ui {
        $found=@([MutexRecoveryNative]::Find($application.Id,'#32770') | Where-Object {
            [StorageTaskDialog]::HasButton($_,6) -and [StorageTaskDialog]::HasButton($_,7)
        })
        if($found.Count -eq 1){$script:confirmation=$found[0];return $true};return $false
    } 'second explicit reset confirmation'
    Click-Task $script:confirmation 6
    Wait-Ui {
        $found=@([MutexRecoveryNative]::Find($application.Id,'#32770') | Where-Object {
            $_ -ne $dialog -and [StorageTaskDialog]::HasButton($_,1) -and ![StorageTaskDialog]::HasButton($_,1650)
        })
        if($found.Count -eq 1){$script:completed=$found[0];return $true};return $false
    } 'reset result after backup'
    $backups=@(Get-ChildItem -LiteralPath (Split-Path $database -Parent) -Directory -Filter 'folder-state-test.sqlite3.backup-*' | Where-Object {$_.FullName -notin $before})
    Assert-Ui ($backups.Count -eq 1) 'Reset did not create exactly one distinct backup.'
    $saved=Join-Path $backups[0].FullName 'folder-state-test.sqlite3'
    Assert-Ui ((Get-FileHash -LiteralPath $saved -Algorithm SHA256).Hash -eq $hash) 'Reset backup does not preserve the damaged original.'
    Click-Task $script:completed 1
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($dialog) } 'original error closed after successful reset'
    $window=Track-UiFolder $folder
    Close-UiFolder $window
    Wait-Ui { $application.HasExited } 'normal process exit after recovery'
    Assert-Ui ((Get-FileHash -LiteralPath $database -Algorithm SHA256).Hash -ne $hash) 'The database was not recreated.'
    Write-Host 'PASS: exact diagnosis, confirmed reset, cancellation preserves data, backup retains original and normal operation resumes.'
} catch {$failure=$_;throw}
finally {
    if($fileUser){$fileUser.Dispose()}
    foreach($explorer in $newExplorerWindows){try{$explorer.Quit()}catch{Write-Warning $_.Exception.Message}}
    if($application -and !$application.HasExited){
        @([MutexRecoveryNative]::Describe($application.Id)) | Set-Content (Join-Path $RunDirectory 'windows-before-cleanup.txt') -Encoding UTF8
        $deadline=[DateTime]::UtcNow.AddSeconds(10)
        do {
            foreach($dialog in @([MutexRecoveryNative]::Find($application.Id,'#32770'))){
                $button=if([StorageTaskDialog]::HasButton($dialog,7)){7}else{1}
                [void][MutexRecoveryNative]::PostMessage($dialog,0x466,[IntPtr]$button,[IntPtr]::Zero)
            }
            foreach($window in @([MutexRecoveryNative]::Find($application.Id,'FileSpacer Item Window'))){[void][MutexRecoveryNative]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
            if($application.HasExited){break}
            Start-Sleep -Milliseconds 100
        }while([DateTime]::UtcNow -lt $deadline)
    }
    Stop-Ui $failure
}
