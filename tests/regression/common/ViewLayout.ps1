# Only the existing-parent game needs UI Automation. Windows PowerShell uses the built-in .NET Framework assemblies.
try {
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    Add-Type -AssemblyName WindowsBase
    $layoutReferences=@([System.Windows.Automation.AutomationElement].Assembly.Location,
        [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
    Add-Type -Path (Join-Path $PSScriptRoot 'ViewLayout.cs') -ReferencedAssemblies $layoutReferences
} catch { throw "LAYOUT MEASUREMENT: cannot load the Windows UI Automation helper: $($_.Exception.Message)" }

function Get-UiStableLayout([IntPtr]$Window,[int]$ExpectedItems,[ValidateSet('before','after')][string]$Label) {
    $viewWindow=Use-UiView $Window {param($view) $view.Window}
    $processId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$processId)
    $deadline=[DateTime]::UtcNow.AddSeconds(5); $signature=$null; $since=$null
    do {
        # Capture performs UIA work on a bounded MTA worker and returns copied values only.
        $layout=[FileSpacerRegression.ViewLayout]::Capture($viewWindow,[int]$processId,$ExpectedItems)
        $layout | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
            Join-Path $env:FS_REGRESSION_RUN ("layout-$Label.json")) -Encoding UTF8
        if($null -ne $signature -and $layout.Signature -ceq $signature) {
            if(([DateTime]::UtcNow-$since).TotalMilliseconds -ge 300) {
                Write-Host "STEP: stable UI Automation layout ($Label), relative physical pixels: $($layout.Signature)"
                return $layout
            }
        } else { $signature=$layout.Signature; $since=[DateTime]::UtcNow }
        Start-Sleep -Milliseconds 100
    } while([DateTime]::UtcNow -lt $deadline)
    throw "LAYOUT MEASUREMENT: no stable item positions within 5 seconds ($Label); inspect layout-$Label.json."
}

function Assert-UiLayoutUnchanged($Before,$After) {
    Assert-Ui ($Before.Signature -ceq $After.Signature) (
        "Reactivation changed icon positions. Before: $($Before.Signature). After: $($After.Signature). See layout-before.json and layout-after.json.")
}

# Fixture preparation, before selection and the first layout capture. Never called after Alt+Up.
function Initialize-UiParentChildWindows([IntPtr]$Parent,[IntPtr]$Child) {
    try {
        $work=[FileSpacerRegression.Native]::WorkArea($Parent)
        $gap=[Math]::Max(8,[int](12*[FileSpacerRegression.Native]::GetDpiForWindow($Parent)/96))
        $width=[int][Math]::Floor(($work.Width-3*$gap)/2)
        $height=[Math]::Min([Math]::Max([FileSpacerRegression.Native]::Bounds($Parent).Height,
            [FileSpacerRegression.Native]::Bounds($Child).Height),$work.Height-2*$gap)
        if($width -le 0 -or $height -le 0) { throw 'No usable monitor work area for the two test windows.' }
        $left=$work.Left+$gap; $top=$work.Top+$gap
        # NOZORDER|NOACTIVATE: geometry preparation does not grant foreground ownership.
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Parent,[IntPtr]::Zero,$left,$top,$width,$height,0x14)) 'Cannot place the parent test window.'
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Child,[IntPtr]::Zero,($left+$width+$gap),$top,$width,$height,0x14)) 'Cannot place the child test window.'
        Wait-Ui {
            $p=[FileSpacerRegression.Native]::Bounds($Parent); $c=[FileSpacerRegression.Native]::Bounds($Child)
            return $p.Width -gt 0 -and $c.Width -gt 0 -and $p.Height -gt 0 -and $c.Height -gt 0 -and
                $p.Left -ge $work.Left -and $c.Right -le $work.Right -and $p.Right -lt $c.Left -and
                $p.Top -ge $work.Top -and $c.Top -ge $work.Top -and $p.Bottom -le $work.Bottom -and $c.Bottom -le $work.Bottom
        } 'two non-overlapping test windows inside the monitor work area' 300 5
        $p=[FileSpacerRegression.Native]::Bounds($Parent); $c=[FileSpacerRegression.Native]::Bounds($Child)
        Write-Host "STEP: adjacent fixture windows prepared before layout baseline: parent=($($p.Left),$($p.Top),$($p.Width),$($p.Height)); child=($($c.Left),$($c.Top),$($c.Width),$($c.Height))"
    } catch {
        $reason="Cannot arrange the parent and child test windows: $($_.Exception.Message)"
        # A placement failure may also originate in the application's window
        # constraints. Keep it as FAIL rather than attributing it to foreground preparation.
        throw $reason
    }
}
