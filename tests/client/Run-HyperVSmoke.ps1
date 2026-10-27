param(
    [Parameter(Mandatory=$true)][System.Management.Automation.Runspaces.PSSession]$Session,
    [Parameter(Mandatory=$true)]$Guest,
    [Parameter(Mandatory=$true)][string]$RunDirectory,
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [switch]$Regression
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$runId=[IO.Path]::GetFileName($RunDirectory)
$taskName='FileSpacer-Smoke-'+$runId
$guestRoot=$null;$taskRegistered=$false
$report=[ordered]@{Status='BLOCKED';RunId=$runId;TaskStarted=$false;VMConnectAbsent=$true;ConsolePreparation=$null;TaskExitCode=$null;GuestResult=$null;Error=$null;CleanupErrors=@()}
try {
    if(@(Get-Process vmconnect -ErrorAction SilentlyContinue).Count){throw 'Close VMConnect consoles, leaving the VM running and its desktop session signed in.'}
    $desktops=@($Guest.ExplorerSessions | Where-Object { $_.BelongsToConnectedAccount })
    if($desktops.Count-ne1){throw 'Expected exactly one guest desktop for the connected account.'}
    $inputDirectory=Join-Path $RunDirectory 'input'
    $waitSeconds=180
    if($Regression){
        $inputDirectory=Join-Path $RunDirectory 'package'
        $selection=Get-Content -LiteralPath (Join-Path $inputDirectory 'selection.json') -Raw | ConvertFrom-Json
        if(@($selection.Cases).Count-lt1 -or @($selection.Cases).Count-gt200 -or $selection.TimeoutSeconds-lt30 -or $selection.TimeoutSeconds-gt3600){throw 'Invalid prepared test selection.'}
        $waitSeconds=120+@($selection.Cases).Count*([int]$selection.TimeoutSeconds+15)
    }else{
    [void][IO.Directory]::CreateDirectory($inputDirectory)
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'build\debug\FileSpacer.exe') -Destination $inputDirectory
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'build\client-smoke\FileSpacerRegression-HyperV.dll') -Destination (Join-Path $inputDirectory 'FileSpacerRegression.dll')
    foreach($scriptName in @('HyperVSmoke.ps1','SandboxGesture.ps1')){Copy-Item -LiteralPath (Join-Path $ProjectRoot ('tests\client\'+$scriptName)) -Destination $inputDirectory}
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'tests\regression\common\Process.ps1') -Destination $inputDirectory
    }
    $manifest=@{
        RunId=$runId;HostMachine=$env:COMPUTERNAME;GuestMachine=$Guest.Machine;GuestAccountSid=$Guest.AccountSid;GuestSessionId=$desktops[0].SessionId
        BuildLocation='Host';Configuration='Debug'
        ExecutableSha256=(Get-FileHash -LiteralPath (Join-Path $inputDirectory $(if($Regression){'build\debug\FileSpacer.exe'}else{'FileSpacer.exe'})) -Algorithm SHA256).Hash
    }
    if(!$Regression){$manifest.HelperSha256=(Get-FileHash -LiteralPath (Join-Path $inputDirectory 'FileSpacerRegression.dll') -Algorithm SHA256).Hash}
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $inputDirectory 'manifest.json') -Encoding UTF8
    $guestRoot=Invoke-Command -Session $Session -ArgumentList $runId,$Guest.AccountSid -ScriptBlock {
        param($runId,$sid)
        $root=Join-Path $env:SystemDrive ('FileSpacerTests\'+[guid]::NewGuid().ToString('N').Substring(0,8))
        if(Test-Path -LiteralPath $root){throw 'Guest run directory already exists.'}
        [void][IO.Directory]::CreateDirectory((Join-Path $root 'input'))
        $acl=Get-Acl -LiteralPath $root
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid),'Modify','ContainerInherit,ObjectInherit','None','Allow'))
        Set-Acl -LiteralPath $root -AclObject $acl
        return $root
    }
    foreach($file in Get-ChildItem -LiteralPath $inputDirectory){Copy-Item -ToSession $Session -LiteralPath $file.FullName -Destination (Join-Path $guestRoot 'input') -Recurse}
    $report.ConsolePreparation=Invoke-Command -Session $Session -ArgumentList $guestRoot,$Guest.AccountSid,$desktops[0].SessionId -ScriptBlock {
        param($root,$sid,$desktopSession)
        if([Security.Principal.WindowsIdentity]::GetCurrent().User.Value-ne$sid){throw 'Console preparation account differs from the declared guest user.'}
        $before=(& (Join-Path $env:SystemRoot 'System32\query.exe') session $desktopSession | Out-String)
        $guestPrincipal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
        $preparation=[ordered]@{Before=$before;After=$before;IsAdministrator=$guestPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator);ExitCode=0;Transferred=$false;Output=$null}
        if($before-notmatch'(?m)^\s*>?\s*console\s'){
            # Only the owning guest account is transferred. The subsequent UI
            # test still runs with its original Limited interactive token.
            $output=Join-Path $root 'output';[void][IO.Directory]::CreateDirectory($output)
            # This native preparation must also work when the guest's default
            # execution policy refuses loading PS1 files in a remoting session.
            $transfer=New-Object Diagnostics.Process
            try{
                $transfer.StartInfo.FileName=Join-Path $env:SystemRoot 'System32\tscon.exe'
                $transfer.StartInfo.Arguments=$desktopSession.ToString()+' /dest:console'
                $transfer.StartInfo.UseShellExecute=$false;$transfer.StartInfo.CreateNoWindow=$true
                $transfer.StartInfo.RedirectStandardOutput=$true;$transfer.StartInfo.RedirectStandardError=$true
                $encoding=[Text.Encoding]::GetEncoding((Get-Culture).TextInfo.OEMCodePage)
                $transfer.StartInfo.StandardOutputEncoding=$encoding;$transfer.StartInfo.StandardErrorEncoding=$encoding
                if(!$transfer.Start()){throw 'Cannot start the guest console preparation.'}
                $stdout=$transfer.StandardOutput.ReadToEndAsync();$stderr=$transfer.StandardError.ReadToEndAsync()
                if(!$transfer.WaitForExit(10000)){throw 'Guest console preparation timed out.'}
                if(!$stdout.Wait(5000) -or !$stderr.Wait(5000)){throw 'Guest console preparation output did not close.'}
                $preparation.ExitCode=[int]$transfer.ExitCode
                [IO.File]::WriteAllText((Join-Path $output 'console-preparation.stdout.log'),$stdout.Result)
                [IO.File]::WriteAllText((Join-Path $output 'console-preparation.stderr.log'),$stderr.Result)
                $preparation.Output=$stdout.Result+$stderr.Result
            }finally{$transfer.Dispose()}
            $preparation.After=(& (Join-Path $env:SystemRoot 'System32\query.exe') session $desktopSession | Out-String)
            $preparation.Transferred=$preparation.ExitCode-eq0
        }
        return [pscustomobject]$preparation
    }
    if($report.ConsolePreparation.ExitCode-ne0){throw ('Guest console preparation failed: '+$report.ConsolePreparation.Output)}
    Invoke-Command -Session $Session -ArgumentList $taskName,$guestRoot,$Guest.AccountSid,([bool]$Regression),$waitSeconds -ScriptBlock {
        param($taskName,$root,$sid,$regression,$waitSeconds)
        $powershell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $worker=Join-Path $root $(if($regression){'input\tests\client\HyperVSmoke.ps1'}else{'input\HyperVSmoke.ps1'})
        $arguments='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+$worker+'"'
        if($regression){$arguments+=' -Regression'}
        $action=New-ScheduledTaskAction -Execute $powershell -Argument $arguments -WorkingDirectory $root
        $principal=New-ScheduledTaskPrincipal -UserId $sid -LogonType Interactive -RunLevel Limited
        $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Seconds ($waitSeconds+60)) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
        [void](Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings)
    }
    $taskRegistered=$true
    if(@(Get-Process vmconnect -ErrorAction SilentlyContinue).Count){throw 'A VMConnect console opened before the guest test; no gesture was started.'}
    Invoke-Command -Session $Session -ArgumentList $taskName -ScriptBlock {param($name) Start-ScheduledTask -TaskName $name}
    $report.TaskStarted=$true
    $deadline=[DateTime]::UtcNow.AddSeconds($waitSeconds)
    do {
        if(@(Get-Process vmconnect -ErrorAction SilentlyContinue).Count){$report.VMConnectAbsent=$false}
        $completed=Invoke-Command -Session $Session -ArgumentList $guestRoot -ScriptBlock {param($root) Test-Path -LiteralPath (Join-Path $root 'output\result.json')}
        if($completed){break}
        if([DateTime]::UtcNow-ge$deadline){throw 'No guest result before its deadline. Preserve the guest run for diagnosis.'}
        Start-Sleep -Milliseconds 500
    }while($true)
    Copy-Item -FromSession $Session -LiteralPath (Join-Path $guestRoot 'output') -Destination $RunDirectory -Recurse
    $report.GuestResult=Get-Content -LiteralPath (Join-Path $RunDirectory 'output\result.json') -Raw | ConvertFrom-Json
    if($report.GuestResult.RunId-ne$runId -or $report.GuestResult.GuestMachine-ne$Guest.Machine -or $report.GuestResult.ExecutableSha256-ne$manifest.ExecutableSha256){throw 'Guest result does not match the host-built deployment.'}
    $report.TaskExitCode=[int](Invoke-Command -Session $Session -ArgumentList $taskName -ScriptBlock {
        param($name)
        $deadline=[DateTime]::UtcNow.AddSeconds(10)
        while((Get-ScheduledTask -TaskName $name).State-eq'Running'){
            if([DateTime]::UtcNow-ge$deadline){throw 'Guest worker did not exit after its result.'}
            Start-Sleep -Milliseconds 100
        }
        return (Get-ScheduledTaskInfo -TaskName $name).LastTaskResult
    })
    $report.Status=$report.GuestResult.Status
    $report.Error=$report.GuestResult.Error
    if($report.Status-eq'PASS' -and $report.TaskExitCode-ne0){$report.Status='FAIL';$report.Error='Guest worker returned a nonzero exit code after a PASS report.'}
    if($report.Status-eq'PASS' -and !$report.VMConnectAbsent){$report.Status='REVIEW';$report.Error='A VMConnect console was present during the test; closed-console execution is not established.'}
}catch{$report.Error=$_.Exception.Message}
finally {
    if($guestRoot -and !(Test-Path -LiteralPath (Join-Path $RunDirectory 'output'))){
        try {
            $hasOutput=Invoke-Command -Session $Session -ArgumentList $guestRoot -ScriptBlock {param($root) Test-Path -LiteralPath (Join-Path $root 'output')}
            if($hasOutput){Copy-Item -FromSession $Session -LiteralPath (Join-Path $guestRoot 'output') -Destination $RunDirectory -Recurse}
        }catch{$report.CleanupErrors+=@($_.Exception.Message)}
    }
    if($taskRegistered){
        try {
            Invoke-Command -Session $Session -ArgumentList $taskName -ScriptBlock {
                param($name)
                if((Get-ScheduledTask -TaskName $name).State-eq'Running'){throw 'Guest task still running; preserve it so its state cleanup can finish.'}
                Unregister-ScheduledTask -TaskName $name -Confirm:$false
            }
        }catch{$report.CleanupErrors+=@($_.Exception.Message)}
    }
    if($report.CleanupErrors.Count){$report.Status='FAIL'}
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $RunDirectory 'smoke.json') -Encoding UTF8
}
return [pscustomobject]$report
