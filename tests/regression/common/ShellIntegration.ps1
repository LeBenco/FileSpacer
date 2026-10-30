. (Join-Path $PSScriptRoot 'EssentialUi.ps1')
. (Join-Path $PSScriptRoot 'SearchUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'ShellIntegration.cs')

function Focus-ShellFixture([IntPtr]$Window,[string]$Name='') {
    Activate-Ui $Window
    $viewWindow=Use-UiView $Window { param($view) if ($Name) { $view.Select($Name) }; $view.Focus(); $view.Window }
    Wait-Ui { [FileSpacerRegression.ShellIntegration]::ViewFocused($Window,$viewWindow) } 'keyboard focus inside the Shell view'
}
function Start-ShellRename([IntPtr]$Window,[string]$Name) {
    Focus-ShellFixture $Window $Name
    $viewWindow=Use-UiView $Window { param($view) $view.Window }
    [FileSpacerRegression.Native]::Keys($Window,0x71,$false,$false)
    Wait-Ui { [FileSpacerRegression.ShellIntegration]::RenameFocused($Window,$viewWindow) } 'native in-place rename editor inside the Shell view'
    return $viewWindow
}
function Set-ShellRenameText([IntPtr]$Window,[IntPtr]$ViewWindow,[string]$Text) {
    try {
        [FileSpacerRegression.ShellIntegration]::ReplaceRenameText($Window,$ViewWindow,$Text)
        Wait-Ui {
            if (![FileSpacerRegression.ShellIntegration]::RenameFocused($Window,$ViewWindow)) { return $false }
            $edit=[FileSpacerRegression.Native]::FocusedWindowInOwningThread($Window)
            return [FileSpacerRegression.Native]::Text($edit) -ceq $Text
        } 'replacement text consumed by the native rename editor'
    }
    catch {
        $failure=$_
        try {
            $focus=[FileSpacerRegression.Native]::FocusedWindowInOwningThread($Window)
            $foreground=[FileSpacerRegression.Native]::GetForegroundWindow()
            Write-Host "RENAME DIAGNOSTIC: owner=$Window view=$ViewWindow foreground=$foreground ($([FileSpacerRegression.Native]::Class($foreground))) focus=$focus ($([FileSpacerRegression.Native]::Class($focus))) active=$([FileSpacerRegression.Native]::ActiveWindowInOwningThread($Window))"
        } catch { Write-Host "RENAME DIAGNOSTIC unavailable: $($_.Exception.Message)" }
        throw $failure
    }
}
function Rename-ShellFixture([IntPtr]$Window,[string]$Folder,[string]$Before,[string]$After) {
    $viewWindow=Start-ShellRename $Window $Before
    Set-ShellRenameText $Window $viewWindow $After
    [FileSpacerRegression.Native]::Keys($Window,0x0d,$false,$false)
    Wait-Ui { ![FileSpacerRegression.ShellIntegration]::RenameFocused($Window,$viewWindow) } 'native rename editor closed after Enter'
    Wait-Ui { !(Test-Path -LiteralPath (Join-Path $Folder $Before)) -and (Test-Path -LiteralPath (Join-Path $Folder $After)) } 'rename changes the filesystem through the real UI'
    Assert-UiSelected $Window $After
}
function Remove-ShellFixture([IntPtr]$Window,[string]$Folder,[string]$Name) {
    $target=Join-Path $Folder $Name
    Assert-Ui ($target.StartsWith($script:UiRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Deletion target is outside the test fixture.'
    Focus-ShellFixture $Window $Name
    [FileSpacerRegression.ShellIntegration]::Shortcut($Window,0x2e,$false,$true)
    $confirmation=@{Sent=$false}
    Wait-Ui {
        if (!(Test-Path -LiteralPath $target)) { return $true }
        $ownerId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$ownerId)
        foreach ($dialog in [FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$ownerId)) {
            if (![FileSpacerRegression.ShellIntegration]::OwnedBy($dialog,$Window)) { continue }
            $yes=[FileSpacerRegression.Native]::GetDlgItem($dialog,6)
            if ($yes -ne [IntPtr]::Zero -and !$confirmation.Sent) {
                $confirmation.Sent=$true
                Assert-Ui ([FileSpacerRegression.Native]::PostMessage($yes,0xf5,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot accept the fixture deletion confirmation.'
            }
        }
        return $false
    } 'Shift+Delete removes only the selected fixture item'
}
function Open-ShellNamespace([string]$ParsingName) {
    $canonical=[FileSpacerRegression.ShellIntegration]::CanonicalName($ParsingName)
    $application=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$ParsingName+'"') -OutputDirectory $env:FS_REGRESSION_RUN
    [void]$script:UiProcesses.Add($application)
    $found=@{Window=[IntPtr]::Zero}
    $observed=@{}
    try {
        Wait-Ui {
            foreach ($candidate in [FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,0)) {
                $ownerId=[uint32]0
                [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($candidate,[ref]$ownerId)
                if ($ownerId -ne $application.Id) { continue }
                # Track our window before its Shell view is ready, including a
                # failed/redirected navigation, so cleanup can still close it.
                if (!$script:UiWindows.Contains($candidate)) { [void]$script:UiWindows.Add($candidate) }
                $entry=@{Window=$candidate.ToInt64();Title=[FileSpacerRegression.Native]::Text($candidate);ParsingName=$null}
                $observed[$candidate.ToInt64()]=$entry
                $probe=Get-UiView $candidate
                if ($null -eq $probe) { continue }
                try {
                    $entry.ParsingName=$probe.FolderParsingName
                    if ([string]::Equals($entry.ParsingName,$canonical,[StringComparison]::OrdinalIgnoreCase)) { $found.Window=$candidate;return $true }
                } finally { $probe.Dispose() }
            }
            return $false
        } ('registered Shell namespace '+$canonical)
    } catch {
        $failure=$_
        try {
            Save-ShellCoverage 'namespace-failure.json' @{Requested=$ParsingName;Canonical=$canonical;ProcessId=$application.Id;Candidates=@($observed.Values);Error=$failure.Exception.Message}
            foreach ($entry in $observed.Values) { Save-ShellScreenshot ([IntPtr]$entry.Window) ('namespace-failure-'+$entry.Window+'.png') -WithoutActivation }
        } catch { Write-Warning ('Cannot collect namespace diagnostics: '+$_.Exception.Message) }
        throw $failure
    }
    Register-UiWindowProcess $found.Window
    Wait-Ui { (Use-UiView $found.Window {param($view) $view.Count}) -gt 0 } 'nonempty native namespace' 500
    return $found.Window
}
function Save-ShellCoverage([string]$Name,$Value) {
    $Value | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN $Name) -Encoding UTF8
}
function Save-ShellScreenshot([IntPtr]$Window,[string]$Name,[switch]$WithoutActivation) {
    Add-Type -AssemblyName System.Drawing
    if (!$WithoutActivation) { Activate-Ui $Window }
    $bounds=[FileSpacerRegression.Native]::Bounds($Window)
    $bitmap=New-Object Drawing.Bitmap -ArgumentList $bounds.Width,$bounds.Height
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left,$bounds.Top,0,0,$bitmap.Size)
        $bitmap.Save((Join-Path $env:FS_REGRESSION_RUN $Name),[Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose();$bitmap.Dispose() }
}
