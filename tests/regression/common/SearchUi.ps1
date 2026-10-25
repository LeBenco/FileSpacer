function Assert-SearchNames([IntPtr]$Window,[string[]]$Names) {
    [string[]]$orderedNames=@($Names)
    [Array]::Sort($orderedNames,[StringComparer]::OrdinalIgnoreCase)
    $expected=$orderedNames -join ';'
    $observed=@{Names='';Attempt=0;NativeMilliseconds=0}
    try {
        Wait-Ui {
            ++$observed.Attempt
            $timing=$env:FS_SEARCH_TIMINGS -eq '1'
            if ($timing) {
                $watch=[Diagnostics.Stopwatch]::StartNew()
                $observed.NativeMilliseconds=0
                $record=@{Event='begin';Phase=$env:FS_SEARCH_PHASE;Attempt=$observed.Attempt;ExpectedCount=$orderedNames.Count;Utc=[DateTime]::UtcNow.ToString('o')}
                try { [IO.File]::AppendAllText((Join-Path $env:FS_REGRESSION_RUN 'search-observations.jsonl'),($record | ConvertTo-Json -Compress)+"`r`n") }
                catch { Write-Warning ('Cannot record search timing: '+$_.Exception.Message) }
            }
            $readCompleted=$false
            try {
                $observed.Names=(Use-UiView $Window {
                    param($view)
                    if (!$timing) { return $view.Names() }
                    $nativeWatch=[Diagnostics.Stopwatch]::StartNew()
                    try { $view.Names() } finally { $observed.NativeMilliseconds=$nativeWatch.ElapsedMilliseconds }
                }) -join ';'
                $readCompleted=$true
            } finally {
                if ($timing) {
                    $watch.Stop()
                    $record=@{Event='end';Phase=$env:FS_SEARCH_PHASE;Attempt=$observed.Attempt;ExpectedCount=$orderedNames.Count;
                        TotalMilliseconds=$watch.ElapsedMilliseconds;NativeNamesMilliseconds=$observed.NativeMilliseconds;
                        ReadCompleted=$readCompleted;IdentitiesMatch=($readCompleted -and $observed.Names -ceq $expected);Utc=[DateTime]::UtcNow.ToString('o')}
                    try { [IO.File]::AppendAllText((Join-Path $env:FS_REGRESSION_RUN 'search-observations.jsonl'),($record | ConvertTo-Json -Compress)+"`r`n") }
                    catch { Write-Warning ('Cannot record search timing: '+$_.Exception.Message) }
                }
            }
            return $observed.Names -ceq $expected
        } ('visible search items: '+$expected) 400
    } catch {
        Write-Host ('DIAG: expected=['+$expected+'], observed=['+$observed.Names+']')
        throw
    }
}
