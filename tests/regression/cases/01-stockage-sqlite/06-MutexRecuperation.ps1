# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$helper=Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
$probe=Build-NativeTest -Name FolderIdentityProbe -CaseDirectory $PSScriptRoot -ProductionSources @('FolderIdentity.cpp')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryUi.cs')
$script:dismissedDialogs=@{}
function Require($ok,[string]$message){if(!$ok){throw $message}}
function Wait-Folder([int]$processId){
    $end=[DateTime]::UtcNow.AddSeconds(25)
    do{
        $additional=@([MutexRecoveryNative]::Find($processId,'#32770') | Where-Object {$_ -ne $initialDialog})
        Require ($additional.Count -eq 0) 'Additional product dialog after acknowledging the initialization storage error.'
        $found=@([MutexRecoveryNative]::Find($processId,'FileSpacer Item Window'))
        if($found.Count -eq 1){return $found[0]}
        Start-Sleep -Milliseconds 50
    }while([DateTime]::UtcNow -lt $end)
    throw 'Product folder window did not open.'
}
function Dismiss-Errors([int]$processId){
    foreach($dialog in @([MutexRecoveryNative]::Find($processId,'#32770'))){
        $tag=$dialog.ToInt64().ToString()
        if($script:dismissedDialogs.ContainsKey($tag)){continue}
        @([MutexRecoveryNative]::Describe($processId))|ConvertTo-Json|Set-Content (Join-Path $RunDirectory ('dialog-'+$tag+'.json')) -Encoding UTF8
        $script:dismissedDialogs[$tag]=$true
        Require ([MutexRecoveryNative]::PostMessage($dialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Close identified fixture error dialog.'
    }
}
$foreground=[MutexRecoveryNative]::GetForegroundWindow()
[uint32]$foregroundProcessId=0
[void][MutexRecoveryNative]::GetWindowThreadProcessId($foreground,[ref]$foregroundProcessId)
$foregroundProcess=Get-Process -Id $foregroundProcessId -ErrorAction SilentlyContinue
$foregroundName=if($null -eq $foregroundProcess){'<none>'}else{$foregroundProcess.ProcessName}
$preflight=[ordered]@{ForegroundProcess=$foregroundName;ForegroundClass=[MutexRecoveryNative]::ClassName($foreground);EverythingRunning=@(Get-Process Everything -ErrorAction SilentlyContinue).Count}
$preflight|ConvertTo-Json|Set-Content (Join-Path $RunDirectory 'desktop-preflight.json') -Encoding UTF8
if($foregroundName -in @('chrome','msedge','LockApp','LogonUI')){
    throw ('BLOCKED: ForegroundPrerequisite: '+$foregroundName+' occupies the guest desktop; no foreign process was closed.')
}
$folder=Join-Path $RunDirectory 'known-folder'
[void][IO.Directory]::CreateDirectory($folder)
$identity=@(& $probe $folder)
Require ($LASTEXITCODE -eq 0 -and $identity.Count -eq 2) 'Folder identity probe failed.'
$key=$identity[0]
$database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
[void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
Invoke-TestProgram $helper @('--seed-product',$database,$key)
$prefix='Local\FileSpacer.MutexRecovery.'+[Guid]::NewGuid().ToString('N')
$ready=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($prefix+'.ready'))
$release=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($prefix+'.release'))
$holder=$null;$application=$null;$firstFailure=$null
$initialDialog=[IntPtr]::Zero
$processes=New-Object Collections.ArrayList
try{
    $holder=Start-RecordedProcess -Executable $helper -Arguments ('--child "'+$database+'" "'+$prefix+'" hold') -OutputPath (Join-Path $RunDirectory 'holder.out') -ErrorPath (Join-Path $RunDirectory 'holder.err')
    Require ($ready.WaitOne(15000)) 'Controlled mutex holder did not become ready.'
    $application=Start-TestApplication -Executable (Join-Path $ProjectRoot 'build\debug\FileSpacer.exe') -Arguments ('/test "'+$folder+'"') -OutputDirectory $RunDirectory
    [void]$processes.Add($application)
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    $errorObserved=$false
    do{
        $dialogs=@([MutexRecoveryNative]::Find($application.Id,'#32770'))
        if($dialogs.Count){
            Require ($dialogs.Count -eq 1) 'Unexpected simultaneous product dialogs during blocked initialization.'
            @([MutexRecoveryNative]::Describe($application.Id))|ConvertTo-Json|Set-Content (Join-Path $RunDirectory 'dialog-during-identification.json') -Encoding UTF8
            $messages=[MutexRecoveryNative]::Messages((Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'),278)
            Require ($messages.Count -gt 0) 'Product storage-error string resources were not found.'
            if([MutexRecoveryNative]::ContainsText($dialogs[0],$messages) -and [MutexRecoveryNative]::ButtonCount($dialogs[0]) -eq 1){
                $initialDialog=$dialogs[0];$errorObserved=$true;break
            }
        }
        Start-Sleep -Milliseconds 50
    }while([DateTime]::UtcNow -lt $deadline)
    @([MutexRecoveryNative]::Describe($application.Id))|ConvertTo-Json|Set-Content (Join-Path $RunDirectory 'windows-before-release.json') -Encoding UTF8
    Require $errorObserved 'Controlled initialization timeout was not observed before the watchdog; scenario not established.'
    [void]$release.Set()
    Require ($holder.Process.WaitForExit(20000)) 'Controlled holder did not exit.'
    Require ((Complete-RecordedProcess $holder) -eq 0) 'Controlled holder failed.'
    # Exactly one acknowledgement, of the identified storage error, is part of this scenario.
    # Other dialogs are observed as failures; only finally may close them for cleanup.
    Require ([MutexRecoveryNative]::PostMessage($initialDialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Acknowledge identified initialization storage error.'
    $window=Wait-Folder $application.Id
    $rect=New-Object MutexRecoveryNative+Rect
    Require ([MutexRecoveryNative]::GetWindowRect($window,[ref]$rect)) 'Read product window rectangle.'
    $dpi=[MutexRecoveryNative]::GetDpiForWindow($window)
    Require ($dpi -gt 0) 'Read product window DPI.'
    $left=$rect.Left+37;$top=$rect.Top+29
    Require ([MutexRecoveryNative]::SetWindowPos($window,[IntPtr]::Zero,$left,$top,$rect.Right-$rect.Left,$rect.Bottom-$rect.Top,0x14)) 'Move product window.'
    Require ([MutexRecoveryNative]::PostMessage($window,0x232,[IntPtr]::Zero,[IntPtr]::Zero)) 'End product move gesture.'
    $expectedX=[int][Math]::Round($left*96.0/$dpi)
    $expectedY=[int][Math]::Round($top*96.0/$dpi)
    $end=[DateTime]::UtcNow.AddSeconds(5)
    $recovered=$false;$stored=$null
    do{
        Require (@([MutexRecoveryNative]::Find($application.Id,'#32770')).Count -eq 0) 'Unexpected additional product dialog during recovery measurement.'
        $lines=@(& $helper '--inspect-product' $database $key)
        Require ($LASTEXITCODE -eq 0) 'Inspect product database failed.'
        $stored=($lines -join "`n")|ConvertFrom-Json
        if([Math]::Abs($stored.x-$expectedX) -le 1 -and [Math]::Abs($stored.y-$expectedY) -le 1){$recovered=$true;break}
        Start-Sleep -Milliseconds 100
    }while([DateTime]::UtcNow -lt $end)
    [ordered]@{InitialErrorDialogObserved=$errorObserved;ExpectedX=$expectedX;ExpectedY=$expectedY;Stored=$stored;RecoveredInSameProcess=$recovered;RealWindowsSingleton=$true}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $RunDirectory 'recovery.json') -Encoding UTF8
    Require $stored.knownHistoryPreserved 'Unknown search history was overwritten.'
    Require $recovered 'Product did not resume persistence after the initialization mutex was released; the real singleton retains its failed initialization.'
    Write-Host 'PASS: real product singleton recovers and preserves known history.'
}catch{$firstFailure=$_;throw}
finally{
    [void]$release.Set()
    if($application){
        @([MutexRecoveryNative]::Describe($application.Id))|ConvertTo-Json|Set-Content (Join-Path $RunDirectory 'windows-before-cleanup.json') -Encoding UTF8
        $end=[DateTime]::UtcNow.AddSeconds(10)
        do{
            Dismiss-Errors $application.Id
            foreach($window in @([MutexRecoveryNative]::Find($application.Id,'FileSpacer Item Window'))){
                [void][MutexRecoveryNative]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
            }
            if($application.HasExited){break}
            Start-Sleep -Milliseconds 100
        }while([DateTime]::UtcNow -lt $end)
    }
    try{
        if($holder){
            Require ($holder.Process.WaitForExit(30000)) 'Holder cleanup timed out.'
            Require ((Complete-RecordedProcess $holder) -eq 0) 'Holder cleanup failed.'
        }
        Complete-TestApplications $processes
    }catch{
        if($null -eq $firstFailure){throw}
        throw ('Initial failure: '+$firstFailure.Exception.Message+'; cleanup failure: '+$_.Exception.Message)
    }finally{
        if($holder){$holder.Process.Dispose()}
        $ready.Dispose();$release.Dispose()
    }
}
