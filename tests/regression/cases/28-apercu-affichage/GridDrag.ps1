. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
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
function Move-GridIcons([bool]$Snap,[bool]$ReleaseEarly=$false) {
    Activate-Ui $window
    Use-UiView $window {param($view) $view.Reveal('item-0000')}
    $body=Use-UiView $window {param($view) $view.Window}
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
        if($ReleaseEarly){[FileSpacerViewMenuTests.GridKey]::Hold($false)}
        $script:GridDropStartedAt=[Diagnostics.Stopwatch]::GetTimestamp()
        [FileSpacerRegression.AltDragInput]::ReleasePrimary()
        Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native icon drop completed' 150
    } finally {
        [FileSpacerViewMenuTests.GridKey]::Hold($false)
        [FileSpacerRegression.AltDragInput]::End()
    }
}
