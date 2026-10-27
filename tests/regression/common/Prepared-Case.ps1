param([Parameter(Mandatory=$true)][string]$ProjectRoot,[Parameter(Mandatory=$true)][string]$Case)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Prepared.ps1')
& (Join-Path $PSScriptRoot ('..\cases\'+$Case)) -ProjectRoot $ProjectRoot
if (!$?) { exit 1 }
exit 0
