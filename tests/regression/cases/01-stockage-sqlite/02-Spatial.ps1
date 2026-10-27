# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$Executable = Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
$Probe = Build-NativeTest -Name FolderIdentityProbe -CaseDirectory (Join-Path $PSScriptRoot '..\01-stockage-sqlite') -ProductionSources @('FolderIdentity.cpp')
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path $Executable).Path
$Probe = (Resolve-Path $Probe).Path
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class SpatialNative {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] public struct Rect {
        public int Left, Top, Right, Bottom;
    }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetProp(IntPtr h, string s);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr z, int x, int y, int w, int hgt, uint f);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    public static IntPtr[] Find(string property) {
        var result = new List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr p) {
            var name = new StringBuilder(64);
            GetClassName(h, name, name.Capacity);
            if (name.ToString() == "FileSpacer Item Window" && GetProp(h, property) != IntPtr.Zero)
                result.Add(h);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
    public static Rect Rectangle(IntPtr h) {
        Rect r;
        if (!GetWindowRect(h, out r)) throw new Exception("GetWindowRect failed");
        return r;
    }
}
'@
function Require($condition, [string]$message) {
    if (!$condition) { throw $message }
}
function Identity([string]$path) {
    $lines = @(& $Probe $path)
    Require ($LASTEXITCODE -eq 0 -and $lines.Count -eq 2) "Identity probe failed: $path"
    return @{ Property=$lines[0]; Kind=$lines[1] }
}
function Windows([string]$property) { return @([SpatialNative]::Find($property)) }
function Wait-Count([string]$property, [int]$count) {
    $deadline = [DateTime]::UtcNow.AddSeconds(25)
    do {
        $found = @(Windows $property)
        if ($found.Count -eq $count) { return }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Window count: expected $count, found $($found.Count) for $property"
}
$previousDpiContext = [SpatialNative]::SetThreadDpiAwarenessContext([IntPtr](-4))
$processes = New-Object System.Collections.ArrayList
function Open([string]$path) {
    $process = Start-TestApplication -Executable $Executable -Arguments ('/test "' + $path + '"') -OutputDirectory $RunDirectory
    [void]$processes.Add($process)
}
function Drain-Launches {
    # Existing-window requests exit; the process that owns a new window remains alive.
    # Observe for several seconds so slow starts cannot make the count pass too early.
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 100
        foreach ($p in $processes) {
            if ($p.HasExited) { Require ($p.ExitCode -eq 0) "FileSpacer exited with $($p.ExitCode)" }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
}
function Close-Folder([string]$property) {
    foreach ($h in (Windows $property)) { [void][SpatialNative]::PostMessage($h, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) }
    Wait-Count $property 0
}
function Check-Rect($actual, $expected) {
    foreach ($field in @('Left','Top','Right','Bottom')) {
        Require ([Math]::Abs($actual.$field - $expected.$field) -le 2) "Geometry mismatch for $field : $($actual.$field) vs $($expected.$field)"
    }
}
$root = Join-Path $env:TEMP ('FileSpacer-spatial-' + [Guid]::NewGuid().ToString('N'))
$a = Join-Path $root 'A'
$b = Join-Path $root 'B'
$renamed = Join-Path $root 'A-renamed'
$alias = Join-Path $root 'A-alias'
$keys = New-Object System.Collections.ArrayList
$testFailure = $null
try {
    [void](New-Item -ItemType Directory -Path $a,$b -Force)
    $ia = Identity $a
    $ib = Identity $b
    [void]$keys.Add($ia.Property)
    [void]$keys.Add($ib.Property)
    Require ($ia.Kind -eq 'ntfs') 'Use a local NTFS TEMP directory for this test.'
    Require ($ia.Property -ne $ib.Property) 'Two different folders have the same identity.'
    $junction = New-Item -ItemType Junction -Path $alias -Target $a
    Require ((Identity $alias).Property -eq $ia.Property) 'Junction and target have different identities.'
    $shell = New-Object -ComObject WScript.Shell
    $shortcutPath = Join-Path $root 'A.lnk'
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $a
    $shortcut.Save()
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    1..12 | ForEach-Object { Open $a }
    Wait-Count $ia.Property 1
    Drain-Launches
    Require ((Windows $ia.Property).Count -eq 1) 'Parallel launches created duplicate windows.'
    Write-Host 'PASS: twelve launches, one folder window.'
    Open $alias
    Open $shortcutPath
    Open $b
    Wait-Count $ib.Property 1
    Drain-Launches
    Require ((Windows $ia.Property).Count -eq 1) 'Alias or shortcut created a duplicate.'
    Write-Host 'PASS: junction, shortcut, and distinct second folder.'
    $window = (Windows $ia.Property)[0]
    [void][SpatialNative]::ShowWindow($window, 9)
    Start-Sleep -Milliseconds 400
    $original = [SpatialNative]::Rectangle($window)
    # Resize from top/left: move the origin while retaining the bottom/right edges.
    Require ([SpatialNative]::SetWindowPos($window, [IntPtr]::Zero,
        $original.Left+24, $original.Top+24,
        $original.Right-$original.Left-24, $original.Bottom-$original.Top-24, 0x14)) 'SetWindowPos failed'
    Start-Sleep -Milliseconds 400
    $expected = [SpatialNative]::Rectangle($window)
    Close-Folder $ia.Property
    Open $a
    Wait-Count $ia.Property 1
    Start-Sleep -Milliseconds 800
    $window = (Windows $ia.Property)[0]
    Check-Rect ([SpatialNative]::Rectangle($window)) $expected
    Write-Host 'PASS: top/left resize restores both origin and size.'
    foreach ($show in @(6,3)) {
        [void][SpatialNative]::ShowWindow($window, $show)
        Start-Sleep -Milliseconds 400
        Close-Folder $ia.Property
        Open $a
        Wait-Count $ia.Property 1
        Start-Sleep -Milliseconds 800
        $window = (Windows $ia.Property)[0]
        [void][SpatialNative]::ShowWindow($window, 9)
        Start-Sleep -Milliseconds 400
        Check-Rect ([SpatialNative]::Rectangle($window)) $expected
    }
    Write-Host 'PASS: minimized/maximized closing preserves the normal rectangle.'
    Rename-Item -LiteralPath $a -NewName 'A-renamed'
    Require ((Identity $renamed).Property -eq $ia.Property) 'NTFS identity changed on rename.'
    Open $renamed
    Drain-Launches
    Require ((Windows $ia.Property).Count -eq 1) 'Rename created another folder window.'
    Write-Host 'PASS: NTFS rename retains identity and reuses the open window.'
    # SQLite persistence must also survive changes performed while FileSpacer is closed.
    Close-Folder $ia.Property
    Rename-Item -LiteralPath $renamed -NewName 'A-closed-renamed'
    $renamed = Join-Path $root 'A-closed-renamed'
    Require ((Identity $renamed).Property -eq $ia.Property) 'Closed rename changed NTFS identity.'
    Open $renamed
    Wait-Count $ia.Property 1
    Start-Sleep -Milliseconds 800
    Check-Rect ([SpatialNative]::Rectangle((Windows $ia.Property)[0])) $expected
    Close-Folder $ia.Property
    $moved = Join-Path $b 'A-moved'
    Move-Item -LiteralPath $renamed -Destination $moved
    Require ((Identity $moved).Property -eq $ia.Property) 'Same-volume move changed NTFS identity.'
    Open $moved
    Wait-Count $ia.Property 1
    Start-Sleep -Milliseconds 800
    Check-Rect ([SpatialNative]::Rectangle((Windows $ia.Property)[0])) $expected
    Write-Host 'PASS: closed NTFS rename and same-volume move retain saved geometry.'
    Write-Host 'PASS: spatial identity and geometry regression checks.'
} catch { $testFailure=$_; throw }
finally {
    [void][SpatialNative]::SetThreadDpiAwarenessContext($previousDpiContext)
    foreach ($key in $keys) {
        try { Close-Folder $key } catch { Write-Warning $_ }
    }
    try { Complete-TestApplications $processes }
    catch {
        if ($null -eq $testFailure) { throw }
        Write-Warning ('Application exit failure after initial test failure: '+$_.Exception.Message)
    }
    finally {
        if (Test-Path -LiteralPath $alias) {
            # Remove only the junction, never recursively traverse its target.
            [IO.Directory]::Delete($alias)
        }
        if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
    }
}
