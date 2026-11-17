# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\..\regression\cases\28-apercu-affichage\GridDrag.ps1')
. (Join-Path $PSScriptRoot '..\..\common\PrivateDiagnostics.ps1')
$failure=$null
$samples=New-Object Collections.ArrayList
function Write-GridPhase([string]$Phase) {
    @{Phase=$Phase;Utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Compress|Add-Content (Join-Path $env:FS_REGRESSION_RUN 'phases.jsonl') -Encoding UTF8
}
function Set-LargeGridSeed {
    Use-UiView $window {param($view) if($view.SnapToGrid){$view.SnapToGrid=$false}}
    Wait-Ui {Use-UiView $window {param($view) !$view.SnapToGrid-and!$view.AutoArrange}} 'large native placement settings settled before seeding' 150
    Use-UiView $window {
        param($view)
        $names=@($view.Names()|Sort-Object)
        $view.Select($names[0]);$view.Select($names[1],$true)
        $points=New-Object FileSpacerRegression.Point[] $names.Count
        for($i=0;$i-lt$names.Count;++$i){
            $point=New-Object FileSpacerRegression.Point
            if($i-lt6){$point.X=43+($i%3)*220;$point.Y=37+[int][Math]::Floor($i/3)*190}
            else {$point.X=43+($i%50)*120;$point.Y=1000+[int][Math]::Floor($i/50)*110}
            $points[$i]=$point
        }
        $view.PositionAll($names,$points)
    }
}
try {
    Initialize-Ui 'large-icon-grid' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot 'LargeGrid'
    [void][IO.Directory]::CreateDirectory($folder)
    for($i=0;$i-lt3000;++$i){[void][IO.Directory]::CreateDirectory((Join-Path $folder ('item-{0:D4}'-f$i)))}
    $window=Open-UiFolder $folder 3000
    Write-GridPhase 'opened'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,40,40,850,600,0x14)) 'Cannot size large grid fixture.'
    Set-GridMode
    Write-GridPhase 'free-mode-ready'
    for($round=0;$round-lt3;++$round){
        Write-GridPhase ('seed-'+$round)
        Set-LargeGridSeed
        Write-GridPhase ('seeded-'+$round)
        $selection=Read-PreviewState $window
        Assert-Ui (@($selection.Positions-split';'|ForEach-Object {($_-split'=',2)[1]}|Sort-Object -Unique).Count-eq3000) 'The stress fixture must have 3000 distinct positions before alignment.'
        Write-GridPhase ('native-'+$round)
        $watch=[Diagnostics.Stopwatch]::StartNew()
        Use-UiView $window {param($view) $view.SnapToGrid=$true;$view.SnapToGrid=$false}
        $watch.Stop();$nativeMilliseconds=$watch.Elapsed.TotalMilliseconds
        Write-GridPhase ('native-finished-'+$round)
        $expected=Read-PreviewState $window
        Assert-Ui ($expected.Positions-ne$selection.Positions) 'The large fixture was already aligned before the native comparison.'
        Set-LargeGridSeed
        Write-GridPhase ('command-'+$round)
        $watch.Restart()
        [void][FileSpacerRegression.Native]::Message($window,0x111,(Get-UiId 'IDM_REALIGN_ICONS'),0)
        $watch.Stop();$commandMilliseconds=$watch.Elapsed.TotalMilliseconds
        Write-GridPhase ('command-finished-'+$round)
        $actual=Read-PreviewState $window
        Assert-Ui ($actual.Positions-eq$expected.Positions) 'Large one-shot command differs from the native grid.'
        Assert-Ui ($actual.Selected-eq$selection.Selected-and$actual.Focus-eq$selection.Focus) 'Large alignment changed selection or focus.'
        Assert-Ui (Use-UiView $window {param($view) $view.Count-eq3000-and!$view.SnapToGrid-and!$view.AutoArrange}) 'Large alignment lost items or kept temporary placement flags.'
        [void]$samples.Add(@{Round=$round;Count=3000;NativeMs=$nativeMilliseconds;CommandMs=$commandMilliseconds})
        $samples|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-timings.json') -Encoding UTF8
    }
    Write-GridPhase 'closing-aligned-folder'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 3000
    Wait-Ui {(Read-PreviewState $window).Positions-eq$expected.Positions} 'all 3000 aligned coordinates survive reopening' 150
    Set-LargeGridSeed
    Write-GridPhase 'free-drag'
    $stationary=Read-PreviewState $window
    Move-GridIcons $false
    $free=Read-PreviewState $window
    Assert-Ui ($free.Positions-ne$stationary.Positions) 'The ordinary free drag must move its selection.'
    $freeDragMilliseconds=1000.0*([Diagnostics.Stopwatch]::GetTimestamp()-$script:GridDropStartedAt)/[Diagnostics.Stopwatch]::Frequency
    @{Count=3000;FreeDragMs=$freeDragMilliseconds;SnapDragMs=$null}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-timings.json') -Encoding UTF8
    Use-UiView $window {param($view) $view.SnapToGrid=$true}
    $nativeDrop=Read-PreviewState $window
    Set-LargeGridSeed
    Write-GridPhase 'snap-drag'
    Move-GridIcons $true $false $false $true
    $snapped=Read-PreviewState $window
    $snapDragMilliseconds=1000.0*([Diagnostics.Stopwatch]::GetTimestamp()-$script:GridDropStartedAt)/[Diagnostics.Stopwatch]::Frequency
    @($stationary,$free,$nativeDrop,$snapped)|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-states.json') -Encoding UTF8
    $original=@{};foreach($entry in($stationary.Positions-split';')){$pair=$entry-split'=',2;$original[$pair[0]]=$pair[1]}
    $nativePoints=@{};foreach($entry in($nativeDrop.Positions-split';')){$pair=$entry-split'=',2;$nativePoints[$pair[0]]=$pair[1]}
    foreach($entry in($snapped.Positions-split';')){
        $pair=$entry-split'=',2;$name=$pair[0]
        $wanted=if($name-in($free.Selected-split';')){$nativePoints[$name]}else{$original[$name]}
        Assert-Ui ($pair[1]-eq$wanted) ('Large Snap coordinate differs for '+$name)
    }
    Assert-Ui ($snapped.Selected-eq$free.Selected-and$snapped.Focus-eq$free.Focus) 'Large Snap changed selection or focus.'
    Assert-Ui (Use-UiView $window {param($view) $view.Count-eq3000-and!$view.SnapToGrid-and!$view.AutoArrange}) 'Large Snap changed flags or contents.'
    @{Count=3000;FreeDragMs=$freeDragMilliseconds;SnapDragMs=$snapDragMilliseconds}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-timings.json') -Encoding UTF8
    Close-UiFolder $window
    Wait-Ui {@($script:UiProcesses|Where-Object {!$_.HasExited}).Count-eq0} 'process exited before collecting public grid feedback' 0 15
    # Collect feedback from the same drag whose final coordinates were verified.
    $feedback=($script:UiProcesses|ForEach-Object {if($_.PSObject.Properties['RegressionOutput']){$_.RegressionOutput.Output.GetAwaiter().GetResult()}})-join"`n"
    $preview=@([regex]::Matches($feedback,'GRID PREVIEW updates=(\d+) total_us=(\d+) max_us=(\d+) setup_us=(\d+)')|Where-Object {[int]$_.Groups[1].Value-ge60})
    if ((Get-TestConfiguration)-eq'Debug') {
        Assert-Ui ($preview.Count-eq1) 'Missing the measured marker motion stress in the real drag.'
        @{Items=3000;Updates=[int]$preview[0].Groups[1].Value;TotalMicroseconds=[long]$preview[0].Groups[2].Value;MaximumMicroseconds=[long]$preview[0].Groups[3].Value;SetupMicroseconds=[long]$preview[0].Groups[4].Value}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'marker-timings.json') -Encoding UTF8
        $publicPrepared=$feedback-match'GRID PREPARE path=public elapsed_ms='
        $publicApplied=$feedback-match'GRID SNAP path=public elapsed_ms='
        Assert-Ui ($publicPrepared-and$publicApplied) 'Missing native public prepare or apply proof for the real Snap.'
        $proof='Actual public prepare/apply diagnostics; exact positions verified against native folder alignment.'
    } else {
        $publicPrepared=$null;$publicApplied=$null
        $proof='Release has no Debug diagnostics. The single public implementation is exercised by sixty acknowledged marker motions and exact native folder alignment comparisons.'
    }
    @{Backend='Public';Configuration=(Get-TestConfiguration);PreparedDiagnostic=$publicPrepared;AppliedDiagnostic=$publicApplied;AcknowledgedMarkerMotions=60;Proof=$proof}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-backend.json') -Encoding UTF8
    Write-PrivateDiagnostics $ProjectRoot $env:FS_REGRESSION_RUN
    Write-Host ('PASS: three alignment cycles over 3000 items; exact coordinates, selection, focus and persistence; 60 marker motions; public native grid. Timings include automation and readback; no performance threshold.')
} catch {
    $failure=$_
    @{Message=$_.Exception.Message;Stack=$_.ScriptStackTrace;Position=$_.InvocationInfo.PositionMessage}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'first-error.json') -Encoding UTF8
    try { Write-PrivateDiagnostics $ProjectRoot $env:FS_REGRESSION_RUN } catch { Write-Warning ('Additional private diagnostic failure: '+$_.Exception.Message) }
    throw $failure
} finally {
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    [FileSpacerRegression.AltDragInput]::End()
    Stop-Ui -Failure $failure
}
