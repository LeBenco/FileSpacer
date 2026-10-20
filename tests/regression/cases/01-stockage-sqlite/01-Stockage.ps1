# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name StorageRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage
Invoke-TestProgram $test @((Join-Path $RunDirectory "single.sqlite3"))
. (Join-Path $PSScriptRoot '..\..\common\Process.ps1')
$database=Join-Path $RunDirectory 'concurrent.sqlite3'
$writers=@()
try {
    0..7 | ForEach-Object {
        $writers += Start-RecordedProcess -Executable $test -Arguments ('--writer "'+$database+'" writer-'+$_) -OutputPath (Join-Path $RunDirectory "writer-$_.out") -ErrorPath (Join-Path $RunDirectory "writer-$_.err")
    }
    foreach ($writer in $writers) {
        if (!$writer.Process.WaitForExit(45000)) { throw 'Concurrent writer timed out' }
        if ((Complete-RecordedProcess $writer) -ne 0) { throw "Concurrent writer failed: $($writer.Process.Id)" }
    }
    Invoke-TestProgram $test @('--verify-writers', $database, '8')
} finally {
    foreach ($writer in $writers) { if (!$writer.Process.HasExited) { $writer.Process.Kill(); $writer.Process.WaitForExit() }; $writer.Process.Dispose() }
}
