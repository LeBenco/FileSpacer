# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
$ownedPanels=New-Object Collections.ArrayList
$panelPaths=@()
$observedShellWindows=@{}
$preexistingShellWindows=@()

function Get-ExternalPanels {
    for ($index=0; $index -lt $script:UiShellWindows.Count; ++$index) {
        $registered=$null; $entry=$null
        try {
            $registered=$script:UiShellWindows.Item($index)
            if ($null -eq $registered) { continue }
            $window=[IntPtr]([long]$registered.HWND)
            if ($window -eq [IntPtr]::Zero) { continue }
            if ([FileSpacerRegression.Native]::Class($window) -eq 'FileSpacer Item Window') { continue }
            # Neither Document.Folder nor LocationURL is required for a virtual
            # folder: ask the active Shell view for its actual folder identity.
            $entry=@{Window=$window.ToInt64();ParsingName='';Error='';HResult='';
                Title=[FileSpacerRegression.Native]::Text($window);
                Class=[FileSpacerRegression.Native]::Class($window);
                Visible=[FileSpacerRegression.Native]::IsWindowVisible($window)}
            $observedShellWindows[$window.ToInt64()]=$entry
            $path=[FileSpacerRegression.Native]::ActiveFolderParsingName($registered,$window)
            if ([string]::IsNullOrEmpty($path)) { continue }
            $entry.ParsingName=$path
            if (@($panelPaths | Where-Object { [string]::Equals($_,$path,[StringComparison]::OrdinalIgnoreCase) }).Count -eq 0) { continue }
            [pscustomobject]@{Window=$window;ParsingName=$path;Visible=[FileSpacerRegression.Native]::IsWindowVisible($window)}
        } catch [Runtime.InteropServices.COMException] {
            # A window may disappear or not yet expose its active folder.
            if ($null -ne $entry) {
                $entry.Error=$_.Exception.Message
                $entry.HResult='0x'+$_.Exception.HResult.ToString('X8')
            }
        } finally {
            if ($null -ne $registered -and [Runtime.InteropServices.Marshal]::IsComObject($registered)) {
                [void][Runtime.InteropServices.Marshal]::ReleaseComObject($registered)
            }
        }
    }
}
function Wait-ExternalPanel([string]$Step) {
    $state=@{Window=[IntPtr]::Zero;Path=''}
    try { Wait-Ui {
        $panels=@(Get-ExternalPanels | Where-Object { $preexistingShellWindows -notcontains $_.Window.ToInt64() })
        foreach ($entry in $panels) {
            if (!$ownedPanels.Contains($entry.Window)) { [void]$ownedPanels.Add($entry.Window) }
        }
        if ($panels.Count -ne 1 -or !$panels[0].Visible) { return $false }
        $state.Window=$panels[0].Window; $state.Path=$panels[0].ParsingName
        return $true
    } ('visible external Control Panel: '+$Step) 500 } catch {
        Save-ShellCoverage ($Step+'-failure.json') @{Windows=@($observedShellWindows.Values);Error=$_.Exception.Message}
        throw
    }
    $ownerId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($state.Window,[ref]$ownerId)
    $owner=[Diagnostics.Process]::GetProcessById([int]$ownerId)
    try {
        Assert-Ui (![string]::Equals($owner.Path,$script:UiExecutable,[StringComparison]::OrdinalIgnoreCase)) 'Control Panel is still hosted by FileSpacer.'
        Save-ShellCoverage ($Step+'.json') @{Window=$state.Window.ToInt64();ParsingName=$state.Path;Owner=$owner.Path}
    } finally { $owner.Dispose() }
    return $state.Window
}
function Close-ExternalPanel([IntPtr]$Window) {
    Assert-Ui ($ownedPanels.Contains($Window)) 'Refusing to close an untracked Shell window.'
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot close the external Control Panel test window.'
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($Window) } 'external test panel closes'
    [void]$ownedPanels.Remove($Window)
}

try {
    Initialize-Ui 'control-panel-external'
    $targets=@('shell:::{21EC2020-3AEA-1069-A2DD-08002B30309D}',
        'shell:::{26EE0668-A00A-44D7-9371-BEB064C98683}',
        'shell:::{26EE0668-A00A-44D7-9371-BEB064C98683}\0', 'shell:ControlPanelFolder')
    $panelPaths=@($targets | ForEach-Object { [FileSpacerRegression.ShellIntegration]::CanonicalName($_) } | Select-Object -Unique)
    if (@(Get-ExternalPanels).Count) { throw 'BLOCKED: Close existing Control Panel windows before this test; they will not be closed by the test.' }
    $preexistingShellWindows=@($observedShellWindows.Keys)
    Save-ShellCoverage 'before-launch.json' @{Windows=@($observedShellWindows.Values)}
    Set-UiPreference 'KeepSourceWindowOpen' 0
    # A physical folder with the same display name must remain a normal folder.
    $physical=Join-Path $script:UiRoot 'Control Panel'
    New-Item -ItemType Directory -Path $physical | Out-Null
    foreach ($name in @('AnchorA.txt','OtherB.txt','OtherC.txt')) { Set-Content -LiteralPath (Join-Path $physical $name) -Value 'fixture' }
    $source=Open-UiFolder $physical 3
    $index=0
    foreach ($target in $targets) {
        ++$index
        $application=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$target+'"') -OutputDirectory $env:FS_REGRESSION_RUN
        [void]$script:UiProcesses.Add($application)
        $panel=Wait-ExternalPanel ('direct-'+$index)
        Wait-Ui { $application.HasExited } 'external-only FileSpacer launch exits normally'
        Assert-Ui ($application.ExitCode -eq 0) 'External launch process failed.'
        Assert-Ui (@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,[uint32]$application.Id)).Count -eq 0) 'External launch left a FileSpacer folder window.'
        Assert-Ui ([FileSpacerRegression.Native]::IsWindow($source)) 'Direct external launch closed the existing folder.'
        Close-ExternalPanel $panel
    }
    foreach ($control in @($false,$true)) {
        Activate-Ui $source
        Use-UiView $source { param($view) $view.Select('AnchorA.txt'); $view.Focus() }
        [FileSpacerRegression.Native]::Keys($source,0x4c,$true,$false)
        Wait-Ui { [FileSpacerRegression.ShellIntegration]::EditFocused($source) } 'FileSpacer address editor receives focus'
        [FileSpacerRegression.ShellIntegration]::TypeText($source,$targets[0])
        [FileSpacerRegression.Native]::Keys($source,0x0d,$control,$false)
        $panel=Wait-ExternalPanel ('address-control-'+$control)
        Assert-Ui ([FileSpacerRegression.Native]::IsWindow($source)) 'Opening the panel closed its source folder.'
        Assert-UiSelected $source 'AnchorA.txt'
        Assert-Ui ((Use-UiView $source { param($view) $view.Count }) -eq 3) 'Source view was replaced by a Control Panel view.'
        Assert-Ui (@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,0)).Count -eq 1) 'Navigation left an extra FileSpacer window.'
        Close-ExternalPanel $panel
    }
    Write-Host 'PASS: four Control Panel identities open externally, direct launch exits, address navigation with/without Ctrl preserves the source and selection, and a physical Control Panel folder remains spatial.'
} catch { $testFailure=$_;throw }
finally {
    try {
        foreach ($panel in @($ownedPanels)) {
            if ([FileSpacerRegression.Native]::IsWindow($panel)) { Close-ExternalPanel $panel }
        }
    } finally { Stop-Ui -Failure $testFailure }
}
