# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null;$samples=@()
try {
    Initialize-Ui 'shell-load'
    $folder=Join-Path $script:UiRoot 'Five-thousand';[void][IO.Directory]::CreateDirectory($folder)
    $expected=@(foreach ($index in 1..5000) {
        $name=('item-{0:D5}.txt' -f $index);[IO.File]::WriteAllText((Join-Path $folder $name),'load-fixture');$name
    })
    foreach ($iteration in 1..3) {
        $watch=[Diagnostics.Stopwatch]::StartNew();$window=Open-UiFolder $folder 5000;$watch.Stop()
        $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$ownerId)
        $ownerProcess=@($script:UiProcesses | Where-Object {$_.Id -eq $ownerId -and !$_.HasExited})[0]
        $ownerProcess.Refresh()
        $samples+=@{Iteration=$iteration;OpenUntilStableMilliseconds=$watch.ElapsedMilliseconds;
            PrivateBytes=$ownerProcess.PrivateMemorySize64;WorkingSetBytes=$ownerProcess.WorkingSet64;Handles=$ownerProcess.HandleCount;ProcessId=$ownerId}
        Save-ShellCoverage 'measurements.json' @{Samples=$samples;IncludesStableWaitMilliseconds=300;ComparisonToBaseline='NOT_PERFORMED';TimingIsObservation=$true}
        Assert-SearchNames $window $expected
        Activate-Ui $window
        $edit=Get-UiSearchEdit $window;$button=Get-UiControl (Get-UiControl $window 'FileSpacer Search Box') 'Button'
        foreach ($query in @('item-02500.txt','no-load-match','')) {
            [FileSpacerRegression.Native]::SetText($edit,$query)
            [void][FileSpacerRegression.Native]::Message($button,0xf5,0,0)
            if ($query -eq '') { Assert-SearchNames $window $expected }
            elseif ($query -eq 'item-02500.txt') { Assert-SearchNames $window @('item-02500.txt') }
            else { Assert-SearchNames $window @() }
        }
        Focus-ShellFixture $window 'item-02500.txt'
        [FileSpacerRegression.Native]::Keys($window,0x74,$false,$false)
        Assert-SearchNames $window $expected
        Close-UiFolder $window
        Wait-Ui { @($script:UiProcesses | Where-Object {!$_.HasExited}).Count -eq 0 } 'load sample processes exit'
        foreach ($application in $script:UiProcesses) { Assert-Ui ($application.ExitCode -eq 0) 'Load sample process crashed.' }
    }
    Write-Host 'PASS: 5000 exact identities, filtering/clearing/refresh and three process restarts. Performance samples are observations, not proof of equivalence to the baseline.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
