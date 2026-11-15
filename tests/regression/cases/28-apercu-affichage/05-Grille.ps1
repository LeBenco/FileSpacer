# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'GridDrag.ps1')
$failure=$null
function Read-GridState {
    Use-UiView $window {param($view) @{Window=$view.Window.ToInt64();Positions=$view.Positions();Selected=(@($view.Selected())|Sort-Object)-join ';';Focus=$view.Focused();Grid=$view.SnapToGrid;Auto=$view.AutoArrange}}
}
function Set-GridSeed {
    Use-UiView $window {param($view) if($view.SnapToGrid){$view.SnapToGrid=$false}}
    Wait-Ui {Use-UiView $window {param($view) !$view.SnapToGrid-and!$view.AutoArrange}} 'native placement settings settled before writing fixture coordinates' 150
    Use-UiView $window {
        param($view)
        $names=@($view.Names()|Sort-Object)
        $view.Select($names[0]);$view.Select($names[1],$true)
        for($i=0;$i-lt$names.Count;++$i){$view.Position($names[$i],43+($i%3)*220,37+[int][Math]::Floor($i/3)*190)}
    }
    Wait-Ui {
        $state=Read-GridState
        $script:GridLastObserved=$state
        !$state.Grid-and!$state.Auto-and$state.Positions-eq'item-0000=43,37;item-0001=263,37;item-0002=483,37;item-0003=43,227;item-0004=263,227;item-0005=483,227'
    } 'explicit off-grid fixture coordinates and free-placement flags' 150
    return Read-GridState
}
function Choose-GridCommand([int]$Command) {
    $initialMode=Read-PreviewMode $window
    $null=Open-PreviewMenu $window
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    $index=-1
    for($i=0;$i-lt$popup.Entries.Count;++$i){if($popup.Entries[$i].Command-eq$Command){$index=$i;break}}
    Assert-Ui ($index-ge0) ('Grid command absent from View menu: '+$Command)
    Assert-Ui (($popup.Entries[$index].State-band3)-eq0) 'Grid command unexpectedly disabled.'
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0)
    Wait-Ui {$mode=Read-PreviewMode $window;$mode.Size-ne$initialMode.Size} 'another icon size previewed before the one-shot command'
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$index,$window)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window $initialMode
}

try {
    Initialize-Ui 'icon-grid' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot 'GridFixture'
    [void][IO.Directory]::CreateDirectory($folder)
    for($i=0;$i-lt6;++$i){[void][IO.Directory]::CreateDirectory((Join-Path $folder ('item-{0:D4}'-f$i)))}
    $window=Open-UiFolder $folder 6
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,40,40,850,600,0x14)) 'Cannot size grid fixture.'
    Set-GridMode
    $initial=Set-GridSeed
    Use-UiView $window {param($view) $view.SnapToGrid=$true}
    $native=Read-GridState
    @($initial,$native)|ConvertTo-Json -Depth 4|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-initial.json') -Encoding UTF8
    Assert-Ui ($native.Positions-ne$initial.Positions) 'Off-grid fixture was already aligned.'
    $initial=Set-GridSeed
    Choose-GridCommand (Get-UiId 'IDM_REALIGN_ICONS')
    Wait-Ui { (Read-GridState).Positions-eq$native.Positions } 'one-shot positions equal the native grid' 150
    $aligned=Read-GridState
    Assert-Ui (!$aligned.Grid-and!$aligned.Auto-and$aligned.Selected-eq$initial.Selected-and$aligned.Focus-eq$initial.Focus) 'One-shot alignment changed flags, selection or focus.'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 6
    Wait-Ui { (Read-GridState).Positions-eq$native.Positions } 'one-shot grid positions survive reopening' 150
    Assert-Ui (!(Read-GridState).Grid) 'One-shot alignment became a persistent grid setting.'
    Write-Host 'PASS: one-shot native grid, unchanged selection/focus/flags, persisted coordinates.'
    $initial=Set-GridSeed
    Move-GridIcons $false
    $free=Read-GridState
    Assert-Ui ($free.Positions-ne$initial.Positions) 'Native drag did not move the intended selection.'
    Use-UiView $window {param($view) $view.SnapToGrid=$true}
    $nativeDrop=Read-GridState
    $initial=Set-GridSeed
    Move-GridIcons $true
    $snapped=Read-GridState
    @($free,$nativeDrop,$snapped)|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-drag.json') -Encoding UTF8
    Assert-Ui (!$snapped.Grid-and!$snapped.Auto-and$snapped.Selected-eq$free.Selected-and$snapped.Focus-eq$free.Focus) 'S changed the persistent placement settings or native selection/focus.'
    # The native-grid comparison covers actual coordinates, not a hard-coded pitch or origin.
    $expected=@{};foreach($entry in ($nativeDrop.Positions-split';')){$pair=$entry-split'=',2;$expected[$pair[0]]=$pair[1]}
    $actual=@{};foreach($entry in ($snapped.Positions-split';')){$pair=$entry-split'=',2;$actual[$pair[0]]=$pair[1]}
    foreach($name in ($free.Selected-split';')){Assert-Ui ($actual[$name]-eq$expected[$name]) ('S did not use the native grid for '+$name)}
    $original=@{};foreach($entry in ($initial.Positions-split';')){$pair=$entry-split'=',2;$original[$pair[0]]=$pair[1]}
    foreach($name in @($original.Keys|Where-Object {$_-notin($free.Selected-split';')})){
        Assert-Ui ($actual[$name]-eq$original[$name]) ('S moved an unselected icon: '+$name)
    }
    $null=Set-GridSeed
    Move-GridIcons $true $true
    Wait-Ui { (Read-GridState).Positions-eq$free.Positions } 'releasing S before the drop retains native free placement' 150
    Write-Host 'PASS: real Shell drag with S uses the native grid; releasing S leaves free placement.'
    Close-UiFolder $window
} catch {
    $failure=$_
    @{Message=$_.Exception.Message;Stack=$_.ScriptStackTrace;Observed=$script:GridLastObserved}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'first-error.json') -Encoding UTF8
    throw
} finally {
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    [FileSpacerRegression.AltDragInput]::End()
    Stop-Ui -Failure $failure
}
