param(
    [ValidateSet('Placement','ObserveExplorer','CompareDetails','CompareList','DesktopPlacement','ProfilePlacement','VisualTiming','CachedViews','SelectionStrategies','SelectionIndexes','SelectionCallbacks','SelectionFresh','SelectionRestore','SelectionErrors','SelectionPaired','SelectionOptimized','SelectionMode','SelectionKeyboard','ScrollPercent','ScrollProvider','SelectionInternals','SelectionMatrix','SelectionAccessibility','SelectionComparisonCounts','SelectionCausal','SelectionAutoArrange')][string]$Case='CompareDetails',
    [ValidateRange(30,900)][int]$TimeoutSeconds=300,
    [ValidateSet('SelfTest','ExtraLarge','Large','Medium','Small','Details','List','Tiles','Content','Placement','Protocol','PlacementProtocol','IconsCpu','ProfileIcons','AllViewsApi','ContentCpu')][string]$VisualScenario='SelfTest'
)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$stage=[IO.Path]::GetFullPath((Join-Path $project 'tests\regression\cases\36-reactivity-prototype'))
if(Test-Path -LiteralPath $stage){throw 'The temporary experiment case directory already exists; it will not be overwritten.'}
$patterns=@{Placement='01-*';ObserveExplorer='02-*';CompareDetails='03-*';CompareList='07-*';DesktopPlacement='04-*';ProfilePlacement='08-*';VisualTiming='09-*';CachedViews='06-*';SelectionStrategies='10-*';SelectionIndexes='11-*';SelectionCallbacks='12-*';SelectionFresh='13-*';SelectionRestore='14-*';SelectionErrors='15-*';SelectionPaired='16-*';SelectionOptimized='17-*';SelectionMode='18-*';SelectionKeyboard='19-*';ScrollPercent='20-*';ScrollProvider='21-*';SelectionInternals='22-*';SelectionMatrix='23-*';SelectionAccessibility='24-*';SelectionComparisonCounts='25-*';SelectionCausal='26-*';SelectionAutoArrange='27-*'}
$files=@('01-Placement.ps1','02-ObserveExplorer.ps1','03-CompareDetails.ps1','04-DesktopPlacement.ps1','06-CachedViews.ps1','07-CompareList.ps1','08-ProfilePlacement.ps1','09-VisualTiming.ps1','VisualTimingTests.cpp','VisualTimingTests.rc','PresentationTiming.h','Fixture.ps1',
    'PlacementTests.cpp','ExplorerObservationTests.cpp','ExplorerObservationTests.rc','DesktopPlacementTests.cpp','DesktopPlacementTests.rc','CachedViewTests.cpp','DumpStacks.cpp','10-SelectionStrategies.ps1','11-SelectionIndexes.ps1','12-SelectionCallbacks.ps1','13-SelectionFresh.ps1','14-SelectionRestore.ps1','15-SelectionErrors.ps1','16-SelectionPaired.ps1','17-SelectionOptimized.ps1','18-SelectionMode.ps1','19-SelectionKeyboard.ps1','20-ScrollPercent.ps1','21-ScrollProvider.ps1','ScrollProviderTests.cpp','SelectionStrategiesTests.cpp','SelectionReference.inc','22-SelectionInternals.ps1','SelectionInternalsTests.cpp','23-SelectionMatrix.ps1','24-SelectionAccessibility.ps1','25-SelectionComparisonCounts.ps1','26-SelectionCausal.ps1','27-SelectionAutoArrange.ps1')
foreach($name in $files){if(!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))){throw ('Missing experiment source: '+$name)}}
foreach($directory in @('baseline','hybrid')){foreach($name in @('FileSpacer.exe','FileSpacerFolder.dll')){
    if(!(Test-Path -LiteralPath (Join-Path $project ('build\'+$directory+'\'+$name)))){throw ('Build variant unavailable: '+$directory+'\'+$name)}
}}
$output='build\regression\experiment-'+$Case.ToLowerInvariant()+'-'+[Guid]::NewGuid().ToString('N')
$written=@()
[void][IO.Directory]::CreateDirectory($stage)
Push-Location -LiteralPath $project
try{
    foreach($name in $files){
        $target=Join-Path $stage $name;Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $target;$written+=$target
        if($Case-eq'VisualTiming'-and$name-eq'09-VisualTiming.ps1'-and$VisualScenario-ne'SelfTest'){
            # Host environment variables are not inherited by the VM controller.
            # Select the scenario in the owned staged driver before preparation/hashing.
            $text=[IO.File]::ReadAllText($target);$default="else{'SelfTest'}"
            if(!$text.Contains($default)){throw 'Visual scenario default no longer matches the staging contract.'}
            [IO.File]::WriteAllText($target,$text.Replace($default,"else{'"+$VisualScenario+"'}"),[Text.UTF8Encoding]::new($false))
        }
    }
    & $env:ComSpec /d /c tests.cmd -VM AgentWin10 -Pattern ('36-reactivity-prototype\'+$patterns[$Case]) -TimeoutSeconds $TimeoutSeconds -RunDirectory $output
    $result=$LASTEXITCODE
}finally{
    Pop-Location
    foreach($target in $written){Remove-Item -LiteralPath $target -Force}
    if(@(Get-ChildItem -LiteralPath $stage -Force).Count-eq0){Remove-Item -LiteralPath $stage}
}
exit $result
