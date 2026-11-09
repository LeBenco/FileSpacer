# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
$failure=$null
$script:ColumnsDiagnostic=''
try {
    Initialize-Ui 'columns' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    Set-UiPreference 'DetailsColumnsEnabled' 1
    Set-UiPreference 'DetailsColumnsDeferred' 1
    $folder=Join-Path $script:UiRoot 'First'
    [void][IO.Directory]::CreateDirectory($folder)
    1..100 | ForEach-Object { [IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns') }
    $window=Open-UiFolder $folder 100
    Use-UiView $window {param($view) $view.Mode=4}
    Wait-UiViewMode $window 4
    $initial=Wait-Columns $window
    Write-Host ('INITIAL '+($initial.Widths -join ',')+' available='+$initial.Available)
    $windowBounds=[FileSpacerRegression.Native]::Bounds($window)
    [void][FileSpacerRegression.Native]::Message($window,0x231,0,0)
    Resize-ColumnsWindow $window ($windowBounds.Width+160)
    Resize-ColumnsWindow $window ($windowBounds.Width+240)
    $during=Read-Columns $window
    Assert-Ui (($during.Widths -join ',') -eq ($initial.Widths -join ',')) 'Columns changed inside the suspended resize.'
    [void][FileSpacerRegression.Native]::Message($window,0x232,0,0)
    $grown=Wait-Columns $window
    for($i=0;$i -lt $grown.Widths.Count;++$i) {
        $expected=$initial.Widths[$i]*$grown.Available/$initial.Available
        Assert-Ui ([Math]::Abs($grown.Widths[$i]-$expected) -le 2) 'A column did not grow in proportion to its initial width.'
    }
    Resize-ColumnsWindow $window $windowBounds.Width
    $returned=Wait-Columns $window
    Assert-Ui (($returned.Widths -join ',') -eq ($initial.Widths -join ',')) 'A resize round trip changed the original proportions.'
    $header=$returned.Headers[0]
    $start=New-Object FileSpacerRegression.MousePoint
    $start.X=[int]$header.Right-1; $start.Y=[int]($header.Top+$header.Height/2)
    $end=New-Object FileSpacerRegression.MousePoint
    $end.X=$start.X+40; $end.Y=$start.Y
    Activate-Ui $window
    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
    [FileSpacerRegression.AltDragInput]::Move($end)
    [FileSpacerRegression.AltDragInput]::End()
    $dragged=Wait-Columns $window
    Write-Host ('DRAGGED '+($dragged.Widths -join ',')+' available='+$dragged.Available)
    Assert-Ui ([Math]::Abs($dragged.Widths[0]-$initial.Widths[0]-40) -le 2) 'The first column did not follow the real mouse drag.'
    Assert-Ui ([Math]::Abs($dragged.Widths[1]-$initial.Widths[1]+40) -le 2) 'The right neighbour did not compensate the drag.'
    for($i=2;$i -lt $dragged.Widths.Count;++$i) { Assert-Ui ($dragged.Widths[$i] -eq $initial.Widths[$i]) 'An unrelated column changed.' }
    $last=$dragged.Headers[$dragged.Headers.Count-1]
    $start.X=[int]$last.Right-2; $start.Y=[int]($last.Top+$last.Height/2)
    $end.X=$start.X-30; $end.Y=$start.Y
    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
    [FileSpacerRegression.AltDragInput]::Move($end)
    [FileSpacerRegression.AltDragInput]::End()
    $fixed=Wait-Columns $window
    Assert-Ui (($fixed.Widths -join ',') -eq ($dragged.Widths -join ',')) 'The final separator changed the fixed right edge.'
    $minimum=$null
    for($attempt=0;$attempt -lt 2;++$attempt) {
        $current=Wait-Columns $window
        $edge=$current.Headers[0]
        $start.X=[int]$edge.Right-1; $start.Y=[int]($edge.Top+$edge.Height/2)
        $end.X=$start.X+200; $end.Y=$start.Y
        [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
        [FileSpacerRegression.AltDragInput]::Move($end)
        [FileSpacerRegression.AltDragInput]::End()
        $limited=Wait-Columns $window
        Assert-Ui ($limited.Widths[1] -gt 0 -and $limited.Widths[1] -lt $dragged.Widths[1]) 'The neighbour did not reach a positive minimum.'
        for($i=2;$i -lt $limited.Widths.Count;++$i) { Assert-Ui ($limited.Widths[$i] -eq $dragged.Widths[$i]) 'Reaching the minimum changed an unrelated column.' }
        if($null -ne $minimum) { Assert-Ui (($limited.Widths -join ',') -eq ($minimum.Widths -join ',')) 'A second drag exceeded the minimum.' }
        $minimum=$limited
    }
    $start.X=[int]$minimum.Headers[0].Right-1
    $end.X=$start.X+$dragged.Widths[0]-$minimum.Widths[0]
    [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
    [FileSpacerRegression.AltDragInput]::Move($end)
    [FileSpacerRegression.AltDragInput]::End()
    $restored=Wait-Columns $window
    Assert-Ui (($restored.Widths -join ',') -eq ($dragged.Widths -join ',')) 'Restoring the separator after its minimum changed the widths.'
    Resize-ColumnsWindow $window ($windowBounds.Width+160)
    $after=Wait-Columns $window
    for($i=0;$i -lt $after.Widths.Count;++$i) {
        Assert-Ui ([Math]::Abs($after.Widths[$i]-$dragged.Widths[$i]*$after.Available/$dragged.Available) -le 2) 'The new manual proportions were not retained.'
    }
    [void][FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr](Get-UiId 'IDM_FILL_COLUMNS'),[IntPtr]::Zero)
    Wait-Ui { $body=Use-UiView $window {param($view) $view.Window}; @([FileSpacerRegression.Native]::Children($body,'SysListView32')).Count -eq 0 } 'native modern profile after disabling the option' 300
    Set-UiPreference 'ViewMenuPreviewEnabled' 1
    $popup=Open-PreviewMenu $window
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    $entry=-1
    for($i=0;$i -lt $popup.Entries.Count;++$i) { if($popup.Entries[$i].Command -eq (Get-UiId 'IDM_FILL_COLUMNS')) { $entry=$i } }
    Assert-Ui ($entry -ge 0) 'The option is missing from the actual View menu.'
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$entry,$window)
    Wait-PreviewMenuClosed $window
    [void](Wait-Columns $window)
    Use-UiView $window {param($view) $view.Mode=3}
    Wait-UiViewMode $window 3
    Wait-Ui { $body=Use-UiView $window {param($view) $view.Window}; @([FileSpacerRegression.Native]::Children($body,'SysListView32')).Count -eq 0 } 'List mode retains its modern profile with the option enabled' 300
    Use-UiView $window {param($view) $view.Mode=4}
    Wait-UiViewMode $window 4
    [void](Wait-Columns $window)
    $secondFolder=Join-Path $script:UiRoot 'Second'
    [void][IO.Directory]::CreateDirectory($secondFolder)
    $secondWindow=Open-UiFolder $secondFolder 0
    Use-UiView $secondWindow {param($view) $view.Mode=4}
    Wait-UiViewMode $secondWindow 4
    [void](Wait-Columns $secondWindow)
    [void][FileSpacerRegression.Native]::PostMessage($secondWindow,0x111,[IntPtr](Get-UiId 'IDM_FILL_COLUMNS'),[IntPtr]::Zero)
    Wait-ModernColumns $secondWindow
    [void](Wait-Columns $window)
    Write-Host 'PASS: real FileSpacer/native Shell, exact fill with a vertical scrollbar, deferred proportional resize, no round-trip drift, real separator drag, adjacent compensation, fixed right edge, updated proportions, actual menu with preview enabled, independent directory windows and return to modern view.'
} catch { $failure=$_; throw } finally {
    try { [FileSpacerRegression.AltDragInput]::End() } finally { Stop-Ui -Failure $failure }
}
