# Use Process.Start on the actual Process instance so its exit code is retained.
# Stdout and stderr are drained concurrently to avoid blocking a noisy child.
function Start-RecordedProcess {
    param([string]$Executable,[string]$Arguments,[string]$OutputPath,[string]$ErrorPath)
    $process=New-Object Diagnostics.Process
    $process.StartInfo.FileName=$Executable
    $process.StartInfo.Arguments=$Arguments
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    $process.StartInfo.StandardOutputEncoding=[Text.Encoding]::Default
    $process.StartInfo.StandardErrorEncoding=[Text.Encoding]::Default
    try {
        if (!$process.Start()) { throw "Cannot start $Executable" }
        return [pscustomobject]@{
            Process=$process
            Output=$process.StandardOutput.ReadToEndAsync()
            Error=$process.StandardError.ReadToEndAsync()
            OutputPath=$OutputPath
            ErrorPath=$ErrorPath
        }
    } catch { $process.Dispose(); throw }
}
function Complete-RecordedProcess($Job) {
    if (!$Job.Process.HasExited) { throw 'Cannot collect the result of a running process.' }
    $Job.Process.WaitForExit()
    if (!$Job.Output.Wait(5000) -or !$Job.Error.Wait(5000)) { throw 'Child output streams did not close.' }
    $utf8=New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Job.OutputPath,$Job.Output.Result,$utf8)
    [IO.File]::WriteAllText($Job.ErrorPath,$Job.Error.Result,$utf8)
    return [int]$Job.Process.ExitCode
}
function Start-TestApplication {
    param([string]$Executable,[string]$Arguments,[string]$WorkingDirectory='')
    $process=New-Object Diagnostics.Process
    $process.StartInfo.FileName=$Executable
    $process.StartInfo.Arguments=$Arguments
    $process.StartInfo.UseShellExecute=$false
    # Suppress only the Debug console. Do not pass SW_HIDE to the folder window.
    $process.StartInfo.CreateNoWindow=$true
    if ($WorkingDirectory) { $process.StartInfo.WorkingDirectory=$WorkingDirectory }
    $startedUtc=[DateTime]::UtcNow
    try {
        if (!$process.Start()) { throw "Cannot start $Executable" }
        $process | Add-Member -NotePropertyName RegressionStartedUtc -NotePropertyValue $startedUtc
        if ($env:FS_REGRESSION_RUN) {
            try {
                $trackingPath=Join-Path $env:FS_REGRESSION_RUN 'application-processes.json'
                $tracked=@()
                if (Test-Path -LiteralPath $trackingPath) { $tracked=@((Get-Content -LiteralPath $trackingPath -Raw | ConvertFrom-Json)) }
                $tracked+=@{ProcessId=$process.Id; StartedUtc=$startedUtc.ToString('o')}
                ConvertTo-Json -InputObject @($tracked) | Set-Content -LiteralPath $trackingPath -Encoding UTF8
            } catch { Write-Warning ('Cannot record application process: '+$_.Exception.Message) }
        }
        return $process
    } catch { $process.Dispose(); throw }
}

# Diagnostics are optional evidence, never a substitute for exit-code assertions.
function Save-TestApplicationDiagnostics([int]$ApplicationId,[datetime]$StartedUtc=[datetime]::MinValue) {
    if (!$env:FS_REGRESSION_RUN) { return }
    try {
        foreach ($prefix in @('FileSpacer-selection-initialisation-','FileSpacer-search-')) {
            $trace=Join-Path $env:TEMP ($prefix+$ApplicationId+'.log')
            if ((Test-Path -LiteralPath $trace) -and (Get-Item -LiteralPath $trace).LastWriteTimeUtc -ge $StartedUtc) {
                $destination=Join-Path $env:FS_REGRESSION_RUN 'application-traces'
                [void][IO.Directory]::CreateDirectory($destination)
                Copy-Item -LiteralPath $trace -Destination $destination -Force
            }
        }
        foreach ($extension in @('txt','dmp')) {
            $report=Join-Path $env:LOCALAPPDATA ('FileSpacer\recovery-test-'+$ApplicationId+'.'+$extension)
            if ((Test-Path -LiteralPath $report) -and (Get-Item -LiteralPath $report).LastWriteTimeUtc -ge $StartedUtc) {
                $destination=Join-Path $env:FS_REGRESSION_RUN 'application-recovery'
                [void][IO.Directory]::CreateDirectory($destination)
                Copy-Item -LiteralPath $report -Destination $destination -Force
            }
        }
    } catch { Write-Warning ('Cannot collect application diagnostics: '+$_.Exception.Message) }
}

# A vanished window is not proof of a normal application exit.
function Complete-TestApplications($Processes) {
    $exitFailures=New-Object Collections.Generic.List[string]
    $exitRecords=New-Object Collections.Generic.List[object]
    foreach ($applicationProcess in $Processes) {
        $applicationId=0
        $record=[ordered]@{ProcessId=$null; Exited=$false; ExitCode=$null; ExitCodeHex=$null; Error=$null}
        try {
            $applicationId=$applicationProcess.Id
            $record.ProcessId=$applicationId
            if (!$applicationProcess.WaitForExit(10000)) {
                $exitFailures.Add("Application still running: PID $applicationId")
            } else {
                $code=[int]$applicationProcess.ExitCode
                $hex='0x{0:X8}' -f $code
                $record.Exited=$true; $record.ExitCode=$code; $record.ExitCodeHex=$hex
                if ($code -ne 0) { $exitFailures.Add("Application exited with $hex ($code): PID $applicationId") }
            }
        } catch { $record.Error=$_.Exception.Message; $exitFailures.Add($_.Exception.Message) }
        finally {
            $exitRecords.Add([pscustomobject]$record)
            if ($applicationId) {
                $startedUtc=[datetime]::MinValue
                if ($applicationProcess.PSObject.Properties['RegressionStartedUtc']) { $startedUtc=$applicationProcess.RegressionStartedUtc }
                Save-TestApplicationDiagnostics $applicationId $startedUtc
            }
            $applicationProcess.Dispose()
        }
    }
    if ($env:FS_REGRESSION_RUN) {
        try {
            ConvertTo-Json -InputObject @($exitRecords.ToArray()) -Depth 4 |
                Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'application-exits.json') -Encoding UTF8
        } catch { Write-Warning ('Cannot record application exit codes: '+$_.Exception.Message) }
    }
    if ($exitFailures.Count) { throw ($exitFailures -join '; ') }
}
