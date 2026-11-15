. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Drawing
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $PSScriptRoot 'GridKey.cs')
function Set-GridMode {
    # Use the user-facing mode command: opening View first completes any
    # profile handoff before the Shell changes its representation.
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,2,$window)
    Wait-PreviewMenuClosed $window
    Wait-Ui {$mode=Read-PreviewMode $window;$mode.Mode-eq1-and$mode.Size-eq48} 'stable icon mode before changing native placement flags' 150
    # A renderer replacement invalidates the previous COM view. Get its successor.
    Use-UiView $window {param($view) $view.AutoArrange=$false}
    Wait-Ui {Use-UiView $window {param($view) $view.Mode-eq1-and!$view.AutoArrange}} 'stable native free icon view before seeding' 150
}
function Move-GridIcons([bool]$Snap,[bool]$ReleaseEarly=$false,[bool]$InspectMarker=$false,[bool]$PulseMarker=$false,[bool]$Cancel=$false) {
    Activate-Ui $window
    Use-UiView $window {param($view) $view.Reveal('item-0000')}
    $body=Use-UiView $window {param($view) $view.Window}
    $iconSize=(Read-PreviewMode $window).Size
    $start=[FileSpacerRegression.AltDragInput]::ItemPoint($body,'item-0000')
    $bounds=[FileSpacerRegression.Native]::ClientBounds($body)
    $target=New-Object FileSpacerRegression.MousePoint
    $target.X=$bounds.Left+420;$target.Y=$bounds.Top+345
    try {
        [FileSpacerRegression.AltDragInput]::Begin($window,$start,$false,$false)
        Wait-Ui {![FileSpacerRegression.AltDragInput]::Idle($window)} 'native icon press captured before motion' 0 5
        [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
        for($step=1;$step-le6;++$step){
            $point=New-Object FileSpacerRegression.MousePoint
            $point.X=$start.X+[int](($target.X-$start.X)*$step/6)
            $point.Y=$start.Y+[int](($target.Y-$start.Y)*$step/6)
            [FileSpacerRegression.AltDragInput]::Move($point)
            if($step-eq3-and$Snap){[FileSpacerViewMenuTests.GridKey]::Hold($true)}
        }
        if($Cancel){
            Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'marker shown before cancelling the native drag' 0 5
            $before=[FileSpacerViewMenuTests.GridKey]::Marker($body)
            $list=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})
            Assert-Ui ($list.Count-eq1) 'Expected one scrolling Shell list.'
            $origin=[FileSpacerViewMenuTests.GridKey]::Origin($list[0])
            Assert-Ui ([FileSpacerRegression.Native]::Message($list[0],0x1014,0,37)-ne0) 'Cannot scroll the active native drag.'
            Wait-Ui {([FileSpacerViewMenuTests.GridKey]::Origin($list[0]))[1]-ne$origin[1]} 'native view scrolled during drag' 0 5
            $point=New-Object FileSpacerRegression.MousePoint
            $point.X=$target.X+1;$point.Y=$target.Y
            [FileSpacerRegression.AltDragInput]::Move($point)
            Wait-Ui {$mark=[FileSpacerViewMenuTests.GridKey]::Marker($body);$mark.Visible-and$mark.Top-ne$before.Top} 'marker geometry refreshed after scrolling' 0 5
            [FileSpacerRegression.AltDragInput]::EscapeHeld()
            Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native drag cancelled before releasing the mouse' 0 5
            Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'cancellation removes the Snap marker' 0 5
            return
        }
        if($Snap-and$PulseMarker){
            Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'Snap marker available before motion stress' 0 5
            $first=[FileSpacerViewMenuTests.GridKey]::Marker($body)
            $shifted=New-Object FileSpacerRegression.MousePoint
            $shifted.X=$target.X+120;$shifted.Y=$target.Y
            for($sample=0;$sample-lt60;++$sample){
                $point=if($sample%2-eq0){$shifted}else{$target}
                [FileSpacerRegression.AltDragInput]::Move($point)
                Wait-Ui {$mark=[FileSpacerViewMenuTests.GridKey]::Marker($body);$mark.Visible-and(($mark.Left-eq$first.Left)-eq($sample%2-eq1))} 'marker follows each stress motion' 0 5
            }
        }
        if($Snap-and$InspectMarker){
            Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'native Snap marker shown during S drag' 0 5
            $before=[FileSpacerViewMenuTests.GridKey]::Marker($body)
            Assert-Ui ($before.Class-eq'Static'-and$before.InputTransparent) 'Snap frame must remain input-transparent.'
            Assert-Ui ($before.Height-eq$iconSize-and$before.Width-eq$iconSize) ('Snap frame size differs from the icons: '+($before|ConvertTo-Json -Compress))
            [FileSpacerViewMenuTests.GridKey]::Hold($false)
            Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'releasing S removes marker without releasing mouse' 0 5
            [FileSpacerViewMenuTests.GridKey]::Hold($true)
            Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'repressing S restores marker without mouse motion' 0 5
            $shifted=New-Object FileSpacerRegression.MousePoint
            $shifted.X=$target.X+120;$shifted.Y=$target.Y
            if($iconSize-ge96){
                [FileSpacerRegression.AltDragInput]::Move($shifted)
                Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'occupied destination suppresses an uncertain preview' 0 5
                $shifted.X=$target.X+240
            }
            [FileSpacerRegression.AltDragInput]::Move($shifted)
            Wait-Ui {$mark=[FileSpacerViewMenuTests.GridKey]::Marker($body);$mark.Visible-and$mark.Left-ne$before.Left} 'marker follows the next grid destination' 0 5
            [FileSpacerRegression.AltDragInput]::Move($target)
            Wait-Ui {$mark=[FileSpacerViewMenuTests.GridKey]::Marker($body);$mark.Visible-and$mark.Left-eq$before.Left-and$mark.Top-eq$before.Top} 'marker returns to the original destination' 0 5
            $script:GridMarkerBeforeDrop=[FileSpacerViewMenuTests.GridKey]::Marker($body)
            $script:GridMarkerBeforeDrop|ConvertTo-Json|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'snap-marker.json') -Encoding UTF8
            $bitmap=New-Object Drawing.Bitmap ($bounds.Right-$bounds.Left),($bounds.Bottom-$bounds.Top)
            $graphics=[Drawing.Graphics]::FromImage($bitmap)
            try {$graphics.CopyFromScreen($bounds.Left,$bounds.Top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $env:FS_REGRESSION_RUN 'snap-marker.png'),[Drawing.Imaging.ImageFormat]::Png)}
            finally {$graphics.Dispose();$bitmap.Dispose()}
        }
        if($ReleaseEarly){
            [FileSpacerViewMenuTests.GridKey]::Hold($false)
            if($InspectMarker){Wait-Ui {![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'marker hidden before a free drop' 0 5}
        }
        $script:GridDropStartedAt=[Diagnostics.Stopwatch]::GetTimestamp()
        [FileSpacerRegression.AltDragInput]::ReleasePrimary()
        Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native icon drop completed' 150
        if($InspectMarker){Assert-Ui (![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible) 'Snap marker survived mouse release.'}
    } finally {
        [FileSpacerViewMenuTests.GridKey]::Hold($false)
        [FileSpacerRegression.AltDragInput]::End()
    }
}
