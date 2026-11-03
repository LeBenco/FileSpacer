# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$run=$env:FS_REGRESSION_RUN
if(!$run){throw 'Run this contract through tests.cmd.'}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $ProjectRoot 'tests\regression\Run-Regression.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Runner parsing failed.'}
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Write-RegressionProgress'},$true)
if(!$function){throw 'Production progress writer is missing.'}
. ([scriptblock]::Create($function.Extent.Text))
$path=Join-Path $run 'observed-progress.json'
Write-RegressionProgress @{Expected=3;Completed=0;Current='initial'} $path
# Match the actual CI observer: a read handle shared for reads, writes and deletion.
$reader=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
try {
    foreach($current in @('a long initial case name','short',('dernier-'+[char]0xe9))) {
        Write-RegressionProgress @{Expected=3;Completed=1;Current=$current} $path
        $snapshot=[IO.File]::ReadAllText($path,[Text.Encoding]::UTF8)|ConvertFrom-Json
        if($snapshot.Current -ne $current -or $snapshot.Expected -ne 3 -or $snapshot.Completed -ne 1){throw 'Shared progress update lost or retained stale content.'}
    }
} finally { $reader.Dispose() }
$exclusive=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try {
    $failed=$false
    try { Write-RegressionProgress @{Expected=3;Completed=2;Current='blocked'} $path } catch { $failed=$true }
    if(!$failed){throw 'An actual incompatible file lock must still fail.'}
} finally { $exclusive.Dispose() }
Write-RegressionProgress @{Expected=3;Completed=3;Current='final'} $path
$snapshot=[IO.File]::ReadAllText($path,[Text.Encoding]::UTF8)|ConvertFrom-Json
if($snapshot.Completed -ne 3 -or $snapshot.Current -ne 'final'){throw 'Progress writer did not release its handle after failure.'}
Write-Host 'PASS: production progress updates coexist with the CI reader, truncate old content, preserve UTF-8 and report incompatible locks.'
