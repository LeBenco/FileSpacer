# Test-Category: UI
# A folder of 5,000 files opens with saved free positions for every item.
# The product restores them in one SelectAndPositionItems batch under the
# redraw guard (ShellViewIcons.cpp, IconRedrawGuard). The case checks that
# all positions come back identical, that the Shell list control is visible
# again afterwards, and that the restoration stays under the 25 s limit used
# by 01-stockage-sqlite/13 (09-RepriseProvisoire -MeasureRecovery).
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [ValidateRange(2,5000)][int]$ItemCount=5000)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot '..\01-stockage-sqlite\StorageRecoveryUi.cs')
$seed=Build-NativeTest -Name LargeLayoutSeed -CaseDirectory $PSScriptRoot -ProductionSources @('FolderIdentity.cpp','FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
# Same bound as the loaded recovery of case 13: LimitSeconds=25 in 09-RepriseProvisoire.ps1.
$limitSeconds=25
# Layout seeded in 96-DPI units; the product applies MulDiv(value, dpi, 96).
$columns=40;$originX=24;$originY=24;$stepX=120;$stepY=100
function Scale([int]$Value,[uint32]$Dpi) { return [int][Math]::Floor(([int64]$Value*$Dpi+48)/96) }
$failure=$null
try {
    Initialize-Ui -Name 'LargeLayoutRedraw'
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $folder=Join-Path $script:UiRoot 'large-folder'
    [void][IO.Directory]::CreateDirectory($folder)
    for($index=0;$index -lt $ItemCount;++$index){[IO.File]::WriteAllText((Join-Path $folder ('item-{0:D5}' -f $index)),'fixture')}
    $database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
    [void][IO.Directory]::CreateDirectory((Split-Path $database -Parent))
    # The helper reports failures on stderr, which the case log keeps.
    $expectedLines=@(& $seed '--seed' $database $folder $columns $originX $originY $stepX $stepY)
    $seedExit=$LASTEXITCODE
    $expectedLines|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'seeded-layout.txt') -Encoding UTF8
    Assert-Ui ($seedExit -eq 0) ('Layout seeding failed (exit '+$seedExit+').')
    $expected=@{}
    foreach($line in $expectedLines){ if($line -match '^(.+)=(-?\d+),(-?\d+)$'){$expected[$Matches[1]]=@([int]$Matches[2],[int]$Matches[3])} }
    Assert-Ui ($expected.Count -eq $ItemCount) ('Seeded layout has '+$expected.Count+' entries; expected '+$ItemCount+'.')
    $witnessName='item-{0:D5}' -f ($ItemCount-1)
    Assert-Ui ($expected.ContainsKey($witnessName)) ('Seeded layout lacks the witness '+$witnessName+'.')

    $watch=[Diagnostics.Stopwatch]::StartNew()
    $window=Open-UiFolder $folder $ItemCount
    $readyMilliseconds=$watch.Elapsed.TotalMilliseconds
    $dpi=[MutexRecoveryNative]::GetDpiForWindow($window)
    Assert-Ui ($dpi -gt 0) 'Window DPI unavailable.'
    $wanted=$witnessName+'='+(Scale $expected[$witnessName][0] $dpi)+','+(Scale $expected[$witnessName][1] $dpi)
    Wait-Ui { (Use-UiView $window {param($view) $view.Position($witnessName)}) -eq $wanted } ('saved position of '+$witnessName+' restored') 0 $limitSeconds
    $restoredMilliseconds=$watch.Elapsed.TotalMilliseconds
    Write-Host ('DIAG: items='+$ItemCount+', dpi='+$dpi+', view ready='+[int]$readyMilliseconds+' ms, witness '+$witnessName+' restored='+[int]$restoredMilliseconds+' ms since process start, limit='+$limitSeconds+' s')

    $state=Use-UiView $window {param($view) [pscustomobject]@{Mode=$view.Mode;AutoArrange=$view.AutoArrange;Count=$view.Count;ViewWindow=$view.Window.ToInt64();Positions=$view.Positions()}}
    $readMilliseconds=$watch.Elapsed.TotalMilliseconds-$restoredMilliseconds
    Write-Host ('DIAG: full position read of '+$state.Count+' items took '+[int]$readMilliseconds+' ms')
    Assert-Ui ($state.Mode -eq 1 -and !$state.AutoArrange) 'Saved Icons mode without automatic arrangement was not restored.'
    Assert-Ui ($state.Count -eq $ItemCount) ('Item count changed: '+$state.Count)
    $actual=@{}
    foreach($entry in $state.Positions.Split(';')){ if($entry){$pair=$entry.Split('=');$actual[$pair[0]]=$pair[1].Split(',')} }
    $mismatches=New-Object Collections.Generic.List[string]
    foreach($name in $expected.Keys){
        $x=Scale $expected[$name][0] $dpi;$y=Scale $expected[$name][1] $dpi
        if(!$actual.ContainsKey($name)){$mismatches.Add($name+' missing from the view');continue}
        if([int]$actual[$name][0] -ne $x -or [int]$actual[$name][1] -ne $y){$mismatches.Add($name+' expected '+$x+','+$y+' actual '+($actual[$name] -join ','))}
    }
    if($mismatches.Count){$mismatches|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'position-mismatches.txt') -Encoding UTF8}
    $firstMismatch=if($mismatches.Count){$mismatches[0]}else{''}
    Assert-Ui ($actual.Count -eq $ItemCount -and $mismatches.Count -eq 0) ('Restored layout differs: '+$mismatches.Count+' of '+$ItemCount+' positions ('+$firstMismatch+'); see position-mismatches.txt.')
    Assert-Ui ($restoredMilliseconds -le $limitSeconds*1000) ('Restoration took '+[int]$restoredMilliseconds+' ms; limit '+$limitSeconds+' s.')
    # Test-side inspection only: after the batch, SetRedraw(TRUE) must have
    # given the Shell's list control its WS_VISIBLE style back.
    $lists=@([FileSpacerRegression.Native]::Children([IntPtr]$state.ViewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -eq 1) ('Expected one Shell list control under the view window; found '+$lists.Count+'.')
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowVisible($lists[0])) 'Shell list control is not visible after the restoration batch.'
    [ordered]@{ItemCount=$ItemCount;Dpi=$dpi;LimitSeconds=$limitSeconds;ViewReadyMilliseconds=$readyMilliseconds;
        WitnessRestoredMilliseconds=$restoredMilliseconds;FullReadMilliseconds=$readMilliseconds;Witness=$wanted;
        Mode=$state.Mode;AutoArrange=$state.AutoArrange;Mismatches=$mismatches.Count;ListVisible=$true
    }|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'large-layout-redraw.json') -Encoding UTF8
    Close-UiFolder $window
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count -eq 0} 'folder process normal exit'
    Write-Host ('PASS '+$ItemCount+' saved free positions restored identically in '+[int]$restoredMilliseconds+' ms (limit '+$limitSeconds+' s).')
} catch {$failure=$_;throw}
finally { Stop-Ui $failure }
