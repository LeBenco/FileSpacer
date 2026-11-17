# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\..\regression\cases\28-apercu-affichage\GridDrag.ps1')
$failure=$null
$records=New-Object Collections.Generic.List[object]
function Get-GeometryList {
    $body=Use-UiView $window {param($view) $view.Window}
    $lists=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})
    Assert-Ui ($lists.Count-eq1) 'Exactly one interactive native icon list is required.'
    return $lists[0]
}
function Compare-FrameDrop($Variant) {
    $list=Get-GeometryList
    Use-UiView $window {param($view) $view.Select('item-0000');$view.Reveal('item-0000')}
    $before=Read-PreviewState $window
    $body=Use-UiView $window {param($view) $view.Window}
    $bounds=[FileSpacerRegression.Native]::ClientBounds($list)
    $index=Use-UiView $window {param($view) $view.IndexOf('item-0000')}
    $image=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$index)
    # A label's UIA point can sit below a very large image: that grab offset
    # would put the intended frame partly outside the viewport, where it is hidden.
    $start=New-Object FileSpacerRegression.MousePoint
    $start.X=$bounds.Left+[int](($image[0]+$image[2])/2)
    $start.Y=$bounds.Top+[int](($image[1]+$image[3])/2)
    $target=New-Object FileSpacerRegression.MousePoint
    $target.X=$bounds.Left+[int]($bounds.Width*.65);$target.Y=$bounds.Top+[int]($bounds.Height*.55)
    if($Variant.Placement-eq'Automatic'){
        # Stay in an actual native insertion zone, rather than empty space
        # several rows beyond the six-item fixture.
        $last=Use-UiView $window {param($view) $view.IndexOf('item-0005')}
        $image=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$last)
        $target.X=$bounds.Left+$image[2]+2;$target.Y=$bounds.Top+[int](($image[1]+$image[3])/2)
    }
    try {
        [FileSpacerRegression.AltDragInput]::Begin($window,$start,$false,$false)
        Wait-Ui {![FileSpacerRegression.AltDragInput]::Idle($window)} 'native icon press captured' 0 5
        for($step=1;$step-le6;++$step){
            $point=New-Object FileSpacerRegression.MousePoint
            $point.X=$start.X+[int](($target.X-$start.X)*$step/6)
            $point.Y=$start.Y+[int](($target.Y-$start.Y)*$step/6)
            [FileSpacerRegression.AltDragInput]::Move($point)
        }
        if($Variant.Placement-eq'Free'){[FileSpacerViewMenuTests.GridKey]::Hold($true)}
        @{Variant=$Variant;Client=([FileSpacerRegression.Native]::ClientBounds($list));Origin=([FileSpacerViewMenuTests.GridKey]::Origin($list));Icon=([FileSpacerViewMenuTests.GridKey]::IconBounds($list,0));Start=$start;Target=$target;Mode=(Read-PreviewMode $window);State=(Read-PreviewState $window)}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'last-gesture.json') -Encoding UTF8
        Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'prediction frame visible' 0 5
        $frame=[FileSpacerViewMenuTests.GridKey]::Marker($body)
        $originBefore=[FileSpacerViewMenuTests.GridKey]::Origin($list)
        $clientBefore=[FileSpacerRegression.Native]::ClientBounds($list)
        [FileSpacerRegression.AltDragInput]::ReleasePrimary()
        Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native icon drop finished' 150
        Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'frame removed after drop'
        $index=Use-UiView $window {param($view) $view.IndexOf('item-0000')}
        $icon=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$index)
        $originAfter=[FileSpacerViewMenuTests.GridKey]::Origin($list)
        $predicted=@(($frame.Left-$clientBefore.Left+$originBefore[0]),($frame.Top-$clientBefore.Top+$originBefore[1]))
        $actual=if($Variant.Placement-eq'Free'){
            # Free Snap predicts public item coordinates; permanent placement
            # predicts the native image rectangle, which has its own padding.
            $position=Use-UiView $window {param($view) $view.Position('item-0000')}
            @((($position-split'=',2)[1]-split',')|ForEach-Object {[int]$_})
        } else {@(($icon[0]+$originAfter[0]),($icon[1]+$originAfter[1]))}
        $after=Read-PreviewState $window
        $record=@{Placement=$Variant.Placement;Mode=$Variant.Mode;Size=$Variant.Size;Scrolled=$Variant.Scrolled;Dpi=[FileSpacerRegression.Native]::GetDpiForWindow($window);Predicted=$predicted;Actual=$actual}
        $records.Add($record)
        ConvertTo-Json -InputObject $records.ToArray() -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'frame-positions.json') -Encoding UTF8
        Assert-Ui (($predicted-join',')-ceq($actual-join',')) ('Frame differs from native destination: '+($record|ConvertTo-Json -Compress))
        Assert-Ui ($after.Selected-eq$before.Selected-and$after.Focus-eq$before.Focus) 'Prediction changed native selection or focus.'
        if($Variant.Placement-eq'Free'){
            $initial=@{};foreach($entry in($before.Positions-split';')){$pair=$entry-split'=',2;$initial[$pair[0]]=$pair[1]}
            foreach($entry in($after.Positions-split';')){$pair=$entry-split'=',2;if($pair[0]-ne'item-0000'){Assert-Ui ($pair[1]-eq$initial[$pair[0]]) ('Snap changed stationary '+$pair[0])}}
        }
    } finally {[FileSpacerViewMenuTests.GridKey]::Hold($false);[FileSpacerRegression.AltDragInput]::End()}
}
try {
    Initialize-Ui 'frame-destination-matrix' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $folder=Join-Path $script:UiRoot 'Items';[void][IO.Directory]::CreateDirectory($folder)
    for($i=0;$i-lt6;++$i){[IO.File]::WriteAllText((Join-Path $folder ('item-{0:D4}'-f$i)),'')}
    $window=Open-UiFolder $folder 6
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+10),($work.Top+10),($work.Width-20),($work.Height-20),0x14)) 'Cannot size geometry fixture.'
    Set-GridMode
    # Mid-scale icon sizes and scroll origins exercise the separate public-grid
    # predictor. The ordinary grid case already covers the common 48px routes.
    $variants=@(foreach($size in @(96,128,192,256)){foreach($scroll in @($false,$true)){@{Placement='Free';Mode=1;Size=$size;Scrolled=$scroll}}})
    $variants+=@(@{Placement='Grid';Mode=1;Size=128;Scrolled=$true},@{Placement='Grid';Mode=1;Size=256;Scrolled=$false},@{Placement='Automatic';Mode=1;Size=192;Scrolled=$false},@{Placement='Automatic';Mode=2;Size=16;Scrolled=$false},@{Placement='Grid';Mode=6;Size=48;Scrolled=$false})
    foreach($variant in $variants){
        Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize($variant.Mode,$variant.Size)}
        Wait-PreviewMode $window @{Mode=$variant.Mode;Size=$variant.Size}
        # The user-facing command completes pending profile changes before seeding.
        Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
        $seed=Use-UiView $window {param($view)
            $pitch=$view.Spacing;$names=@($view.Names()|Sort-Object)
            $points=@(for($i=0;$i-lt$names.Count;++$i){
                $x=43+($i%3)*2*$pitch.X;$y=$(if($variant.Scrolled){1000}else{37})+[int][Math]::Floor($i/3)*2*$pitch.Y
                $view.Position($names[$i],$x,$y);$names[$i]+'='+$x+','+$y
            })
            $view.Select('item-0000');$view.Reveal('item-0000')
            $points-join';'
        }
        Wait-Ui {(Use-UiView $window {param($view) $view.Positions()})-eq$seed} 'exact isolated fixture coordinates before dragging' 150
        if($variant.Placement-eq'Grid'){Choose-Placement 'IDM_KEEP_ICONS_ALIGNED'}
        elseif($variant.Placement-eq'Automatic'){Choose-Placement 'IDM_AUTO_ARRANGE_ICONS'}
        Compare-FrameDrop $variant
    }
    Close-UiFolder $window
    Write-Host 'PASS: native frame/destination geometry across intermediate sizes, scrolling and the remaining placement/view routes.'
} catch {$failure=$_;throw}
finally {[FileSpacerViewMenuTests.GridKey]::Hold($false);[FileSpacerRegression.AltDragInput]::End();Stop-Ui -Failure $failure}
