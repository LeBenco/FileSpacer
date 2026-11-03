param([Parameter(Mandatory=$true)][string]$RunDirectory,[string]$Commit='',[string]$Scope='')
$ErrorActionPreference='Stop'
function Format-Cell([string]$Value) {
    return ($Value -replace '&','&amp;' -replace '<','&lt;' -replace '>','&gt;' -replace '\|','&#124;' -replace '`','&#96;' -replace '\s+',' ').Trim()
}
$report=New-Object Collections.Generic.List[string]
$diagnostics=New-Object Collections.Generic.List[string]
$report.Add('## Regression summary')
$report.Add('')
$report.Add("Commit: $Commit | Scope: $Scope")
$report.Add('')
$workerPath=Join-Path $RunDirectory 'worker-result.json'
$workerSucceeded=$false
if(Test-Path $workerPath){
    $worker=Get-Content $workerPath -Raw|ConvertFrom-Json
    $diagnostics.Add(('Worker: {0}; admin={1}; session={2}; runner exit={3}' -f $worker.Status,$worker.Administrator,$worker.SessionId,$worker.RunnerExitCode))
    if($worker.Error){$diagnostics.Add('Worker error: '+(Format-Cell $worker.Error))}
    $workerSucceeded=$worker.Status-eq'PASS' -and !$worker.Administrator -and $worker.RunnerExitCode-eq0
}else{$diagnostics.Add('Worker report missing: execution cannot be validated.')}
$summaryPath=Join-Path $RunDirectory 'output\summary.json'
$success=$false
if(Test-Path $summaryPath){
    $summary=Get-Content $summaryPath -Raw|ConvertFrom-Json
    $results=Get-Content (Join-Path $RunDirectory 'output\results.json') -Raw|ConvertFrom-Json
    $results=@($results)
    $failed=@($results|Where-Object Status -ne 'PASS')
    $success=$workerSucceeded -and $summary.Status-eq'PASS' -and $summary.Expected-gt0 -and $summary.Passed-eq$summary.Expected -and $results.Count-eq$summary.Expected -and !$failed.Count
    $verdict=if($success){'PASS'}else{'FAIL - suite not validated'}
    $report.Add(('**{0}** - {1}/{2} tests passed.' -f $verdict,$summary.Passed,$summary.Expected))
    $report.Add('')
    $report.Add('| Category | Tests | Passed | Failed | Blocked | Not run |')
    $report.Add('| :--- | ---: | ---: | ---: | ---: | ---: |')
    foreach($group in @($results|Group-Object Category|Sort-Object Name)){
        $cases=@($group.Group)
        $passed=@($cases|Where-Object Status -eq 'PASS').Count
        $errors=@($cases|Where-Object Status -eq 'FAIL').Count
        $blocked=@($cases|Where-Object Status -eq 'BLOCKED').Count
        $report.Add(('| {0} | {1} | {2} | {3} | {4} | 0 |' -f (Format-Cell $group.Name),$cases.Count,$passed,$errors,$blocked))
    }
    if($summary.NotRun){$report.Add(('| Not assigned (never started) | {0} | 0 | 0 | 0 | {0} |' -f $summary.NotRun))}
    $report.Add(('| **Total** | **{0}** | **{1}** | **{2}** | **{3}** | **{4}** |' -f $summary.Expected,$summary.Passed,$summary.Failed,$summary.Blocked,$summary.NotRun))
    if($failed.Count){
        $report.Add('')
        $report.Add('### Failed or blocked tests')
        $report.Add('')
        $report.Add('| Category | Test | Status | Exit | Reason / native evidence |')
        $report.Add('| :--- | :--- | :--- | ---: | :--- |')
    }
    foreach($case in @($failed|Sort-Object Category,Test)){
        $detail=([string]$case.Detail -replace '\s+',' ').Trim()
        if($detail.Length-gt300){$detail=$detail.Substring(0,300)+'...'}
        $detail=Format-Cell $detail
        $directory=Join-Path (Join-Path $RunDirectory 'output') ($case.Test -replace '\.ps1$','')
        $log=Join-Path $directory 'output.log'
        if(Test-Path $log){
            $evidence=@(Get-Content $log | Where-Object { $_ -match '^(FAIL:|Contents:|Factory=|FolderBinding=|CanonicalCompare=|Converted=|Expected=|RESULT:)' } | Select-Object -Last 3)
            foreach($line in $evidence){
                if($line.Length-gt300){$line=$line.Substring(0,300)+'...'}
                $detail+='<br>'+(Format-Cell $line)
            }
        }
        $report.Add(('| {0} | {1} | {2} | {3} | {4} |' -f (Format-Cell $case.Category),(Format-Cell $case.Test),(Format-Cell $case.Status),$case.ExitCode,$detail))
    }
    if($summary.Error){$diagnostics.Add('Runner/cleanup error: '+(Format-Cell $summary.Error))}
}else{
    $report.Add('**FAIL - no regression report.** Build, preparation or worker failed before completion.')
    foreach($name in @('worker-result.json','runner.stderr.log','preparation.log')){
        $path=Join-Path $RunDirectory $name
        if(Test-Path $path){$report.Add('');$report.Add($name+':');foreach($line in @(Get-Content $path -Tail 6)){$report.Add('    '+$line)}}
    }
}
$report.Add('')
foreach($line in $diagnostics){$report.Add($line);$report.Add('')}
$report.Add('Full logs, JSON results and screenshots are in the run artifacts.')
$report|Write-Host
$report|Out-File (Join-Path $RunDirectory 'regression-summary.md') -Encoding UTF8
if($env:GITHUB_STEP_SUMMARY){$report|Out-File $env:GITHUB_STEP_SUMMARY -Append -Encoding UTF8}
if(!$success){throw 'The selected regression suite is not validated; see the summary above.'}
