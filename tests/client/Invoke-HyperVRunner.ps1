param([string]$ProjectRoot=(Join-Path $PSScriptRoot '..\..'),[string]$VMName='AgentWin10',[string]$RegressionRunId='')
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
if($VMName -match '[\\/:*?"<>|]' -or $VMName -in @('.','..')){throw 'Invalid VM profile name.'}
$installation=Join-Path $env:ProgramData ('FileSpacerTests\'+$VMName)
$configPath=Join-Path $installation 'runner.json'
if(!(Test-Path -LiteralPath $configPath)){throw ('Configure the VM runner once with Install-HyperVRunner.ps1 -VMName "'+$VMName+'".')}
$config=Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
if($config.VMName-ne$VMName){throw 'Configured VM differs from the request.'}
$project=(Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\')
$workspace=[IO.Path]::GetFullPath($config.WorkspaceRoot).TrimEnd('\')
if($project-ne$workspace -and !$project.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'The checkout is outside the configured workspace.'}
$mutex=New-Object Threading.Mutex($false,('Local\FileSpacer-'+$VMName+'-Smoke'))
$locked=$false
try {
    try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
    if(!$locked){throw ('Another test request is active for '+$VMName)}
    if((Get-ScheduledTask -TaskName $config.TaskName).State-eq'Running'){throw ('The VM runner is already running: '+$VMName)}
    $runId=if($RegressionRunId){$RegressionRunId}else{[guid]::NewGuid().ToString('N')}
    if($runId -notmatch '^[0-9a-f]{32}$'){throw 'Invalid regression run identifier.'}
    $prefix=if($RegressionRunId){'build\regression\vm-'}else{'build\client-smoke\hyperv-smoke-'}
    $run=Join-Path $project ($prefix+$runId)
    [void][IO.Directory]::CreateDirectory($run)
    $temporary=$config.RequestFile+'.tmp'
    @{RunId=$runId;ProjectRoot=$project;Mode=$(if($RegressionRunId){'Regression'}else{'Smoke'})} | ConvertTo-Json | Set-Content -LiteralPath $temporary -Encoding UTF8
    Move-Item -LiteralPath $temporary -Destination $config.RequestFile -Force
    Start-ScheduledTask -TaskName $config.TaskName
    Write-Host ('Guest test requested: '+$runId)
    $waitSeconds=240
    if($RegressionRunId){$selection=Get-Content -LiteralPath (Join-Path $run 'package\selection.json') -Raw | ConvertFrom-Json;$waitSeconds=180+@($selection.Cases).Count*([int]$selection.TimeoutSeconds+15)}
    $deadline=[DateTime]::UtcNow.AddSeconds($waitSeconds)
    do {
        $recordFile=Join-Path $installation 'last-run.json'
        if(Test-Path -LiteralPath $recordFile){
            $record=Get-Content -LiteralPath $recordFile -Raw | ConvertFrom-Json
            if($record.RunId-eq$runId -and $record.State-eq'Complete'){break}
        }
        if([DateTime]::UtcNow-ge$deadline){throw ('No matching completed runner result before deadline: '+$run)}
        Start-Sleep -Milliseconds 250
    }while($true)
    $taskDeadline=[DateTime]::UtcNow.AddSeconds(10)
    while((Get-ScheduledTask -TaskName $config.TaskName).State-eq'Running'){
        if([DateTime]::UtcNow-ge$taskDeadline){throw 'The host controller did not exit after its result.'}
        Start-Sleep -Milliseconds 100
    }
    $exitCode=(Get-ScheduledTaskInfo -TaskName $config.TaskName).LastTaskResult
    foreach($name in @('host-controller.stdout.log','host-controller.stderr.log','host-launcher.error.log')){
        $hostLog=Join-Path $installation $name
        if(Test-Path -LiteralPath $hostLog){Copy-Item -LiteralPath $hostLog -Destination $run}
    }
    if($RegressionRunId){
        # The protected controller propagates a nonzero connection exit. The
        # connection report retains the actual guest FAIL/INCOMPLETE status.
        $connectionFile=Join-Path $run 'connection.json'
        if(Test-Path -LiteralPath $connectionFile){
            $connection=Get-Content -LiteralPath $connectionFile -Raw | ConvertFrom-Json
            $record.Status=$connection.Status
        }
    }
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'controller-result.json') -Encoding UTF8
    $record | ConvertTo-Json | Write-Host
    if($RegressionRunId -and (Test-Path -LiteralPath (Join-Path $run 'output\regression\summary.json'))){
        Get-Content -LiteralPath (Join-Path $run 'output\regression\summary.json') | Write-Host
    }
    if($record.Status-ne'PASS' -or $exitCode-ne0){throw ($VMName+' test did not pass: '+$record.Error+'; task exit='+$exitCode+'; reports='+$run)}
    Write-Host ('PASS: unattended '+$VMName+' launch; reports='+$run)
    $global:LASTEXITCODE=0
}finally{if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()}
