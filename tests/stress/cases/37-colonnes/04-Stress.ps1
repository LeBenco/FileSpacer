# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\..\regression\cases\37-colonnes\Columns.ps1')
$failure=$null;$records=New-Object Collections.ArrayList
$counts=@(100,1000)
if($env:FS_COLUMN_STRESS_COUNTS){$counts=@($env:FS_COLUMN_STRESS_COUNTS.Split(',')|ForEach-Object{[int]$_}|Sort-Object -Unique)}
if(@($counts|Where-Object{$_ -lt 1 -or $_ -gt 200000}).Count){throw 'Unsupported stress size.'}
$script:StressCpu=$null;$script:BusySeconds=0.0;$script:StressItems=0;$script:StressDeferred=$true
function Check-Budget([string]$Phase,[double]$ResponseMs=0) {
    if($null -eq $script:StressCpu){return}
    $sample=$script:StressCpu.Read()
    if($sample.Percent -ge 50){$script:BusySeconds+=$sample.Seconds}else{$script:BusySeconds=0}
    $reason=$null
    if($ResponseMs -gt 500){$reason='UI response exceeded 500 ms'}
    if($script:BusySeconds -ge 1){$reason='total system CPU at least 50% for one second'}
    if($reason){
        @{Outcome='LimitReached';Items=$script:StressItems;Deferred=$script:StressDeferred;Phase=$Phase;Reason=$reason;
          ResponseMs=$ResponseMs;SystemCpuPercent=$sample.Percent;LastCompletedSample=@($records|Select-Object -Last 1)}|
            ConvertTo-Json -Depth 6|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'stress-limit.json') -Encoding UTF8
        throw ('STRESS LIMIT: '+$reason+'; files='+$script:StressItems+'; phase='+$Phase)
    }
}
function Cpu($Process){$Process.Refresh();return $Process.TotalProcessorTime.TotalMilliseconds}
function Wait-Result([IntPtr]$Window) {
    $watch=[Diagnostics.Stopwatch]::StartNew();$result=$null
    while($watch.ElapsedMilliseconds -le 500){
        $result=Read-Columns $Window
        if($result -and [Math]::Abs(($result.Widths|Measure-Object -Sum).Sum-$result.Available) -le 1){return $result}
        Check-Budget 'final-layout' $watch.Elapsed.TotalMilliseconds
        Start-Sleep -Milliseconds 10
    }
    Check-Budget 'final-layout' 501
    throw 'Final layout did not settle.'
}
function Divider([IntPtr]$Window,$Snapshot,[int]$Delta) {
    $start=New-Object FileSpacerRegression.MousePoint
    $start.X=[int]$Snapshot.Headers[0].Right-1;$start.Y=[int]($Snapshot.Headers[0].Top+$Snapshot.Headers[0].Height/2)
    $end=New-Object FileSpacerRegression.MousePoint;$end.X=$start.X+$Delta;$end.Y=$start.Y
    [FileSpacerRegression.AltDragInput]::BeginConfigured($Window,$start,0,0)
    [FileSpacerRegression.AltDragInput]::Move($end)
    [FileSpacerRegression.AltDragInput]::End()
}
function Measure-ResizeGesture([IntPtr]$Window,$Process,[string]$Kind,[bool]$Deferred) {
    Activate-Ui $Window
    $before=Wait-Columns $Window 100 30
    $bounds=[FileSpacerRegression.Native]::Bounds($Window)
    $body=Get-ColumnsViewWindow $Window
    $bodyWidth=[FileSpacerRegression.Native]::ClientBounds($body).Width
    $start=New-Object FileSpacerRegression.MousePoint
    if($Kind -eq 'column'){
        $start.X=[int]$before.Headers[0].Right-1;$start.Y=[int]($before.Headers[0].Top+$before.Headers[0].Height/2)
    }else{
        $corner=$false
        foreach($offset in @(3,4,5,6,7)){
            $start.X=$bounds.Right-$offset;$start.Y=$bounds.Bottom-$offset
            $packed=($start.X -band 65535) -bor (($start.Y -band 65535) -shl 16)
            if([FileSpacerRegression.Native]::Message($Window,0x84,0,$packed) -eq 17){$corner=$true;break}
        }
        Assert-Ui $corner 'No verified native sizing corner.'
    }
    $point=New-Object FileSpacerRegression.MousePoint
    $cpu=Cpu $Process;$watch=[Diagnostics.Stopwatch]::StartNew();$worstReply=0.0
    [FileSpacerRegression.AltDragInput]::BeginConfigured($Window,$start,0,0)
    [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
    foreach($step in 1..6){
        $delta=$step*$(if($Kind -eq 'column'){4}else{16})
        $point.X=$start.X+$delta;$point.Y=$start.Y+$(if($Kind -eq 'column'){0}else{$delta/2})
        [FileSpacerRegression.AltDragInput]::Move($point)
        Start-Sleep -Milliseconds 16
        $reply=[Diagnostics.Stopwatch]::StartNew()
        [void][FileSpacerRegression.Native]::Message($Window,0,0,0)
        $worstReply=[Math]::Max($worstReply,$reply.Elapsed.TotalMilliseconds)
        Check-Budget ($Kind+'-motion') $reply.Elapsed.TotalMilliseconds
    }
    $motionMs=$watch.Elapsed.TotalMilliseconds;$motionCpu=(Cpu $Process)-$cpu
    $cpu=Cpu $Process;$watch.Restart()
    foreach($pause in 1..6){Start-Sleep -Milliseconds 100;Check-Budget ($Kind+'-held')}
    [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
    $during=Read-Columns $Window
    $heldMs=$watch.Elapsed.TotalMilliseconds;$heldCpu=(Cpu $Process)-$cpu
    if($Deferred){
        Assert-Ui (($during.Widths -join ',') -eq ($before.Widths -join ',')) 'A stationary held gesture refreshed deferred columns.'
        if($Kind -eq 'window'){Assert-Ui ([FileSpacerRegression.Native]::ClientBounds($body).Width -eq $bodyWidth) 'Deferred window content changed before release.'}
    }elseif($Kind -eq 'column'){
        Assert-Ui ($during.Widths[0] -eq $before.Widths[0]+24 -and $during.Widths[1] -eq $before.Widths[1]-24) 'Live column resize did not update during the held pause.'
    }else{
        Assert-Ui ([FileSpacerRegression.Native]::ClientBounds($body).Width -eq $bodyWidth+96) 'Live viewport did not follow its frame.'
        Assert-Ui (($during.Widths -join ',') -ne ($before.Widths -join ',')) 'Live window resize did not recalculate the columns.'
    }
    $cpu=Cpu $Process;$watch.Restart()
    [FileSpacerRegression.AltDragInput]::End()
    $after=Wait-Result $Window
    [FileSpacerColumnTests.ColumnsUi]::FlushPaint($Window)
    $releaseMs=$watch.Elapsed.TotalMilliseconds;$releaseCpu=(Cpu $Process)-$cpu
    Check-Budget ($Kind+'-release') $releaseMs
    if($Kind -eq 'column'){
        Assert-Ui ($after.Widths[0] -eq $before.Widths[0]+24 -and $after.Widths[1] -eq $before.Widths[1]-24) 'Released column resize did not compensate its neighbour.'
        Divider $Window $after -24
    }else{
        for($i=0;$i -lt $after.Widths.Count;++$i){
            Assert-Ui ([Math]::Abs($after.Widths[$i]-$before.Widths[$i]*$after.Available/$before.Available) -le 2) 'Window resizing changed the original column proportions.'
        }
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$bounds.Left,$bounds.Top,$bounds.Width,$bounds.Height,0x14)) 'Cannot restore stress geometry.'
    }
    [void](Wait-Columns $Window 100 30)
    return [pscustomobject]@{Kind=$Kind;MotionMs=$motionMs;MotionCpuMs=$motionCpu;HeldMs=$heldMs;HeldCpuMs=$heldCpu;
        ReleaseMs=$releaseMs;ReleaseCpuMs=$releaseCpu;WorstReplyMs=$worstReply;TotalCpuMs=$motionCpu+$heldCpu+$releaseCpu}
}
try{
    Initialize-Ui 'gesture-stress' -FixtureParent (Join-Path $ProjectRoot 'build')
    if($env:FS_COLUMN_STRESS_BINARY){
        $candidate=[IO.Path]::GetFullPath($env:FS_COLUMN_STRESS_BINARY)
        Assert-Ui ($candidate.StartsWith($ProjectRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and (Test-Path $candidate)) 'Stress binary outside this checkout.'
        $script:UiExecutable=$candidate
    }
    if($env:FS_COLUMN_STRESS_STOP_GUARD -eq '1'){$script:StressCpu=New-Object FileSpacerColumnTests.SystemCpuProbe}
    Set-UiPreference 'DetailsColumnsEnabled' 1;Set-UiPreference 'ViewMenuPreviewEnabled' 0
    @{Binary=$script:UiExecutable;Sha256=(Get-FileHash $script:UiExecutable -Algorithm SHA256).Hash;Counts=$counts;
      Cpu=(Get-CimInstance Win32_Processor|Select-Object -ExpandProperty Name);LogicalProcessors=[Environment]::ProcessorCount;
      DragFullWindows=[FileSpacerColumnTests.ColumnsUi]::DragFullWindows();Os=[Environment]::OSVersion.VersionString;
      StopGuard=[bool]$script:StressCpu;StopResponseMs=500;StopSystemCpuPercent=50;StopBusySeconds=1}|
        ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'gesture-stress-build.json') -Encoding UTF8
    $cache=Join-Path $ProjectRoot 'build\column-stress-fixtures';[void][IO.Directory]::CreateDirectory($cache)
    foreach($count in $counts){
        $script:StressItems=$count;Check-Budget 'before-fixture'
        $folder=Join-Path $cache ('items-'+$count)
        if(!(Test-Path $folder)){
            [void][IO.Directory]::CreateDirectory($folder)
            for($i=0;$i -lt $count;++$i){
                [IO.File]::WriteAllBytes((Join-Path $folder ('item-{0:D6}.txt' -f $i)),[byte[]]@())
                if($i%128 -eq 0){Check-Budget 'fixture'}
            }
        }
        Assert-Ui ([IO.Directory]::GetFiles($folder).Count -eq $count -and [IO.Directory]::GetDirectories($folder).Count -eq 0) 'Incorrect fixture size.'
        foreach($deferred in @($true,$false)){
            $script:StressDeferred=$deferred
            Set-UiPreference 'DetailsColumnsDeferred' ([int]$deferred)
            $window=Open-UiFolder $folder $count
            # Only the owned fixture window is made topmost for verified input.
            Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr](-1),0,0,0,0,0x13)) 'Cannot expose the owned stress window.'
            Activate-Ui $window
            Use-UiView $window {param($view) $view.Mode=4};Wait-UiViewMode $window 4
            Wait-Ui {(Get-ColumnsItemCount $window) -eq $count} 'complete stress enumeration' 300 120
            [void](Wait-Columns $window 300 120)
            $owner=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$owner)
            $process=[Diagnostics.Process]::GetProcessById([int]$owner)
            try{
                $column=Measure-ResizeGesture $window $process 'column' $deferred
                $frame=Measure-ResizeGesture $window $process 'window' $deferred
                $record=[pscustomobject]@{Items=$count;ActualItems=(Get-ColumnsItemCount $window);Deferred=$deferred;Column=$column;Window=$frame}
                [void]$records.Add($record)
                $records|ConvertTo-Json -Depth 6|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'gesture-stress.json') -Encoding UTF8
                Write-Host ('MEASURE '+($record|ConvertTo-Json -Depth 6 -Compress))
            }finally{$process.Dispose()}
            Close-UiFolder $window
        }
    }
    Write-Host ('PASS: '+$records.Count+' samples; real column and frame drags, stationary held pauses, final geometry and CPU by phase.')
}catch{$failure=$_;throw}finally{
    try{[FileSpacerRegression.AltDragInput]::End()}finally{Stop-Ui -Failure $failure}
}
