# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null;$observations=@()
try {
    Initialize-Ui 'shell-monitors-dpi'
    $folder=Join-Path $script:UiRoot 'Monitor-fixture';[void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'Payload'),'fixture')
    $window=Open-UiFolder $folder 1
    Use-UiView $window {param($view) $view.Mode=4;$view.Select('Payload')}
    $monitors=[FileSpacerRegression.ShellIntegration]::Monitors();$ordinal=0
    Assert-Ui ($monitors.Count -gt 0) 'No monitor work area is available.'
    foreach ($monitor in $monitors) {
        $width=[Math]::Min(760,$monitor.Width-40);$height=[Math]::Min(520,$monitor.Height-40)
        Assert-Ui ($width -ge 400 -and $height -ge 300) 'Monitor is too small for the fixture window.'
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($monitor.Left+20),($monitor.Top+20),$width,$height,0x14)) 'Cannot move the real window to the monitor.'
        Assert-SearchNames $window @('Payload');Assert-UiSelected $window 'Payload'
        Wait-Ui {
            $client=[FileSpacerRegression.Native]::ClientBounds($window)
            $view=Use-UiView $window {param($probe) $probe.Bounds}
            $view.Width -gt 0 -and $view.Height -gt 0 -and $view.Left -ge $client.Left-2 -and $view.Top -ge $client.Top-2 -and $view.Right -le $client.Right+2 -and $view.Bottom -le $client.Bottom+2
        } 'native view remains inside the host client area after a real monitor move' 400
        Assert-Ui ((Use-UiView $window {param($view) $view.Mode}) -eq 4) 'A monitor move changed the details view mode.'
        $observations+=@{Monitor=$ordinal;WorkArea=$monitor;Dpi=[FileSpacerRegression.Native]::GetDpiForWindow($window)}
        Save-ShellScreenshot $window ('monitor-'+$ordinal+'.png');$ordinal++
    }
    $distinct=@($observations.Dpi | Sort-Object -Unique).Count
    Save-ShellCoverage 'coverage.json' @{Observed=$observations;DistinctDpi=$distinct;MixedDpiExercised=($distinct -gt 1);VisualAndPersistenceReview='V04 still required; no synthetic WM_DPICHANGED'}
    if ($distinct -lt 2) { Write-Host 'REVIEW COVERAGE: no transition between two different real DPIs was available. This scenario does not certify mixed DPI.' }
    Write-Host 'PASS: identities, selection, details mode and view containment survive moves over all available monitor work areas.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
