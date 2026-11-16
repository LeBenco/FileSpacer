# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'GridDrag.ps1')
$failure=$null
$feedbackFailures=New-Object Collections.Generic.List[string]
function Check-PlacementFeedback([scriptblock]$Condition,[string]$Description) {
    try { Wait-Ui $Condition $Description 150 5 }
    catch {
        $feedbackFailures.Add(($script:PlacementRoute+': '+$_.Exception.Message))
        @($feedbackFailures)|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'feedback-failures.json') -Encoding UTF8
    }
}
function Read-PlacementState {
    Use-UiView $window {param($view) @{Positions=$view.Positions()}}
}
function Get-PlacementPoints($State) {
    $points=@{}
    foreach($entry in ($State.Positions-split';')){$pair=$entry-split'=',2;$points[$pair[0]]=@($pair[1]-split','|ForEach-Object {[int]$_})}
    return $points
}
function Get-PlacementCursor([string]$Name,[bool]$Label=$false) {
    $positions=Get-PlacementPoints (Read-PlacementState)
    $origin=[FileSpacerViewMenuTests.GridKey]::Origin($list)
    $bounds=[FileSpacerRegression.Native]::ClientBounds($list)
    $point=New-Object FileSpacerRegression.MousePoint
    $point.X=$bounds.Left+$positions[$Name][0]-$origin[0]+24
    $point.Y=$bounds.Top+$positions[$Name][1]-$origin[1]+24
    if($Label){
        $index=Use-UiView $window {param($view) $view.IndexOf($Name)}
        $icon=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$index)
        $point.X=$bounds.Left+[int](($icon[0]+$icon[2])/2)
        $point.Y=$bounds.Top+$icon[3]+8
    }
    return $point
}
function Get-DestinationHighlight {
    $index=Use-UiView $window {param($view) $view.IndexOf('Destination')}
    return ([FileSpacerRegression.Native]::Message($list,0x102c,$index,8)-band8)-ne0
}
function Start-PlacementDrag([string]$Name,$Target) {
    Activate-Ui $window
    Use-UiView $window {param($view) $view.Select($Name)}
    $script:before=Read-PlacementState
    $start=Get-PlacementCursor $Name
    [FileSpacerRegression.AltDragInput]::Begin($window,$start,$false,$false)
    Wait-Ui {![FileSpacerRegression.AltDragInput]::Idle($window)} 'native item press captured before motion' 0 5
    for($step=1;$step-le6;++$step){
        $point=New-Object FileSpacerRegression.MousePoint
        $point.X=$start.X+[int](($Target.X-$start.X)*$step/6)
        $point.Y=$start.Y+[int](($Target.Y-$start.Y)*$step/6)
        [FileSpacerRegression.AltDragInput]::Move($point)
    }
}
function Complete-PlacementDrag {
    [FileSpacerRegression.AltDragInput]::ReleasePrimary()
    Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'placement drag completed' 150
}
function Assert-StationaryPositions($Before,$After,[string]$Moving) {
    $original=Get-PlacementPoints $Before;$actual=Get-PlacementPoints $After
    foreach($name in $original.Keys){if($name-ne$Moving){Assert-Ui (($original[$name]-join',')-ceq($actual[$name]-join',')) ('Stationary icon moved: '+$name)}}
}
try {
    Initialize-Ui 'free-drop-targets' -FixtureParent (Join-Path $ProjectRoot 'build')
    # These routes use different IDropTarget implementations; both are required.
    foreach($mixed in @($false,$true)){
        $script:PlacementRoute=if($mixed){'mixed-provider'}else{'native-provider'}
        Set-UiPreference 'MixedFolderSorting' ([int]$mixed)
        $folder=Join-Path $script:UiRoot $script:PlacementRoute
        [void][IO.Directory]::CreateDirectory((Join-Path $folder 'Destination'))
        foreach($name in @('Move-a.txt','Move-b.txt')){[IO.File]::WriteAllText((Join-Path $folder $name),('payload-'+$name))}
        $window=Open-UiFolder $folder 3
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,40,40,850,600,0x14)) 'Cannot size placement fixture.'
        Set-GridMode
        Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
        Wait-Ui {Use-UiView $window {param($view) (($view.FolderClassId-eq[guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604')-eq$mixed)}} 'requested native or mixed drop-target route' 150
        $body=Use-UiView $window {param($view) $view.Window}
        $lists=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})
        Assert-Ui ($lists.Count-eq1) 'Expected one interactive native list.'
        $list=$lists[0]
        # Obtain a destination on the real native grid; no fixed pitch or phase.
        Use-UiView $window {param($view) $view.SnapToGrid=$true}
        $native=Get-PlacementPoints (Read-PlacementState)
        Use-UiView $window {param($view)
            $view.SnapToGrid=$false
            $view.Position('Move-a.txt',360,130);$view.Position('Move-b.txt',550,130)
        }
        Wait-Ui {Use-UiView $window {param($view) !$view.AutoArrange-and!$view.SnapToGrid}} 'free placement ready' 150
        # Occupied grid destination: feedback and placement must both remain valid.
        Use-UiView $window {param($view) $view.Position('Move-a.txt',360,130)}
        $image=Get-PlacementCursor 'Destination'
        Start-PlacementDrag 'Move-a.txt' $image
        [FileSpacerViewMenuTests.GridKey]::Hold($true)
        Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'S frame on occupied folder cell' 0 5
        Check-PlacementFeedback {!(Get-DestinationHighlight)} 'S reserves the occupied folder image for placement'
        $mark=[FileSpacerViewMenuTests.GridKey]::Marker($body)
        $origin=[FileSpacerViewMenuTests.GridKey]::Origin($list);$client=[FileSpacerRegression.Native]::ClientBounds($list)
        $expected=@($mark.Left-$client.Left+$origin[0],$mark.Top-$client.Top+$origin[1])
        Complete-PlacementDrag
        [FileSpacerViewMenuTests.GridKey]::Hold($false)
        Wait-Ui {$points=Get-PlacementPoints (Read-PlacementState);($points['Move-a.txt']-join',')-ceq($expected-join',')} 'S drop matches its preview' 150
        $after=Read-PlacementState
        Assert-StationaryPositions $before $after 'Move-a.txt'
        Assert-Ui (($expected-join',')-ceq($native['Destination']-join',')) 'The occupied native grid cell was not used.'
        Assert-Ui ((Test-Path (Join-Path $folder 'Move-a.txt'))-and!(Test-Path (Join-Path $folder 'Destination/Move-a.txt'))) 'S transferred a file into the folder.'
        Write-Host 'PASS: S places on an occupied folder cell without transferring files or moving stationary icons.'
        # A cached target must re-enter its native handler when S is released.
        Use-UiView $window {param($view) $view.Position('Move-a.txt',360,130)}
        Start-PlacementDrag 'Move-b.txt' $image
        [FileSpacerViewMenuTests.GridKey]::Hold($true)
        Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'S frame before returning to a native drop' 0 5
        [FileSpacerViewMenuTests.GridKey]::Hold($false)
        $next=New-Object FileSpacerRegression.MousePoint;$next.X=$image.X+1;$next.Y=$image.Y
        [FileSpacerRegression.AltDragInput]::Move($next)
        Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible-and(Get-DestinationHighlight)} 'native folder highlight restored after releasing S' 150 5
        Complete-PlacementDrag
        $target=Join-Path $folder 'Destination/Move-b.txt'
        Wait-Ui {(Test-Path $target)-and!(Test-Path (Join-Path $folder 'Move-b.txt'))} 'native image drop moves the file into the folder'
        Assert-Ui ([IO.File]::ReadAllText($target)-ceq'payload-Move-b.txt') 'Native drop changed file contents.'
        Wait-Ui {(Use-UiView $window {param($view) $view.Count})-eq2} 'source view reflects the native file move'
        Assert-StationaryPositions $before (Read-PlacementState) 'Move-b.txt'
        Write-Host 'PASS: releasing S restores the native file drop with exact file contents.'
        # The label lies outside the image square but inside the target item.
        $label=Get-PlacementCursor 'Destination' $true
        $index=Use-UiView $window {param($view) $view.IndexOf('Destination')}
        @{State=Read-PlacementState;Label=@($label.X,$label.Y);Icon=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$index);Client=[FileSpacerRegression.Native]::ClientBounds($list);Mode=Read-PreviewMode $window;Provider=Use-UiView $window {param($view) $view.FolderClassId}}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN ($script:PlacementRoute+'-geometry.json')) -Encoding UTF8
        Start-PlacementDrag 'Move-a.txt' $label
        Assert-Ui (![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible) 'A free drag without S displayed a square.'
        Check-PlacementFeedback {!(Get-DestinationHighlight)} 'label placement clears the folder drop highlight'
        Complete-PlacementDrag
        $after=Read-PlacementState
        Assert-Ui ($after.Positions-ne$before.Positions) 'Dropping on the label did not reposition the icon.'
        Assert-StationaryPositions $before $after 'Move-a.txt'
        Assert-Ui ((Test-Path (Join-Path $folder 'Move-a.txt'))-and!(Test-Path (Join-Path $folder 'Destination/Move-a.txt'))) 'A label placement transferred the file.'
        Write-Host ('Observed file and placement outcomes: '+$script:PlacementRoute)
        Close-UiFolder $window
    }
    Assert-Ui ($feedbackFailures.Count-eq0) ($feedbackFailures-join'; ')
} catch {
    $failure=$_
    @{Message=$_.Exception.Message;Stack=$_.ScriptStackTrace;Route=$script:PlacementRoute}|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'first-error.json') -Encoding UTF8
    throw
} finally {
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    [FileSpacerRegression.AltDragInput]::End()
    Stop-Ui -Failure $failure
}
