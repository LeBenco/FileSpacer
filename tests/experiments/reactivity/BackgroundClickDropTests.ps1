# Test-Category: UI
param([string]$ProjectRoot)
$ErrorActionPreference='Stop'
. (Join-Path $ProjectRoot 'tests/regression/common/EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
# Only the native input methods are used; UIA is not used to find targets.
Add-Type -Path (Join-Path $ProjectRoot 'tests/regression/cases/15-deplacement-alt/MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $ProjectRoot 'tests/regression/cases/15-deplacement-alt/NativeTargets.cs')
Add-Type -Path (Join-Path $ProjectRoot 'tests/regression/cases/28-apercu-affichage/ViewMenuUi.cs')
$failure=$null;$previousCache=$env:FS_SELECTION_PRODUCT_CACHE
try {
    Initialize-Ui 'background-click-drop' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $env:FS_SELECTION_PRODUCT_CACHE=Join-Path $ProjectRoot 'build/remaining-selection/cache'
    Assert-Ui (Test-Path -LiteralPath $env:FS_SELECTION_PRODUCT_CACHE) 'A qualified private cache is required.'
    foreach($mode in @(@{Name='free-icons';Mode=1;Label='Moyennes|Medium'},@{Name='free-tiles';Mode=6;Label='Mosa|Tiles'})) {
        $folder=Join-Path $script:UiRoot $mode.Name
        [void][IO.Directory]::CreateDirectory($folder)
        $names=@('AnchorA','AnchorB','OtherC')+@(0..126|ForEach-Object {'ZZ{0:D3}'-f$_})
        foreach($name in $names){[void][IO.Directory]::CreateDirectory((Join-Path $folder $name))}
        $window=Open-UiFolder $folder 130
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,100,100,800,500,0x14)) 'Cannot place drop fixture.'
        Activate-Ui $window
        $popup=[FileSpacerViewMenuTests.MenuUi]::Open($window,(Get-UiCommandToolbar $window),(Get-UiId 'IDM_VIEW_MENU'))
        $entries=@($popup.Entries|Where-Object {$_.Label.Replace('&','')-match$mode.Label})
        Assert-Ui ($entries.Count-eq1) 'The requested classic view is not unambiguous.'
        [FileSpacerViewMenuTests.MenuUi]::Choose($popup,[Array]::IndexOf($popup.Entries,$entries[0]),$window)
        Wait-UiViewMode $window $mode.Mode
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize($mode.Mode,48);$view.AutoArrange=$false;$view.SortBy($false,$true);$view.Focus()}
        Wait-UiViewMode $window $mode.Mode 48
        [FileSpacerRegression.Native]::Keys($window,0x41,$true,$false)
        Wait-Ui {(Use-UiView $window {param($view) $view.SelectedCount})-eq130} 'select all drop fixture items'
        Use-UiView $window {param($view) $view.Deselect('AnchorB');$view.Deselect('OtherC')}
        $expected=@($names|Where-Object {$_-notin@('AnchorB','OtherC')}|Sort-Object)
        Assert-UiSelected $window ($expected-join';')
        $list=Get-UiControl $window 'SysListView32'
        $sourceIndex=Use-UiView $window {param($view) $view.IndexOf('AnchorA')}
        $targetIndex=Use-UiView $window {param($view) $view.IndexOf('OtherC')}
        $source=[FileSpacerRegression.NativeTargets]::Item($list,$sourceIndex,130,$true,$false)
        $target=[FileSpacerRegression.NativeTargets]::Item($list,$targetIndex,130,$false,$false)
        $from=New-Object FileSpacerRegression.MousePoint;$from.X=$source.X;$from.Y=$source.Y
        $to=New-Object FileSpacerRegression.MousePoint;$to.X=$target.X;$to.Y=$target.Y
        $threshold=New-Object FileSpacerRegression.MousePoint;$threshold.X=$from.X+32;$threshold.Y=$from.Y+24
        $before=[FileSpacerRegression.Native]::Bounds($window)
        try {
            [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$from,0,0)
            Wait-Ui {[FileSpacerRegression.AltDragInput]::Captured($window)} 'selected item press captured' 0 5
            [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
            [FileSpacerRegression.AltDragInput]::Move($threshold)
            [FileSpacerRegression.AltDragInput]::Move($to)
            Start-Sleep -Milliseconds 100
        } finally {[FileSpacerRegression.AltDragInput]::End()}
        Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)} 'native drop ended' 100 10
        Wait-Ui {
            $script:dropNames=@(Get-ChildItem -LiteralPath (Join-Path $folder 'OtherC') -Directory|ForEach-Object {$_.Name}|Sort-Object)
            $script:sourceNames=@(Get-ChildItem -LiteralPath $folder -Directory|ForEach-Object {$_.Name}|Sort-Object)
            ($script:dropNames-join';')-ceq($expected-join';')-and($script:sourceNames-join';')-ceq'AnchorB;OtherC'
        } 'all 128 exact identities moved on disk' 200 30
        Wait-Ui {((Use-UiView $window {param($view) $view.Names()})-join';')-ceq'AnchorB;OtherC'} 'source view reflects all moved identities' 200 30
        $after=[FileSpacerRegression.Native]::Bounds($window)
        Assert-Ui ($before.Left-eq$after.Left-and$before.Top-eq$after.Top-and$before.Right-eq$after.Right-and$before.Bottom-eq$after.Bottom) 'Item drag moved the folder window.'
        @{View=$mode.Name;Expected=$expected;Destination=$script:dropNames;Source=$script:sourceNames;Input=[FileSpacerRegression.AltDragInput]::Describe($window)}|ConvertTo-Json -Depth 3|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN ($mode.Name+'-drop.json'))
        Close-UiFolder $window
        Complete-TestApplications $script:UiProcesses
        $script:UiProcesses.Clear()
        Write-Host ('PASS: '+$mode.Name+' moved all 128 selected directories into the target.')
    }
}catch{$failure=$_;throw}
finally {
    $env:FS_SELECTION_PRODUCT_CACHE=$previousCache
    Stop-Ui $failure
}
