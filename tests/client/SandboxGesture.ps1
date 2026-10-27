param([string]$InputRoot='C:\SmokeInput',[string]$OutputRoot='C:\SmokeOutput',[switch]$TestProfile)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$manifest=Get-Content -LiteralPath (Join-Path $inputRoot 'manifest.json') -Raw|ConvertFrom-Json
if($env:COMPUTERNAME-eq$manifest.HostMachine){throw 'The client gesture must not run on the build host.'}
if($TestProfile){
    if($manifest.Configuration-ne'Debug' -or $env:COMPUTERNAME-ne$manifest.GuestMachine){throw 'Expected the declared Debug Hyper-V guest.'}
}elseif($env:USERNAME-ne'WDAGUtilityAccount'){throw 'Dedicated Sandbox guest required.'}
$local=Join-Path $env:TEMP ('FileSpacer-gesture-'+$manifest.RunId)
[void](New-Item -ItemType Directory -Path $local)
foreach($file in @('FileSpacer.exe','FileSpacerRegression.dll','Process.ps1')){Copy-Item -LiteralPath (Join-Path $inputRoot $file) -Destination $local}
$executable=Join-Path $local 'FileSpacer.exe';$assembly=Join-Path $local 'FileSpacerRegression.dll'
if((Get-FileHash $executable -Algorithm SHA256).Hash-ne$manifest.ExecutableSha256 -or (Get-FileHash $assembly -Algorithm SHA256).Hash-ne$manifest.HelperSha256){throw 'Guest binary hash mismatch.'}
[void][Reflection.Assembly]::LoadFrom($assembly)
. (Join-Path $local 'Process.ps1')
$settingsKey=if($TestProfile){'Software\FileSpacer\Test'}else{'Software\FileSpacer'}
$key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($settingsKey)
try {
    foreach($name in @('AdminWarning','UpdateCheckEnabled')){$key.SetValue($name,0,[Microsoft.Win32.RegistryValueKind]::DWord)}
    $key.SetValue('LastOpenedVersion',2147483647,[Microsoft.Win32.RegistryValueKind]::DWord)
    $key.SetValue('WindowShortcuts',0x01003222,[Microsoft.Win32.RegistryValueKind]::DWord)
}finally{$key.Dispose()}
function Wait-Guest([scriptblock]$Condition,[string]$Description,[int]$Seconds=10){
    $until=[DateTime]::UtcNow.AddSeconds($Seconds)
    do{if(& $Condition){return};Start-Sleep -Milliseconds 20}while([DateTime]::UtcNow-lt$until)
    throw ('Timed out: '+$Description)
}
@{RunId=$manifest.RunId;GuestMachine=$env:COMPUTERNAME;ExecutableSha256=$manifest.ExecutableSha256}|ConvertTo-Json|Set-Content (Join-Path $outputRoot 'ready.json') -Encoding UTF8
$phases=if($TestProfile){@('console-closed')}else{@('behind','minimized')}
foreach($phase in $phases){
    if(!$TestProfile){Wait-Guest {Test-Path -LiteralPath (Join-Path $inputRoot ($phase+'.start'))} ('host phase signal '+$phase) 600}
    $phaseOutput=Join-Path $outputRoot $phase;[void](New-Item -ItemType Directory -Path $phaseOutput)
    $env:FS_REGRESSION_RUN=$phaseOutput
    $report=[ordered]@{Phase=$phase;Status='FAIL';RunId=$manifest.RunId;GuestMachine=$env:COMPUTERNAME;Configuration=$manifest.Configuration;BuildLocation='Host';Window=0;Before=$null;After=$null;ActivationAccepted=$false;ForegroundBefore=0;InputReady=$false;InputBefore=$null;InputAfter=$null;NormalExit=$false;Error=$null;CleanupErrors=@()}
    $process=$null;$window=[IntPtr]::Zero;$shell=$null;$shellWindows=$null;$oldDpi=[IntPtr]::Zero
    try{
        $oldDpi=[FileSpacerRegression.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
        $fixture=Join-Path $local $phase;[void](New-Item -ItemType Directory -Path $fixture)
        [IO.File]::WriteAllText((Join-Path $fixture 'probe.txt'),'guest gesture fixture')
        $arguments=if($TestProfile){'/test "'+$fixture+'"'}else{'"'+$fixture+'"'}
        $process=Start-TestApplication -Executable $executable -Arguments $arguments -OutputDirectory $phaseOutput
        Wait-Guest {
            if($process.HasExited){throw ('FileSpacer exited early: '+$process.ExitCode)}
            $found=@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',$phase,[uint32]$process.Id))
            if($found.Count-eq1 -and [FileSpacerRegression.Native]::IsWindowVisible($found[0])){$script:window=$found[0];return $true};return $false
        } 'visible folder'
        $report.Window=$window.ToInt64()
        $shell=New-Object -ComObject Shell.Application;$shellWindows=$shell.Windows();$viewWindow=[IntPtr]::Zero
        Wait-Guest {
            for($index=0;$index-lt$shellWindows.Count;++$index){
                $registered=$shellWindows.Item($index);$view=$null
                try{
                    $view=[FileSpacerRegression.Native]::View($registered,$window)
                    if($view -and $view.FolderParsingName-eq$fixture -and ($view.Names()-join';')-eq'probe.txt'){$script:viewWindow=$view.Window;return $true}
                }finally{if($view){$view.Dispose()};if($registered){[void][Runtime.InteropServices.Marshal]::ReleaseComObject($registered)}}
            };return $false
        } 'actual Shell view'
        if(![FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,40,40,650,400,0x14)){throw 'Guest placement failed.'}
        Wait-Guest {$r=[FileSpacerRegression.Native]::Bounds($window);$r.Width-eq650-and$r.Height-eq400} 'initial geometry'
        # Record the API result, but let the real Alt-right gesture activate an
        # inactive target, as exercised by the existing native inactive-window case.
        $report.ActivationAccepted=[FileSpacerRegression.Native]::Activate($window)
        $report.ForegroundBefore=[FileSpacerRegression.Native]::GetForegroundWindow().ToInt64()
        $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
        if($lists.Count-ne1){throw 'Expected one native item container.'}
        $area=[FileSpacerRegression.Native]::ClientBounds($lists[0])
        $point=New-Object FileSpacerRegression.MousePoint;$point.X=[int]($area.Left+$area.Width/2);$point.Y=[int]($area.Top+3*$area.Height/4)
        [FileSpacerRegression.NativeTargets]::Background($lists[0],$point.X,$point.Y)
        $report.Before=[FileSpacerRegression.Native]::Bounds($window)
        $report.InputBefore=[FileSpacerRegression.AltDragInput]::Describe($window)
        [FileSpacerRegression.AltDragInput]::BeginRight($window,$point,$true,$false)
        [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
        $report.InputReady=$true
        Wait-Guest {[FileSpacerRegression.AltDragInput]::Captured($window)} 'gesture captured'
        $end=New-Object FileSpacerRegression.MousePoint;$end.X=$point.X+96;$end.Y=$point.Y+72
        [FileSpacerRegression.AltDragInput]::Move($end)
        Wait-Guest {$r=[FileSpacerRegression.Native]::Bounds($window);$r.Left-eq$report.Before.Left -and $r.Top-eq$report.Before.Top -and $r.Width-eq746 -and $r.Height-eq472} 'real Alt-right resize'
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Guest {[FileSpacerRegression.AltDragInput]::Idle($window)} 'gesture released'
        $report.After=[FileSpacerRegression.Native]::Bounds($window)
        $report.InputAfter=[FileSpacerRegression.AltDragInput]::Describe($window)
        $report.Status='PASS'
    }catch{
        $report.Error=$_.Exception.Message
        if(!$report.InputReady){$report.Status='BLOCKED'}else{$report.Status='FAIL'}
    }finally{
        try{[FileSpacerRegression.AltDragInput]::End()}catch{$report.CleanupErrors+=@($_.Exception.Message)}
        if($process){
            try{
                if($window-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($window)){
                    $owner=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$owner)
                    if($owner-ne$process.Id){throw 'Guest window owner changed.'}
                    if(![FileSpacerRegression.Native]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)){throw 'Guest close failed.'}
                }
            }catch{$report.CleanupErrors+=@($_.Exception.Message)}
            try{Complete-TestApplications @($process);$report.NormalExit=$true}catch{$report.CleanupErrors+=@($_.Exception.Message)}
        }
        foreach($com in @($shellWindows,$shell)){if($com){try{[void][Runtime.InteropServices.Marshal]::ReleaseComObject($com)}catch{$report.CleanupErrors+=@($_.Exception.Message)}}}
        if($oldDpi-ne[IntPtr]::Zero){[void][FileSpacerRegression.Native]::SetThreadDpiAwarenessContext($oldDpi)}
        if($report.CleanupErrors.Count -or !$report.NormalExit){$report.Status='FAIL'}
        $temporary=Join-Path $outputRoot ($phase+'.tmp');$report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $temporary -Encoding UTF8
        Move-Item -LiteralPath $temporary -Destination (Join-Path $outputRoot ($phase+'.json'))
    }
}
