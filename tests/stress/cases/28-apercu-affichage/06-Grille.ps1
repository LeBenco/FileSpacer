# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\..\regression\cases\28-apercu-affichage\GridDrag.ps1')
$failure=$null
$samples=New-Object Collections.ArrayList
$previousGridCache=$env:FS_SELECTION_PRODUCT_CACHE
$cacheTest=Build-NativeTest -Name SelectionCacheTests -CaseDirectory (Join-Path $PSScriptRoot '..\..\..\regression\cases\18-vue-shell-native') -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib')
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
    Remove-Item Env:\FS_SELECTION_PRODUCT_CACHE -ErrorAction SilentlyContinue
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
    Assert-Ui ($free.Positions-ne$stationary.Positions) 'The native reference drag must move its selection.'
    $freeDragMilliseconds=1000.0*([Diagnostics.Stopwatch]::GetTimestamp()-$script:GridDropStartedAt)/[Diagnostics.Stopwatch]::Frequency
    @{Count=3000;NativeFreeDragMs=$freeDragMilliseconds;SnapDragMs=$null}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-timings.json') -Encoding UTF8
    Use-UiView $window {param($view) $view.SnapToGrid=$true}
    $nativeDrop=Read-PreviewState $window
    Set-LargeGridSeed
    Write-GridPhase 'snap-drag'
    Move-GridIcons $true
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
    Assert-Ui (Use-UiView $window {param($view) $view.Count-eq3000-and!$view.SnapToGrid-and!$view.AutoArrange}) 'Large Snap changed flags or contents.'
    @{Count=3000;NativeFreeDragMs=$freeDragMilliseconds;SnapDragMs=$snapDragMilliseconds}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-timings.json') -Encoding UTF8
    Close-UiFolder $window
    Wait-Ui {@($script:UiProcesses|Where-Object {!$_.HasExited}).Count-eq0} 'public broker exited before qualified comparison' 0 15
    $cache=Join-Path $env:FS_REGRESSION_RUN 'qualified-cache'
    [void][IO.Directory]::CreateDirectory($cache)
    Write-GridPhase 'qualifying-grid'
    Invoke-TestProgram $cacheTest @('--build-cache',$cache)
    Invoke-TestProgram $cacheTest @('--grid-supported',$cache)
    # A corrupt grid witness must disable only this optional calculation.
    $bad=Join-Path $env:FS_REGRESSION_RUN 'invalid-grid-cache'
    [void][IO.Directory]::CreateDirectory($bad)
    Get-ChildItem -LiteralPath $cache -File|Where-Object {$_.Extension-in@('.bin','.range','.grid')}|Copy-Item -Destination $bad
    $descriptor=@(Get-ChildItem -LiteralPath $bad -Filter '*.grid' -File)
    Assert-Ui ($descriptor.Count-eq1) 'Missing qualified control descriptor.'
    $bytes=[IO.File]::ReadAllBytes($descriptor[0].FullName);$bytes[32]=$bytes[32]-bxor1
    [IO.File]::WriteAllBytes($descriptor[0].FullName,$bytes)
    Invoke-TestProgram $cacheTest @('--grid-fallback',$bad)
    Invoke-TestProgram $cacheTest @('--range-supported',$bad)
    $env:FS_SELECTION_PRODUCT_CACHE=$cache
    $window=Open-UiFolder $folder 3000
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,40,40,850,600,0x14)) 'Cannot size qualified grid fixture.'
    Set-GridMode
    Set-LargeGridSeed
    Write-GridPhase 'qualified-snap-drag'
    Move-GridIcons $true
    $qualified=Read-PreviewState $window
    $qualifiedMilliseconds=1000.0*([Diagnostics.Stopwatch]::GetTimestamp()-$script:GridDropStartedAt)/[Diagnostics.Stopwatch]::Frequency
    Assert-Ui ($qualified.Positions-eq$snapped.Positions-and$qualified.Selected-eq$snapped.Selected-and$qualified.Focus-eq$snapped.Focus) 'Qualified Snap differs from the public fallback.'
    Assert-Ui (Use-UiView $window {param($view) !$view.SnapToGrid-and!$view.AutoArrange}) 'Qualified Snap changed persistent flags.'
    Close-UiFolder $window
    Wait-Ui {@($script:UiProcesses|Where-Object {!$_.HasExited}).Count-eq0} 'qualified process exited and flushed its activation proof' 0 15
    $proof=($script:UiProcesses|ForEach-Object {if($_.PSObject.Properties['RegressionOutput']){$_.RegressionOutput.Output.GetAwaiter().GetResult()}})-join"`n"
    Assert-Ui ($proof-match'GRID SNAP qualified=1 selected=\d+ count=3000') 'No proof that the real drag used the qualified calculation.'
    $publicTime=[regex]::Match($proof,'GRID SNAP path=public elapsed_ms=(\d+)')
    $qualifiedTime=[regex]::Match($proof,'GRID SNAP path=qualified elapsed_ms=(\d+)')
    Assert-Ui ($publicTime.Success-and$qualifiedTime.Success) 'Missing internal measurements for one of the Snap paths.'
    @{Count=3000;NativeFreeDragMs=$freeDragMilliseconds;PublicSnapMs=$snapDragMilliseconds;QualifiedSnapMs=$qualifiedMilliseconds;PublicOperationMs=[long]$publicTime.Groups[1].Value;QualifiedOperationMs=[long]$qualifiedTime.Groups[1].Value}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-timings.json') -Encoding UTF8
    $samples|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-timings.json') -Encoding UTF8
    Write-Host 'PASS: three alignment cycles over 3000 items; exact native coordinates, selection, focus and persistence. Timings include automation; no performance threshold.'
} catch {
    $failure=$_
    @{Message=$_.Exception.Message;Stack=$_.ScriptStackTrace;Position=$_.InvocationInfo.PositionMessage}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'first-error.json') -Encoding UTF8
    throw
} finally {
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    [FileSpacerRegression.AltDragInput]::End()
    Stop-Ui -Failure $failure
    if($null-eq$previousGridCache){Remove-Item Env:\FS_SELECTION_PRODUCT_CACHE -ErrorAction SilentlyContinue}else{$env:FS_SELECTION_PRODUCT_CACHE=$previousGridCache}
}
