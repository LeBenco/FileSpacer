# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
function Post-SystemCommand([IntPtr]$Window,[int]$Command) {
    Write-Host ('STEP: queued WM_SYSCOMMAND 0x{0:X}, HWND={1}' -f $Command,$Window)
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Window,0x112,[IntPtr]$Command,[IntPtr]::Zero)) 'Cannot post WM_SYSCOMMAND.'
}
function Assert-WindowState([IntPtr]$Window,[bool]$Maximized,[bool]$Minimized) {
    try {
        Wait-Ui {
            $style=[FileSpacerRegression.Native]::Style($Window,-16)
            return (($style -band 0x01000000) -ne 0) -eq $Maximized -and
                (($style -band 0x20000000) -ne 0) -eq $Minimized
        } "queued command result: maximized=$Maximized, minimized=$Minimized" 200 15
    } catch {
        Write-Host ('DIAG: HWND={0}, style=0x{1:X8}, foreground={2}' -f $Window,
            [FileSpacerRegression.Native]::Style($Window,-16),[FileSpacerRegression.Native]::GetForegroundWindow())
        throw
    }
}
try {
    Initialize-Ui 'queued-window-commands'
    $folder=Join-Path $script:UiRoot 'QueuedCommands'
    [void](New-Item -ItemType Directory -Path $folder)
    [IO.File]::WriteAllText((Join-Path $folder 'item.txt'),'fixture')
    $window=Open-UiFolder $folder 1
    # Establish the starting state through the same queued path being tested.
    Post-SystemCommand $window 0xf120 # SC_RESTORE
    Assert-WindowState $window $false $false
    Post-SystemCommand $window 0xf030 # SC_MAXIMIZE
    Assert-WindowState $window $true $false
    Post-SystemCommand $window 0xf020 # SC_MINIMIZE
    Wait-Ui { ([FileSpacerRegression.Native]::Style($window,-16) -band 0x20000000) -ne 0 } 'queued minimize' 200 15
    Post-SystemCommand $window 0xf120
    Assert-WindowState $window $true $false
    Post-SystemCommand $window 0xf120
    Assert-WindowState $window $false $false
    Write-Host 'PASS: queued maximize, minimize and restore reach the native window procedure.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
