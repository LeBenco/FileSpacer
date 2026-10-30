param([string]$Ready,[string]$Finish)
$ErrorActionPreference='Stop'
$gate=New-Object Threading.Mutex($false,'Local\FileSpacer.Test.Preferences')
try {
    $owned=$false
    try{$owned=$gate.WaitOne(10000)}catch [Threading.AbandonedMutexException]{$owned=$true}
    if(!$owned){throw 'The external holder did not acquire the reservation.'}
    [IO.File]::WriteAllText($Ready,'owned')
    $deadline=[DateTime]::UtcNow.AddSeconds(20)
    while(!(Test-Path -LiteralPath $Finish)) {
        if([DateTime]::UtcNow -gt $deadline){throw 'Mutex holder timed out.'}
        Start-Sleep -Milliseconds 20
    }
    # Exit without ReleaseMutex to exercise native abandoned ownership.
} finally {$gate.Dispose()}
