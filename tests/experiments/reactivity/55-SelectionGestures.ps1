# Test-Category: UI
param([string]$ProjectRoot,[switch]$IncludePublic=($env:FS_GESTURE_PUBLIC-eq'1'),
    [ValidateRange(1,3)][int]$Repetitions=$(if($env:FS_GESTURE_REPETITIONS){[int]$env:FS_GESTURE_REPETITIONS}else{1}),
    [ValidateRange(128,5000)][int]$FixtureCount=$(if($env:FS_GESTURE_COUNT){[int]$env:FS_GESTURE_COUNT}else{256}))
$ErrorActionPreference='Stop'
. (Join-Path $ProjectRoot 'tests\regression\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\15-deplacement-alt\NativeTargets.cs')
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\28-apercu-affichage\ViewMenuUi.cs')
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class GestureComposition {
    [DllImport("dwmapi.dll")] public static extern int DwmFlush();
    [DllImport("user32.dll")] public static extern uint GetDoubleClickTime();
    [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] public static extern System.IntPtr WindowData(System.IntPtr window,int index);
}
'@
if(Get-Process FileSpacer -ErrorAction SilentlyContinue){throw 'Close FileSpacer before building the native helper.'}
$counter=Build-NativeTest -Name RemainingSelectionTests -CaseDirectory (Join-Path $ProjectRoot 'tests\experiments\reactivity') -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib','Dbghelp.lib','Psapi.lib')
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
    $arms=@(if($IncludePublic){'private';'public'}else{'private'})
    foreach($arm in $arms) {
        $env:FS_SELECTION_PRODUCT_CACHE=if($arm-eq'private'){$cache}else{''}
        $folder=Join-Path $script:UiRoot $arm;[void][IO.Directory]::CreateDirectory($folder)
        for($i=0;$i-lt$FixtureCount;++$i){[IO.File]::WriteAllText((Join-Path $folder ('File{0:D5}.txt'-f$i)),'fixture')}
        $window=Open-UiFolder $folder $FixtureCount
        Activate-Ui $window
        foreach($mode in @(@{Name='medium';Mode=1;Free=$false},@{Name='tiles';Mode=6;Free=$false},@{Name='free-icons';Mode=1;Free=$true},@{Name='free-tiles';Mode=6;Free=$true})) {
            if($env:FS_GESTURE_VIEWS-and$mode.Name-notin($env:FS_GESTURE_VIEWS-split',')){continue}
            # Opening the FS menu selects the classic profile. Direct COM mode
            # changes from modern Details otherwise bypass that product policy.
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
            $list=Get-UiControl $window 'SysListView32'
            if($env:FS_GESTURE_INSPECT_SECONDS) {
                $seconds=[int]$env:FS_GESTURE_INSPECT_SECONDS
                Assert-Ui ($seconds-ge1-and$seconds-le60) 'Inspection pause must be bounded to sixty seconds.'
                [uint32]$targetId=0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$targetId)
                @{ProcessId=$targetId;Window=$window.ToInt64();Object=('0x'+[GestureComposition]::WindowData($window,-21).ToInt64().ToString('x'))}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'inspect.json')
                Start-Sleep -Seconds $seconds # Owned fixture only; outside every timed action.
            }
            $gestures=if($env:FS_GESTURE_ACTIONS){@($env:FS_GESTURE_ACTIONS-split',')}else{@('escape','background','selected-item')}
            Assert-Ui (@($gestures|Where-Object {$_-notin@('escape','background','selected-item','escape-background')}).Count-eq0) 'Unknown gesture.'
            foreach($gesture in $gestures) {
                for($rep=0;$rep-lt$Repetitions;++$rep) {
                    Use-UiView $window {param($view) $view.Reveal('File00000.txt');$view.Focus()}
                    $frame=[FileSpacerRegression.Native]::Bounds($window)
                    Assert-Ui ([FileSpacerRegression.Native]::SetCursorPos($frame.Left+100,$frame.Top+10)) 'Cannot park pointer before preparing selection.'
                    [FileSpacerRegression.Native]::Keys($window,0x41,$true,$false)
                    Wait-GestureCount $window $FixtureCount
                    Wait-Ui {(Use-UiView $window {param($view) $view.SelectedCount})-eq$FixtureCount} 'full selection stable before the measured gesture' 100
                    $target=$null;$locating=[Diagnostics.Stopwatch]::StartNew()
                    if($gesture-eq'selected-item'){$target=[FileSpacerRegression.NativeTargets]::Item($list,0,$FixtureCount,$true,$false)}
                    if($gesture-in@('background','escape-background')) {
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
                    $locateMs=$locating.Elapsed.TotalMilliseconds;$locating.Stop()
                    $before=Read-RangeCounter $window
                    if($gesture-ne'escape'){Start-Sleep -Milliseconds ([int][GestureComposition]::GetDoubleClickTime()+20)}
                    $clock=[Diagnostics.Stopwatch]::StartNew()
                    $hitMs=0.0
                    if($gesture-eq'escape-background') {
                        $hitStart=$clock.Elapsed.TotalMilliseconds
                        [FileSpacerRegression.NativeTargets]::Background($list,$target.X,$target.Y)
                        $hitMs=$clock.Elapsed.TotalMilliseconds-$hitStart
                        [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
                        Wait-GestureCount $window 0
                        [FileSpacerRegression.Native]::ClickControl($window,$list,$target.X,$target.Y)
                    }
                    elseif($gesture-eq'escape'){[FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)}
                    else{[FileSpacerRegression.Native]::ClickControl($window,$list,$target.X,$target.Y)}
                    $expected=if($gesture-eq'selected-item'){1}else{0}
                    Wait-GestureCount $window $expected
                    $stateMs=$clock.Elapsed.TotalMilliseconds
                    Assert-Ui ([GestureComposition]::DwmFlush()-ge0) 'Composition failed.'
                    $composedMs=$clock.Elapsed.TotalMilliseconds;$clock.Stop()
                    $frame=[FileSpacerRegression.Native]::Bounds($window)
                    Assert-Ui ([FileSpacerRegression.Native]::SetCursorPos($frame.Left+100,$frame.Top+10)) 'Cannot park pointer outside the Shell view.'
                    $calls=(Read-RangeCounter $window)-$before
                    if($arm-eq'private'-and$gesture-in@('escape','escape-background')){Assert-Ui ($calls-eq1) 'The qualified private Escape path was not exercised exactly once.'}
                    $selected=Use-UiView $window {param($view) $view.Selected()}
                    Assert-Ui (($selected-join';')-eq$(if($expected){'File00000.txt'}else{''})) 'Wrong selected identities.'
                    Assert-Ui ((Use-UiView $window {param($view) $view.Count})-eq$FixtureCount) 'Fixture count changed.'
                    $rows.Add([pscustomobject]@{arm=$arm;view=$mode.Name;gesture=$gesture;items=$FixtureCount;rep=$rep;state_ms=$stateMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);composed_ms=$composedMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);range_calls=$calls;locate_ms=$locateMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);hit_ms=$hitMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture)})
                    $rows|Export-Csv -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'gestures.csv') -NoTypeInformation
                    Write-Host ('GESTURE '+$arm+' '+$mode.Name+' '+$gesture+' '+[math]::Round($composedMs,3)+' ms; range='+$calls)
                }
            }
        }
        Close-UiFolder $window
        Complete-TestApplications $script:UiProcesses
        $script:UiProcesses.Clear()
    }
    $viewCount=if($env:FS_GESTURE_VIEWS){@($env:FS_GESTURE_VIEWS-split',').Count}else{4}
    $actionCount=if($env:FS_GESTURE_ACTIONS){@($env:FS_GESTURE_ACTIONS-split',').Count}else{3}
    Assert-Ui ($rows.Count-eq($arms.Count*$viewCount*$actionCount*$Repetitions)) 'Incomplete gesture matrix.'
}catch{$failure=$_;throw}
finally{Remove-Item Env:\FS_SELECTION_PRODUCT_CACHE -ErrorAction SilentlyContinue;Stop-Ui $failure}
