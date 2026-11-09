# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [ValidateSet('plain','filtered','locked','closing','closingLocked')][string]$Scenario='plain',
    [ValidateRange(2,5000)][int]$ItemCount=2,[switch]$MeasureRecovery)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\StorageSnapshot.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryUi.cs')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryInput.cs') -ReferencedAssemblies ([FileSpacerRegression.Native].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\28-apercu-affichage\ViewMenuUi.cs') -ReferencedAssemblies ([FileSpacerRegression.Native].Assembly.Location)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Drawing
$titleReferences=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location,[System.Drawing.Bitmap].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\33-menu-titre\TitleMenuUi.cs') -ReferencedAssemblies $titleReferences
$helper=Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
$probe=Build-NativeTest -Name FolderIdentityProbe -CaseDirectory $PSScriptRoot -ProductionSources @('FolderIdentity.cpp')
function Read-Presentation([IntPtr]$Window,[switch]$Light,[string]$PositionWitness='') {
    $view=Get-UiView $Window
    Assert-Ui ($null -ne $view) 'Registered native view missing.'
    try {
        $mode=$view.Mode;$arranged=$view.AutoArrange
        $positions=if(!$Light -and $mode -notin @(3,4) -and !$arranged){$view.Positions()}else{''}
        $selected=@();$focused=''
        if(!$Light){$selected=@($view.Selected());$focused=$view.Focused()}
        $witnessPosition=''
        if($PositionWitness){$witnessPosition=$view.Position($PositionWitness)}
        return [pscustomobject]@{Mode=$mode;IconSize=$view.IconSize;AutoArrange=$arranged;Count=$view.Count;
            Positions=$positions;Selected=$selected;Focused=$focused;WitnessPosition=$witnessPosition;
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
            [Math]::Abs([int]$wanted[$name][1]-[int]$found[$name][1]) -le 1) ('Saved position replaced for '+$name+'; expected='+($wanted[$name] -join ',')+'; actual='+($found[$name] -join ','))
    }
}
$holder=$null;$ready=$null;$release=$null;$failure=$null
try {
    Initialize-Ui -Name ('SQLiteProvisional-'+$Scenario) -FixtureParent $RunDirectory
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    Set-UiPreference 'WindowStateLockEnabled' 1
    Set-UiPreference 'SearchHistoryLimit' 20
    $folder=Join-Path $script:UiRoot 'known-folder'
    [void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'alpha'),'old')
    [IO.File]::WriteAllText((Join-Path $folder 'beta'),'old')
    for($index=2;$index -lt $ItemCount;++$index){[IO.File]::WriteAllText((Join-Path $folder ('item-{0:D5}' -f $index)),'fixture')}
    $probeJob=$null
    $probeOutput=Join-Path $RunDirectory 'fixture-identity.stdout.log'
    $probeError=Join-Path $RunDirectory 'fixture-identity.stderr.log'
    $probeEvidence=[ordered]@{Executable=$probe;ExecutableCharacters=$probe.Length;Folder=$folder;FolderCharacters=$folder.Length;ExitCode=$null;Error=$null}
    try{
        $probeJob=Start-RecordedProcess -Executable $probe -Arguments ('"'+$folder+'"') -OutputPath $probeOutput -ErrorPath $probeError
        $probeJob.Process.WaitForExit()
        $probeEvidence.ExitCode=Complete-RecordedProcess $probeJob
    }catch{$probeEvidence.Error=$_.Exception.Message;throw}
    finally{
        $probeEvidence|ConvertTo-Json -Depth 3|Set-Content (Join-Path $RunDirectory 'fixture-identity.json') -Encoding UTF8
        if($probeJob){$probeJob.Process.Dispose()}
    }
    $identity=@(Get-Content -LiteralPath $probeOutput)
    Assert-Ui ($probeEvidence.ExitCode -eq 0 -and $identity.Count -eq 2) 'Fixture identity failed; see fixture-identity.json/stdout/stderr.'
    $key=$identity[0];$database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
    [void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
    Invoke-TestProgram $helper @('--seed-product',$database,$key)
    $first=Open-UiFolder $folder $ItemCount
    Use-UiView $first {param($view) $view.SetViewModeAndIconSize(1,64)}
    Wait-UiViewMode $first 1 64
    Use-UiView $first {param($view) $view.AutoArrange=$false}
    Wait-Ui {$state=Read-Presentation $first -Light;return $state.Mode -eq 1 -and !$state.AutoArrange -and $state.Count -eq $ItemCount} 'enumerated reference icon mode' 300
    Use-UiView $first {param($view) $view.Position('alpha',350,160);$view.Position('beta',560,280)}
    $reference=Read-Presentation $first
    $reference|ConvertTo-Json -Depth 4|Set-Content (Join-Path $RunDirectory 'reference-view.json') -Encoding UTF8
    if($Scenario -in @('locked','closingLocked')){
        [void][FileSpacerRegression.Native]::Message($first,0x111,(Get-UiId 'IDM_TOGGLE_STATE_LOCK'),0)
        $lockLabels=@([MutexRecoveryNative]::Messages($script:UiExecutable,(Get-UiId 'IDS_WINDOW_STATE_LOCKED')))
        Wait-Ui {
            $texts=@([FileSpacerRegression.Native]::Children($first,'Static')|ForEach-Object{[FileSpacerRegression.Native]::Text($_)})
            foreach($label in $lockLabels){
                foreach($text in $texts){if($text.EndsWith(' - '+$label,[StringComparison]::Ordinal)){return $true}}
            }
            return $false
        } 'reference lock badge from the current product resources' 200
        $reference=Read-Presentation $first
    }
    Close-UiFolder $first
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count -eq 0} 'reference process normal exit'
    if($Scenario -in @('locked','closingLocked')){
        $verifiedReference=Open-UiFolder $folder $ItemCount
        $persistedReference=Read-Presentation $verifiedReference
        $persistedReference|ConvertTo-Json -Depth 5|Set-Content (Join-Path $RunDirectory 'phase-persisted-reference.json') -Encoding UTF8
        Assert-Positions $reference.Positions $persistedReference.Positions
        Assert-Ui ($persistedReference.Mode -eq $reference.Mode -and $persistedReference.IconSize -eq $reference.IconSize) 'Locked reference was not persisted before the outage.'
        Wait-Ui {
            $texts=@([FileSpacerRegression.Native]::Children($verifiedReference,'Static')|ForEach-Object{[FileSpacerRegression.Native]::Text($_)})
            foreach($label in $lockLabels){foreach($text in $texts){if($text.EndsWith(' - '+$label,[StringComparison]::Ordinal)){return $true}}}
            return $false
        } 'persisted reference lock badge' 200
        Close-UiFolder $verifiedReference
        Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count -eq 0} 'verified reference process normal exit'
    }
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
        $messages=[MutexRecoveryNative]::Messages($script:UiExecutable,(Get-UiId 'IDS_FOLDER_STATE_ERROR'))
        if([MutexRecoveryNative]::ContainsText($dialogs[0],$messages) -and [MutexRecoveryNative]::ButtonCount($dialogs[0]) -eq 1){$script:storageDialog=$dialogs[0];return $true}
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
        try{return $view.Count -eq $ItemCount -and [FileSpacerRegression.Native]::IsWindowEnabled($view.Window)}finally{$view.Dispose()}
    } 'interactive provisional native view while SQLite stays blocked' 0 90
    $window=$script:lateWindow;[void]$script:UiWindows.Add($window)
    Assert-Ui (!$holder.Process.HasExited -and !$release.WaitOne(0)) 'Holder released before the provisional scenario was established.'
    $provisionalMilliseconds=$started.Elapsed.TotalMilliseconds
    Use-UiView $window {param($view) $view.Select('alpha')}
    $before=Read-Presentation $window
    Assert-Ui ($before.Mode -eq 4) 'Scenario requires the real Details fallback before SQL is readable.'

    Activate-Ui $window
    $popup=[FileSpacerViewMenuTests.MenuUi]::Open($window,(Get-UiCommandToolbar $window),(Get-UiId 'IDM_VIEW_MENU'))
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,2,$window)
    Wait-Ui {$null -eq [FileSpacerViewMenuTests.MenuUi]::Active($window)} 'native mode menu closed'
    Wait-Ui {$p=Read-Presentation $window -Light;return $p.Mode -notin @(3,4) -and $p.Count -eq $ItemCount} 'provisional native spatial mode' 300
    Use-UiView $window {param($view) $view.AutoArrange=$false}
    $spatialBefore=Read-Presentation $window
    Assert-Ui (!$spatialBefore.AutoArrange -and $spatialBefore.IconSize -ne $reference.IconSize) 'Distinct provisional native size/manual layout not established.'
    # No physical input in the view has occurred since switching to spatial.
    # The first button-down must establish the icon baseline before this move.
    Activate-Ui $window
    $point=[TitleMenuUi]::CatalogPoint($window,'alpha')
    $bounds=Use-UiView $window {param($view) $view.Bounds}
    $inputMilliseconds=$null
    $captureBeforeUp=$null;$captureAfterUp=$null
    if($MeasureRecovery){
        Use-UiView $window {param($view) $view.Select('beta')}
        Wait-Ui {((Use-UiView $window {param($view) $view.Selected()}) -join ';') -eq 'beta'} 'loaded input witness starts on beta' 200
        $down=$false;$inputWatch=[Diagnostics.Stopwatch]::StartNew()
        try{
            [StorageRecoveryInput]::Down($window,$point[0],$point[1]);$down=$true
            Wait-Ui {((Use-UiView $window {param($view) $view.Selected()}) -join ';') -eq 'alpha'} 'loaded first input reached the native view' 0 30
            $inputWatch.Stop();$inputMilliseconds=$inputWatch.Elapsed.TotalMilliseconds
        }finally{
            if($down){
                $captureBeforeUp=[StorageRecoveryInput]::CaptureOwner($window)
                [StorageRecoveryInput]::Up()
                $captureAfterUp=[StorageRecoveryInput]::CaptureOwner($window)
            }
        }
    }else{
        $targetX=[Math]::Min($point[0]+180,$bounds.Right-40)
        $targetY=[Math]::Min($point[1]+120,$bounds.Bottom-50)
        Assert-Ui ($targetX-$point[0] -ge 60 -and $targetY-$point[1] -ge 60) 'Insufficient exposed view for the first drag.'
        $inputTarget=[StorageRecoveryInput]::PhysicalTarget($window,$point[0],$point[1])
        [TitleMenuUi]::Drag($inputTarget,$point[0],$point[1],$targetX,$targetY,$false)
        Wait-Ui {(Read-Presentation $window).Positions -ne $spatialBefore.Positions} 'first native icon drag changed placement' 300
    }
    $changed=Read-Presentation $window
    $provisionalDpi=[MutexRecoveryNative]::GetDpiForWindow($window)
    $captureAfterObservation=[StorageRecoveryInput]::CaptureOwner($window)
    Assert-Ui ($changed.Selected.Count -eq 1 -and $changed.Selected[0] -eq 'alpha') 'Dragged alpha selection not established.'
    Assert-Ui (!$holder.Process.HasExited -and !$release.WaitOne(0)) 'Provisional changes were made after the outage ended.'
    $alpha=@($changed.Positions.Split(';')|Where-Object{$_ -like 'alpha=*'})
    $other=@($reference.Positions.Split(';')|Where-Object{$_ -and $_ -notlike 'alpha=*'})
    Assert-Ui ($alpha.Count -eq 1 -and $other.Count -eq ($ItemCount - 1)) 'Expected icon identities missing.'
    $expected=if($MeasureRecovery){$reference.Positions}else{(@($alpha[0])+$other|Sort-Object) -join ';'}
    $selectionBeforeRecovery=$changed
    if($Scenario -eq 'filtered'){
        # The existing contract does not record positions changed under a
        # filter. Preserve the complete unfiltered move made above instead.
        [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'alpha')
        [void][FileSpacerRegression.Native]::Message((Get-UiSearchSubmit $window),0xf5,0,0)
        Wait-Ui {(Read-Presentation $window -Light).Count -eq 1} 'filtered provisional alpha' 300
        $selectionBeforeRecovery=Read-Presentation $window
    }
    [ordered]@{Reference=$reference;SpatialBefore=$spatialBefore;Changed=$changed;ExpectedPositions=$expected;
        SelectionBeforeRecovery=$selectionBeforeRecovery;InputUntilSelectionMilliseconds=$inputMilliseconds;
        Scenario=$Scenario;ItemCount=$ItemCount;ProvisionalReadyMilliseconds=$provisionalMilliseconds;
        CaptureBeforeUp=$captureBeforeUp;CaptureAfterUp=$captureAfterUp;CaptureAfterObservation=$captureAfterObservation
    }|ConvertTo-Json -Depth 6|Set-Content (Join-Path $RunDirectory 'phase-before-recovery.json') -Encoding UTF8
    $menuBeforeClose=$false
    if($Scenario -eq 'closing'){
        # A genuine native item context menu keeps live recovery deferred.
        # Closing while it is active exercises the direct merge on destruction.
        [FileSpacerRegression.Native]::Keys($window,0x5d,$false,$false)
        Wait-Ui {[StorageRecoveryInput]::NativeMenuActive($window)} 'native context menu on the product UI thread' 200
    }
    $expectedWitness=''
    if($MeasureRecovery){
        $expectedWitness=@($reference.Positions.Split(';')|Where-Object{$_ -like 'beta=*'})
        Assert-Ui ($expectedWitness.Count-eq1) 'SQL recovery witness beta missing.'
        $expectedWitness=$expectedWitness[0]
        $pending=Read-Presentation $window -Light -PositionWitness 'beta'
        Assert-Ui ($pending.WitnessPosition-ne$expectedWitness) 'Recovery witness already equals SQL before release.'
        [ordered]@{ExpectedSql=$expectedWitness;ObservedBeforeRelease=$pending.WitnessPosition;Window=$window.ToInt64();
            Read='One named item position; no full position enumeration';Trigger='Injected PostMessage WM_ACTIVATE/WA_INACTIVE'
        }|ConvertTo-Json|Set-Content (Join-Path $RunDirectory 'witness-before-release.json') -Encoding UTF8
    }
    if($Scenario -in @('closing','closingLocked')){
        # Observe while SQL is still unavailable. A COM read after release can
        # dispatch recovery before the closing path has been requested.
        $after=Read-Presentation $window
        Assert-Positions $changed.Positions $after.Positions
    }
    [void]$release.Set()
    Assert-Ui ($holder.Process.WaitForExit(15000)) 'Holder did not release.'
    Assert-Ui ((Complete-RecordedProcess $holder) -eq 0) 'Holder failed.'
    if($Scenario -in @('closing','closingLocked')){
        $menuBeforeClose=[StorageRecoveryInput]::NativeMenuActive($window)
        if($Scenario -eq 'closing'){Assert-Ui $menuBeforeClose 'Native menu closed before the closing-path test.'}
        [StorageRecoveryInput]::RequestClose($window)
        Assert-Ui ([StorageRecoveryInput]::WaitForExit($app.Handle,25000)) 'Timed out: close before visible replay (COM-aware wait).'
    }else{
        $observer=$null;$observerWatch=$null;$observerSamples=New-Object Collections.Generic.List[object]
        if($MeasureRecovery){
            $app.Refresh()
            $observerWatch=[Diagnostics.Stopwatch]::StartNew()
            $observer=[ordered]@{Status='RUNNING';ProcessId=$app.Id;ProcessStartedUtc=$app.RegressionStartedUtc.ToString('o');
                Window=$window.ToInt64();ViewWindow=$changed.ViewWindow;LimitSeconds=25;
                CpuBeforeMilliseconds=$app.TotalProcessorTime.TotalMilliseconds;
                SawDisabled=$false;SawEnabledAfterDisabled=$false;DisabledMilliseconds=$null;EnabledMilliseconds=$null;
                FirstPresentationReadMilliseconds=$null;ElapsedMilliseconds=$null;Error=$null;Samples=@()}
        }
        try {
            Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x0006,[IntPtr]::Zero,[IntPtr]::Zero)) 'Natural deactivation notification failed.'
            Wait-Ui {
            if($MeasureRecovery){
                $app.Refresh()
                Assert-Ui (!$app.HasExited) 'Observed recovery process exited before its input edge.'
                foreach($knownWindow in @($window,[IntPtr]$changed.ViewWindow)){
                    $ownerId=[uint32]0
                    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($knownWindow,[ref]$ownerId)
                    Assert-Ui ($ownerId-eq$app.Id) 'Observed recovery HWND no longer belongs to the known process.'
                }
                $enabled=[FileSpacerRegression.Native]::IsWindowEnabled([IntPtr]$changed.ViewWindow)
                $elapsed=$observerWatch.Elapsed.TotalMilliseconds
                [void]$observerSamples.Add([pscustomobject]@{Milliseconds=$elapsed;Enabled=$enabled;
                    CpuMilliseconds=($app.TotalProcessorTime.TotalMilliseconds-$observer.CpuBeforeMilliseconds)})
                if(!$enabled){
                    if(!$observer.SawDisabled){$observer.DisabledMilliseconds=$elapsed}
                    $observer.SawDisabled=$true
                    return $false
                }
                # An initially enabled view is not evidence of completed recovery.
                # No presentation COM call may run until this measured input edge.
                if(!$observer.SawDisabled){return $false}
                if(!$observer.SawEnabledAfterDisabled){$observer.EnabledMilliseconds=$elapsed}
                $observer.SawEnabledAfterDisabled=$true
            }
            if(![FileSpacerRegression.Native]::IsWindowEnabled([IntPtr]$changed.ViewWindow)){return $false}
            if($observer -and $null-eq$observer.FirstPresentationReadMilliseconds){
                $observer.FirstPresentationReadMilliseconds=$observerWatch.Elapsed.TotalMilliseconds
            }
            $p=Read-Presentation $window -Light
            if(!$p.Enabled -or $p.Mode-ne$changed.Mode -or $p.IconSize-ne$changed.IconSize){return $false}
            if($MeasureRecovery){
                $witness=Read-Presentation $window -Light -PositionWitness 'beta'
                return $witness.Enabled -and $witness.WitnessPosition-eq$expectedWitness
            }
            if($Scenario -ne 'filtered'){
                $lockedWitness=Read-Presentation $window -Light -PositionWitness 'beta'
                $wantedBeta=@($expected.Split(';')|Where-Object{$_ -like 'beta=*'})
                return $lockedWitness.Enabled -and $wantedBeta.Count-eq1 -and $lockedWitness.WitnessPosition-eq$wantedBeta[0]
            }
            return $true
            } 'changed provisional native presentation recovered' 300 25
            if($observer){$observer.Status='PASS'}
        } catch {
            if($observer){$observer.Status='FAIL';$observer.Error=$_.Exception.Message}
            throw
        } finally {
            if($observer){
                $observer.ElapsedMilliseconds=$observerWatch.Elapsed.TotalMilliseconds
                $observer.Samples=@($observerSamples.ToArray())
                $observer|ConvertTo-Json -Depth 5|Set-Content (Join-Path $RunDirectory 'recovery-input-edge.json') -Encoding UTF8
            }
        }
        $after=Read-Presentation $window
        if($Scenario -ne 'filtered'){Assert-Positions $expected $after.Positions}
        Assert-Ui (($after.Selected -join ';') -eq ($selectionBeforeRecovery.Selected -join ';') -and $after.Focused -eq $selectionBeforeRecovery.Focused) 'Recovery replaced current selection/focus.'
        if($Scenario -eq 'filtered'){
            # Mode and size can already match before asynchronous SQL recovery.
            # Observe the committed full position set before clearing the filter;
            # otherwise that refresh races the recovery this scenario must test.
            $recoveryExpected=Join-Path $RunDirectory 'expected-before-clearing-filter.txt'
            [IO.File]::WriteAllText($recoveryExpected,([string][MutexRecoveryNative]::GetDpiForWindow($window)+"`n"+$expected),(New-Object Text.UTF8Encoding($false)))
            $recoveryCheck=Join-Path $RunDirectory 'recovery-sql-verification'
            Wait-Ui {
                $check=Start-RecordedProcess -Executable $helper -Arguments ('--verify-product "'+$database+'" "'+$key+'" "'+$folder+'" "'+$recoveryExpected+'"') -OutputPath ($recoveryCheck+'.stdout.log') -ErrorPath ($recoveryCheck+'.stderr.log')
                try {
                    if(!$check.Process.WaitForExit(5000)){
                        $check.Process.Kill();$check.Process.WaitForExit()
                        [void](Complete-RecordedProcess $check)
                        throw 'Read-only recovery verification did not finish.'
                    }
                    return (Complete-RecordedProcess $check) -eq 0
                } finally { $check.Process.Dispose() }
            } 'committed recovered icon positions before clearing the filter' 300 25
            [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'')
            [void][FileSpacerRegression.Native]::Message((Get-UiSearchSubmit $window),0xf5,0,0)
            Wait-Ui {(Read-Presentation $window -Light).Count -eq $ItemCount} 'full native view after clearing filter' 300
            Assert-Positions $expected (Read-Presentation $window).Positions
        }
        $after|ConvertTo-Json -Depth 5|Set-Content (Join-Path $RunDirectory 'phase-after-recovery.json') -Encoding UTF8
        Close-UiFolder $window
    }
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count -eq 0} 'provisional process normal exit'
    Assert-Ui ((Complete-RecordedProcess $app.RegressionOutput) -eq 0) 'Provisional process did not exit normally.'
    if($Scenario -in @('closing','closingLocked')){
        # Check the committed state before a new view can capture anything.
        $closedSnapshot=Join-Path $RunDirectory 'closed-process-sql'
        Copy-TestDatabaseSnapshot $database $closedSnapshot
        $closedExpected=Join-Path $RunDirectory 'expected-before-reopen.txt'
        [IO.File]::WriteAllText($closedExpected,([string]$provisionalDpi+"`n"+$expected),(New-Object Text.UTF8Encoding($false)))
        $closedCheck=@(& $helper '--verify-product' (Join-Path $closedSnapshot 'folder-state-test.sqlite3') $key $folder $closedExpected)
        $closedExit=$LASTEXITCODE
        $closedCheck|Set-Content (Join-Path $RunDirectory 'closed-process-verification.log') -Encoding UTF8
        Assert-Ui ($closedExit-eq0) 'Closed provisional process did not persist the expected canonical positions/history before reopening.'
    }
    $reopened=Open-UiFolder $folder $ItemCount
    $wantedAlpha=@($expected.Split(';')|Where-Object{$_ -like 'alpha=*'})
    Assert-Ui ($wantedAlpha.Count-eq1) 'Reopen witness alpha missing.'
    Wait-Ui {
        $witness=Read-Presentation $reopened -Light -PositionWitness 'alpha'
        return $witness.Enabled -and $witness.WitnessPosition-eq$wantedAlpha[0]
    } 'fresh process restored the moved alpha position' 300 25
    $persisted=Read-Presentation $reopened
    $persisted|ConvertTo-Json -Depth 5|Set-Content (Join-Path $RunDirectory 'phase-fresh-process.json') -Encoding UTF8
    if($Scenario -in @('locked','closingLocked')){
        # The saved lock protects geometry/view settings, not icon positions.
        Assert-Positions $expected $persisted.Positions
        Assert-Ui ($persisted.Mode -eq $reference.Mode -and $persisted.IconSize -eq $reference.IconSize) 'Automatic recovery changed the locked reference.'
    }else{
        Assert-Positions $expected $persisted.Positions
        Assert-Ui ($persisted.Mode -eq $changed.Mode -and $persisted.IconSize -eq $changed.IconSize) 'Explicit provisional presentation was not persisted.'
    }
    $dpi=[MutexRecoveryNative]::GetDpiForWindow($reopened)
    Close-UiFolder $reopened
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count-eq0} 'fresh process final normal exit'
    foreach($finished in $script:UiProcesses){Assert-Ui ((Complete-RecordedProcess $finished.RegressionOutput)-eq0) 'Application exit failed before SQL export.'}
    $snapshot=Join-Path $RunDirectory 'post-close-sql'
    Copy-TestDatabaseSnapshot $database $snapshot
    $expectedFile=Join-Path $RunDirectory 'expected-post-close-positions.txt'
    [IO.File]::WriteAllText($expectedFile,([string]$dpi+"`n"+$expected),(New-Object Text.UTF8Encoding($false)))
    $analysis=Join-Path $RunDirectory 'post-close-sql-check'
    Copy-TestDatabaseSnapshot (Join-Path $snapshot 'folder-state-test.sqlite3') $analysis
    $verification=@(& $helper '--verify-product' (Join-Path $analysis 'folder-state-test.sqlite3') $key $folder $expectedFile)
    Assert-Ui ($LASTEXITCODE-eq0) 'Canonical SQL positions or history differ after the final process exit.'
    $verification|Set-Content -LiteralPath (Join-Path $RunDirectory 'post-close-verification.json') -Encoding UTF8
    $stored=@(& $helper '--inspect-product' $database $key)
    Assert-Ui ($LASTEXITCODE -eq 0) 'Inspect preserved history.'
    $history=($stored -join "`n")|ConvertFrom-Json
    Assert-Ui $history.knownHistoryPreserved 'Late recovery replaced unknown search history.'
    [ordered]@{ProvisionalReadyMilliseconds=$provisionalMilliseconds;Before=$before;After=$after;Persisted=$persisted;
        Reference=$reference;Changed=$changed;Scenario=$Scenario;ItemCount=$ItemCount;InputUntilSelectionMilliseconds=$inputMilliseconds;MenuBeforeClose=$menuBeforeClose;ExpectedPositions=$expected;KnownHistoryPreserved=$history.knownHistoryPreserved;RealProductSingleton=$true;PhysicalInput=$true
    }|ConvertTo-Json -Depth 6|Set-Content (Join-Path $RunDirectory 'provisional-recovery.json') -Encoding UTF8
    Write-Host ('PASS real provisional native menu/first drag, unknown SQL positions, selection/history and '+$Scenario+' recovery.')
} catch {$failure=$_;throw}
finally {
    try {
    if($release){[void]$release.Set()}
    if($holder -and !$holder.Process.HasExited){[void]$holder.Process.WaitForExit(15000)}
    if($holder -and $holder.Process.HasExited){[void](Complete-RecordedProcess $holder)}
    foreach($process in @($script:UiProcesses|Where-Object{!$_.HasExited})){
        # Cleanup also knows frames that failed before Shell registration.
        foreach($window in @([MutexRecoveryNative]::Find($process.Id,'FileSpacer Item Window'))){
            [void][FileSpacerRegression.Native]::PostMessage($window,0x1f,[IntPtr]::Zero,[IntPtr]::Zero)
            if(!$script:UiWindows.Contains($window)){[void]$script:UiWindows.Add($window)}
        }
        foreach($dialog in @([MutexRecoveryNative]::Find($process.Id,'#32770'))){
            [void][MutexRecoveryNative]::PostMessage($dialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        }
    }
    Stop-Ui $failure
    if($ready){$ready.Dispose()};if($release){$release.Dispose()}
    } catch {
        if(!$failure){$failure=$_}
        throw
    }
}
