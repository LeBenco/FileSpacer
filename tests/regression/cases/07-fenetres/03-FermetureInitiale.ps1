# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'early-close'
    foreach ($iteration in 1..4) {
        $name='EarlyClose-'+$iteration+'-'+[Guid]::NewGuid().ToString('N')
        $folder=Join-Path $script:UiRoot $name
        [void][IO.Directory]::CreateDirectory($folder)
        foreach ($index in 1..32) { [IO.File]::WriteAllText((Join-Path $folder ($index.ToString()+'.txt')),'fixture') }
        $application=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$folder+'"')
        [void]$script:UiProcesses.Add($application)
        $found=@{Window=[IntPtr]::Zero}
        # Observe only the native host. Waiting for the Shell view would hide
        # precisely the early-close lifetime defect this scenario exercises.
        Wait-Ui {
            $windows=@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',$name,[uint32]$application.Id))
            if ($windows.Count -ne 1) { return $false }
            $found.Window=$windows[0]; return $true
        } 'early native folder window'
        $window=$found.Window
        [void]$script:UiWindows.Add($window)
        Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Early WM_CLOSE failed.'
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($window) } 'early folder closure'
        Wait-Ui { $application.HasExited } 'early-close process exit'
        Assert-Ui ($application.ExitCode -eq 0) ('Early close crashed: PID '+$application.Id+', exit '+$application.ExitCode)
    }
    Write-Host 'PASS: four early folder closures exit normally with the production native Shell view.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
