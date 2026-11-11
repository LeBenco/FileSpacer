param([Parameter(Mandatory=$true)][string]$ProjectRoot,[Parameter(Mandatory=$true)][string]$Case,
    [ValidateSet('Regression','Stress')][string]$Suite='Regression')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Prepared.ps1')
$caseRoot=if($Suite-eq'Stress'){Join-Path $ProjectRoot 'tests\stress\cases'}else{Join-Path $PSScriptRoot '..\cases'}
& (Join-Path $caseRoot $Case) -ProjectRoot $ProjectRoot
if (!$?) { exit 1 }
exit 0
