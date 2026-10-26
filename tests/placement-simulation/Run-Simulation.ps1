param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateRange(1,100)][int]$Seeds = 3,
    [ValidateRange(1,100)][int]$Repetitions = 3,
    [switch]$VerifyOnly,
    [switch]$Quick,
    [switch]$WorstCases
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$output = Join-Path $projectRoot ('build\placement-simulation\' + $Configuration.ToLowerInvariant())
if ($WorstCases) { $output = Join-Path $projectRoot ('build\placement-worst-cases\' + $Configuration.ToLowerInvariant()) }
[void][IO.Directory]::CreateDirectory($output)
$legacy = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'LegacyGrid.inc'))
$coreStart = $legacy.IndexOf('constexpr LONG WindowPlacementGrid::cellSize;')
if ($coreStart -le 0) { throw 'Historical geometry markers changed.' }
$gridClass = $legacy.Substring(0, $coreStart)
$gridCore = $legacy.Substring($coreStart)
if ($gridCore -match 'EnumWindows|GetWindowRect|SetWindowPos|CreateWindow|FindWindow|GetMonitorInfo') {
    throw 'Window access found in the geometry extraction.'
}
if (([regex]::Matches($gridClass,'cellSize = 10;')).Count -ne 1) {
    throw 'Production cell-size declaration changed; review the extraction.'
}
$generated = '// Historical grid from LegacyGrid.inc; independent of current production placement.' + "`r`n"
foreach ($step in @(10,5,2,1)) {
    $generated += "namespace grid$step {`r`n" + $gridClass.Replace('cellSize = 10;',"cellSize = $step;") + $gridCore + "}`r`n"
}
[IO.File]::WriteAllText((Join-Path $output 'Grids.inc'), $generated)
$flags = '/Od /MTd /D FILESPACER_DEBUG'
if ($Configuration -eq 'Release') { $flags = '/O2 /MT' }
$sourceName = 'PlacementSimulation'
if ($WorstCases) { $sourceName = 'WorstCases' }
$build = @"
@echo off
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "$projectRoot"
cl.exe /nologo /EHsc /W4 /WX /std:c++14 /Z7 $flags /I src /I "$output" /Fo"$output\$sourceName.obj" /Fe"$output\$sourceName.exe" "$PSScriptRoot\$sourceName.cpp" /link /incremental:no /pdb:"$output\$sourceName.pdb" User32.lib
exit /b %errorlevel%
"@
$buildPath = Join-Path $output 'Build-Simulation.cmd'
[IO.File]::WriteAllText($buildPath, $build.Replace("`r`n","`n").Replace("`n","`r`n"), [Text.Encoding]::ASCII)
& $env:ComSpec /d /c $buildPath 2>&1 | Tee-Object -FilePath (Join-Path $output 'build.log') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Simulation build failed: $LASTEXITCODE" }
$runDirectory = Join-Path $output (Get-Date -Format 'yyyyMMdd-HHmmss')
[void][IO.Directory]::CreateDirectory($runDirectory)
$arguments = @($runDirectory, "$Seeds", "$Repetitions")
if ($VerifyOnly) { $arguments = @('--verify-only') }
elseif ($Quick -and !$WorstCases) { $arguments += '--quick' }
$metadata = [ordered]@{
    configuration = $Configuration; seeds = $Seeds; repetitions = $Repetitions
    quick = [bool]$Quick; verifyOnly = [bool]$VerifyOnly; worstCases = [bool]$WorstCases
    units = 'logical pixels; no DPI or window APIs queried'
    flags = $flags; compiler = 'MSVC x64, C++14, /W4 /WX'
    cpu = (Get-ItemProperty 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0').ProcessorNameString
    productionHeaderSha256 = (Get-FileHash (Join-Path $projectRoot 'src\WindowPlacement.h')).Hash
    productionSourceSha256 = (Get-FileHash (Join-Path $projectRoot 'src\WindowPlacement.cpp')).Hash
    simulationSha256 = (Get-FileHash (Join-Path $PSScriptRoot 'PlacementSimulation.cpp')).Hash
    driverSha256 = (Get-FileHash (Join-Path $PSScriptRoot ($sourceName+'.cpp'))).Hash
    generatedSha256 = (Get-FileHash (Join-Path $output 'Grids.inc')).Hash
    historicalGridSha256 = (Get-FileHash (Join-Path $PSScriptRoot 'LegacyGrid.inc')).Hash
}
$metadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDirectory 'metadata.json') -Encoding UTF8
$executable = Join-Path $output ($sourceName+'.exe')
& $executable @arguments 2>&1 | Tee-Object -FilePath (Join-Path $runDirectory 'run.log') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Simulation failed: $LASTEXITCODE; see $runDirectory" }
Write-Host "Results: $runDirectory"
