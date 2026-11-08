# Test-Category: UI
param([string]$ProjectRoot,[switch]$IncludePublic=($env:FS_GESTURE_PUBLIC-eq'1'),
    [ValidateRange(1,3)][int]$Repetitions=$(if($env:FS_GESTURE_REPETITIONS){[int]$env:FS_GESTURE_REPETITIONS}else{1}),
    [ValidateRange(128,5000)][int]$FixtureCount=$(if($env:FS_GESTURE_COUNT){[int]$env:FS_GESTURE_COUNT}else{256}))
$ErrorActionPreference='Stop'
. (Join-Path $ProjectRoot 'tests\regression\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\15-deplacement-alt\NativeTargets.cs')
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\28-apercu-affichage\ViewMenuUi.cs')
if($env:FS_GESTURE_ACTIONS-match'marquee|activation|item-drag'){
    Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,WindowsBase
    $references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
    Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
}
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class GestureComposition {
    [DllImport("dwmapi.dll")] public static extern int DwmFlush();
    [DllImport("user32.dll")] public static extern uint GetDoubleClickTime();
}
'@
if(Get-Process FileSpacer -ErrorAction SilentlyContinue){throw 'Close FileSpacer before building the native helper.'}
$counter=Build-NativeTest -Name RemainingSelectionTests -CaseDirectory (Join-Path $ProjectRoot 'tests\experiments\reactivity') -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib','Dbghelp.lib','Psapi.lib')
$presentation=$env:FS_GESTURE_PRESENTATION-eq'1'
$preseed=$env:FS_GESTURE_PRESEED-eq'1'
if($preseed){Assert-Ui ($env:FS_GESTURE_VIEWS-and@($env:FS_GESTURE_VIEWS-split',').Count-eq1) 'Persisted-view preparation requires one view.'}
if($presentation){$observer=Build-NativeTest -Name GesturePresentationTests -CaseDirectory (Join-Path $ProjectRoot 'tests\experiments\reactivity') -Standard c++14 -ControlFlowGuard -ProductionSources @('DPI.cpp') -Libraries @('Ole32.lib','Gdi32.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Dwmapi.lib')}
$capture=$null
function Set-GestureStage([string]$Name){
    @{Stage=$Name;Utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'gesture-stage.json') -Encoding UTF8
}
$qualified=if($env:FS_GESTURE_CACHE){[IO.Path]::GetFullPath($env:FS_GESTURE_CACHE)}else{Join-Path $ProjectRoot 'build\remaining-selection\cache'}
Assert-Ui (Test-Path -LiteralPath $qualified) 'Supply the current Shell-qualified cache with FS_GESTURE_CACHE.'
$rows=New-Object Collections.Generic.List[object]
function Read-RangeCounter([IntPtr]$Window) {
    [uint32]$targetId=0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$targetId)
    # A console helper launched with '&' can allocate a visible console when
    # its PowerShell parent has none. Keep observation from stealing focus or
    # generating extra top-level-window accessibility events.
    $job=Start-RecordedProcess -Executable $counter -Arguments (Join-ProcessArguments @('--application-counter',[string]$targetId,$script:UiExecutable)) -OutputPath (Join-Path $env:FS_REGRESSION_RUN 'counter.stdout.log') -ErrorPath (Join-Path $env:FS_REGRESSION_RUN 'counter.stderr.log')
    try {
        if(!$job.Process.WaitForExit(10000)){$job.Process.Kill();$job.Process.WaitForExit();throw 'Owned counter helper timed out.'}
        $code=Complete-RecordedProcess $job
        $value=$job.Output.Result.Trim()
        if($code-ne0-or$value-notmatch'^\d+$'){throw 'Cannot read the isolated application counter.'}
        return [uint64]$value
    }finally{$job.Process.Dispose()}
}
function Wait-GestureCount([IntPtr]$Window,[int]$Expected) {
    $limit=[DateTime]::UtcNow.AddSeconds(30)
    do {
        $actual=Use-UiView $Window {param($view) $view.SelectedCount}
        if($actual-eq$Expected){return}
        Start-Sleep -Milliseconds 2
    }while([DateTime]::UtcNow-lt$limit)
    throw "Selection did not reach $Expected (last $actual)."
}
function Close-GestureObservation($Capture) {
    if($null-eq$Capture){return}
    try {
        if(!$Capture.Job.Process.HasExited){$Capture.Job.Process.Kill();$Capture.Job.Process.WaitForExit()}
        [void](Complete-RecordedProcess $Capture.Job)
    }finally{
        $Capture.Job.Process.Dispose()
        foreach($event in $Capture.Events.Values){$event.Dispose()}
    }
}
function Wait-GestureObserverEvent($Capture,[string]$Name) {
    $limit=[DateTime]::UtcNow.AddSeconds(15)
    do {
        if($Capture.Events[$Name].WaitOne(50)){return}
        if($Capture.Job.Process.HasExited){throw ('Presentation observer failed: '+$Capture.Job.Error.Result)}
    }while([DateTime]::UtcNow-lt$limit)
    throw ('Presentation observer timed out: '+$Name)
}
function Start-GestureObservation([IntPtr]$Window,[string]$Label) {
    $stem='Local\FileSpacer-Gesture-'+[Guid]::NewGuid().ToString('N')
    $events=@{};$job=$null
    try {
        foreach($name in @('ready','begin','started','returned')){$events[$name]=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($stem+'-'+$name))}
        [uint32]$targetId=0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$targetId)
        $body=Use-UiView $Window {param($view) $view.Window}
        $prefix=Join-Path $env:FS_REGRESSION_RUN $Label
        $job=Start-RecordedProcess -Executable $observer -Arguments (Join-ProcessArguments @([string]$targetId,[string]$body.ToInt64(),$script:UiExecutable,$prefix,$stem)) -OutputPath ($prefix+'.stdout.log') -ErrorPath ($prefix+'.stderr.log')
        $result=[pscustomobject]@{Job=$job;Events=$events;Prefix=$prefix}
        Wait-GestureObserverEvent $result 'ready'
        return $result
    }catch{
        if($null-ne$job){Close-GestureObservation ([pscustomobject]@{Job=$job;Events=$events})}
        else{foreach($event in $events.Values){$event.Dispose()}}
        throw
    }
}
$failure=$null
try {
    Initialize-Ui 'selection-gestures' -FixtureParent (Join-Path $ProjectRoot 'build')
    if($env:FS_GESTURE_EXECUTABLE) {
        $candidate=[IO.Path]::GetFullPath($env:FS_GESTURE_EXECUTABLE)
        $allowed=[IO.Path]::GetFullPath((Join-Path $ProjectRoot 'build')).TrimEnd('\')+'\'
        Assert-Ui ($candidate.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)-and(Test-Path -LiteralPath $candidate)) 'Use an owned executable below the project build directory.'
        $script:UiExecutable=$candidate
    }
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $cache=Join-Path $script:UiRoot 'qualified-cache';[void][IO.Directory]::CreateDirectory($cache)
    Get-ChildItem -LiteralPath $qualified -File | Where-Object {$_.Extension-eq'.bin'-or$_.Name-like'*-r1.range'} | Copy-Item -Destination $cache
    $managerCache=Join-Path $script:UiRoot 'manager-only-cache'
    if($env:FS_GESTURE_MANAGER_ONLY-eq'1'){
        [void][IO.Directory]::CreateDirectory($managerCache)
        Get-ChildItem -LiteralPath $cache -Filter '*.bin' -File|Copy-Item -Destination $managerCache
    }
    $arms=@('private';if($env:FS_GESTURE_MANAGER_ONLY-eq'1'){'manager-only'};if($IncludePublic){'public'})
    foreach($arm in $arms) {
        $env:FS_SELECTION_PRODUCT_CACHE=if($arm-eq'private'){$cache}elseif($arm-eq'manager-only'){$managerCache}else{''}
        $folder=Join-Path $script:UiRoot $arm;[void][IO.Directory]::CreateDirectory($folder)
        $initialCount=if($preseed){128}else{$FixtureCount}
        for($i=0;$i-lt$initialCount;++$i){[IO.File]::WriteAllText((Join-Path $folder ('File{0:D5}.txt'-f$i)),'fixture')}
        $other=[IntPtr]::Zero
        if($env:FS_GESTURE_ACTIONS-match'activation'){
            $otherFolder=Join-Path $script:UiRoot ($arm+'-other');[void][IO.Directory]::CreateDirectory($otherFolder)
            $other=Open-UiFolder $otherFolder 0
            Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($other,[IntPtr]::Zero,1000,100,600,400,0x14)) 'Cannot place the other activation window.'
        }
        Set-GestureStage 'open-initial-fixture'
        $window=Open-UiFolder $folder $initialCount
        Activate-Ui $window
        foreach($mode in @(@{Name='medium';Mode=1;Free=$false},@{Name='tiles';Mode=6;Free=$false},@{Name='free-icons';Mode=1;Free=$true},@{Name='free-tiles';Mode=6;Free=$true})) {
            if($env:FS_GESTURE_VIEWS-and$mode.Name-notin($env:FS_GESTURE_VIEWS-split',')){continue}
            # Opening the FS menu selects the classic profile. Direct COM mode
            # changes from modern Details otherwise bypass that product policy.
            Set-GestureStage 'configure-view-menu'
            $popup=[FileSpacerViewMenuTests.MenuUi]::Open($window,(Get-UiCommandToolbar $window),(Get-UiId 'IDM_VIEW_MENU'))
            $labelPattern=if($mode.Mode-eq6){'Mosa|Tiles'}else{'Moyennes|Medium'}
            $popup.Entries|Select-Object Command,Label|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'view-menu.json')
            $matched=@($popup.Entries|Where-Object {$_.Label.Replace('&','')-match$labelPattern})
            Assert-Ui ($matched.Count-eq1) ('Native view-menu entry is not unambiguous: '+$labelPattern)
            $entry=[Array]::IndexOf($popup.Entries,$matched[0])
            [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$entry,$window)
            Wait-Ui {$null-eq[FileSpacerViewMenuTests.MenuUi]::Active($window)} 'view menu closed'
            Wait-UiViewMode $window $mode.Mode
            Use-UiView $window {param($view) $view.SetViewModeAndIconSize($mode.Mode,48);$view.AutoArrange=!$mode.Free;$view.SortBy($false,$true);$view.Focus()}
            Wait-UiViewMode $window $mode.Mode 48
            if($presentation-or$env:FS_GESTURE_SEMANTICS-eq'1'){
                for($attempt=0;$attempt-lt4;++$attempt){
                    $body=Use-UiView $window {param($view) $view.Bounds}
                    if($body.Width-eq800-and$body.Height-eq400){break}
                    $outer=[FileSpacerRegression.Native]::Bounds($window)
                    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,100,100,($outer.Width+800-$body.Width),($outer.Height+400-$body.Height),0x14)) 'Cannot fit gesture viewport.'
                    Start-Sleep -Milliseconds 50
                }
                $body=Use-UiView $window {param($view) $view.Bounds}
                Assert-Ui ($body.Width-eq800-and$body.Height-eq400) 'Gesture viewport differs from 800 x 400.'
            }
            if($preseed){
                Set-GestureStage 'close-seed-fixture'
                Close-UiFolder $window
                Complete-TestApplications $script:UiProcesses
                $script:UiProcesses.Clear()
                for($i=$initialCount;$i-lt$FixtureCount;++$i){[IO.File]::WriteAllText((Join-Path $folder ('File{0:D5}.txt'-f$i)),'fixture')}
                Set-GestureStage 'reopen-persisted-fixture'
                $window=Open-UiFolder $folder $FixtureCount
                Activate-Ui $window
                Wait-UiViewMode $window $mode.Mode 48
                Assert-Ui ((Use-UiView $window {param($view) $view.AutoArrange})-eq(!$mode.Free)) 'Persisted arrangement differs.'
                if($presentation){
                    $body=Use-UiView $window {param($view) $view.Bounds}
                    Assert-Ui ($body.Width-eq800-and$body.Height-eq400) 'Persisted viewport differs from 800 x 400.'
                }
            }
            $list=Get-UiControl $window 'SysListView32'
            $gestures=if($env:FS_GESTURE_ACTIONS){@($env:FS_GESTURE_ACTIONS-split',')}else{@('escape','background','selected-item')}
            Assert-Ui (@($gestures|Where-Object {$_-notin@('escape','background','selected-item','escape-background','escape-background-once','marquee','marquee-cancel','marquee-ctrl','marquee-shift','marquee-late-ctrl','marquee-late-shift','activation-background','item-drag')}).Count-eq0) 'Unknown gesture.'
            foreach($gesture in $gestures) {
                for($rep=0;$rep-lt$Repetitions;++$rep) {
                    $marquee=$gesture-like'marquee*'
                    $itemDrag=$gesture-eq'item-drag'
                    if($itemDrag){Assert-Ui ($mode.Free-and!$presentation-and$env:FS_GESTURE_SEMANTICS-eq'1') 'Item drag requires a free view and position comparison, without presentation timing.'}
                    Assert-Ui (!($marquee-and$presentation)) 'Marquee comparison records identities; use presentation disabled.'
                    Use-UiView $window {param($view) $view.Reveal('File00000.txt');$view.Focus()}
                    $positionsBefore=if($env:FS_GESTURE_SEMANTICS-eq'1'){Use-UiView $window {param($view) $view.Positions()}}else{''}
                    $frame=[FileSpacerRegression.Native]::Bounds($window)
                    Assert-Ui ([FileSpacerRegression.Native]::SetCursorPos($frame.Left+100,$frame.Top+10)) 'Cannot park pointer before preparing selection.'
                    Set-GestureStage 'select-all'
                    [FileSpacerRegression.Native]::Keys($window,0x41,$true,$false)
                    Wait-GestureCount $window $FixtureCount
                    Wait-Ui {(Use-UiView $window {param($view) $view.SelectedCount})-eq$FixtureCount} 'full selection stable before the measured gesture' 100
                    Assert-Ui ((Use-UiView $window {param($view) $view.SelectionSnapshotCount})-eq$FixtureCount) 'Cannot warm the ordinary selection snapshot.'
                    Set-GestureStage 'locate-target'
                    $target=$null;$locating=[Diagnostics.Stopwatch]::StartNew()
                    if($gesture-eq'selected-item'){$target=[FileSpacerRegression.NativeTargets]::Item($list,0,$FixtureCount,$true,$false)}
                    if($itemDrag-or$marquee-or$gesture-in@('background','escape-background','escape-background-once','activation-background')) {
                        $rect=[FileSpacerRegression.Native]::Bounds($list)
                        $lastError=''
                        foreach($y in @(10,30,50,80,110,150,200,250)) {
                            foreach($x in @(5,20,50,80,110,150,200,250)) {
                                try {
                                    [FileSpacerRegression.NativeTargets]::Background($list,$rect.Left+$x,$rect.Top+$y)
                                    $target=New-Object FileSpacerRegression.Point;$target.X=$rect.Left+$x;$target.Y=$rect.Top+$y;break
                                }catch{$lastError=$_.Exception.Message}
                            }
                            if($null-ne$target){break}
                        }
                        Assert-Ui ($null-ne$target) ('No verified exposed blank point: '+$lastError)
                    }
                    $endPoint=if($marquee){[FileSpacerRegression.NativeTargets]::Item($list,8,$FixtureCount,$true,$false)}else{$null}
                    $latePoint=if($gesture-like'marquee-late-*'){[FileSpacerRegression.NativeTargets]::Item($list,9,$FixtureCount,$true,$false)}else{$null}
                    if($itemDrag){$endPoint=$target;$target=[FileSpacerRegression.NativeTargets]::Item($list,0,$FixtureCount,$true,$false)}
                    $locateMs=$locating.Elapsed.TotalMilliseconds;$locating.Stop()
                    if($presentation-and$gesture-ne'escape'){
                        Assert-Ui ([FileSpacerRegression.Native]::SetCursorPos($target.X,$target.Y)) 'Cannot position pointer before the visual click measurement.'
                    }
                    Set-GestureStage 'observe-counter-and-baseline'
                    $before=Read-RangeCounter $window
                    if($gesture-ne'escape'){Start-Sleep -Milliseconds ([int][GestureComposition]::GetDoubleClickTime()+20)}
                    if($gesture-eq'activation-background'){
                        Assert-Ui (!$presentation) 'Activation comparison uses identities rather than the foreground-only presentation observer.'
                        Activate-Ui $other
                    }
                    if($presentation){
                        $capture=Start-GestureObservation $window ($arm+'-'+$mode.Name+'-'+$gesture+'-'+$rep)
                        [void]$capture.Events['begin'].Set()
                        Wait-GestureObserverEvent $capture 'started'
                    }
                    Set-GestureStage ('input-'+$gesture)
                    $inputTick=[Diagnostics.Stopwatch]::GetTimestamp()
                    $clock=[Diagnostics.Stopwatch]::StartNew()
                    $hitMs=0.0
                    if($gesture-eq'activation-background'){
                        $point=New-Object FileSpacerRegression.MousePoint;$point.X=$target.X;$point.Y=$target.Y
                        try{[FileSpacerRegression.AltDragInput]::BeginConfigured($window,$point,0,0)}finally{[FileSpacerRegression.AltDragInput]::End()}
                        Wait-Ui {([FileSpacerRegression.Native]::GetForegroundWindow()-eq$window)-and[FileSpacerRegression.AltDragInput]::Idle($window)} 'activation click finished' 100 5
                    }
                    elseif($marquee-or$itemDrag){
                        $startPoint=New-Object FileSpacerRegression.MousePoint;$startPoint.X=$target.X;$startPoint.Y=$target.Y
                        $finishPoint=New-Object FileSpacerRegression.MousePoint;$finishPoint.X=$endPoint.X;$finishPoint.Y=$endPoint.Y
                        try{
                            $modifiers=if($gesture-eq'marquee-ctrl'){1}elseif($gesture-eq'marquee-shift'){4}else{0}
                            [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$startPoint,0,$modifiers)
                            Wait-Ui {[FileSpacerRegression.AltDragInput]::Captured($window)} 'native marquee capture' 0 5
                            [FileSpacerRegression.AltDragInput]::Move($finishPoint)
                            Start-Sleep -Milliseconds 100
                            if($gesture-like'marquee-late-*'){
                                Wait-Ui {$count=[FileSpacerRegression.Native]::Message($list,0x1032,0,0);$count-gt0-and$count-lt$FixtureCount} 'plain marquee subset before modifier change' 0 5
                                [FileSpacerRegression.AltDragInput]::PressModifierDuringDrag($(if($gesture-eq'marquee-late-ctrl'){1}else{4}))
                                $nextPoint=New-Object FileSpacerRegression.MousePoint;$nextPoint.X=$latePoint.X;$nextPoint.Y=$latePoint.Y
                                [FileSpacerRegression.AltDragInput]::Move($nextPoint)
                                Start-Sleep -Milliseconds 100
                            }
                            if($gesture-eq'marquee-cancel'){
                                Wait-Ui {$count=[FileSpacerRegression.Native]::Message($list,0x1032,0,0);$count-gt0-and$count-lt$FixtureCount} 'actual marquee subset before cancellation' 0 5
                                [FileSpacerRegression.AltDragInput]::EscapeHeld()
                            }
                        }finally{[FileSpacerRegression.AltDragInput]::End()}
                        if($gesture-eq'marquee-cancel'){
                            $tooltip=[IntPtr]([FileSpacerRegression.Native]::Message($list,0x104e,0,0)) # LVM_GETTOOLTIPS
                            $cancel=@{Selected=@(Use-UiView $window {param($view) $view.Selected()});Focus=(Use-UiView $window {param($view) $view.FocusIndex});Anchor=(Use-UiView $window {param($view) $view.AnchorIndex});Input=[FileSpacerRegression.AltDragInput]::Describe($window);PositionsUnchanged=((Use-UiView $window {param($view) $view.Positions()})-eq$positionsBefore);TooltipVisible=[FileSpacerRegression.Native]::IsWindowVisible($tooltip)}
                            $cancelPath=Join-Path $env:FS_REGRESSION_RUN ('cancel-'+$mode.Name+'-'+$rep+'.json')
                            $cancel|ConvertTo-Json -Depth 3|Set-Content -LiteralPath $cancelPath
                            # Preserve the immediate state, then observe whether ordinary
                            # pointer departure releases a capture associated with hover.
                            $park=New-Object FileSpacerRegression.MousePoint;$park.X=$frame.Left+100;$park.Y=$frame.Top+10
                            [FileSpacerRegression.AltDragInput]::Move($park)
                            $cancel.AfterPointerDeparture=[FileSpacerRegression.AltDragInput]::Describe($window)
                            $cancel|ConvertTo-Json -Depth 3|Set-Content -LiteralPath $cancelPath
                            Assert-Ui (@($cancel.Selected).Count-eq0-and$cancel.PositionsUnchanged) 'Cancelled marquee changed native selection or positions.'
                            # A plain Shell host also retains capture after Escape.
                            # Test a distinct next user click rather than releasing
                            # capture or replaying the cancellation to force success.
                            $next=[FileSpacerRegression.NativeTargets]::Item($list,0,$FixtureCount,$false,$false)
                            $nextPoint=New-Object FileSpacerRegression.MousePoint;$nextPoint.X=$next.X;$nextPoint.Y=$next.Y
                            try{[FileSpacerRegression.AltDragInput]::BeginConfigured($window,$nextPoint,0,0)}finally{[FileSpacerRegression.AltDragInput]::End()}
                            Wait-GestureCount $window 1
                        }
                        try{Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native marquee ended' 100 5}
                        catch{throw ($_.Exception.Message+'; '+[FileSpacerRegression.AltDragInput]::Describe($window))}
                    }
                    elseif($gesture-in@('escape-background','escape-background-once')) {
                        if($gesture-eq'escape-background'){
                            $hitStart=$clock.Elapsed.TotalMilliseconds
                            [FileSpacerRegression.NativeTargets]::Background($list,$target.X,$target.Y)
                            $hitMs=$clock.Elapsed.TotalMilliseconds-$hitStart
                        }
                        [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
                        Wait-GestureCount $window 0
                        if($presentation){[void][FileSpacerRegression.Native]::ClickControlAtPointer($window,$list,$target.X,$target.Y)}
                        else{[FileSpacerRegression.Native]::ClickControl($window,$list,$target.X,$target.Y)}
                    }
                    elseif($gesture-eq'escape'){[FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)}
                    else{
                        if($presentation){$inputTick=[FileSpacerRegression.Native]::ClickControlAtPointer($window,$list,$target.X,$target.Y)}
                        else{[FileSpacerRegression.Native]::ClickControl($window,$list,$target.X,$target.Y)}
                    }
                    $expected=if($marquee){-1}elseif($itemDrag-or$gesture-eq'activation-background'){$FixtureCount}elseif($gesture-eq'selected-item'){1}else{0}
                    if(!$marquee){Wait-GestureCount $window $expected}
                    $stateMs=$clock.Elapsed.TotalMilliseconds
                    Assert-Ui ([GestureComposition]::DwmFlush()-ge0) 'Composition failed.'
                    $composedMs=$clock.Elapsed.TotalMilliseconds;$clock.Stop()
                    $frame=[FileSpacerRegression.Native]::Bounds($window)
                    Assert-Ui ([FileSpacerRegression.Native]::SetCursorPos($frame.Left+100,$frame.Top+10)) 'Cannot park pointer outside the Shell view.'
                    $visible=$null
                    $markerOffset=0.0;$visibleFirst='';$visibleFinal=''
                    if($null-ne$capture){
                        [void]$capture.Events['returned'].Set()
                        if(!$capture.Job.Process.WaitForExit(10000)){throw 'Presentation observer did not finish.'}
                        $exitCode=Complete-RecordedProcess $capture.Job
                        Assert-Ui ($exitCode-eq0) ('Presentation observer failed: '+$capture.Job.Error.Result)
                        $visible=Import-Csv -LiteralPath ($capture.Prefix+'-presentation.csv')
                        Assert-Ui ($capture.Job.Output.Result-match'GESTURE_MARKER qpc=(\d+) rate=(\d+)') 'Missing visual start marker.'
                        $markerOffset=1000.0*($inputTick-[double]$matches[1])/[double]$matches[2]
                        $culture=[Globalization.CultureInfo]::InvariantCulture
                        $first=[double]::Parse($visible.first_changed_ms,$culture)-$markerOffset
                        $final=[double]::Parse($visible.final_observed_ms,$culture)-$markerOffset
                        Assert-Ui ($markerOffset-ge0-and$first-ge0-and$final-ge$first) 'Visible change preceded the measured gesture.'
                        $visibleFirst=$first.ToString('F3',$culture);$visibleFinal=$final.ToString('F3',$culture)
                        Close-GestureObservation $capture;$capture=$null
                    }
                    Set-GestureStage 'verify-counter-and-identities'
                    $calls=(Read-RangeCounter $window)-$before
                    if($arm-eq'private'-and$env:FS_GESTURE_EXPECT_CLICK_RANGE-eq'1'){
                        $expectedCalls=if($gesture-in@('escape','escape-background','escape-background-once')){1}else{0}
                        Assert-Ui ($calls-eq$expectedCalls) ('Unexpected private call count for '+$mode.Name+'/'+$gesture+': '+$calls+' instead of '+$expectedCalls)
                    }
                    if($arm-ne'private'){Assert-Ui ($calls-eq0) 'A fallback arm invoked the unavailable private range.'}
                    if($arm-eq'private'-and$gesture-in@('escape','escape-background','escape-background-once')){Assert-Ui ($calls-eq1) 'The qualified private Escape path was not exercised exactly once.'}
                    $selected=@(Use-UiView $window {param($view) $view.Selected()})
                    Assert-Ui ((Use-UiView $window {param($view) $view.SelectionSnapshotCount})-eq$selected.Count) 'Ordinary selection snapshot is stale after the gesture.'
                    if(!$marquee){
                        $expectedNames=if($itemDrag-or$gesture-eq'activation-background'){@(for($index=0;$index-lt$FixtureCount;++$index){'File{0:D5}.txt'-f$index})}elseif($expected){@('File00000.txt')}else{@()}
                        Assert-Ui (($selected-join';')-eq($expectedNames-join';')) 'Wrong selected identities.'
                    }
                    elseif($gesture-eq'marquee'){Assert-Ui ($selected.Count-gt0-and$selected.Count-lt$FixtureCount) 'Marquee did not produce a proper subset.'}
                    elseif($gesture-eq'marquee-cancel'){Assert-Ui (($selected-join';')-eq'File00000.txt') 'The next item click after cancellation selected the wrong identity.'}
                    Assert-Ui ((Use-UiView $window {param($view) $view.Count})-eq$FixtureCount) 'Fixture count changed.'
                    $positionsAfter=if($env:FS_GESTURE_SEMANTICS-eq'1'){Use-UiView $window {param($view) $view.Positions()}}else{''}
                    if($itemDrag){
                        Assert-Ui ($positionsAfter-ne$positionsBefore) 'Native item drag did not move positions.'
                        @{Before=$positionsBefore;After=$positionsAfter}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN ($arm+'-'+$mode.Name+'-drag-'+$rep+'.json'))
                    }
                    elseif($env:FS_GESTURE_SEMANTICS-eq'1'){Assert-Ui ($positionsAfter-eq$positionsBefore) 'Gesture moved free icon positions.'}
                    $focus=Use-UiView $window {param($view) $view.FocusIndex};$anchor=Use-UiView $window {param($view) $view.AnchorIndex}
                    $rows.Add([pscustomobject]@{arm=$arm;view=$mode.Name;gesture=$gesture;items=$FixtureCount;rep=$rep;persisted_view=$preseed;state_ms=$stateMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);composed_ms=$composedMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);range_calls=$calls;locate_ms=$locateMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);hit_ms=$hitMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);presentation_quality=$(if($null-ne$visible){$visible.quality}else{''});visible_first_ms=$visibleFirst;visible_final_ms=$visibleFinal;marker_offset_ms=$markerOffset.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);selected_json=(ConvertTo-Json -InputObject @($selected) -Compress);focus=$focus;anchor=$anchor})
                    $rows|Export-Csv -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'gestures.csv') -NoTypeInformation
                    Write-Host ('GESTURE '+$arm+' '+$mode.Name+' '+$gesture+' '+[math]::Round($composedMs,3)+' ms; range='+$calls)
                }
            }
        }
        Set-GestureStage 'normal-close'
        if($other-ne[IntPtr]::Zero){Close-UiFolder $other}
        Close-UiFolder $window
        Complete-TestApplications $script:UiProcesses
        $script:UiProcesses.Clear()
    }
    $viewCount=if($env:FS_GESTURE_VIEWS){@($env:FS_GESTURE_VIEWS-split',').Count}else{4}
    $actionCount=if($env:FS_GESTURE_ACTIONS){@($env:FS_GESTURE_ACTIONS-split',').Count}else{3}
    Assert-Ui ($rows.Count-eq($arms.Count*$viewCount*$actionCount*$Repetitions)) 'Incomplete gesture matrix.'
    if($IncludePublic){
        foreach($row in @($rows|Where-Object arm -eq 'private')){
            $reference=@($rows|Where-Object {$_.arm-eq'public'-and$_.view-eq$row.view-and$_.gesture-eq$row.gesture-and$_.rep-eq$row.rep})
            Assert-Ui ($reference.Count-eq1) 'Missing native/public reference row.'
            foreach($property in @('selected_json','focus','anchor')){
                Assert-Ui ($row.$property-ceq$reference[0].$property) ('Private/native behavior differs for '+$row.view+'/'+$row.gesture+'/'+$property)
            }
        }
    }
}catch{$failure=$_;throw}
finally{try{Close-GestureObservation $capture}finally{Remove-Item Env:\FS_SELECTION_PRODUCT_CACHE -ErrorAction SilentlyContinue;Stop-Ui $failure}}
