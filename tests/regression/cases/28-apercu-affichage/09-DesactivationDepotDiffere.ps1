# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'GridDrag.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'DeferredDropActivation.cs')
$native=Build-NativeTest -Name DeferredDropActivation -CaseDirectory $PSScriptRoot -DynamicLibrary -ControlFlowGuard -Standard c++14
$header=[IO.File]::ReadAllText((Join-Path $ProjectRoot 'src/FolderWindow.h'))
$match=[regex]::Match($header,'(?m)^\s*MSG_ICON_SNAP\s*=\s*WM_APP\s*\+\s*(\d+)')
Assert-Ui $match.Success 'The deferred-placement message definition must be read from the current source.'
$message=[uint32](0x8000+[int]$match.Groups[1].Value)
$failure=$null;$target=$null
try {
    Initialize-Ui 'deferred-placement-activation' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 6;$window=Open-UiFolder $folder 6
    Set-GridMode;Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
    Use-UiView $window {
        param($view)
        $names=@($view.Names()|Sort-Object)
        for($i=0;$i-lt$names.Count;++$i){$view.Position($names[$i],43+($i%3)*220,37+[int][Math]::Floor($i/3)*190)}
        $view.Select('item-0000.txt')
    }
    $target=New-Object FileSpacerViewMenuTests.DeferredDropActivation $native
    Activate-Ui $window
    $body=Use-UiView $window {param($view) $view.Window}
    $list=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})[0]
    $index=Use-UiView $window {param($view) $view.IndexOf('item-0000.txt')}
    $icon=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$index);$client=[FileSpacerRegression.Native]::ClientBounds($list)
    $start=New-Object FileSpacerRegression.MousePoint
    $start.X=$client.Left+[int](($icon[0]+$icon[2])/2);$start.Y=$client.Top+[int](($icon[1]+$icon[3])/2)
    $drop=New-Object FileSpacerRegression.MousePoint;$drop.X=$client.Left+420;$drop.Y=$client.Top+345
    $before=Use-UiView $window {param($view) $view.Positions()}
    [FileSpacerRegression.AltDragInput]::Begin($window,$start,$false,$false)
    Wait-Ui {![FileSpacerRegression.AltDragInput]::Idle($window)} 'native icon press captured'
    for($i=1;$i-le6;++$i){
        $point=New-Object FileSpacerRegression.MousePoint
        $point.X=$start.X+[int](($drop.X-$start.X)*$i/6);$point.Y=$start.Y+[int](($drop.Y-$start.Y)*$i/6)
        [FileSpacerRegression.AltDragInput]::Move($point)
    }
    [FileSpacerViewMenuTests.GridKey]::Hold($true)
    Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'Snap frame before the accepted drop' 150
    $frame=[FileSpacerViewMenuTests.GridKey]::Marker($body)
    $origin=[FileSpacerViewMenuTests.GridKey]::Origin($list);$client=[FileSpacerRegression.Native]::ClientBounds($list)
    $expected='{0},{1}'-f($frame.Left-$client.Left+$origin[0]),($frame.Top-$client.Top+$origin[1])
    # A deterministic real activation after release avoids a flaky Alt+Tab race.
    $target.Hook($window,$message)
    [FileSpacerRegression.AltDragInput]::ReleasePrimary()
    Wait-Ui {$target.Result-eq15} 'real deactivation before deferred-placement dispatch' 0 5
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native drag cancellation completed' 150
    $observed=@{Actual=''}
    try {
        Wait-Ui {$observed.Actual=Use-UiView $window {param($view) ($view.Position('item-0000.txt')-split'=',2)[1]};$observed.Actual-eq$expected} 'accepted placement survives deactivation' 150 5
    } finally {
        @{Proof=$target.Result;Expected=$expected;Actual=$observed.Actual;Before=$before;After=Use-UiView $window {param($view) $view.Positions()}}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'deactivation-boundary.json') -Encoding UTF8
    }
    Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow()-eq$target.Window) 'Deferred positioning stole foreground activation.'
    Assert-Ui (@(Get-ChildItem $folder -File).Count-eq6) 'Icon placement changed the fixture files.'
    Write-Host 'PASS: an accepted icon placement survives deactivation without taking foreground activation back.'
}catch{$failure=$_;throw}
finally {
    [FileSpacerViewMenuTests.GridKey]::Hold($false);[FileSpacerRegression.AltDragInput]::End()
    try {if($target){$target.Dispose()}}finally{Stop-Ui -Failure $failure}
}
