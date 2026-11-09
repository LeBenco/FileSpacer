# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryUi.cs')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$taskReferences=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'StorageTaskDialog.cs') -ReferencedAssemblies $taskReferences
$helper=Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
$probe=Build-NativeTest -Name FolderIdentityProbe -CaseDirectory $PSScriptRoot -ProductionSources @('FolderIdentity.cpp')
function Read-Presentation([IntPtr]$Window) {
    $view=Get-UiView $Window
    Assert-Ui ($null -ne $view) 'Registered native view missing.'
    try {
        $mode=$view.Mode;$arranged=$view.AutoArrange
        $positions=if($mode -notin @(3,4) -and !$arranged){$view.Positions()}else{''}
        return [pscustomobject]@{Mode=$mode;IconSize=$view.IconSize;AutoArrange=$arranged;Count=$view.Count;
            Positions=$positions;Selected=@($view.Selected());Focused=$view.Focused();
            ViewWindow=$view.Window.ToInt64();Enabled=[FileSpacerRegression.Native]::IsWindowEnabled($view.Window)}
    } finally {$view.Dispose()}
}
function Assert-Positions([string]$Expected,[string]$Actual) {
    $wanted=@{};$found=@{}
    foreach($entry in $Expected.Split(';')){if($entry){$pair=$entry.Split('=');$wanted[$pair[0]]=$pair[1].Split(',')}}
    foreach($entry in $Actual.Split(';')){if($entry){$pair=$entry.Split('=');$found[$pair[0]]=$pair[1].Split(',')}}
    Assert-Ui ($wanted.Count -eq $found.Count) 'Icon identity set changed.'
    foreach($name in $wanted.Keys){
        Assert-Ui ($found.ContainsKey($name)) ('Missing icon '+$name)
        Assert-Ui ([Math]::Abs([int]$wanted[$name][0]-[int]$found[$name][0]) -le 1 -and
            [Math]::Abs([int]$wanted[$name][1]-[int]$found[$name][1]) -le 1) ('Saved position replaced for '+$name)
    }
}
$holder=$null;$ready=$null;$release=$null;$failure=$null
try {
    Initialize-Ui -Name 'SQLiteLate' -FixtureParent $RunDirectory
    Set-UiPreference 'SearchHistoryLimit' 20
    $folder=Join-Path $script:UiRoot 'known-folder'
    [void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'alpha'),'old')
    [IO.File]::WriteAllText((Join-Path $folder 'beta'),'old')
    $identity=@(& $probe $folder)
    Assert-Ui ($LASTEXITCODE -eq 0 -and $identity.Count -eq 2) 'Fixture identity failed.'
    $key=$identity[0];$database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
    [void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
    Invoke-TestProgram $helper @('--seed-product',$database,$key)
    $first=Open-UiFolder $folder 2
    Use-UiView $first {param($view) $view.SetViewModeAndIconSize(1,64)}
    Wait-UiViewMode $first 1 64
    Use-UiView $first {param($view) $view.AutoArrange=$false}
    Wait-Ui {$state=Read-Presentation $first;return $state.Mode -eq 1 -and !$state.AutoArrange -and $state.Count -eq 2} 'enumerated reference icon mode' 300
    Use-UiView $first {param($view) $view.Position('alpha',350,160);$view.Position('beta',560,280)}
    $reference=Read-Presentation $first
    $reference|ConvertTo-Json -Depth 4|Set-Content (Join-Path $RunDirectory 'reference-view.json') -Encoding UTF8
    Close-UiFolder $first
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count -eq 0} 'reference process normal exit'
    # The stored PIDL may now differ from the same live file's metadata.
    [IO.File]::WriteAllText((Join-Path $folder 'alpha'),('changed'*1024))
    $prefix='Local\FileSpacer.LateRecovery.'+[Guid]::NewGuid().ToString('N')
    $ready=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($prefix+'.ready'))
    $release=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($prefix+'.release'))
    $holder=Start-RecordedProcess -Executable $helper -Arguments ('--child "'+$database+'" "'+$prefix+'" hold-long') -OutputPath (Join-Path $RunDirectory 'holder.out') -ErrorPath (Join-Path $RunDirectory 'holder.err')
    Assert-Ui ($ready.WaitOne(15000)) 'Long holder not ready.'
    $started=[Diagnostics.Stopwatch]::StartNew()
    $app=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$folder+'"') -OutputDirectory $RunDirectory
    [void]$script:UiProcesses.Add($app)
    $script:storageDialog=[IntPtr]::Zero
    Wait-Ui {
        $dialogs=@([MutexRecoveryNative]::Find($app.Id,'#32770'))
        if($dialogs.Count -ne 1){return $false}
        if([StorageTaskDialog]::HasButton($dialogs[0],1650) -and [StorageTaskDialog]::HasButton($dialogs[0],1651) -and
                [StorageTaskDialog]::HasButton($dialogs[0],1)){$script:storageDialog=$dialogs[0];return $true}
        return $false
    } 'identified initial storage error' 0 15
    Assert-Ui ([MutexRecoveryNative]::PostMessage($script:storageDialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Acknowledge identified storage error.'
    $script:lateWindow=[IntPtr]::Zero
    Wait-Ui {
        $windows=@(Find-UiFolder $folder)
        if($windows.Count -ne 1){return $false}
        $script:lateWindow=$windows[0]
        $view=Get-UiView $script:lateWindow
        if($null -eq $view){return $false}
        try{return $view.Count -eq 2 -and [FileSpacerRegression.Native]::IsWindowEnabled($view.Window)}finally{$view.Dispose()}
    } 'interactive provisional native view while SQLite stays blocked' 0 90
    $window=$script:lateWindow;[void]$script:UiWindows.Add($window)
    Assert-Ui (!$holder.Process.HasExited -and !$release.WaitOne(0)) 'Holder released before the provisional scenario was established.'
    $provisionalMilliseconds=$started.Elapsed.TotalMilliseconds
    Use-UiView $window {param($view) $view.Select('alpha')}
    $before=Read-Presentation $window
    Assert-Ui ($before.Mode -eq 4) 'Scenario requires the real Details fallback before SQL is readable.'
    [void]$release.Set()
    Assert-Ui ($holder.Process.WaitForExit(15000)) 'Holder did not release.'
    Assert-Ui ((Complete-RecordedProcess $holder) -eq 0) 'Holder failed.'
    # An ordinary activation/state notification supplies the next natural retry.
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x0006,[IntPtr]::Zero,[IntPtr]::Zero)) 'Natural deactivation notification failed.'
    Wait-Ui {
        $state=Read-Presentation $window
        return $state.Enabled -and $state.Mode -eq $reference.Mode -and $state.IconSize -eq $reference.IconSize
    } 'late native presentation recovery' 300 25
    $after=Read-Presentation $window
    Assert-Positions $reference.Positions $after.Positions
    Assert-Ui ($after.Selected.Count -eq 1 -and $after.Selected[0] -eq 'alpha' -and $after.Focused -eq 'alpha') 'Recovery replaced current selection/focus.'
    Close-UiFolder $window
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count -eq 0} 'late process normal exit'
    $reopened=Open-UiFolder $folder 2
    $persisted=Read-Presentation $reopened
    Assert-Positions $reference.Positions $persisted.Positions
    Assert-Ui ($persisted.Mode -eq $reference.Mode -and $persisted.IconSize -eq $reference.IconSize) 'Recovered presentation did not survive a fresh process.'
    $stored=@(& $helper '--inspect-product' $database $key)
    Assert-Ui ($LASTEXITCODE -eq 0) 'Inspect preserved history.'
    $history=($stored -join "`n")|ConvertFrom-Json
    Assert-Ui $history.knownHistoryPreserved 'Late recovery replaced unknown search history.'
    [ordered]@{ProvisionalReadyMilliseconds=$provisionalMilliseconds;Before=$before;After=$after;Persisted=$persisted;
        Reference=$reference;KnownHistoryPreserved=$history.knownHistoryPreserved;RealProductSingleton=$true;PhysicalInput=$false
    }|ConvertTo-Json -Depth 6|Set-Content (Join-Path $RunDirectory 'late-recovery.json') -Encoding UTF8
    Write-Host 'PASS real late recovery, metadata identity, preserved SQL icon positions and current selection/focus.'
} catch {$failure=$_;throw}
finally {
    if($release){[void]$release.Set()}
    if($holder -and !$holder.Process.HasExited){[void]$holder.Process.WaitForExit(15000)}
    if($holder -and $holder.Process.HasExited){[void](Complete-RecordedProcess $holder)}
    foreach($process in @($script:UiProcesses|Where-Object{!$_.HasExited})){
        # Cleanup also knows frames that failed before Shell registration.
        foreach($window in @([MutexRecoveryNative]::Find($process.Id,'FileSpacer Item Window'))){
            if(!$script:UiWindows.Contains($window)){[void]$script:UiWindows.Add($window)}
        }
        foreach($dialog in @([MutexRecoveryNative]::Find($process.Id,'#32770'))){
            [void][MutexRecoveryNative]::PostMessage($dialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        }
    }
    Stop-Ui $failure
    if($ready){$ready.Dispose()};if($release){$release.Dispose()}
}
