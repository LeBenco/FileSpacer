# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\SearchUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'close-after-command'
    Set-UiPreference 'LiveNameSearch' 0
    foreach ($scenario in @('refresh','filter','empty-filter','clear-filter')) {
        foreach ($iteration in 1..2) {
            $name='Close-'+$scenario+'-'+$iteration
            $folder=Join-Path $script:UiRoot $name
            [void][IO.Directory]::CreateDirectory($folder)
            [IO.File]::WriteAllText((Join-Path $folder 'keep.txt'),'fixture')
            foreach ($index in 1..255) { [IO.File]::WriteAllText((Join-Path $folder ('other-'+$index+'.txt')),'fixture') }
            $window=Open-UiFolder $folder 256
            Activate-Ui $window
            $edit=Get-UiSearchEdit $window
            $button=Get-UiControl (Get-UiControl $window 'FileSpacer Search Box') 'Button'
            if ($scenario -eq 'clear-filter') {
                [FileSpacerRegression.Native]::SetText($edit,'keep')
                [void][FileSpacerRegression.Native]::Message($button,0xf5,0,0)
                Assert-SearchNames $window @('keep.txt')
            }
            if ($scenario -ne 'refresh') {
                $query=switch ($scenario) { 'filter' {'keep'} 'empty-filter' {'no-match-unique'} 'clear-filter' {''} }
                [FileSpacerRegression.Native]::SetText($edit,$query)
            }
            Write-Host ('STEP: '+$scenario+', iteration '+$iteration+'; queue one command then WM_CLOSE without waiting for its result')
            if ($scenario -eq 'refresh') {
                Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr](Get-UiId 'IDM_REFRESH'),[IntPtr]::Zero)) 'Refresh command could not be queued.'
            } else {
                Assert-Ui ([FileSpacerRegression.Native]::PostMessage($button,0xf5,[IntPtr]::Zero,[IntPtr]::Zero)) 'Search submit could not be queued.'
            }
            Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Following WM_CLOSE could not be queued.'
            Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($window) } 'folder closes after queued command'
            Wait-Ui { @($script:UiProcesses | Where-Object { !$_.HasExited }).Count -eq 0 } 'all tracked application processes exit after queued command'
            foreach ($applicationProcess in $script:UiProcesses) {
                Assert-Ui ($applicationProcess.ExitCode -eq 0) ('Close after command crashed: PID '+$applicationProcess.Id+', exit '+$applicationProcess.ExitCode)
            }
            Write-Host ('PASS: normal application exit after queued '+$scenario+', iteration '+$iteration)
        }
    }
    Write-Host 'PASS: eight queued refresh/search/clear commands followed by closure exit normally. Internal enumeration timing is not assumed.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
