param(
    [string]$VMName='AgentWin10',
    [string]$ReportDirectory='',
    [switch]$Elevated,
    [switch]$RunSmoke,
    [switch]$RunRegression,
    [System.Management.Automation.PSCredential]$GuestCredential,
    [string]$ProjectRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1') -ErrorAction Stop

# Run manually: the UAC and credential prompts are the only host interaction.
# The default is read-only; -RunSmoke explicitly starts the prepared guest test.
if(!$ProjectRoot){$ProjectRoot=Join-Path $PSScriptRoot '..\..'}
$projectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
if($RunSmoke -and !$RunRegression){
    foreach($relative in @('build\debug\FileSpacer.exe','build\client-smoke\FileSpacerRegression-HyperV.dll')){
        if(!(Test-Path -LiteralPath (Join-Path $projectRoot $relative))){throw 'Compile on the host first with prepare-client-hyperv.cmd.'}
    }
    if(@(Get-Process vmconnect -ErrorAction SilentlyContinue).Count){throw 'Close VMConnect consoles while leaving AgentWin10 running and signed in.'}
}
if (!$ReportDirectory) {
    $prefix=if($RunSmoke){'hyperv-smoke-'}else{'hyperv-access-'}
    $ReportDirectory=Join-Path $projectRoot ('build\client-smoke\'+$prefix+[guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $ReportDirectory)
}
$ReportDirectory=(Resolve-Path -LiteralPath $ReportDirectory).Path
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if($GuestCredential){throw 'Stored credentials require the configured privileged controller.'}
    if ($Elevated) { throw 'The elevated process does not have administrator rights.' }
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -VMName '+$VMName+' -ReportDirectory "'+$ReportDirectory+'" -Elevated'
    if($RunSmoke){$arguments+=' -RunSmoke'}
    # Do not expose a host console. Windows itself displays authentication prompts.
    $process=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -PassThru
    try {
        $process.WaitForExit()
        $resultFile=Join-Path $ReportDirectory 'connection.json'
        Write-Host ('Report: '+$resultFile)
        if (!(Test-Path -LiteralPath $resultFile)) { throw 'The elevated helper exited without a connection report.' }
        $result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
        $expected=if($RunSmoke){'PASS'}else{'CONNECTED'}
        if ($process.ExitCode -ne 0 -or $result.Status -ne $expected) {
            throw ('Hyper-V connection blocked: '+$result.Error)
        }
        if($RunSmoke){Write-Host 'PASS: real guest Shell, Alt-right resize and normal exit with VMConnect absent.'}
        else{Write-Host 'CONNECTED: PowerShell Direct only. No UI test has been executed.'}
    } finally { $process.Dispose() }
    return
}

$session=$null
$credential=$null
$report=[ordered]@{Status='BLOCKED';Stage='HyperV';VMName=$VMName;HostMachine=$env:COMPUTERNAME;Guest=$null;TestsExecuted=$false;Smoke=$null;Error=$null;CleanupErrors=@()}
try {
    $vm=Get-VM -Name $VMName -ErrorAction Stop
    if ($vm.State -ne 'Running') { throw 'AgentWin10 must already be running; it will not be started automatically.' }
    $report.Stage='Authentication'
    $credential=$GuestCredential
    if(!$credential){$credential=Get-Credential -Message 'Windows account inside AgentWin10 and its password (not a Windows Hello PIN). No credentials will be saved.'}
    if (!$credential) { throw 'Guest credentials were not supplied.' }
    $session=New-PSSession -VMId $vm.Id -Credential $credential -ErrorAction Stop
    $report.Stage='Inventory'
    $report.Guest=Invoke-Command -Session $session -ScriptBlock {
        $system=Get-CimInstance Win32_OperatingSystem
        $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
        $desktops=@(Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" | ForEach-Object {
            $owner=Invoke-CimMethod -InputObject $_ -MethodName GetOwnerSid
            [pscustomobject]@{SessionId=$_.SessionId;BelongsToConnectedAccount=($owner.ReturnValue -eq 0 -and $owner.Sid -eq $identity.User.Value)}
        })
        [pscustomobject]@{
            Machine=$env:COMPUTERNAME
            Account=$identity.Name
            AccountSid=$identity.User.Value
            Windows=$system.Caption
            WindowsVersion=$system.Version
            ExplorerSessions=$desktops
            ExistingFileSpacerProcesses=@(Get-Process FileSpacer -ErrorAction SilentlyContinue | Select-Object Id,SessionId)
        }
    }
    if ($report.Guest.Machine -eq $env:COMPUTERNAME) { throw 'The remote command unexpectedly identified the host.' }
    $report.Status='CONNECTED'
    $report.Stage='Complete'
    if($RunSmoke -or $RunRegression){
        $report.Stage='Smoke'
        $report.Smoke=& (Join-Path $PSScriptRoot 'Run-HyperVSmoke.ps1') -Session $session -Guest $report.Guest -RunDirectory $ReportDirectory -ProjectRoot $projectRoot -Regression:$RunRegression
        $report.Status=$report.Smoke.Status
        $report.TestsExecuted=$report.Smoke.TaskStarted
        $report.Error=$report.Smoke.Error
        if($report.Status-eq'PASS'){$report.Stage='Complete'}
    }
} catch {
    $report.Error=$_.Exception.Message
} finally {
    if ($session) {
        try { Remove-PSSession -Session $session -ErrorAction Stop }
        catch { $report.CleanupErrors+=@($_.Exception.Message) }
    }
    if ($credential -and !$GuestCredential) { $credential.Password.Dispose(); $credential=$null }
    if ($report.CleanupErrors.Count) { $report.Status='BLOCKED' }
    $temporary=Join-Path $ReportDirectory 'connection.tmp'
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $temporary -Encoding UTF8
    Move-Item -LiteralPath $temporary -Destination (Join-Path $ReportDirectory 'connection.json') -Force
}
$expected=if($RunSmoke -or $RunRegression){'PASS'}else{'CONNECTED'}
if ($report.Status -ne $expected) { throw ('Hyper-V operation did not pass: '+$report.Error) }
if($RunSmoke){Write-Host 'PASS: guest smoke test completed.'}else{Write-Host 'CONNECTED: PowerShell Direct only. No UI test has been executed.'}
