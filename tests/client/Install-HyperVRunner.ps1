param([string]$WorkspaceRoot='',[string]$HostUserSid='',[string]$ReportDirectory='',[switch]$Elevated,[switch]$UpdateController,[string]$VMName='AgentWin10')
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
if($VMName -match '[\\/:*?"<>|]' -or $VMName -in @('.','..')){throw 'Invalid VM profile name.'}
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1') -ErrorAction Stop
$existingConfig=Join-Path $env:ProgramData ('FileSpacerTests\'+$VMName+'\runner.json')
if(!$WorkspaceRoot -and $UpdateController -and (Test-Path -LiteralPath $existingConfig)){$WorkspaceRoot=(Get-Content -LiteralPath $existingConfig -Raw | ConvertFrom-Json).WorkspaceRoot}
if(!$WorkspaceRoot){$WorkspaceRoot=Join-Path $PSScriptRoot '..\..'}
$WorkspaceRoot=(Resolve-Path -LiteralPath $WorkspaceRoot).Path.TrimEnd('\')
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if(!$HostUserSid){$HostUserSid=$identity.User.Value}
if(!$ReportDirectory){
    $ReportDirectory=Join-Path $WorkspaceRoot ('build\client-smoke\runner-setup-'+[guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($ReportDirectory)
}
$ReportDirectory=(Resolve-Path -LiteralPath $ReportDirectory).Path
$principal=[Security.Principal.WindowsPrincipal]::new($identity)
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){
    if($Elevated){throw 'Runner setup requires a Windows administrator token.'}
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -WorkspaceRoot "'+$WorkspaceRoot+'" -HostUserSid '+$HostUserSid+' -ReportDirectory "'+$ReportDirectory+'" -VMName "'+$VMName+'" -Elevated'
    if($UpdateController){$arguments+=' -UpdateController'}
    $child=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -PassThru
    try{$child.WaitForExit();if($child.ExitCode-ne0){throw ('Runner setup failed; see '+$ReportDirectory)}}finally{$child.Dispose()}
    return
}
$report=[ordered]@{Status='BLOCKED';TaskName=('FileSpacer-'+$VMName+'-Smoke');Error=$null}
$credential=$null;$session=$null
try {
    if($identity.User.Value-ne$HostUserSid){throw 'Elevate with the same host Windows account so DPAPI and the interactive task use the same identity.'}
    $installation=Join-Path $env:ProgramData ('FileSpacerTests\'+$VMName)
    $launcherSource=Join-Path $PSScriptRoot '..\..\build\client-smoke\NoConsoleRunner.exe'
    if(!(Test-Path -LiteralPath $launcherSource)){throw 'Compile NoConsoleRunner.exe on the host before configuring its task.'}
    $installationParent=Split-Path $installation -Parent
    if($UpdateController){
        $config=Get-Content -LiteralPath (Join-Path $installation 'runner.json') -Raw | ConvertFrom-Json
        if($config.HostUserSid-ne$HostUserSid -or $config.WorkspaceRoot-ne$WorkspaceRoot -or $config.VMName-ne$VMName){throw 'Existing runner identity or workspace differs from the requested update.'}
        if((Get-ScheduledTask -TaskName $report.TaskName).State-eq'Running'){throw 'Wait for the existing runner before updating its controller.'}
        $administrators=[Security.Principal.SecurityIdentifier]::new('S-1-5-32-544')
        foreach($name in @('HyperVRunnerTask.ps1','Connect-HyperV.ps1','Run-HyperVSmoke.ps1')){
            $target=Join-Path $installation $name
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $target -Force
            $fileAcl=Get-Acl -LiteralPath $target;$fileAcl.SetOwner($administrators);Set-Acl -LiteralPath $target -AclObject $fileAcl
        }
        $launcher=Join-Path $installation 'NoConsoleRunner.exe'
        Copy-Item -LiteralPath $launcherSource -Destination $launcher -Force
        $fileAcl=Get-Acl -LiteralPath $launcher;$fileAcl.SetOwner($administrators);Set-Acl -LiteralPath $launcher -AclObject $fileAcl
        $action=New-ScheduledTaskAction -Execute $launcher -WorkingDirectory $installation
        [void](Set-ScheduledTask -TaskName $report.TaskName -Action $action)
        $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 240) -MultipleInstances IgnoreNew -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
        [void](Set-ScheduledTask -TaskName $report.TaskName -Settings $settings)
        $config | Add-Member -NotePropertyName ProtocolVersion -NotePropertyValue 2 -Force
        $config | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $installation 'runner.json') -Encoding UTF8
        $report.Status='UPDATED'
        return
    }
    if((Test-Path -LiteralPath $installation) -or (Get-ScheduledTask -TaskName $report.TaskName -ErrorAction SilentlyContinue)){throw 'An existing runner installation must be reviewed before replacing it.'}
    $vm=Get-VM -Name $VMName -ErrorAction Stop
    if($vm.State-ne'Running'){throw ('VM must already be running: '+$VMName)}
    $credential=Get-Credential -Message ('Dedicated test account in '+$VMName+'. Windows will encrypt these credentials locally for unattended test launches.')
    if(!$credential -or $credential.Password.Length-eq0){throw 'A nonempty guest password is required.'}
    $session=New-PSSession -VMId $vm.Id -Credential $credential -ErrorAction Stop
    $machine=Invoke-Command -Session $session -ScriptBlock {$env:COMPUTERNAME}
    if($machine-eq$env:COMPUTERNAME){throw 'The connection unexpectedly identified the host.'}
    Remove-PSSession -Session $session;$session=$null
    [void][IO.Directory]::CreateDirectory($installation)
    $acl=New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    $administrators=[Security.Principal.SecurityIdentifier]::new('S-1-5-32-544')
    $acl.SetOwner($administrators)
    foreach($entry in @(@{Sid=$administrators;Rights='FullControl'},@{Sid=[Security.Principal.SecurityIdentifier]::new('S-1-5-18');Rights='FullControl'},@{Sid=[Security.Principal.SecurityIdentifier]::new($HostUserSid);Rights='ReadAndExecute'})){
        $rule=[Security.AccessControl.FileSystemAccessRule]::new($entry.Sid,[Security.AccessControl.FileSystemRights]$entry.Rights,[Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit',[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Allow)
        $acl.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $installationParent -AclObject $acl
    Set-Acl -LiteralPath $installation -AclObject $acl
    foreach($name in @('HyperVRunnerTask.ps1','Connect-HyperV.ps1','Run-HyperVSmoke.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $installation}
    Copy-Item -LiteralPath $launcherSource -Destination (Join-Path $installation 'NoConsoleRunner.exe')
    $credential | Export-Clixml -LiteralPath (Join-Path $installation 'guest-credential.xml')
    $requestDirectory=Join-Path $env:LOCALAPPDATA ('FileSpacerClientTests\'+$VMName)
    [void][IO.Directory]::CreateDirectory($requestDirectory)
    @{WorkspaceRoot=$WorkspaceRoot;RequestFile=(Join-Path $requestDirectory 'request.json');HostUserSid=$HostUserSid;VMName=$VMName;ProtocolVersion=2;TaskName=$report.TaskName} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $installation 'runner.json') -Encoding UTF8
    foreach($file in Get-ChildItem -LiteralPath $installation -File){
        $fileAcl=Get-Acl -LiteralPath $file.FullName
        $fileAcl.SetOwner($administrators)
        Set-Acl -LiteralPath $file.FullName -AclObject $fileAcl
    }
    $action=New-ScheduledTaskAction -Execute (Join-Path $installation 'NoConsoleRunner.exe') -WorkingDirectory $installation
    $taskPrincipal=New-ScheduledTaskPrincipal -UserId $HostUserSid -LogonType Interactive -RunLevel Highest
    $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 240) -MultipleInstances IgnoreNew -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    [void](Register-ScheduledTask -TaskName $report.TaskName -Action $action -Principal $taskPrincipal -Settings $settings -Description ('Run FileSpacer tests only in '+$VMName+'. Host controller is protected; guest credentials use Windows DPAPI.'))
    $report.Status='CONFIGURED'
}catch{$report.Error=$_.Exception.Message}
finally {
    if($session){Remove-PSSession -Session $session}
    if($credential){$credential.Password.Dispose()}
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ReportDirectory 'setup.json') -Encoding UTF8
}
if($report.Status-notin@('CONFIGURED','UPDATED')){throw $report.Error}
