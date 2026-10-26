# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\NativeTargets.cs') -ReferencedAssemblies @([ComponentModel.Win32Exception].Assembly.Location)
$testFailure=$null; $propertyWindow=[IntPtr]::Zero
function Properties-Match([IntPtr]$Candidate) {
    if (![FileSpacerRegression.Native]::IsWindowVisible($Candidate) -or @([FileSpacerRegression.Native]::Children($Candidate,'SysTabControl32')).Count -ne 1) { return $false }
    $texts=@([FileSpacerRegression.Native]::Children($Candidate,'Edit') | ForEach-Object {[FileSpacerRegression.Native]::Text($_)})
    return @($texts | Where-Object {$_ -eq $folder -or $_.EndsWith(' '+$folder,[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1
}
function Close-Properties {
    if ($script:propertyWindow -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:propertyWindow)) {
        Assert-Ui (Properties-Match $script:propertyWindow) 'Only close the verified fixture property sheet.'
        [void][FileSpacerRegression.Native]::PostMessage($script:propertyWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:propertyWindow)} 'fixture properties closed'
        $script:propertyWindow=[IntPtr]::Zero
    }
}
function Observe-Properties([int]$Bindings,[string]$Phase,[bool]$Motion=$false,[int]$Modifiers=2,[bool]$InitialMulti=$false) {
    Set-UiPreference 'WindowShortcuts' $Bindings
    Activate-Ui $window
    # Each phase starts a separate physical gesture from the same fixture state.
    Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA'); if($InitialMulti) {$view.Select('OtherC',$true)}}
    if ($InitialMulti) {Assert-UiSelected $window 'AnchorA;OtherC'} else {Assert-UiSelected $window 'AnchorA'}
    $index=Use-UiView $window {param($view) $view.IndexOf('OtherC')}
    $position=[FileSpacerRegression.NativeTargets]::Item($list,$index,2,$InitialMulti,$false)
    $point=New-Object FileSpacerRegression.MousePoint; $point.X=$position.X; $point.Y=$position.Y
    $before=[FileSpacerRegression.Native]::Bounds($window)
    [FileSpacerRegression.AltDragInput]::BeginDoubleConfigured($window,$point,$Modifiers)
    if($Motion) {
        [FileSpacerRegression.AltDragInput]::VerifyHeld($true)
        $end=New-Object FileSpacerRegression.MousePoint;$end.X=$point.X+96;$end.Y=$point.Y+72
        [FileSpacerRegression.AltDragInput]::Move($end)
        # Match the held-motion observation interval of regression 15/18.
        Wait-Ui {Assert-Ui (![FileSpacerRegression.AltDragInput]::Moving($window)) 'Native double-click entered a window move loop.';return $true} 'held second-press motion' 200 5
    }
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui {
        $candidates=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId) | Where-Object {Properties-Match $_})
        if ($candidates.Count -eq 1) {$script:propertyWindow=$candidates[0]; return $true}; return $false
    } 'native Properties sheet for the fixture'
    $texts=@([FileSpacerRegression.Native]::Children($propertyWindow,'Edit') | ForEach-Object {[FileSpacerRegression.Native]::Text($_)})
    $title=[FileSpacerRegression.Native]::Text($propertyWindow)
    $target=@($texts | Where-Object {$_ -in @('AnchorA','OtherC')}) -join ';'
    if ($Modifiers -eq 2) {Assert-Ui ($target -eq 'OtherC') 'Plain Alt-double-click must show Properties of the pointed OtherC fixture.'}
    Close-Properties
    $selection=Use-UiView $window {param($view) $view.Selected() -join ';'}
    $focus=Use-UiView $window {param($view) $view.Focused()}
    $mark=Use-UiView $window {param($view) $view.SelectionMark()}
    $after=[FileSpacerRegression.Native]::Bounds($window)
    Assert-Ui ($before.Left -eq $after.Left -and $before.Top -eq $after.Top -and $before.Width -eq $after.Width -and $before.Height -eq $after.Height) 'Native double-click must not move or resize the window.'
    $names=Use-UiView $window {param($view) $view.Names() -join ';'}
    $result=[pscustomobject]@{Phase=$Phase;Motion=$Motion;Target=$target;Title=$title;Selection=$selection;Focus=$focus;SelectionMark=$mark;Names=$names}
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN ('native-properties-'+$Phase+'.json')) -Encoding UTF8
    Write-Host ('NATIVE PROPERTY REFERENCE: '+($result | ConvertTo-Json -Compress))
    Assert-Ui ($names -eq 'AnchorA;OtherC') 'Double-click motion created an unexpected item in the native fixture.'
    return $result
}
try {
    Initialize-Ui 'native-properties'
    $folder=Join-Path $script:UiRoot 'Fixture'; [void](New-Item -ItemType Directory -Path $folder)
    foreach ($name in @('AnchorA','OtherC')) {[void](New-Item -ItemType Directory -Path (Join-Path $folder $name))}
    Set-UiPreference 'WindowShortcuts' 0x01001202
    $window=Open-UiFolder $folder 2
    $processId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$processId)
    $viewWindow=Use-UiView $window {param($view) $view.Window}
    $lists=@([FileSpacerRegression.Native]::Children($viewWindow,'SysListView32'))
    Assert-Ui ($lists.Count -eq 1) 'Validated native Shell list required.'; $list=$lists[0]
    $native=Observe-Properties 0x01001202 'native-with-both-actions-disabled'
    $configured=Observe-Properties 0x01003222 'default-window-bindings'
    $resizeLeft=Observe-Properties 0x0100222a 'resize-with-alt-left'
    @($native,$configured,$resizeLeft) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'native-properties-comparison.json') -Encoding UTF8
    foreach ($observed in @($configured,$resizeLeft)) {
        Assert-Ui ($native.Target -eq $observed.Target -and $native.Title -eq $observed.Title -and $native.Selection -eq $observed.Selection -and $native.Focus -eq $observed.Focus -and $native.SelectionMark -eq $observed.SelectionMark) 'Configured window gestures changed native Explorer Properties target/selection.'
    }
    $nativeMotion=Observe-Properties 0x01001202 'native-motion-with-both-actions-disabled' $true
    $configuredMotion=Observe-Properties 0x01003222 'motion-default-window-bindings' $true
    $resizeMotion=Observe-Properties 0x0100222a 'motion-resize-with-alt-left' $true
    @($nativeMotion,$configuredMotion,$resizeMotion) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'native-motion-comparison.json') -Encoding UTF8
    foreach($observed in @($configuredMotion,$resizeMotion)) {
        Assert-Ui ($nativeMotion.Target -eq $observed.Target -and $nativeMotion.Title -eq $observed.Title -and $nativeMotion.Selection -eq $observed.Selection -and $nativeMotion.Names -eq $observed.Names -and $nativeMotion.Focus -eq $observed.Focus -and $nativeMotion.SelectionMark -eq $observed.SelectionMark) 'Configured window gestures changed native double-click motion behavior.'
    }
    $pairs=@()
    foreach($modifiers in @(3,6,7)) {
        $reference=Observe-Properties 0x01001202 ('native-modifiers-'+$modifiers) $true $modifiers
        $binding=0x01001220 -bor $modifiers
        $observed=Observe-Properties $binding ('configured-modifiers-'+$modifiers) $true $modifiers
        $pairs+=@{Reference=$reference;Observed=$observed;Modifiers=$modifiers}
    }
    $reference=Observe-Properties 0x01001202 'native-control-multiselection' $true 3 $true
    $observed=Observe-Properties 0x01001223 'configured-control-multiselection' $true 3 $true
    $pairs+=@{Reference=$reference;Observed=$observed;Modifiers='Ctrl+Alt, initially selected target'}
    foreach($pair in $pairs) {
        $reference=$pair.Reference; $observed=$pair.Observed
        Assert-Ui ($reference.Target -eq $observed.Target -and $reference.Title -eq $observed.Title -and $reference.Selection -eq $observed.Selection -and $reference.Focus -eq $observed.Focus -and $reference.SelectionMark -eq $observed.SelectionMark -and $reference.Names -eq $observed.Names) ('Configured modifier double-click differs from its native Shell reference: '+$pair.Modifiers)
    }
    Write-Host 'PASS: Properties target, selection, unchanged geometry and folder contents match the native Shell baseline, with and without second-press motion.'
} catch {$testFailure=$_;throw}
finally {try {[FileSpacerRegression.AltDragInput]::End(); Close-Properties} finally {Stop-Ui -Failure $testFailure}}
