# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$executable = Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
$temporaryBase = [IO.Path]::GetFullPath($env:TEMP)
$temporaryRoot = [IO.Path]::GetFullPath((Join-Path $temporaryBase ('FS-delete-' + [Guid]::NewGuid().ToString('N'))))
if ([IO.Path]::GetDirectoryName($temporaryRoot).TrimEnd('\') -ne $temporaryBase.TrimEnd('\')) {
    throw 'The fixture root is outside the temporary directory.'
}
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
Add-Type -Path (Join-Path $PSScriptRoot '..\..\common\EssentialUi.cs')
$shell = $null
$shellWindows = $null

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FileSpacerDeletionTest {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct FileOperation {
        public IntPtr Window;
        public uint Function;
        [MarshalAs(UnmanagedType.LPWStr)] public string From;
        [MarshalAs(UnmanagedType.LPWStr)] public string To;
        public ushort Flags;
        [MarshalAs(UnmanagedType.Bool)] public bool Aborted;
        public IntPtr Mappings;
        [MarshalAs(UnmanagedType.LPWStr)] public string Title;
    }
    [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
    static extern int SHFileOperationW(ref FileOperation operation);
    public static void DeleteFolder(string path, bool recycle, string fixtureRoot) {
        string target = System.IO.Path.GetFullPath(path);
        string root = System.IO.Path.GetFullPath(fixtureRoot).TrimEnd('\\') + '\\';
        if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("The deletion target is outside the fixture root.");
        var operation = new FileOperation();
        operation.Function = 3; // FO_DELETE
        operation.From = target + '\0' + '\0';
        operation.Flags = (ushort)(0x4 | 0x10 | 0x400 | (recycle ? 0x40 : 0));
        int result = SHFileOperationW(ref operation);
        if (result != 0 || operation.Aborted)
            throw new InvalidOperationException("Shell deletion failed: " + result + "; aborted=" + operation.Aborted);
    }
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
function Has-ReadyView([IntPtr]$Window, [int]$ExpectedCount = 0) {
    for ($index = 0; $index -lt $shellWindows.Count; ++$index) {
        $registered = $shellWindows.Item($index)
        if ($null -eq $registered) { continue }
        try {
            $view = [FileSpacerRegression.Native]::View($registered, $Window)
            if ($null -ne $view) {
                try { return $view.Count -eq $ExpectedCount }
                finally { $view.Dispose() }
            }
        } finally { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($registered) }
    }
    return $false
}

New-Item -ItemType Directory -Path $closedFolder, $openFolder | Out-Null
try {
    # Deliberately start from the folder being tested, with a relative argument.
    $first = Start-TestApplication -Executable $executable -WorkingDirectory $closedFolder -Arguments '/test "."' -OutputDirectory $RunDirectory
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
    $second = Start-TestApplication -Executable $executable -WorkingDirectory $openFolder -Arguments '/test "."' -OutputDirectory $RunDirectory
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

    $shell = New-Object -ComObject Shell.Application
    $shellWindows = $shell.Windows()
    $ancestorCases = @(
        @{ Mode='Recycle'; Depth=0 },
        @{ Mode='Recycle'; Depth=1 },
        @{ Mode='Recycle'; Depth=2 },
        @{ Mode='Permanent'; Depth=1 },
        @{ Mode='Permanent'; Depth=2 },
        @{ Mode='Filesystem'; Depth=2 }
    )
    foreach ($case in $ancestorCases) {
        $ancestorFolder = Join-Path $parentFolder ($case.Mode + '-' + [Guid]::NewGuid().ToString('N'))
        $descendantFolder = $ancestorFolder
        for ($depth = 0; $depth -lt $case.Depth; ++$depth) {
            $descendantFolder = Join-Path $descendantFolder ('Child-' + [Guid]::NewGuid().ToString('N'))
        }
        New-Item -ItemType Directory -Path $descendantFolder -Force | Out-Null
        $descendantName = Split-Path $descendantFolder -Leaf
        $titles += $descendantName
        $key = & $probe --key $descendantFolder
        if ($LASTEXITCODE -ne 0 -or !$key) { throw 'Descendant identity lookup failed.' }
        $application = Start-TestApplication -Executable $executable -WorkingDirectory $ProjectRoot `
            -Arguments ('/test "' + $descendantFolder + '"') -OutputDirectory $RunDirectory
        $processes += $application
        Wait-Condition { (Find-TestWindow $descendantName) -ne [IntPtr]::Zero } 'descendant window'
        $window = Find-TestWindow $descendantName
        [void][FileSpacerDeletionTest]::GetWindowThreadProcessId($window, [ref]$windowProcess)
        if ($windowProcess -ne $application.Id) { throw 'The descendant window belongs to another process.' }
        Wait-Condition { Has-ReadyView $window } 'registered native Shell view'
        [void][FileSpacerDeletionTest]::SetWindowPos($window, [IntPtr]::Zero, 150, 150, 650, 490, 0x14)
        Wait-Condition { Has-State $key } 'saved descendant state'

        # A nearby path with the same prefix is not an ancestor. Its real Shell
        # deletion must leave both this window and its stored state intact.
        $sibling = $ancestorFolder + '-sibling'
        New-Item -ItemType Directory -Path $sibling | Out-Null
        [FileSpacerDeletionTest]::DeleteFolder($sibling, $true, $temporaryRoot)
        Start-Sleep -Milliseconds 300
        if ((Find-TestWindow $descendantName) -eq [IntPtr]::Zero -or !(Has-State $key)) {
            throw 'Deleting a sibling affected the descendant.'
        }

        $target = [IO.Path]::GetFullPath($ancestorFolder)
        $fixtureRoot = [IO.Path]::GetFullPath($temporaryRoot) + [IO.Path]::DirectorySeparatorChar
        if (!$target.StartsWith($fixtureRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'The deletion target is outside the fixture root.'
        }
        if ($case.Mode -eq 'Filesystem') {
            Remove-Item -LiteralPath $target -Recurse -Force
        } else {
            [FileSpacerDeletionTest]::DeleteFolder($target, ($case.Mode -eq 'Recycle'), $temporaryRoot)
        }
        if (Test-Path -LiteralPath $target) { throw 'The ancestor was not removed.' }
        Wait-Condition { (Find-TestWindow $descendantName) -eq [IntPtr]::Zero } 'closure after ancestor removal'
        Wait-Condition { !(Has-State $key) } 'descendant state removal'
        if ((Find-TestWindow $titles[2]) -eq [IntPtr]::Zero -or $first.HasExited) {
            throw 'The unrelated parent window was closed.'
        }
        Write-Host ('PASS: {0}, depth {1}; descendant closed and state removed; unrelated window preserved.' -f $case.Mode, $case.Depth)
    }

    # All four windows share one process and one ancestor receiver. Reopening a
    # closed child exercises removal/reinsertion before dispatch closes a batch.
    $sharedRoot = Join-Path $temporaryRoot ('Shared-' + [Guid]::NewGuid().ToString('N'))
    $batchFolder = Join-Path $sharedRoot ('Batch-' + [Guid]::NewGuid().ToString('N'))
    $childFolder = Join-Path $batchFolder ('Child-' + [Guid]::NewGuid().ToString('N'))
    $leafFolder = Join-Path $childFolder ('Leaf-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $leafFolder -Force | Out-Null
    $sharedFolders = @($leafFolder, $childFolder, $batchFolder, $sharedRoot)
    $sharedNames = @($sharedFolders | ForEach-Object { Split-Path $_ -Leaf })
    $titles += $sharedNames
    $sharedKeys = @($sharedFolders | ForEach-Object {
        $identity = & $probe --key $_
        if ($LASTEXITCODE -ne 0 -or !$identity) { throw 'Shared-process identity lookup failed.' }
        $identity
    })
    $sharedProcess = Start-TestApplication -Executable $executable -WorkingDirectory $ProjectRoot `
        -Arguments ('/test "' + $leafFolder + '"') -OutputDirectory $RunDirectory
    $processes += $sharedProcess
    for ($index = 0; $index -lt $sharedFolders.Count; ++$index) {
        Wait-Condition { (Find-TestWindow $sharedNames[$index]) -ne [IntPtr]::Zero } 'shared-process window'
        $window = Find-TestWindow $sharedNames[$index]
        [void][FileSpacerDeletionTest]::GetWindowThreadProcessId($window, [ref]$windowProcess)
        if ($windowProcess -ne $sharedProcess.Id) { throw 'The batch windows do not share one process.' }
        $expectedItems = if ($index -eq 0) { 0 } else { 1 }
        Wait-Condition { Has-ReadyView $window $expectedItems } 'shared-process native Shell view'
        [void][FileSpacerDeletionTest]::SetWindowPos($window, [IntPtr]::Zero, (180 + 30*$index), (180 + 30*$index), 650, 490, 0x14)
        Wait-Condition { Has-State $sharedKeys[$index] } 'shared-process saved state'
        if ($index -lt $sharedFolders.Count - 1) {
            [void][FileSpacerDeletionTest]::PostMessage($window, 0x111, [UIntPtr]::new([uint32]1002), [IntPtr]::Zero)
        }
    }
    Close-TestWindow $sharedNames[1]
    Wait-Condition { (Find-TestWindow $sharedNames[1]) -eq [IntPtr]::Zero } 'shared child closure'
    $window = Find-TestWindow $sharedNames[0]
    [void][FileSpacerDeletionTest]::PostMessage($window, 0x111, [UIntPtr]::new([uint32]1002), [IntPtr]::Zero)
    Wait-Condition { (Find-TestWindow $sharedNames[1]) -ne [IntPtr]::Zero } 'shared child reopening'
    $window = Find-TestWindow $sharedNames[1]
    [void][FileSpacerDeletionTest]::GetWindowThreadProcessId($window, [ref]$windowProcess)
    if ($windowProcess -ne $sharedProcess.Id) { throw 'The reopened child changed process.' }
    Wait-Condition { Has-ReadyView $window 1 } 'reopened child native Shell view'

    [FileSpacerDeletionTest]::DeleteFolder($batchFolder, $true, $temporaryRoot)
    for ($index = 0; $index -lt 3; ++$index) {
        Wait-Condition { (Find-TestWindow $sharedNames[$index]) -eq [IntPtr]::Zero } 'shared descendant closure'
        Wait-Condition { !(Has-State $sharedKeys[$index]) } 'shared descendant state removal'
    }
    if ((Find-TestWindow $sharedNames[3]) -eq [IntPtr]::Zero -or !(Has-State $sharedKeys[3]) -or $sharedProcess.HasExited) {
        throw 'Batch dispatch affected the surviving parent.'
    }
    Close-TestWindow $sharedNames[3]
    Wait-Condition { $sharedProcess.HasExited } 'shared receiver process exit'
    if ($sharedProcess.ExitCode -ne 0) { throw 'The shared receiver process failed.' }
    Write-Host 'PASS: shared-process batch removal, child reopening, unrelated parent survival, and normal process exit.'
} catch { $testFailure=$_; throw }
finally {
    foreach ($title in $titles) { Close-TestWindow $title }
    foreach ($com in @($shellWindows, $shell)) {
        if ($null -ne $com) { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($com) }
    }
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

foreach ($log in Get-ChildItem -LiteralPath $RunDirectory -Filter 'application-*.stdout.log') {
    $output = Get-Content -LiteralPath $log.FullName -Raw
    if ($output.Contains('SHCreateItemFromParsingName(closedWindowPath(last).c_str()')) {
        throw 'An unavailable history entry still causes a Jump List resolution error.'
    }
    if ($output.Contains('Item could not be accessed!')) {
        throw 'A confirmed deletion used the access-failure fallback.'
    }
}
