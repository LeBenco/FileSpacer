# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $PSScriptRoot 'LongCopyProgress.cs') -ReferencedAssemblies $references
$testFailure=$null;$paused=$null;$progress=[IntPtr]::Zero;$controlHeld=$false;$target='';$length=2GB;$budget=$null
try {
    Initialize-Ui 'shell-long-copy'
    $source=Join-Path $script:UiRoot 'Source';$destination=Join-Path $script:UiRoot 'Target'
    foreach($folder in @($source,$destination)) { [void][IO.Directory]::CreateDirectory($folder) }
    $payload=Join-Path $source 'Payload';$targetFolder=Join-Path $destination 'Payload'
    [void][IO.Directory]::CreateDirectory($payload)
    $file=Join-Path $payload 'Large.bin';$target=Join-Path $targetFolder 'Large.bin'
    $free=(New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($file))).AvailableFreeSpace
    Write-Host ('Fixture disk free bytes: '+$free)
    Assert-Ui ($free -gt 2*$length+512MB) 'Long-copy fixture requires space for both copies plus 512 MiB.'
    $buffer=New-Object byte[] 1MB;(New-Object Random 1907).NextBytes($buffer)
    $stream=[IO.File]::Create($file)
    try { $stream.Write($buffer,0,$buffer.Length);$stream.SetLength($length) }
    finally { $stream.Dispose() }
    # Metadata work and the test-only CPU budget keep this native operation
    # observable on a fast local disk, without requiring a network service.
    for($index=0;$index -lt 1024;$index++) { [IO.File]::WriteAllText((Join-Path $payload ('z-'+$index+'.txt')),('payload-'+$index)) }
    $expected=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    # Clear an initial desktop flyout before preparing any application action.
    [FileSpacerRegression.AltDragInput]::Escape()
    # Open both children from one native view: all three windows must share the
    # same UI thread, unlike separate command-line launches of different folders.
    $witness=Open-UiFolder $script:UiRoot 2
    Focus-ShellFixture $witness 'Source';[FileSpacerRegression.Native]::Keys($witness,0x0d,$false,$false)
    $first=Track-UiFolder $source
    Focus-ShellFixture $witness 'Target';[FileSpacerRegression.Native]::Keys($witness,0x0d,$false,$false)
    $second=Track-UiFolder $destination
    $ownerId=[uint32]0;$otherId=[uint32]0
    $thread=[FileSpacerRegression.Native]::GetWindowThreadProcessId($first,[ref]$ownerId)
    Assert-Ui ([FileSpacerRegression.Native]::GetWindowThreadProcessId($second,[ref]$otherId) -eq $thread -and $otherId -eq $ownerId) 'The two transfer windows must share one process and UI thread.'
    Assert-Ui ([FileSpacerRegression.Native]::GetWindowThreadProcessId($witness,[ref]$otherId) -eq $thread) 'The witness must share the transfer UI thread.'
    $budget=New-Object FileSpacerRegression.CopyCpuBudget $ownerId
    $work=[FileSpacerRegression.Native]::WorkArea($first)
    $width=[int](($work.Width-60)/2);$height=[Math]::Min(480,$work.Height-60)
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($witness,0x112,[IntPtr]0xf020,[IntPtr]::Zero)) 'Cannot minimize the witness.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($first,[IntPtr]::Zero,($work.Left+20),($work.Top+20),$width,$height,0x14)) 'Cannot place the source.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($second,[IntPtr]::Zero,($work.Left+40+$width),($work.Top+20),$width,$height,0x14)) 'Cannot place the destination.'
    Focus-ShellFixture $first 'Payload'
    $sourceView=Use-UiView $first {param($view) $view.Window}
    $destinationView=Use-UiView $second {param($view) $view.Window}
    $from=[FileSpacerRegression.AltDragInput]::ItemPoint($sourceView,'Payload')
    $to=[FileSpacerRegression.AltDragInput]::BackgroundPoint($destinationView)
    [FileSpacerRegression.AltDragInput]::Begin($first,$from,$false,$false)
    $controlHeld=$true;[FileSpacerRegression.ShellIntegration]::Control($true)
    Wait-Ui { [FileSpacerRegression.ShellIntegration]::ControlDown } 'Ctrl held before drag'
    for($step=1;$step -le 12;$step++) {
        $point=New-Object FileSpacerRegression.MousePoint
        $point.X=[int]($from.X+($to.X-$from.X)*$step/12);$point.Y=[int]($from.Y+($to.Y-$from.Y)*$step/12)
        [FileSpacerRegression.AltDragInput]::Move($point);Start-Sleep -Milliseconds 35
    }
    Start-Sleep -Milliseconds 200
    $budget.Limit()
    [FileSpacerRegression.AltDragInput]::End()
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    while(!$paused -and [DateTime]::UtcNow -lt $deadline) {
        foreach($candidate in [FileSpacerRegression.Native]::Top('OperationStatusWindow',[NullString]::Value,$ownerId)) {
            if(![FileSpacerRegression.Native]::IsWindowVisible($candidate)) { continue }
            if([FileSpacerRegression.LongCopyProgress]::Pause($candidate)) { $paused=$candidate;$progress=$candidate;break }
        }
        if(!$paused) { Start-Sleep -Milliseconds 10 }
    }
    Assert-Ui ($null -ne $paused) 'A native transfer with a Pause button must be observed before completion.'
    Wait-Ui { [FileSpacerRegression.LongCopyProgress]::Paused($paused) } 'native transfer reports its paused state'
    [FileSpacerRegression.ShellIntegration]::Control($false);$controlHeld=$false
    Assert-Ui (!(Test-Path -LiteralPath $targetFolder) -or [IO.Directory]::GetFiles($targetFolder).Length -lt 1025 -or ![FileSpacerRegression.LongCopyProgress]::Complete($target,$length)) 'The batch completed before the responsiveness assertion.'
    # A Shell modal copy loop can dispatch WM_CLOSE while bypassing FileSpacer's
    # top-level keyboard handling. Exercise a real application shortcut as well.
    Activate-Ui $witness
    $search=@([FileSpacerRegression.Native]::Children($witness,'FileSpacer Search Box'))
    Assert-Ui ($search.Count -eq 1) 'The witness must expose its search box.'
    $edit=@([FileSpacerRegression.Native]::Children($search[0],'Edit'))
    Assert-Ui ($edit.Count -eq 1 -and [FileSpacerRegression.Native]::FocusedWindowInOwningThread($witness) -ne $edit[0]) 'Search must initially be unfocused.'
    [FileSpacerRegression.Native]::Keys($witness,0x46,$true,$false)
    Wait-Ui { [FileSpacerRegression.Native]::FocusedWindowInOwningThread($witness) -eq $edit[0] } 'Ctrl+F focuses search while the native copy is paused' 0 5
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($witness,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot request witness closure.'
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($witness) } 'unrelated folder closes while the transfer is paused' 0 5
    foreach($window in @($first,$second)) {
        Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot request transfer window closure.'
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($window) } 'transfer folder closes before completion' 0 5
    }
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($progress)) 'Closing the last folder interrupted the transfer.'
    $budget.Dispose();$budget=$null
    [FileSpacerRegression.LongCopyProgress]::Resume($paused);$paused=$null
    Wait-Ui { [FileSpacerRegression.LongCopyProgress]::Complete($target,$length) -and ![FileSpacerRegression.Native]::IsWindow($progress) } 'native copy finishes after closing all folders' 0 120
    Assert-Ui ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ceq $expected) 'Copied bytes differ.'
    Assert-Ui ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ceq $expected) 'Copy changed or removed the source.'
    Assert-Ui ([IO.Directory]::GetFiles($targetFolder).Length -eq 1025) 'The batch has missing or unexpected target files.'
    for($index=0;$index -lt 1024;$index++) {
        $name='z-'+$index+'.txt'
        Assert-Ui ([IO.File]::ReadAllText((Join-Path $targetFolder $name)) -ceq ('payload-'+$index)) ('Incorrect target bytes: '+$name)
        Assert-Ui ([IO.File]::ReadAllText((Join-Path $payload $name)) -ceq ('payload-'+$index)) ('Changed source bytes: '+$name)
    }
    Save-ShellCoverage 'long-copy.json' @{Bytes=$length;SmallFiles=1024;Sha256=$expected;ProcessId=$ownerId;ThreadId=$thread;SearchShortcutDuringPausedCopy=$true;FoldersClosedDuringPausedCopy=3}
    Write-Host 'PASS: native mouse copy of 2 GiB plus 1024 small files permits Ctrl+F and closure of all three same-thread folders while paused, finishes with exact bytes and preserves its source.'
} catch {
    $testFailure=$_
    try {
        $targetCount=if(Test-Path -LiteralPath $targetFolder){[IO.Directory]::GetFiles($targetFolder).Length}else{0}
        Save-ShellCoverage 'failure-fixture.json' @{Root=@([IO.Directory]::GetFileSystemEntries($script:UiRoot));SourceFiles=[IO.Directory]::GetFiles($payload).Length;TargetFiles=$targetCount}
        foreach($candidate in [FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,0)) {
            Save-ShellScreenshot $candidate ('copy-failure-'+$candidate.ToInt64()+'.png') -WithoutActivation
        }
    } catch { Write-Warning ('Cannot collect copy diagnostics: '+$_.Exception.Message) }
    throw $testFailure
}
finally {
    $cleanupFailure=$null
    try {
        if($budget) { $budget.Dispose();$budget=$null }
        [FileSpacerRegression.AltDragInput]::End()
        if($controlHeld) { [FileSpacerRegression.ShellIntegration]::Control($false) }
        if($paused) {
            [FileSpacerRegression.LongCopyProgress]::Resume($paused)
            Wait-Ui { [FileSpacerRegression.LongCopyProgress]::Complete($target,$length) -and ![FileSpacerRegression.Native]::IsWindow($progress) } 'cleanup waits for the resumed native transfer' 0 120
        }
    } catch {
        $cleanupFailure=$_
        Write-Warning ('Native transfer cleanup failed: '+$_.Exception.Message)
    } finally {
        $failure=if($testFailure){$testFailure}else{$cleanupFailure}
        Stop-Ui -Failure $failure
    }
    if(!$testFailure -and $cleanupFailure) { throw $cleanupFailure }
}
