function Get-RegressionOutcome([string]$Status,[int]$ExitCode,[string]$Detail,[string]$ErrorPath,[string]$TestRun,[string]$CleanupDetail='') {
    $reason=$Detail
    $blockedPath=Join-Path $TestRun 'blocked.json'
    $cleanupPath=Join-Path $TestRun 'cleanup-errors.json'
    $cleanupCompletePath=Join-Path $TestRun 'cleanup-complete.json'
    $hasCleanup=Test-Path -LiteralPath $cleanupPath
    $applicationFailures=New-Object Collections.Generic.List[string]
    $applicationExitPath=Join-Path $TestRun 'application-exits.json'
    if (Test-Path -LiteralPath $applicationExitPath) {
        try {
            # Parentheses enumerate the JSON array on both Windows PowerShell 5.1 and PowerShell 7.
            foreach ($record in @((Get-Content -LiteralPath $applicationExitPath -Raw | ConvertFrom-Json))) {
                if ($record.Error) { $applicationFailures.Add(('PID '+$record.ProcessId+': '+$record.Error)) }
                elseif (!$record.Exited) { $applicationFailures.Add(('PID '+$record.ProcessId+': still running')) }
                elseif ($record.ExitCode -ne 0) { $applicationFailures.Add(('PID {0}: 0x{1:X8} ({1})' -f $record.ProcessId,([int]$record.ExitCode))) }
            }
        } catch { $applicationFailures.Add(('Cannot read application exit report: '+$_.Exception.Message)) }
    }
    if ($Status -eq 'PASS' -and $ExitCode -ne 0) {
        $Status='FAIL'; $reason="Test process exited with code $ExitCode."
    }
    if ($hasCleanup -or $CleanupDetail -or $applicationFailures.Count) { $Status='FAIL' }
    # BLOCKED requires positive confirmation that all tracked application
    # processes exited normally. A missing/unreadable cleanup report stays FAIL.
    if ($Status -eq 'FAIL' -and $ExitCode -eq 1 -and !$reason -and (Test-Path -LiteralPath $blockedPath) -and !$hasCleanup -and !$CleanupDetail -and !$applicationFailures.Count -and (Test-Path -LiteralPath $cleanupCompletePath)) {
        try {
            $cleanupComplete=Get-Content -LiteralPath $cleanupCompletePath -Raw | ConvertFrom-Json
            $blocked=Get-Content -LiteralPath $blockedPath -Raw | ConvertFrom-Json
            if ($cleanupComplete.Succeeded -is [bool] -and $cleanupComplete.Succeeded -and $blocked.Kind -eq 'ForegroundPrerequisite' -and $blocked.Reason) {
                return [pscustomobject]@{Status='BLOCKED'; Detail=[string]$blocked.Reason}
            }
        } catch { $reason='Cannot read the explicit test prerequisite report: '+$_.Exception.Message }
    }
    if ($Status -ne 'PASS' -and !$reason -and (Test-Path -LiteralPath $ErrorPath)) {
        $reason=[string](Get-Content -LiteralPath $ErrorPath | Where-Object { $_.Trim().Length } | Select-Object -First 1)
    }
    if ($applicationFailures.Count) { $reason=($reason+'; application exits: '+($applicationFailures -join '; ')).Trim('; ') }
    if ($hasCleanup -and $Status -ne 'PASS') {
        try {
            $cleanup=@((Get-Content -LiteralPath $cleanupPath -Raw | ConvertFrom-Json)) -join '; '
            $reason=($reason+'; cleanup: '+$cleanup).Trim('; ')
        } catch { $reason=($reason+'; cleanup report unreadable').Trim('; ') }
    }
    if ($CleanupDetail) { $reason=($reason+'; runner cleanup: '+$CleanupDetail).Trim('; ') }
    if ($Status -ne 'PASS' -and !$reason) { $reason="Test process exited with code $ExitCode; inspect its logs." }
    return [pscustomobject]@{Status=$Status; Detail=$reason}
}
function Get-RegressionSummary($Results,[int]$Expected,$Fatal) {
    $passed=@($Results | Where-Object { $_.Status -eq 'PASS' }).Count
    $blocked=@($Results | Where-Object { $_.Status -eq 'BLOCKED' }).Count
    $failed=@($Results | Where-Object { $_.Status -eq 'FAIL' }).Count
    $executed=@($Results).Count
    $missing=[Math]::Max(0,$Expected-$executed)
    $status='PASS'
    if ($passed -ne $Expected) { $status='INCOMPLETE' }
    if ($failed -or $Fatal -or $executed -gt $Expected -or ($passed+$failed+$blocked) -ne $executed) { $status='FAIL' }
    return [pscustomobject]@{Status=$status; Expected=$Expected; Executed=$executed; Passed=$passed; Failed=$failed; Blocked=$blocked; NotRun=$missing; Error=$Fatal}
}
function Show-RegressionSummary($Results,$Summary,[string]$Run) {
    Write-Host ''
    Write-Host ("BILAN : {0} REUSSIS | {1} EN ECHEC | {2} BLOQUES | {3} NON EXECUTES" -f $Summary.Passed,$Summary.Failed,$Summary.Blocked,$Summary.NotRun)
    Write-Host 'EN ECHEC : scenario en erreur, origine applicative ou banc a analyser.'
    Write-Host 'BLOQUE : un prealable du banc empeche de valider le scenario.'
    foreach ($result in $Results) {
        if ($result.Status -eq 'PASS') { continue }
        Write-Host ''
        Write-Host ("[{0}] {1}" -f $result.Status,$result.Test)
        Write-Host ("  {0}" -f $result.Detail)
    }
    if ($Summary.Error) { Write-Host ("Erreur du lanceur/restauration : {0}" -f $Summary.Error) }
    Write-Host ''
    if ($Summary.Status -eq 'PASS') { Write-Host 'SUITE VALIDEE' }
    elseif ($Summary.Status -eq 'INCOMPLETE') { Write-Host 'SUITE NON VALIDEE : scenarios bloques ou non executes.' }
    else { Write-Host 'SUITE NON VALIDEE : au moins un echec ou une erreur du lanceur.' }
    Write-Host "Results: $Run"
}
