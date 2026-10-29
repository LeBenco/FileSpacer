$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
if (!$env:FS_REGRESSION_RUN) { throw 'Run these UI tests through tests.cmd / Run-Regression.ps1 so the test registry and database are backed up.' }
. (Join-Path $PSScriptRoot 'Process.ps1')
. (Join-Path $PSScriptRoot 'Native.ps1')
if ($env:FS_REGRESSION_PREPARED) { Add-Type -Path (Join-Path $PSScriptRoot 'EssentialUi.cs') }
else {
    # Windows PowerShell compiles in memory (no assembly location). Keep the
    # helper on disk in the run directory so case helpers can reference it.
    $script:UiAssembly=Join-Path $env:FS_REGRESSION_RUN 'FileSpacerRegression.dll'
    if (!(Test-Path -LiteralPath $script:UiAssembly)) { Add-Type -Path (Join-Path $PSScriptRoot 'EssentialUi.cs') -OutputAssembly $script:UiAssembly }
    Add-Type -Path $script:UiAssembly
}
$script:UiProcesses=New-Object Collections.ArrayList
$script:UiWindows=New-Object Collections.ArrayList
$script:UiSettings=[IntPtr]::Zero
$script:UiRoot=$null
$script:UiShell=$null; $script:UiShellWindows=$null
$script:UiPreviousDpi=[IntPtr]::Zero
$script:UiExecutable=Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
$script:UiSettingsKey='Software\FileSpacer\Test'

function Assert-Ui($Condition,[string]$Message) { if (!$Condition) { throw $Message } }
function Wait-Ui([scriptblock]$Condition,[string]$Description,[int]$StableMilliseconds=0,[int]$Seconds=25) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds); $since=$null
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) {
            if ($null -eq $since) { $since=[DateTime]::UtcNow }
            if (([DateTime]::UtcNow-$since).TotalMilliseconds -ge $StableMilliseconds) { return }
        } else { $since=$null }
        Start-Sleep -Milliseconds 50
    }
    throw "Timed out: $Description"
}
function Get-UiId([string]$Name) {
    $text=(Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\resource.h') -Raw)+"`n"+
        (Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\dialog.h') -Raw)
    $match=[regex]::Match($text,'(?m)^\s*#define\s+'+[regex]::Escape($Name)+'\s+(0x[0-9a-fA-F]+|[0-9]+)\b')
    if (!$match.Success) { throw "Missing current resource identifier: $Name" }
    return [Convert]::ToInt32($match.Groups[1].Value,$(if ($match.Groups[1].Value.StartsWith('0x')) {16} else {10}))
}
function Set-UiPreference([string]$Name,[int]$Value) {
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($script:UiSettingsKey)
    try { $key.SetValue($Name,$Value,[Microsoft.Win32.RegistryValueKind]::DWord) } finally { $key.Dispose() }
}
function Get-UiPreference([string]$Name) {
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($script:UiSettingsKey)
    try { return $key.GetValue($Name,$null) } finally { $key.Dispose() }
}
function Initialize-Ui([string]$Name,[switch]$UseApplicationDefaults,[string]$FixtureParent=$env:TEMP) {
    Assert-Ui ([Environment]::Is64BitProcess -and $env:OS -eq 'Windows_NT') 'Windows x64 is required.'
    $script:UiPreviousDpi=[FileSpacerRegression.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
    Assert-Ui ($script:UiPreviousDpi -ne [IntPtr]::Zero) 'Per-monitor DPI context could not be set.'
    $script:UiRoot=Join-Path $FixtureParent ('FileSpacer-'+$Name+'-'+[Guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $script:UiRoot)
    if (!$UseApplicationDefaults) {
        foreach ($name in @('StatusTextEnabled','ToolbarEnabled','QuickAccessEnabled','PathBarEnabled',
                'RefreshButtonEnabled','UpButtonEnabled','ViewButtonEnabled','KeepSelectionOnActivate')) { Set-UiPreference $name 1 }
        foreach ($name in @('LiveNameSearch','FullNamesOnSelection')) { Set-UiPreference $name 0 }
        # These are fixture defaults in the runner's isolated /test namespace.
        Set-UiPreference 'FolderWindow2Width' 800; Set-UiPreference 'FolderWindow2Height' 500
    }
    $script:UiShell=New-Object -ComObject Shell.Application
    $script:UiShellWindows=$script:UiShell.Windows()
}
function Find-UiFolder([string]$Path) {
    return @([FileSpacerRegression.Native]::Top('FileSpacer Item Window',[IO.Path]::GetFileName($Path),0))
}
function Get-UiView([IntPtr]$Window) {
    for ($i=0; $i -lt $script:UiShellWindows.Count; ++$i) {
        $registered=$script:UiShellWindows.Item($i)
        if ($null -eq $registered) { continue }
        try {
            $view=[FileSpacerRegression.Native]::View($registered,$Window)
            if ($null -ne $view) { return $view }
        } finally { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($registered) }
    }
    return $null
}
function Use-UiView([IntPtr]$Window,[scriptblock]$Action) {
    $view=Get-UiView $Window
    if ($null -eq $view) { throw 'The application did not expose its registered native Shell view.' }
    try { & $Action $view } finally { $view.Dispose() }
}
function Register-UiWindowProcess([IntPtr]$Window) {
    $windowProcessId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$windowProcessId)
    Assert-Ui ($windowProcessId -ne 0) 'Cannot identify the folder window process.'
    if (@($script:UiProcesses | Where-Object { $_.Id -eq $windowProcessId -and !$_.HasExited }).Count) { return }
    $windowProcess=[Diagnostics.Process]::GetProcessById([int]$windowProcessId)
    try {
        # Retain a process handle so its exit code remains observable after exit.
        [void]$windowProcess.Handle
        Assert-Ui ($windowProcess.Path -eq $script:UiExecutable) 'Folder window belongs to another executable.'
        [void]$script:UiProcesses.Add($windowProcess)
    } catch { $windowProcess.Dispose(); throw }
}
function Open-UiFolder([string]$Path,[int]$ExpectedItems=-1) {
    $process=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$Path+'"') -OutputDirectory $env:FS_REGRESSION_RUN
    [void]$script:UiProcesses.Add($process)
    Write-Host "STEP: opening $Path, PID=$($process.Id), expected items=$ExpectedItems"
    Wait-Ui { @(Find-UiFolder $Path).Count -eq 1 } "folder creation: $Path"
    $window=(Find-UiFolder $Path)[0]; [void]$script:UiWindows.Add($window)
    Register-UiWindowProcess $window
    Wait-Ui { [FileSpacerRegression.Native]::IsWindowVisible($window) } 'visible folder'
    Wait-Ui {
        $view=Get-UiView $window
        if ($null -eq $view) { return $false }
        try { return $view.ContentVisible -and ($ExpectedItems -lt 0 -or $view.Count -eq $ExpectedItems) } finally { $view.Dispose() }
    } 'registered Shell view and folder enumeration' 300
    Write-Host "STEP: Shell view ready for $Path, HWND=$window"
    return $window
}
function Track-UiFolder([string]$Path) {
    Write-Host "STEP: waiting for navigation target $Path"
    Wait-Ui { @(Find-UiFolder $Path).Count -eq 1 } "navigation target: $Path"
    $window=(Find-UiFolder $Path)[0]
    if (!$script:UiWindows.Contains($window)) { [void]$script:UiWindows.Add($window) }
    Register-UiWindowProcess $window
    Wait-Ui {
        if (![FileSpacerRegression.Native]::IsWindowVisible($window)) { return $false }
        $view=Get-UiView $window
        if ($null -eq $view) { return $false }
        try { return $view.ContentVisible -and $view.Count -ge 0 } finally { $view.Dispose() }
    } 'visible navigation target with registered Shell view' 300
    Write-Host "STEP: navigation target ready, HWND=$window"
    return $window
}
function Activate-Ui([IntPtr]$Window) {
    # Prefer the direct API; use an actual caption click when Windows denies foreground transfer.
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($Window)) 'Activation target disappeared.'
    try {
        $accepted=[FileSpacerRegression.Native]::Activate($Window)
        Write-Host "STEP: foreground request target=$Window, accepted=$accepted, current=$([FileSpacerRegression.Native]::GetForegroundWindow())"
        try {
            Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $Window } 'direct foreground activation' 0 1
            return
        } catch {
            $point=[FileSpacerRegression.Native]::ActivateByCaptionClick($Window)
            Write-Host "STEP: activation click sent to caption of HWND=$Window at ($($point.X),$($point.Y))"
        }
    } catch {
        $reason="Cannot prepare activation of HWND=$Window : $($_.Exception.Message)"
        @{Kind='ForegroundPrerequisite'; Reason=$reason} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'blocked.json') -Encoding UTF8
        throw "BLOCKED: $reason"
    }
    # A verified click was sent: failure to activate is now an observed behavior failure.
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $Window } 'foreground after verified caption click' 0 5
    Write-Host "STEP: activation confirmed for HWND=$Window"
}
function Close-UiFolder([IntPtr]$Window) {
    if ([FileSpacerRegression.Native]::IsWindow($Window)) {
        # Do not act on a recycled HWND now belonging to another window/process.
        if ([FileSpacerRegression.Native]::Class($Window) -ne 'FileSpacer Item Window') { return }
        $processId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$processId)
        $process=Get-Process -Id $processId -ErrorAction SilentlyContinue
        if ($null -eq $process -or $process.Path -ne $script:UiExecutable) { return }
        Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'WM_CLOSE failed.'
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($Window) } 'folder close'
    }
}
function Show-UiSettings([IntPtr]$Owner) {
    Write-Host "STEP: opening preferences from HWND=$Owner"
    # Post the command directly; the owner need not be foreground first.
    # BM_CLICK requires an active dialog, not global foreground ownership.
    $processId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Owner,[ref]$processId)
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Owner,0x111,[IntPtr](Get-UiId 'IDM_SETTINGS'),[IntPtr]::Zero)) 'Settings command failed.'
    Wait-Ui {
        # A plain $null becomes an empty string at a .NET string parameter.
        $found=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId) | Where-Object {
            @([FileSpacerRegression.Native]::Children($_,'SysTabControl32')).Count -eq 1
        })
        if ($found.Count -eq 1) { $script:UiSettings=$found[0]; return $true }
        return $false
    } 'native settings property sheet'
    Write-Host "STEP: preferences dialog HWND=$script:UiSettings, thread active HWND=$([FileSpacerRegression.Native]::ActiveWindowInOwningThread($script:UiSettings)), foreground HWND=$([FileSpacerRegression.Native]::GetForegroundWindow())"
    Wait-Ui {
        [FileSpacerRegression.Native]::ActiveWindowInOwningThread($script:UiSettings) -eq $script:UiSettings
    } 'active settings dialog in its owning GUI thread' 0 5
    return $script:UiSettings
}
function Set-UiPage([int]$Index) {
    Write-Host "STEP: preferences page $Index"
    Assert-Ui ([FileSpacerRegression.Native]::Message($script:UiSettings,0x465,$Index,0) -ne 0) 'Cannot select property page.'
    $page=[IntPtr][FileSpacerRegression.Native]::Message($script:UiSettings,0x476,0,0)
    Assert-Ui ($page -ne [IntPtr]::Zero) 'No current property page.'
    return $page
}
function Set-UiCheck([IntPtr]$Page,[string]$Name,[bool]$Value) {
    $control=[FileSpacerRegression.Native]::GetDlgItem($Page,(Get-UiId $Name))
    Assert-Ui ($control -ne [IntPtr]::Zero) "Missing preference control: $Name"
    $wanted=[int]$Value
    if ([FileSpacerRegression.Native]::Message($control,0xf0,0,0) -ne $wanted) {
        Write-Host "STEP: preference $Name = $Value"
        [void][FileSpacerRegression.Native]::Message($control,0xf5,0,0)
    }
    Assert-Ui ([FileSpacerRegression.Native]::Message($control,0xf0,0,0) -eq $wanted) "Preference click did not take effect: $Name"
}
function Press-UiSettings([int]$Button) {
    Write-Host "STEP: property sheet button $Button (OK=3, Apply=4, Cancel=5)"
    $sheet=$script:UiSettings
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($sheet,0x471,[IntPtr]$Button,[IntPtr]::Zero)) 'Property sheet button failed.'
    if ($Button -in @(3,5)) {
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($sheet) } 'settings close'
        $script:UiSettings=[IntPtr]::Zero
    }
}
function Get-UiControl([IntPtr]$Window,[string]$Class) {
    $found=@([FileSpacerRegression.Native]::Children($Window,$Class))
    Assert-Ui ($found.Count -eq 1) "Expected one $Class control, found $($found.Count)."
    return $found[0]
}
function Get-UiCommandToolbar([IntPtr]$Window) {
    $id=Get-UiId 'IDC_NAME_SEARCH'
    $found=@([FileSpacerRegression.Native]::Children($Window,'ToolbarWindow32') | Where-Object {
        [FileSpacerRegression.Native]::Message($_,0x419,$id,0) -ge 0
    })
    Assert-Ui ($found.Count -eq 1) 'Cannot identify the application command toolbar.'
    return $found[0]
}
function Get-UiSearchEdit([IntPtr]$Window) { return Get-UiControl (Get-UiControl $Window 'FileSpacer Search Box') 'Edit' }
function Get-UiSearchSubmit([IntPtr]$Window) {
    $search=Get-UiControl $Window 'FileSpacer Search Box'
    # The pre-integration build has one unnamed button. This permits comparisons
    # with that build using the same assertions and fixture, without a new harness.
    $buttons=@([FileSpacerRegression.Native]::Children($search,'Button'))
    if ($buttons.Count -eq 1) { return $buttons[0] }
    $button=[FileSpacerRegression.Native]::GetDlgItem($search,(Get-UiId 'IDC_SEARCH_SUBMIT'))
    Assert-Ui ($button -ne [IntPtr]::Zero) 'Search submit button is missing.'
    return $button
}
function Assert-UiVisible([IntPtr]$Window,[string]$Class,[bool]$Visible) {
    $control=Get-UiControl $Window $Class
    Wait-Ui { [FileSpacerRegression.Native]::IsWindowVisible($control) -eq $Visible } "$Class visibility = $Visible" 200
}
function Assert-UiSelected([IntPtr]$Window,[string]$Name) {
    Wait-Ui {
        $view=Get-UiView $Window; if ($null -eq $view) { return $false }
        try { return ($view.Selected() -join ';') -eq $Name } finally { $view.Dispose() }
    } "stable selection: $Name" 1000
}
function Stop-Ui($Failure=$null) {
    $cleanupErrors=New-Object Collections.Generic.List[string]
    try {
        if ($script:UiSettings -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:UiSettings)) {
            try { Press-UiSettings 5 } catch { $cleanupErrors.Add($_.Exception.Message) }
        }
        foreach ($window in $script:UiWindows) {
            try { Close-UiFolder $window } catch { $cleanupErrors.Add($_.Exception.Message) }
        }
        try { Complete-TestApplications $script:UiProcesses }
        catch { $cleanupErrors.Add($_.Exception.Message) }
    } finally {
        foreach ($com in @($script:UiShellWindows,$script:UiShell)) {
            if ($null -ne $com) {
                try { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($com) }
                catch { $cleanupErrors.Add($_.Exception.Message) }
            }
        }
        if ($script:UiPreviousDpi -ne [IntPtr]::Zero) { [void][FileSpacerRegression.Native]::SetThreadDpiAwarenessContext($script:UiPreviousDpi) }
        if ($script:UiRoot -and (Test-Path -LiteralPath $script:UiRoot)) {
            try { Remove-Item -LiteralPath $script:UiRoot -Recurse -Force }
            catch { $cleanupErrors.Add($_.Exception.Message) }
        }
    }
    if ($cleanupErrors.Count) {
        # A crash/cleanup failure must not be hidden by an earlier BLOCKED marker.
        try {
            ConvertTo-Json -InputObject @($cleanupErrors.ToArray()) |
                Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'cleanup-errors.json') -Encoding UTF8
        } catch { Write-Warning ('Cannot record cleanup errors: '+$_.Exception.Message) }
        if ($null -ne $Failure) { Write-Warning ('Cleanup after initial failure: '+($cleanupErrors -join '; ')) }
        else { throw ($cleanupErrors -join '; ') }
    } else {
        @{Succeeded=$true; ProcessCount=$script:UiProcesses.Count} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'cleanup-complete.json') -Encoding UTF8
    }
}
