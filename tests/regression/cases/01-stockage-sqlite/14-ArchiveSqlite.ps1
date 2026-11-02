# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
. (Join-Path $PSScriptRoot '..\..\common\StorageSnapshot.ps1')
$database=Join-Path $RunDirectory 'closed.sqlite3'
foreach($suffix in @('','-wal','-shm')){[IO.File]::WriteAllBytes(($database+$suffix),[byte[]](1,2,3,4,5))}
Copy-TestDatabaseSnapshot $database (Join-Path $RunDirectory 'closed-export')
$proof=Get-Content -LiteralPath (Join-Path $RunDirectory 'closed-export\snapshot.json') -Raw|ConvertFrom-Json
if(!$proof.Consistent -or $proof.Files.Count-ne3){throw 'Closed snapshot or sidecars were not preserved.'}
foreach($file in $proof.Files){if((Get-FileHash -LiteralPath (Join-Path $RunDirectory ('closed-export\'+$file.Name))).Hash-ne(Get-FileHash -LiteralPath (Join-Path $RunDirectory $file.Name)).Hash){throw 'Snapshot bytes differ.'}}
$holder=[IO.File]::Open($database,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
$rejected=$false
try {try{Copy-TestDatabaseSnapshot $database (Join-Path $RunDirectory 'busy-export')}catch{$rejected=$true}}
finally {$holder.Dispose()}
if(!$rejected){throw 'A database still open by a writer was called quiescent.'}
$proof=Get-Content -LiteralPath (Join-Path $RunDirectory 'busy-export\snapshot.json') -Raw|ConvertFrom-Json
if($proof.Consistent -or !$proof.Error){throw 'Failed export was not recorded.'}
Copy-TestDatabaseSnapshot $database (Join-Path $RunDirectory 'released-export')
Write-Host 'PASS closed database and sidecars, active writer rejection, error evidence and recovery after release.'
