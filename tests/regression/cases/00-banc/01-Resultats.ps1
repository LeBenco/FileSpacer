# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
. (Join-Path $PSScriptRoot '..\..\common\Results.ps1')
$temporaryRoot=Join-Path ([IO.Path]::GetTempPath()) ('FileSpacer-results-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($temporaryRoot)
function Assert-Result([bool]$Condition,[string]$Message) { if (!$Condition) { throw $Message } }
try {
    $errorPath=Join-Path $temporaryRoot 'errors.log'
    $crash='Application exited with 0xC0000409 (-1073740791): PID 1234'
    [IO.File]::WriteAllText($errorPath,$crash)
    $result=Get-RegressionOutcome 'FAIL' 1 '' $errorPath $temporaryRoot 'Recovery helper remained running.'
    Assert-Result ($result.Status -eq 'FAIL') 'A crash must remain FAIL.'
    Assert-Result ($result.Detail.Contains($crash)) 'Runner cleanup hid the initial crash.'
    Assert-Result ($result.Detail.Contains('Recovery helper remained running.')) 'Runner cleanup was lost.'

    [IO.File]::WriteAllText($errorPath,'')
    $result=Get-RegressionOutcome 'PASS' 0 '' $errorPath $temporaryRoot 'Application remained running.'
    Assert-Result ($result.Status -eq 'FAIL') 'Cleanup failure must invalidate a successful test.'
    Assert-Result ($result.Detail.Contains('Application remained running.')) 'Missing cleanup failure reason.'

    foreach ($applicationIds in @(@(),@(1234),@(1234,5678,9012))) {
        $healthyRecords=@($applicationIds | ForEach-Object {
            @{ProcessId=$_; Exited=$true; ExitCode=0; Error=$null}
        })
        ConvertTo-Json -InputObject $healthyRecords |
            Set-Content -LiteralPath (Join-Path $temporaryRoot 'application-exits.json') -Encoding UTF8
        $result=Get-RegressionOutcome 'PASS' 0 '' $errorPath $temporaryRoot
        Assert-Result ($result.Status -eq 'PASS' -and !$result.Detail) 'Normal application exits must remain PASS for empty, singleton and multiple reports.'
    }

    @(
        @{ProcessId=1234; Exited=$true; ExitCode=-1073740791; Error=$null},
        @{ProcessId=5678; Exited=$true; ExitCode=-1073741819; Error=$null}
    ) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $temporaryRoot 'application-exits.json') -Encoding UTF8
    $result=Get-RegressionOutcome 'PASS' 0 '' $errorPath $temporaryRoot
    Assert-Result ($result.Status -eq 'FAIL') 'Recorded application crashes must invalidate a zero test exit code.'
    Assert-Result ($result.Detail.Contains('1234') -and $result.Detail.Contains('0xC0000409') -and $result.Detail.Contains('5678') -and $result.Detail.Contains('0xC0000005')) 'All crash codes and process identifiers must appear in the summary.'
    Remove-Item -LiteralPath (Join-Path $temporaryRoot 'application-exits.json')

    @{Kind='ForegroundPrerequisite'; Reason='No activation target.'} | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $temporaryRoot 'blocked.json') -Encoding UTF8
    @{Succeeded=$true; ProcessCount=1} | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $temporaryRoot 'cleanup-complete.json') -Encoding UTF8
    $result=Get-RegressionOutcome 'FAIL' 1 '' $errorPath $temporaryRoot
    Assert-Result ($result.Status -eq 'BLOCKED') 'A confirmed unmet prerequisite must be BLOCKED.'
    $result=Get-RegressionOutcome 'FAIL' 1 '' $errorPath $temporaryRoot 'Recovery helper remained running.'
    Assert-Result ($result.Status -eq 'FAIL') 'A leftover process must override BLOCKED.'

    @('Application exited with 0xC0000005: PID 5678') | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $temporaryRoot 'cleanup-errors.json') -Encoding UTF8
    $result=Get-RegressionOutcome 'FAIL' 1 '' $errorPath $temporaryRoot
    Assert-Result ($result.Status -eq 'FAIL' -and $result.Detail.Contains('0xC0000005')) 'A cleanup crash must override BLOCKED.'
    @('First cleanup failure.','Second cleanup failure.') | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $temporaryRoot 'cleanup-errors.json') -Encoding UTF8
    $result=Get-RegressionOutcome 'FAIL' 1 '' $errorPath $temporaryRoot
    Assert-Result ($result.Detail.Contains('First cleanup failure.; Second cleanup failure.')) 'Every cleanup failure must appear separately.'
    @('Application exited with 0xC0000005: PID 5678') | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $temporaryRoot 'cleanup-errors.json') -Encoding UTF8
    $result=Get-RegressionOutcome 'FAIL' 1 'Timeout' $errorPath $temporaryRoot 'Helper cleanup failed.'
    Assert-Result ($result.Detail.Contains('Timeout') -and $result.Detail.Contains('0xC0000005') -and $result.Detail.Contains('Helper cleanup failed.')) 'Multiple failures must be preserved.'

    $summary=Get-RegressionSummary @(@{Status='PASS'},@{Status='BLOCKED'}) 3 $null
    Assert-Result ($summary.Status -eq 'INCOMPLETE' -and $summary.NotRun -eq 1) 'Incomplete suite reported incorrectly.'
    $summary=Get-RegressionSummary @(@{Status='PASS'},@{Status='FAIL'}) 2 $null
    Assert-Result ($summary.Status -eq 'FAIL') 'A failed scenario must invalidate the suite.'
    Write-Host 'PASS: initial failures, cleanup failures and blocked prerequisites remain distinct.'
} finally { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force }
