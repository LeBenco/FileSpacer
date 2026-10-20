# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
$Executable=Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path

# Exercise the real application using its separate Debug /test state database.
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class FileSpacerWindowTest {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x, y; }
    [StructLayout(LayoutKind.Sequential)] public struct TITLEBARINFOEX {
        public uint cbSize; public RECT title;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=6)] public uint[] states;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=6)] public RECT[] buttons;
    }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll", EntryPoint="GetWindowLongW")]
    public static extern int GetWindowLong(IntPtr window, int index);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(
        IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(
        IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", SetLastError=true)]
    private static extern IntPtr SendMessageTimeout(IntPtr window, uint message,
        UIntPtr wParam, IntPtr lParam, uint flags, uint timeout, out UIntPtr result);
    [DllImport("kernel32.dll")] private static extern void SetLastError(uint error);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll", EntryPoint="SendMessageW")]
    private static extern IntPtr SendMessage(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", EntryPoint="SendMessageW")]
    private static extern IntPtr SendTitleBarMessage(IntPtr window, uint message, UIntPtr wParam, ref TITLEBARINFOEX info);
    public static RECT ButtonRect(IntPtr window, int index) {
        TITLEBARINFOEX info = new TITLEBARINFOEX();
        info.cbSize = (uint)Marshal.SizeOf(typeof(TITLEBARINFOEX));
        info.states = new uint[6]; info.buttons = new RECT[6];
        SendTitleBarMessage(window, 0x33F, UIntPtr.Zero, ref info);
        return info.buttons[index];
    }
    public static int HitTest(IntPtr window, int x, int y) {
        int packed = unchecked((int)((uint)(ushort)x | ((uint)(ushort)y << 16)));
        return (int)SendMessage(window, 0x84, UIntPtr.Zero, new IntPtr(packed)).ToInt64();
    }
    public static IntPtr WindowAt(int x, int y) {
        POINT point = new POINT(); point.x=x; point.y=y;
        return WindowFromPoint(point);
    }
    public static void SystemCommand(IntPtr window, uint command) {
        UIntPtr result;
        SetLastError(0);
        if (SendMessageTimeout(window, 0x112, new UIntPtr(command), IntPtr.Zero,
                2, 5000, out result) == IntPtr.Zero) {
            throw new Win32Exception(Marshal.GetLastWin32Error(), "System command failed or timed out.");
        }
    }
}
'@

function Wait-Condition([scriptblock]$Condition, [string]$Description) {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    }
    throw "Timed out: $Description"
}
function Open-TestWindow {
    $process = Start-TestApplication -Executable $Executable -Arguments ('/test "' + $folder + '"')
    [void]$script:processes.Add($process)
    Wait-Condition { [FileSpacerWindowTest]::FindWindow('FileSpacer Item Window', $name) -ne [IntPtr]::Zero } 'folder creation'
    $script:window = [FileSpacerWindowTest]::FindWindow('FileSpacer Item Window', $name)
    Wait-Condition { [FileSpacerWindowTest]::IsWindowVisible($script:window) } 'folder display'
}
function Send-SystemCommand([uint32]$Command, [IntPtr]$Target = $script:window) {
    Write-Host ('STEP: WM_SYSCOMMAND 0x{0:X}, HWND={1}' -f $Command,$Target)
    # Send to the window procedure, independently of the queued Shell accelerators.
    [FileSpacerWindowTest]::SystemCommand($Target,$Command)
    Write-Host "STEP: command returned; maximized=$([FileSpacerWindowTest]::IsZoomed($Target)), minimized=$([FileSpacerWindowTest]::IsIconic($Target))"
}
function Wait-Minimized {
    Wait-Condition {
        [FileSpacerWindowTest]::IsIconic($script:window)
    } 'folder minimized'
}
function Wait-Restored([bool]$Maximized) {
    Wait-Condition {
        ![FileSpacerWindowTest]::IsIconic($script:window) -and
        [FileSpacerWindowTest]::IsWindowVisible($script:window) -and
        ([FileSpacerWindowTest]::IsZoomed($script:window) -eq $Maximized)
    } 'folder restored'
    Assert-CaptionButtons
}
function Close-TestWindow {
    $closing = $script:window
    if (![FileSpacerWindowTest]::PostMessage($closing, 0x10, [UIntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Failed to post close command'
    }
    Wait-Condition { ![FileSpacerWindowTest]::IsWindow($closing) } 'folder close and state save'
    $script:window = [IntPtr]::Zero
}
function Assert-NormalRectangle {
    $actual = New-Object FileSpacerWindowTest+RECT
    if (![FileSpacerWindowTest]::GetWindowRect($script:window, [ref]$actual)) { throw 'Cannot read restored rectangle' }
    foreach ($part in @('left', 'top', 'right', 'bottom')) {
        if ($actual.$part -ne $expected.$part) {
            throw "Normal rectangle changed: $part expected $($expected.$part), actual $($actual.$part)"
        }
    }
}
function Assert-CaptionButtons {
    # Query the OS button rectangles and exercise the real WM_NCHITTEST path.
    # System commands alone would miss a visible button with an incorrect click zone.
    foreach ($button in @(@(2,8,'Minimize'), @(3,9,'Maximize/Restore'), @(5,20,'Close'))) {
        $rect = [FileSpacerWindowTest]::ButtonRect($script:window, $button[0])
        if ($rect.right -le $rect.left -or $rect.bottom -le $rect.top) {
            throw "Missing native rectangle for $($button[2])"
        }
        $x = [int][Math]::Floor(($rect.left + $rect.right) / 2.0)
        $y = [int][Math]::Floor(($rect.top + $rect.bottom) / 2.0)
        $target = [FileSpacerWindowTest]::WindowAt($x,$y)
        if ($target -ne $script:window -and [FileSpacerWindowTest]::HitTest($target,$x,$y) -ne -1) {
            throw "A different window/control covers $($button[2])"
        }
        $hit = [FileSpacerWindowTest]::HitTest($script:window,$x,$y)
        if ($hit -ne $button[1]) {
            throw "$($button[2]) hit-test returned $hit instead of $($button[1])"
        }
    }
}

$name = 'FileSpacer-window-test-' + [Guid]::NewGuid().ToString('N')
$folder = Join-Path ([IO.Path]::GetTempPath()) $name
$window = [IntPtr]::Zero
$processes=New-Object Collections.ArrayList
$previousDpiContext = [FileSpacerWindowTest]::SetThreadDpiAwarenessContext([IntPtr](-4))
$testFailure=$null
try {
    [void](New-Item -ItemType Directory -Path $folder)
    Open-TestWindow
    $style = [FileSpacerWindowTest]::GetWindowLong($window, -16)
    if (($style -band 0x30000) -ne 0x30000) { throw 'Minimize/maximize styles are missing' }
    Assert-CaptionButtons
    $initial = New-Object FileSpacerWindowTest+RECT
    if (![FileSpacerWindowTest]::GetWindowRect($window, [ref]$initial)) { throw 'Cannot read initial rectangle' }
    if (![FileSpacerWindowTest]::SetWindowPos($window, [IntPtr]::Zero,
            $initial.left, $initial.top, 700, 500, 0x14)) { throw 'Cannot set test dimensions' }
    $expected = New-Object FileSpacerWindowTest+RECT
    if (![FileSpacerWindowTest]::GetWindowRect($window, [ref]$expected)) { throw 'Cannot read reference rectangle' }

    Send-SystemCommand 0xF030 # SC_MAXIMIZE
    Wait-Condition { [FileSpacerWindowTest]::IsZoomed($window) } 'maximize'
    Assert-CaptionButtons
    Close-TestWindow
    Open-TestWindow
    Wait-Condition { [FileSpacerWindowTest]::IsZoomed($window) } 'restore maximized state from previous session'
    Assert-CaptionButtons
    Send-SystemCommand 0xF120 # SC_RESTORE
    Wait-Condition { ![FileSpacerWindowTest]::IsZoomed($window) } 'restore normal size'
    Assert-CaptionButtons
    Assert-NormalRectangle
    Write-Host 'PASS: reopen maximized, then restore the exact normal rectangle.'

    Send-SystemCommand 0xF020 # SC_MINIMIZE
    Wait-Minimized
    Send-SystemCommand 0xF120 # Restore the visible folder window.
    Wait-Restored $false
    Assert-NormalRectangle
    Write-Host 'PASS: native folder minimize and restore.'
    Send-SystemCommand 0xF020
    Wait-Minimized
    Close-TestWindow
    Open-TestWindow
    if ([FileSpacerWindowTest]::IsIconic($window) -or [FileSpacerWindowTest]::IsZoomed($window)) {
        throw 'Normal folder reopened minimized or maximized'
    }
    Assert-NormalRectangle
    Write-Host 'PASS: minimize a normal folder, close it, reopen normally with the same rectangle.'

    Send-SystemCommand 0xF030
    Wait-Condition { [FileSpacerWindowTest]::IsZoomed($window) } 'maximize before minimizing'
    Send-SystemCommand 0xF020
    Wait-Minimized
    Send-SystemCommand 0xF120
    Wait-Restored $true
    Write-Host 'PASS: minimizing and restoring keeps the native maximized placement.'
    Send-SystemCommand 0xF020
    Wait-Minimized
    Close-TestWindow
    Open-TestWindow
    Wait-Condition { [FileSpacerWindowTest]::IsZoomed($window) } 'reopen maximized without minimized state'
    if ([FileSpacerWindowTest]::IsIconic($window)) { throw 'Minimized state was restored' }
    Send-SystemCommand 0xF120
    Wait-Condition { ![FileSpacerWindowTest]::IsZoomed($window) } 'restore after maximized/minimized reopen'
    Assert-NormalRectangle
    Write-Host 'PASS: minimizing a maximized folder preserves maximize state and normal dimensions.'
    Write-Host 'PASS: native caption-button hit-testing in normal, maximized and reopened states.'
} catch { $testFailure=$_; throw }
finally {
    $cleanupErrors=New-Object Collections.Generic.List[string]
    try {
        if ($window -ne [IntPtr]::Zero -and [FileSpacerWindowTest]::IsWindow($window)) {
            try { Close-TestWindow } catch { $cleanupErrors.Add($_.Exception.Message) }
        }
        try { Complete-TestApplications $processes }
        catch { $cleanupErrors.Add($_.Exception.Message) }
    } finally {
        if ($previousDpiContext -ne [IntPtr]::Zero) {
            [void][FileSpacerWindowTest]::SetThreadDpiAwarenessContext($previousDpiContext)
        }
        if (Test-Path -LiteralPath $folder) {
            try { Remove-Item -LiteralPath $folder -Recurse -Force }
            catch { $cleanupErrors.Add($_.Exception.Message) }
        }
    }
    if ($cleanupErrors.Count) {
        try {
            ConvertTo-Json -InputObject @($cleanupErrors.ToArray()) |
                Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'cleanup-errors.json') -Encoding UTF8
        } catch { Write-Warning ('Cannot record cleanup errors: '+$_.Exception.Message) }
        if ($null -ne $testFailure) { Write-Warning ('Cleanup after initial failure: '+($cleanupErrors -join '; ')) }
        else { throw ($cleanupErrors -join '; ') }
    }
}
