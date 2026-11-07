param(
    [ValidateSet('Placement','ObserveExplorer','CompareDetails','CompareList','DesktopPlacement','ProfilePlacement','VisualTiming','CachedViews','SelectionStrategies','SelectionIndexes','SelectionCallbacks','SelectionFresh','SelectionRestore','SelectionErrors','SelectionPaired','SelectionOptimized','SelectionMode','SelectionKeyboard','ScrollPercent','ScrollProvider','SelectionInternals','SelectionMatrix','SelectionAccessibility','SelectionComparisonCounts','SelectionCausal','SelectionAutoArrange','ArrangementTransitions','OrderTransfer','DirectListView','DirectShellPlacement','LightweightShell','PrepareManualClear','RestoreAll','RestoreFreeBalance','RestoreVisual','FreeRestorePhases','FreeRestoreIdentical')][string]$Case='CompareDetails',
    [ValidateRange(30,900)][int]$TimeoutSeconds=300,
    [ValidateSet('SelfTest','ExtraLarge','Large','Medium','Small','Details','List','Tiles','Content','Placement','Protocol','PlacementProtocol','IconsCpu','ProfileIcons','AllViewsApi','ContentCpu')][string]$VisualScenario='SelfTest',
    [switch]$ReorderOnly,
    [switch]$ProfileMatrix,
    [switch]$HandoffOnly,
    [ValidateSet('placement','placement-close','settings','status','notifications','combined')][string]$LightweightScenario='placement',
    [string]$CacheRecord='',
    [ValidatePattern('^[a-fA-F0-9]{7,40}$')][string]$ReferenceRevision='9042bea'
)
$ErrorActionPreference='Stop'
if($ReorderOnly-and$Case-ne'ArrangementTransitions'){throw 'ReorderOnly belongs to ArrangementTransitions.'}
if($ProfileMatrix-and($Case-ne'ArrangementTransitions'-or$ReorderOnly)){throw 'ProfileMatrix belongs to ArrangementTransitions and excludes ReorderOnly.'}
if($HandoffOnly-and$Case-ne'OrderTransfer'){throw 'HandoffOnly belongs to OrderTransfer.'}
if($PSBoundParameters.ContainsKey('LightweightScenario')-and$Case-ne'LightweightShell'){throw 'LightweightScenario belongs to LightweightShell.'}
if($CacheRecord-and$Case -notin @('RestoreAll','RestoreFreeBalance','RestoreVisual','FreeRestorePhases','FreeRestoreIdentical')){throw 'CacheRecord belongs to restoration investigations.'}
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$stage=[IO.Path]::GetFullPath((Join-Path $project 'tests\regression\cases\36-reactivity-prototype'))
if(Test-Path -LiteralPath $stage){throw 'The temporary experiment case directory already exists; it will not be overwritten.'}
$patterns=@{Placement='01-*';ObserveExplorer='02-*';CompareDetails='03-*';CompareList='07-*';DesktopPlacement='04-*';ProfilePlacement='08-*';VisualTiming='09-*';CachedViews='06-*';SelectionStrategies='10-*';SelectionIndexes='11-*';SelectionCallbacks='12-*';SelectionFresh='13-*';SelectionRestore='14-*';SelectionErrors='15-*';SelectionPaired='16-*';SelectionOptimized='17-*';SelectionMode='18-*';SelectionKeyboard='19-*';ScrollPercent='20-*';ScrollProvider='21-*';SelectionInternals='22-*';SelectionMatrix='23-*';SelectionAccessibility='24-*';SelectionComparisonCounts='25-*';SelectionCausal='26-*';SelectionAutoArrange='27-*';ArrangementTransitions='29-*';OrderTransfer='30-*';DirectListView='31-*';DirectShellPlacement='32-*';LightweightShell='33-*';PrepareManualClear='35-*';RestoreAll='38-*';RestoreFreeBalance='39-*';RestoreVisual='40-*';FreeRestorePhases='41-*';FreeRestoreIdentical='43-*'}
$files=@('01-Placement.ps1','02-ObserveExplorer.ps1','03-CompareDetails.ps1','04-DesktopPlacement.ps1','06-CachedViews.ps1','07-CompareList.ps1','08-ProfilePlacement.ps1','09-VisualTiming.ps1','VisualTimingTests.cpp','VisualTimingTests.rc','PresentationTiming.h','Fixture.ps1',
    'PlacementTests.cpp','ExplorerObservationTests.cpp','ExplorerObservationTests.rc','DesktopPlacementTests.cpp','DesktopPlacementTests.rc','CachedViewTests.cpp','DumpStacks.cpp','10-SelectionStrategies.ps1','11-SelectionIndexes.ps1','12-SelectionCallbacks.ps1','13-SelectionFresh.ps1','14-SelectionRestore.ps1','15-SelectionErrors.ps1','16-SelectionPaired.ps1','17-SelectionOptimized.ps1','18-SelectionMode.ps1','19-SelectionKeyboard.ps1','20-ScrollPercent.ps1','21-ScrollProvider.ps1','ScrollProviderTests.cpp','SelectionStrategiesTests.cpp','SelectionReference.inc','22-SelectionInternals.ps1','SelectionInternalsTests.cpp','23-SelectionMatrix.ps1','24-SelectionAccessibility.ps1','25-SelectionComparisonCounts.ps1','26-SelectionCausal.ps1','27-SelectionAutoArrange.ps1','29-ArrangementTransitions.ps1','ArrangementTransitionsTests.cpp','30-OrderTransfer.ps1','OrderTransferTests.cpp','31-DirectListView.ps1','DirectListViewTests.cpp','32-DirectShellPlacement.ps1','DirectShellPlacementTests.cpp','33-LightweightShell.ps1','LightweightShellTests.cpp','SettingsReference.inc','35-PrepareManualSelectionClear.ps1','SelectionClearTests.cpp','SelectionClearPrototype.h','Run-ManualSelectionClear.ps1')
if($Case -in @('RestoreAll','RestoreFreeBalance','RestoreVisual','FreeRestorePhases','FreeRestoreIdentical')){$files+=@('38-RestoreAll.ps1','39-RestoreFreeBalance.ps1','40-RestoreVisual.ps1','RestoreAllTests.cpp','RestoreAllBaselineTests.cpp','FreeRestorePhaseTests.cpp','41-FreeRestorePhases.ps1','43-FreeRestoreIdentical.ps1')}
foreach($name in $files){if(!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))){throw ('Missing experiment source: '+$name)}}
if($Case -notin @('RestoreAll','RestoreFreeBalance','RestoreVisual','FreeRestorePhases','FreeRestoreIdentical')){foreach($directory in @('baseline','hybrid')){foreach($name in @('FileSpacer.exe','FileSpacerFolder.dll')){
    if(!(Test-Path -LiteralPath (Join-Path $project ('build\'+$directory+'\'+$name)))){throw ('Build variant unavailable: '+$directory+'\'+$name)}
}}
}
$output='build\regression\experiment-'+$Case.ToLowerInvariant()+'-'+[Guid]::NewGuid().ToString('N')
$written=@()
[void][IO.Directory]::CreateDirectory($stage)
Push-Location -LiteralPath $project
try{
    if($Case -in @('RestoreAll','RestoreFreeBalance','RestoreVisual','FreeRestorePhases','FreeRestoreIdentical')){
        [void][IO.Directory]::CreateDirectory((Join-Path $project 'build\restore-all-reference'))
        & (Join-Path $env:ProgramFiles 'Git\bin\bash.exe') -lc ("git show ${ReferenceRevision}:src/ShellViewSelection.cpp > build/restore-all-reference/ShellViewSelection.cpp")
        if($LASTEXITCODE-ne0){throw 'Cannot prepare the exact prior product source.'}
    }
    if($Case-eq'FreeRestorePhases'){ & (Join-Path $PSScriptRoot 'Prepare-FreeRestorePhases.ps1') -ProjectRoot $project }
    foreach($name in $files){
        $target=Join-Path $stage $name;Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $target;$written+=$target
        if($CacheRecord-and$name -in @('38-RestoreAll.ps1','39-RestoreFreeBalance.ps1','40-RestoreVisual.ps1','41-FreeRestorePhases.ps1','43-FreeRestoreIdentical.ps1')){
            $record=Get-Item -LiteralPath $CacheRecord
            if($record.Length-ne160-or$record.Name -notmatch '^[0-9a-f]{32,40}-[0-9a-f]+-s[0-9]+[.]bin$'){throw 'A runtime-generated 160-byte selection cache is required.'}
            # Fixture only; the client revalidates the exact loaded DLL and CFG.
            $encoded=[Convert]::ToBase64String([IO.File]::ReadAllBytes($record.FullName))
            $seed="[IO.File]::WriteAllBytes((Join-Path `$cache '"+$record.Name+"'),[Convert]::FromBase64String('"+$encoded+"'))`r`n"
            $text=[IO.File]::ReadAllText($target)
            if(!$text.Contains('try {')){throw 'Restoration cache fixture insertion point changed.'}
            [IO.File]::WriteAllText($target,$text.Replace('try {',$seed+'try {'),[Text.UTF8Encoding]::new($false))
        }
        if($Case-eq'LightweightShell'-and$name-eq'33-LightweightShell.ps1'){
            $text=[IO.File]::ReadAllText($target);$text=$text.Replace("'placement'","'"+$LightweightScenario+"'");[IO.File]::WriteAllText($target,$text,[Text.UTF8Encoding]::new($false))
        }
        if($HandoffOnly-and$name-eq'30-OrderTransfer.ps1'){
            $text=[IO.File]::ReadAllText($target);$default='[switch]$HandoffOnly'
            if(!$text.Contains($default)){throw 'Handoff selection no longer matches the staging contract.'}
            [IO.File]::WriteAllText($target,$text.Replace($default,$default+'=$true'),[Text.UTF8Encoding]::new($false))
        }
        if($ProfileMatrix-and$name-eq'29-ArrangementTransitions.ps1'){
            $text=[IO.File]::ReadAllText($target);$default='[switch]$ProfileMatrix'
            if(!$text.Contains($default)){throw 'Profile matrix selection no longer matches the staging contract.'}
            [IO.File]::WriteAllText($target,$text.Replace($default,$default+'=$true'),[Text.UTF8Encoding]::new($false))
        }
        if($ReorderOnly-and$name-eq'29-ArrangementTransitions.ps1'){
            $text=[IO.File]::ReadAllText($target);$default='[switch]$ReorderOnly'
            if(!$text.Contains($default)){throw 'Reorder selection no longer matches the staging contract.'}
            [IO.File]::WriteAllText($target,$text.Replace($default,$default+'=$true'),[Text.UTF8Encoding]::new($false))
        }
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
