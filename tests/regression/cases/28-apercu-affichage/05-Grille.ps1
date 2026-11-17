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
function Assert-GridPrediction([int]$Size) {
    $body=Use-UiView $window {param($view) $view.Window}
    $list=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})
    Assert-Ui ($list.Count-eq1) 'Expected one interactive Shell list.'
    $client=$script:GridMarkerClientBeforeDrop
    $spacing=[FileSpacerRegression.Native]::Message($list[0],0x1033,0,0)-band0xffff
    $mark=$script:GridMarkerBeforeDrop
    $origin=$script:GridMarkerOriginBeforeDrop
    $predictedX=$mark.Left-$client.Left+$origin[0]
    $predictedY=$mark.Top-$client.Top+$origin[1]
    $point=@((Read-GridState).Positions-split';'|Where-Object {$_-like'item-0000=*'})[0]-split'=',2
    Assert-Ui ($point[1]-eq('{0},{1}'-f$predictedX,$predictedY)) ('The marker did not predict the actual snapped icon position: actual='+$point[1]+' predicted='+$predictedX+','+$predictedY+' spacing='+$spacing+' client='+($client|ConvertTo-Json -Compress))
}

function Move-PlacementFrame([string]$Anchor) {
    Activate-Ui $window
    $body=Use-UiView $window {param($view) $view.Window}
    $start=[FileSpacerRegression.AltDragInput]::ItemPoint($body,$Anchor)
    $bounds=[FileSpacerRegression.Native]::ClientBounds($body)
    $target=New-Object FileSpacerRegression.MousePoint
    $target.X=$bounds.Left+620;$target.Y=$bounds.Top+345
    try {
        [FileSpacerRegression.AltDragInput]::Begin($window,$start,$false,$false)
        for($step=1;$step-le6;++$step){
            $point=New-Object FileSpacerRegression.MousePoint
            $point.X=$start.X+[int](($target.X-$start.X)*$step/6)
            $point.Y=$start.Y+[int](($target.Y-$start.Y)*$step/6)
            [FileSpacerRegression.AltDragInput]::Move($point)
        }
        Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'permanent placement frame appears without S' 0 5
        $frame=[FileSpacerViewMenuTests.GridKey]::Marker($body)
        $list=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})[0]
        $clientBefore=[FileSpacerRegression.Native]::ClientBounds($list)
        $originBefore=[FileSpacerViewMenuTests.GridKey]::Origin($list)
        [FileSpacerViewMenuTests.GridKey]::Hold($true)
        [FileSpacerViewMenuTests.GridKey]::Hold($false)
        Assert-Ui ([FileSpacerViewMenuTests.GridKey]::Marker($body).Visible) 'Releasing S disabled a permanent placement frame.'
        $script:GridDropStartedAt=[Diagnostics.Stopwatch]::GetTimestamp()
        [FileSpacerRegression.AltDragInput]::ReleasePrimary()
        Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native permanent-placement drop completed' 150
        Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'permanent placement frame removed on release'
        $list=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})[0]
        $client=[FileSpacerRegression.Native]::ClientBounds($list)
        $item=Use-UiView $window {param($view) $view.IndexOf($Anchor)}
        Assert-Ui ($item-ge0) 'Dragged anchor is absent after drop.'
        $icon=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$item)
        $originAfter=[FileSpacerViewMenuTests.GridKey]::Origin($list)
        Assert-Ui ($frame.Left-$clientBefore.Left+$originBefore[0]-eq$icon[0]+$originAfter[0]-and$frame.Top-$clientBefore.Top+$originBefore[1]-eq$icon[1]+$originAfter[1]) ('Placement frame differs from actual icon destination: '+($frame|ConvertTo-Json -Compress)+' actual='+($icon-join','))
        return $frame
    } finally {[FileSpacerViewMenuTests.GridKey]::Hold($false);[FileSpacerRegression.AltDragInput]::End()}
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
    $restoredPositions=Get-PreviewRestoredPositions $window $native.Positions
    Close-UiFolder $window
    $window=Open-UiFolder $folder 6
    Wait-Ui { (Read-GridState).Positions-eq$restoredPositions } 'one-shot grid positions survive reopening through integer DPI storage' 150
    Assert-Ui (!(Read-GridState).Grid) 'One-shot alignment became a persistent grid setting.'
    Write-Host 'PASS: one-shot native grid, unchanged selection/focus/flags, persisted coordinates.'
    $initial=Set-GridSeed
    Move-GridIcons $false
    $free=Read-GridState
    $freeDelta=$script:GridRawDelta
    Assert-Ui ($free.Positions-ne$initial.Positions) 'Native drag did not move the intended selection.'
    Use-UiView $window {param($view) $view.SnapToGrid=$true}
    $nativeDrop=Read-GridState
    $initial=Set-GridSeed
    Move-GridIcons $true $false $true
    $snapped=Read-GridState
    @($free,$nativeDrop,$snapped)|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'grid-drag.json') -Encoding UTF8
    Assert-Ui (!$snapped.Grid-and!$snapped.Auto-and$snapped.Selected-eq$free.Selected-and$snapped.Focus-eq$free.Focus) 'S changed the persistent placement settings or native selection/focus.'
    # The native-grid comparison covers actual coordinates, not a hard-coded pitch or origin.
    $expected=@{};foreach($entry in ($nativeDrop.Positions-split';')){$pair=$entry-split'=',2;$expected[$pair[0]]=$pair[1]}
    $actual=@{};foreach($entry in ($snapped.Positions-split';')){$pair=$entry-split'=',2;$actual[$pair[0]]=$pair[1]}
    Assert-GridPrediction 48
    foreach($name in ($free.Selected-split';')){Assert-Ui ($actual[$name]-eq$expected[$name]) ('S did not use the native grid for '+$name)}
    $original=@{};foreach($entry in ($initial.Positions-split';')){$pair=$entry-split'=',2;$original[$pair[0]]=$pair[1]}
    foreach($name in @($original.Keys|Where-Object {$_-notin($free.Selected-split';')})){
        Assert-Ui ($actual[$name]-eq$original[$name]) ('S moved an unselected icon: '+$name)
    }
    $null=Set-GridSeed
    Move-GridIcons $true $true $true
    # Native scrolling may differ between the reference and S-release gestures.
    # Apply their exact document-coordinate delta difference to the native result.
    $expectedFree=@(foreach($entry in($free.Positions-split';')) {
        $pair=$entry-split'=',2;$point=$pair[1]-split','
        if($pair[0]-in($free.Selected-split';')){
            $pair[0]+'='+([int]$point[0]+$script:GridRawDelta[0]-$freeDelta[0])+','+([int]$point[1]+$script:GridRawDelta[1]-$freeDelta[1])
        }else{$entry}
    })-join';'
    Wait-Ui { (Read-GridState).Positions-eq$expectedFree } 'releasing S before the drop retains native free placement at the actual scroll origin' 150
    Write-Host 'PASS: real Shell drag with S uses the native grid; releasing S leaves free placement.'
    # A second size covers zoom-dependent marker geometry, using the actual
    # mode command rather than changing the Shell behind the product.
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,1,$window)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window @{Mode=1;Size=96}
    $null=Set-GridSeed
    Move-GridIcons $true $false $true
    Assert-GridPrediction 96
    Write-Host 'PASS: native marker toggles with S, follows motion, predicts the drop and adapts to icon zoom.'
    $initial=Set-GridSeed
    # Extend the viewport without adding files, to exercise the preview cache
    # while the native view scrolls during an otherwise ordinary drag.
    Use-UiView $window {param($view) $view.Position('item-0005',483,1000)}
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,40,40,850,599,0x14)) 'Cannot refresh the scrollable fixture viewport.'
    $initial=Read-GridState
    Move-GridIcons $true $false $false $false $true
    Wait-Ui {(Read-GridState).Positions-eq$initial.Positions} 'cancelled Snap retains every original position' 150
    Write-Host 'PASS: Escape removes the marker without dropping or moving icons.'
    # Permanent placement uses the same frame without requiring the Snap key.
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,2,$window)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window @{Mode=1;Size=48}
    $null=Set-GridSeed
    Choose-Placement 'IDM_KEEP_ICONS_ALIGNED'
    $gridFrame=Move-PlacementFrame 'item-0000'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 6
    Assert-PlacementChoice 'IDM_KEEP_ICONS_ALIGNED'
    Choose-Placement 'IDM_AUTO_ARRANGE_ICONS'
    # Click the later selected icon: prediction must account for all selected
    # slots removed before the native insertion point and for the anchor offset.
    Use-UiView $window {param($view) $view.Select('item-0000');$view.Select('item-0002',$true)}
    $autoFrame=Move-PlacementFrame 'item-0002'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 6
    Assert-PlacementChoice 'IDM_AUTO_ARRANGE_ICONS'
    Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 6
    Assert-PlacementChoice 'IDM_FREE_PLACEMENT_ICONS'
    # Small icons and Tiles use different native icon rectangles and cell sizes.
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,3,$window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window 2
    Choose-Placement 'IDM_KEEP_ICONS_ALIGNED'
    $smallFrame=Move-PlacementFrame 'item-0000'
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,6,$window)
    Wait-PreviewMenuClosed $window
    Wait-UiViewMode $window 6
    Choose-Placement 'IDM_AUTO_ARRANGE_ICONS'
    Use-UiView $window {param($view) $view.Select('item-0000')}
    $tileFrame=Move-PlacementFrame 'item-0000'
    @{Grid=$gridFrame;Automatic=$autoFrame;SmallIcons=$smallFrame;Tiles=$tileFrame}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'placement-frames.json') -Encoding UTF8
    Write-Host 'PASS: three exclusive placement modes persist, and permanent frames predict native grid and automatic drops across icon views.'
    # A physical held key can repeat after the mouse button was released.
    # Reserve that whole press for Snap, then restore ordinary type-to-select.
    Set-GridMode
    Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
    [IO.File]::WriteAllText((Join-Path $folder 'S-probe.txt'),'fixture')
    Wait-Ui {(Use-UiView $window {param($view) $view.Count})-eq7} 'S-prefixed fixture appears'
    Use-UiView $window {param($view)
        for($i=0;$i-lt6;++$i){$view.Position(('item-{0:D4}'-f$i),43+($i%3)*220,37+[int][Math]::Floor($i/3)*190)}
        $view.Position('S-probe.txt',680,37);$view.Select('item-0000')
    }
    $beforeKey=Read-GridState
    Move-GridIcons $true $false $false $false $false $true
    Wait-Ui {$state=Read-GridState;$state.Selected-eq$beforeKey.Selected-and$state.Focus-eq$beforeKey.Focus} 'Snap key repeats do not change selection or focus after the drop' 250
    [FileSpacerRegression.Native]::Keys($window,0x53,$false,$false)
    Wait-Ui {(Read-GridState).Selected-eq'S-probe.txt'} 'ordinary S typing works again outside the drag'
    Write-Host 'PASS: a held Snap key cannot type-select another file after dropping; ordinary name search is restored on release.'
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
