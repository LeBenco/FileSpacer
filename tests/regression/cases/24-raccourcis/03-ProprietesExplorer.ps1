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
    return ($texts -contains $folder) -and (($texts -contains 'AnchorA') -or ($texts -contains 'OtherC'))
}
function Close-Properties {
    if ($script:propertyWindow -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:propertyWindow)) {
        Assert-Ui (Properties-Match $script:propertyWindow) 'Only close the verified fixture property sheet.'
        [void][FileSpacerRegression.Native]::PostMessage($script:propertyWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:propertyWindow)} 'fixture properties closed'
        $script:propertyWindow=[IntPtr]::Zero
    }
}
function Observe-Properties([int]$Bindings,[string]$Phase) {
    Set-UiPreference 'WindowShortcuts' $Bindings
    Activate-Ui $window
    # Each phase starts a separate physical gesture from the same fixture state.
    Use-UiView $window {param($view) $view.Focus(); $view.Select('AnchorA')}
    Assert-UiSelected $window 'AnchorA'
    $index=Use-UiView $window {param($view) $view.IndexOf('OtherC')}
    $position=[FileSpacerRegression.NativeTargets]::Item($list,$index,2,$false,$false)
    $point=New-Object FileSpacerRegression.MousePoint; $point.X=$position.X; $point.Y=$position.Y
    [FileSpacerRegression.AltDragInput]::Begin($window,$point,$true,$true)
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui {
        $candidates=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId) | Where-Object {Properties-Match $_})
        if ($candidates.Count -eq 1) {$script:propertyWindow=$candidates[0]; return $true}; return $false
    } 'native Properties sheet for the fixture'
    $texts=@([FileSpacerRegression.Native]::Children($propertyWindow,'Edit') | ForEach-Object {[FileSpacerRegression.Native]::Text($_)})
    $target=@($texts | Where-Object {$_ -in @('AnchorA','OtherC')}) -join ';'
    Close-Properties
    $selection=Use-UiView $window {param($view) $view.Selected() -join ';'}
    $result=[pscustomobject]@{Phase=$Phase;Target=$target;Selection=$selection}
    Write-Host ('NATIVE PROPERTY REFERENCE: '+($result | ConvertTo-Json -Compress))
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
        Assert-Ui ($native.Target -eq $observed.Target -and $native.Selection -eq $observed.Selection) 'Configured window gestures changed native Explorer Properties target/selection.'
    }
    Write-Host 'PASS: Properties target and selection match the native Shell baseline.'
} catch {$testFailure=$_;throw}
finally {try {[FileSpacerRegression.AltDragInput]::End(); Close-Properties} finally {Stop-Ui -Failure $testFailure}}
