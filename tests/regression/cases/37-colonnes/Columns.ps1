. (Join-Path $PSScriptRoot '..\28-apercu-affichage\ViewMenu.ps1')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,WindowsBase
$columnReferences=@([System.Windows.Automation.AutomationElement].Assembly.Location,
    [System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $columnReferences
Add-Type -Path (Join-Path $PSScriptRoot 'ColumnsUi.cs') -ReferencedAssemblies @('Accessibility.dll',([FileSpacerRegression.Native].Assembly.Location))

function Get-ColumnsViewWindow([IntPtr]$Window) {
    $view=$null
    try {
        $view=Get-UiView $Window
        if($null -eq $view) { return [IntPtr]::Zero }
        return $view.Window
    } catch {
        $cause=$_.Exception
        while($cause.InnerException) { $cause=$cause.InnerException }
        if($cause.HResult -notin @(-2147467262,-2147467259,-2147417848,-2147483638)) { throw }
        return [IntPtr]::Zero
    } finally { if($null -ne $view) { $view.Dispose() } }
}
function Read-Columns([IntPtr]$Window) {
    $body=Get-ColumnsViewWindow $Window
    if($body -eq [IntPtr]::Zero) { return $null }
    $lists=@([FileSpacerRegression.Native]::Children($body,'SysListView32'))
    if($lists.Count -ne 1) {
        $description='lists='+$lists.Count+' '+([FileSpacerRegression.Native]::DescribeChildren($body) -join ';')
        if($script:ColumnsDiagnostic -ne $description){Write-Host $description;$script:ColumnsDiagnostic=$description}
        return $null
    }
    $nativeHeaders=@([FileSpacerRegression.Native]::Children($body,'SysHeader32'))
    if($nativeHeaders.Count -ne 1) { return $null }
    $headers=@([FileSpacerColumnTests.ColumnsUi]::Headers($nativeHeaders[0]) | Sort-Object Left)
    if($headers.Count -lt 2) {
        $description='headers='+$headers.Count+' '+([FileSpacerRegression.Native]::DescribeChildren($body) -join ';')
        if($script:ColumnsDiagnostic -ne $description){Write-Host $description;$script:ColumnsDiagnostic=$description}
        return $null
    }
    $bounds=[FileSpacerRegression.Native]::ClientBounds($lists[0])
    return @{Widths=@($headers | ForEach-Object { [int][Math]::Round($_.Width) });Headers=$headers;Available=$bounds.Width}
}
function Get-ColumnsItemCount([IntPtr]$Window) {
    $view=$null
    try {
        $view=Get-UiView $Window
        if($null -eq $view -or !$view.ContentVisible) { return -1 }
        return $view.Count
    } catch {
        $cause=$_.Exception
        while($cause.InnerException) { $cause=$cause.InnerException }
        if($cause.HResult -notin @(-2147467262,-2147467259,-2147417848,-2147483638)) { throw }
        return -1
    } finally { if($null -ne $view) { $view.Dispose() } }
}
function Wait-Columns([IntPtr]$Window,[int]$StableMilliseconds=300,[int]$Seconds=25) {
    $result=@{Value=$null;Last=''}
    Wait-Ui {
        $value=Read-Columns $Window
        if($null -eq $value){return $false}
        $result.Value=$value
        $description=($value.Widths -join ',')+' available='+$value.Available
        if($description -ne $result.Last) { Write-Host ('OBSERVED '+$description);$result.Last=$description }
        return [Math]::Abs(($value.Widths|Measure-Object -Sum).Sum-$value.Available) -le 1
    } 'columns fill the real native viewport' $StableMilliseconds $Seconds
    return $result.Value
}
function Wait-ModernColumns([IntPtr]$Window) {
    Wait-Ui {
        $body=Get-ColumnsViewWindow $Window
        $body -ne [IntPtr]::Zero -and @([FileSpacerRegression.Native]::Children($body,'SysListView32')).Count -eq 0
    } 'modern Details view while column fill is ineffective' 300
}
function Resize-ColumnsWindow([IntPtr]$Window,[int]$Width) {
    $bounds=[FileSpacerRegression.Native]::Bounds($Window)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$bounds.Left,$bounds.Top,$Width,$bounds.Height,0x14)) 'Resize failed.'
}
$script:ColumnsDiagnostic=''
Add-Type -AssemblyName System.Drawing
function Save-ColumnWindow([IntPtr]$Window,[string]$Name) {
    [FileSpacerColumnTests.ColumnsUi]::FlushPaint($Window)
    Start-Sleep -Milliseconds 250
    $bounds=[FileSpacerRegression.Native]::Bounds($Window)
    $bitmap=New-Object Drawing.Bitmap($bounds.Width,$bounds.Height)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left,$bounds.Top,0,0,$bitmap.Size)
        $bitmap.Save((Join-Path $env:FS_REGRESSION_RUN ($Name+'.png')))
    } finally { $graphics.Dispose();$bitmap.Dispose() }
}
