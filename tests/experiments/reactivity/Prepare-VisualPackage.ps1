param([string]$OutputDirectory='build\visual-timing\package')
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$destination=[IO.Path]::GetFullPath((Join-Path $project $OutputDirectory))
if(!$destination.StartsWith($project+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Package output must stay in this checkout.'}
if(Test-Path -LiteralPath $destination){throw 'Package output already exists; it will not be overwritten.'}
if(Test-Path -LiteralPath (Join-Path (Split-Path $destination -Parent) 'native-build-0')){throw 'Choose a fresh package parent directory; compiler artifacts already exist there.'}
$stage=Join-Path $project 'tests\regression\cases\36-reactivity-prototype'
if(Test-Path -LiteralPath $stage){throw 'The temporary experiment directory already exists.'}
$files=@('09-VisualTiming.ps1','Fixture.ps1','VisualTimingTests.cpp','VisualTimingTests.rc','PresentationTiming.h','DesktopPlacementTests.cpp','ExplorerObservationTests.cpp')
[void][IO.Directory]::CreateDirectory($stage)
$written=@()
Push-Location -LiteralPath $project
try{
    foreach($name in $files){$path=Join-Path $stage $name;Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $path;$written+=$path}
    . (Join-Path $project 'tests\regression\common\Prepare-Client.ps1')
    $ProjectRoot=$project
    $selected=@(@{File=(Get-Item -LiteralPath (Join-Path $stage '09-VisualTiming.ps1'));Relative='36-reactivity-prototype\09-VisualTiming.ps1';Category='UI';Scope='Application'})
    [void][IO.Directory]::CreateDirectory($destination)
    New-ClientPackage -Selected $selected -Destination $destination -TimeoutSeconds 900
    foreach($name in @('Run-VisualMeasurements.ps1','Run-Visual.cmd','MESURES-VISUELLES.txt')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $destination $name)}
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination (Join-Path $destination 'README-experiments.md')
    Write-Host ('Prepared package: '+$destination)
}finally{
    Pop-Location
    foreach($path in $written){Remove-Item -LiteralPath $path -Force}
    if(@(Get-ChildItem -LiteralPath $stage -Force).Count-eq0){Remove-Item -LiteralPath $stage}
}
