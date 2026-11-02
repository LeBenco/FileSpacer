param([Parameter(Mandatory=$true)][string]$RunDirectory,[string]$Commit='',[string]$Scope='')
$ErrorActionPreference='Stop'
$report=New-Object Collections.Generic.List[string]
$report.Add("Commit: $Commit | Scope: $Scope")
$report.Add('')
$workerPath=Join-Path $RunDirectory 'worker-result.json'
$workerSucceeded=$false
if(Test-Path $workerPath){
    $worker=Get-Content $workerPath -Raw|ConvertFrom-Json
    $report.Add(('Worker: {0} | admin={1} | session={2} | runner exit={3}' -f $worker.Status,$worker.Administrator,$worker.SessionId,$worker.RunnerExitCode))
    if($worker.Error){$report.Add('Worker error: '+$worker.Error)}
    $workerSucceeded=$worker.Status-eq'PASS' -and !$worker.Administrator -and $worker.RunnerExitCode-eq0
}
$summaryPath=Join-Path $RunDirectory 'output\summary.json'
$success=$false
if(Test-Path $summaryPath){
    $summary=Get-Content $summaryPath -Raw|ConvertFrom-Json
    $report.Add(('Result: {0} | passed {1}/{2} | failed {3} | blocked {4} | not run {5}' -f $summary.Status,$summary.Passed,$summary.Expected,$summary.Failed,$summary.Blocked,$summary.NotRun))
    $results=Get-Content (Join-Path $RunDirectory 'output\results.json') -Raw|ConvertFrom-Json
    $failed=@($results|Where-Object Status -ne 'PASS')
    foreach($case in @($failed|Select-Object -First 20)){
        $detail=([string]$case.Detail -replace '\s+',' ').Trim()
        if($detail.Length-gt300){$detail=$detail.Substring(0,300)+'...'}
        $report.Add('')
        $report.Add(('**{0}** `{1}` (exit {2}): {3}' -f $case.Status,$case.Test,$case.ExitCode,$detail))
        $directory=Join-Path (Join-Path $RunDirectory 'output') ($case.Test -replace '\.ps1$','')
        $log=Join-Path $directory 'output.log'
        if(Test-Path $log){
            $evidence=@(Get-Content $log | Where-Object { $_ -match '^(FAIL:|Contents:|Factory=|FolderBinding=|CanonicalCompare=|Converted=|Expected=|RESULT:)' } | Select-Object -Last 3)
            foreach($line in $evidence){$report.Add('    '+$line)}
        }
    }
    if($failed.Count-gt20){$report.Add(('Further failed cases: '+($failed.Count-20)+'. See results.json.'))}
    if($summary.Error){$report.Add('Runner/cleanup error: '+$summary.Error)}
    $success=$workerSucceeded -and $summary.Status-eq'PASS' -and $summary.Expected-gt0 -and $summary.Passed-eq$summary.Expected
}else{
    $report.Add('No regression summary: build, preparation or worker failed before completion.')
    foreach($name in @('worker-result.json','runner.stderr.log','preparation.log')){
        $path=Join-Path $RunDirectory $name
        if(Test-Path $path){$report.Add('');$report.Add($name+':');foreach($line in @(Get-Content $path -Tail 6)){$report.Add('    '+$line)}}
    }
}
$report.Add('')
$report.Add('Full logs, JSON results and screenshots are in the run artifacts.')
$report|Write-Host
if($env:GITHUB_STEP_SUMMARY){$report|Out-File $env:GITHUB_STEP_SUMMARY -Append -Encoding UTF8}
if(!$success){throw 'The selected regression suite is not validated; see the summary above.'}
