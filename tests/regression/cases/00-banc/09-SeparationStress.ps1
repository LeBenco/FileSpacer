# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
$runner=Join-Path $ProjectRoot 'tests\regression\Run-Regression.ps1'
# A minimal real prepared run must select the saved suite, without a compiler or UI.
$fixture=Join-Path $env:FS_REGRESSION_RUN 'suite-fixture'
$common=Join-Path $fixture 'tests\regression\common';[void][IO.Directory]::CreateDirectory($common)
foreach($name in @('Process','Results','StorageSnapshot','Prepared','Prepared-Case')) {
    $file=$name+[IO.Path]::GetExtension($PSCommandPath)
    Copy-Item -LiteralPath (Join-Path $ProjectRoot ('tests\regression\common\'+$file)) -Destination $common
}
Copy-Item -LiteralPath $runner -Destination (Join-Path $fixture 'tests\regression')
$extension=[IO.Path]::GetExtension($PSCommandPath)
$case='00-fixture\01-Only'+$extension
$stressRoot=Join-Path $fixture 'tests\stress\cases\00-fixture';[void][IO.Directory]::CreateDirectory($stressRoot)
[IO.File]::WriteAllText((Join-Path $stressRoot ('01-Only'+$extension)),"# Test-Category: Internal`r`nparam([string]`$ProjectRoot)`r`nWrite-Host 'PASS: selected stress fixture'`r`n")
$regressionRoot=Join-Path $fixture 'tests\regression\cases\00-fixture';[void][IO.Directory]::CreateDirectory($regressionRoot)
[IO.File]::WriteAllText((Join-Path $regressionRoot ('02-Sentinel'+$extension)),"# Test-Category: Internal`r`nparam([string]`$ProjectRoot)`r`nthrow 'Regression sentinel ran during stress'`r`n")
$fixtureRunner=Join-Path $fixture 'tests\regression\Run-Regression.ps1'
$regression=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $fixtureRunner -ProjectRoot $fixture -ListOnly | ConvertFrom-Json
if($LASTEXITCODE-ne0-or@($regression).Count-ne1-or$regression.Test-notlike '*02-Sentinel*'){throw 'Default discovery included stress.'}
$stress=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $fixtureRunner -ProjectRoot $fixture -Suite Stress -ListOnly | ConvertFrom-Json
if($LASTEXITCODE-ne0-or@($stress).Count-ne1-or$stress.Test-ne$case){throw 'Explicit stress discovery included regression.'}
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'prepared'))
@{Native=@();Managed=@()}|ConvertTo-Json|Set-Content (Join-Path $fixture 'prepared\index.json') -Encoding UTF8
@{Suite='Stress';Cases=@($case);TimeoutSeconds=30}|ConvertTo-Json|Set-Content (Join-Path $fixture 'selection.json') -Encoding UTF8
$output=Join-Path $env:FS_REGRESSION_RUN 'suite-output'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $fixture 'tests\regression\Run-Regression.ps1') -ProjectRoot $fixture -Prepared -RunDirectory $output *> (Join-Path $env:FS_REGRESSION_RUN 'suite-output.log')
if($LASTEXITCODE-ne0){throw 'Prepared stress suite failed.'}
$summary=Get-Content -Raw (Join-Path $output 'summary.json')|ConvertFrom-Json
if($summary.Status-ne'PASS'-or$summary.Expected-ne1-or$summary.Passed-ne1){throw 'Prepared suite selected the wrong cases.'}
Write-Host 'PASS: disjoint default/explicit suite discovery and real prepared stress execution without regression sentinel, compiler or UI.'
