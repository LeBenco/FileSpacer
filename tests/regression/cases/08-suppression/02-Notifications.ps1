# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$executable = Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
$temporaryRoot = Join-Path $env:TEMP ('FileSpacer-deletion-' + [Guid]::NewGuid().ToString('N'))
$parentFolder = Join-Path $temporaryRoot ('Parent-' + [Guid]::NewGuid().ToString('N'))
$closedName = 'After-close-' + [Guid]::NewGuid().ToString('N')
$closedFolder = Join-Path $parentFolder $closedName
$openName = 'While-open-' + [Guid]::NewGuid().ToString('N')
$openFolder = Join-Path $parentFolder $openName
$processes = @()
$testFailure = $null
$titles = @($closedName, $openName, (Split-Path $parentFolder -Leaf))
$database = Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$probe = Build-NativeTest -Name FolderStateProbe -CaseDirectory $PSScriptRoot -ProductionSources @('FolderIdentity.cpp') -Sqlite

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FileSpacerDeletionTest {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, UIntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
}
'@
function Find-TestWindow([string]$Title) {
    [FileSpacerDeletionTest]::FindWindow('FileSpacer Item Window', $Title)
}
function Wait-Condition([scriptblock]$Condition, [string]$Description) {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    }
    throw "Timed out: $Description"
}
function Has-State([string]$Key) {
    if (!(Test-Path -LiteralPath $database)) { return $false }
    $value = & $probe --exists $database $Key
    if ($LASTEXITCODE -ne 0) { throw 'The test database could not be read.' }
    return $value -eq '1'
}
function Close-TestWindow([string]$Title) {
    $window = Find-TestWindow $Title
    if ($window -ne [IntPtr]::Zero) {
        [void][FileSpacerDeletionTest]::PostMessage($window, 0x10, [UIntPtr]::Zero, [IntPtr]::Zero)
    }
}

New-Item -ItemType Directory -Path $closedFolder, $openFolder | Out-Null
try {
    # Deliberately start from the folder being tested, with a relative argument.
    $first = Start-TestApplication -Executable $executable -WorkingDirectory $closedFolder -Arguments '/test "."'
    $processes += $first
    Wait-Condition { (Find-TestWindow $closedName) -ne [IntPtr]::Zero } 'first folder window'
    $window = Find-TestWindow $closedName
    [uint32]$windowProcess = 0
    [void][FileSpacerDeletionTest]::GetWindowThreadProcessId($window, [ref]$windowProcess)
    if ($windowProcess -ne $first.Id) { throw 'The relative launch opened the wrong window.' }
    # Native Up command creates the parent in this same process. Keep it open
    # to prove that deleting the closed child does not require process exit.
    [void][FileSpacerDeletionTest]::PostMessage($window, 0x111, [UIntPtr]::new([uint32]1002), [IntPtr]::Zero)
    Wait-Condition { (Find-TestWindow $titles[2]) -ne [IntPtr]::Zero } 'parent folder window'
    $parentWindow = Find-TestWindow $titles[2]
    [void][FileSpacerDeletionTest]::GetWindowThreadProcessId($parentWindow, [ref]$windowProcess)
    if ($windowProcess -ne $first.Id) { throw 'The parent is not in the same process.' }
    Close-TestWindow $closedName
    Wait-Condition { (Find-TestWindow $closedName) -eq [IntPtr]::Zero } 'first folder closure'
    if ($first.HasExited) { throw 'The process exited before the deletion test.' }
    Remove-Item -LiteralPath $closedFolder -ErrorAction Stop
    Write-Host 'PASS: closed folder deleted while the same FileSpacer process remains active.'

    $key = & $probe --key $openFolder
    if ($LASTEXITCODE -ne 0 -or !$key) { throw 'Folder identity lookup failed.' }
    $second = Start-TestApplication -Executable $executable -WorkingDirectory $openFolder -Arguments '/test "."'
    $processes += $second
    Wait-Condition { (Find-TestWindow $openName) -ne [IntPtr]::Zero } 'second folder window'
    $window = Find-TestWindow $openName
    [void][FileSpacerDeletionTest]::SetWindowPos($window, [IntPtr]::Zero, 120, 120, 620, 470, 0x14)
    Wait-Condition { Has-State $key } 'saved SQLite state'
    # Do not notify the Shell ourselves: test actual filesystem notifications.
    Remove-Item -LiteralPath $openFolder -ErrorAction Stop
    Wait-Condition { (Find-TestWindow $openName) -eq [IntPtr]::Zero } 'automatic closure after deletion'
    Wait-Condition { !(Has-State $key) } 'SQLite row removal'
    Start-Sleep -Milliseconds 1000
    if (Has-State $key) { throw 'Close-time persistence recreated the deleted state.' }
    Write-Host 'PASS: open folder deleted; its window closed; its SQLite row was removed.'
} catch { $testFailure=$_; throw }
finally {
    foreach ($title in $titles) { Close-TestWindow $title }
    try { Complete-TestApplications $processes }
    catch {
        if ($null -eq $testFailure) { throw }
        Write-Warning ('Application exit failure after initial test failure: '+$_.Exception.Message)
    }
    finally {
        try { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction Stop }
        catch [System.IO.IOException], [System.UnauthorizedAccessException] {
            Write-Warning "Temporary test files remain in: $temporaryRoot. $($_.Exception.Message)"
        }
    }
}
