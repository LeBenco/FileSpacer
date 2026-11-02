# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
$RunDirectory=$env:FS_REGRESSION_RUN
if(!$RunDirectory){throw 'Run this contract through tests.cmd.'}
if($env:FS_REGRESSION_PREPARED) {
    $package=Join-Path $ProjectRoot 'prepared\runner-contract'
    if(!(Test-Path -LiteralPath $package)){throw 'Host-prepared runner contract fixture is missing.'}
} else {
    . (Join-Path $ProjectRoot 'tests\regression\common\Prepare-Client.ps1')
    $relative='00-banc\01-Resultats.ps1'
    $selected=@([pscustomobject]@{Relative=$relative;Category='Internal';File=(Get-Item -LiteralPath (Join-Path $ProjectRoot ('tests\regression\cases\'+$relative)))})
    $package=Join-Path $RunDirectory 'minimal-package'
    New-ClientPackage -Selected $selected -Destination $package -TimeoutSeconds 60
}
$execution=Join-Path $RunDirectory 'prepared-execution'
$runner=Join-Path $package 'tests\regression\Run-Regression.ps1'
$log=Join-Path $RunDirectory 'prepared-execution.log'
& (Join-Path $PSHOME 'powershell.exe') -NoProfile -ExecutionPolicy Bypass -File $runner -ProjectRoot $package -Category Internal -Prepared -RunDirectory $execution *> $log
if($LASTEXITCODE-ne0){throw ('Prepared runner failed; see '+$log)}
$summary=Get-Content -LiteralPath (Join-Path $execution 'summary.json') -Raw|ConvertFrom-Json
if($summary.Status-ne'PASS' -or $summary.Expected-ne1 -or $summary.Executed-ne1 -or $summary.Passed-ne1){throw 'Prepared runner did not execute its exact selected case.'}
Write-Host 'PASS: a minimally prepared package executes its runner and selected case without host compilation or missing runtime dependencies.'
