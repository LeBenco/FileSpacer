# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
function Resize-ByBorder([IntPtr]$Window) {
    Activate-Ui $Window
    $bounds=[FileSpacerRegression.Native]::Bounds($Window)
    $start=New-Object FileSpacerRegression.MousePoint
    $corner=$false
    foreach($offset in @(3,4,5,6,7)) {
        $start.X=$bounds.Right-$offset;$start.Y=$bounds.Bottom-$offset
        $packed=($start.X -band 65535) -bor (($start.Y -band 65535) -shl 16)
        if([FileSpacerRegression.Native]::Message($Window,0x84,0,$packed) -eq 17) { $corner=$true;break }
    }
    Assert-Ui $corner 'No native bottom-right sizing border was found.'
    [FileSpacerRegression.AltDragInput]::BeginConfigured($Window,$start,0,0)
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Moving($Window) } 'real native sizing loop entered'
    $point=New-Object FileSpacerRegression.MousePoint
    $point.X=$start.X+48;$point.Y=$start.Y+24
    [FileSpacerRegression.AltDragInput]::Move($point)
    Wait-Ui { [FileSpacerRegression.Native]::Bounds($Window).Width -eq $bounds.Width+48 } 'real border resize applied'
    [FileSpacerRegression.AltDragInput]::End()
    Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($Window) } 'real native sizing loop exited'
    Wait-Ui {
        $client=[FileSpacerRegression.Native]::ClientBounds($Window)
        $row=[FileSpacerRegression.Native]::Bounds((Get-UiControl $Window 'ReBarWindow32'))
        $hostBounds=[FileSpacerRegression.Native]::ClientBounds((Get-UiControl $Window 'FileSpacer Toolbar Host'))
        $commands=[FileSpacerRegression.Native]::Bounds((Get-UiCommandToolbar $Window))
        [Math]::Abs($row.Width-$client.Width) -le 1 -and [Math]::Abs($hostBounds.Width-$row.Width) -le 2 -and
            [Math]::Abs($commands.Right-$hostBounds.Right) -le 1
    } 'toolbar row, band child and commands follow the native border after release' 200 5
    [void](Wait-Columns $Window)
}
$failure=$null
try {
    Initialize-Ui 'column-idle-gesture' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'DetailsColumnsEnabled' 1
    Set-UiPreference 'DetailsColumnsDeferred' 1
    $folder=Join-Path $script:UiRoot 'Details'
    [void][IO.Directory]::CreateDirectory($folder)
    1..5 | ForEach-Object { [IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns') }
    $window=Open-UiFolder $folder 5
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($work.Width -ge 1016 -and $work.Height -ge 624) 'The test needs room for eight real border resizes.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+16),($work.Top+16),600,400,0x14)) 'Cannot place the test window with room for border resizes.'
    Use-UiView $window {param($view) $view.Mode=4};Wait-UiViewMode $window 4
    [void](Wait-Columns $window)
    $otherFolder=Join-Path $script:UiRoot 'Other'
    [void][IO.Directory]::CreateDirectory($otherFolder)
    $otherWindow=Open-UiFolder $otherFolder 0
    foreach($ending in @('Release','Escape','Cancel','Deactivate')) {
        Activate-Ui $window
        Resize-ByBorder $window
        $bounds=[FileSpacerRegression.Native]::Bounds($window)
        $body=Get-ColumnsViewWindow $window
        $before=[FileSpacerRegression.Native]::ClientBounds($body)
        $row=Get-UiControl $window 'ReBarWindow32'
        $rowBefore=[FileSpacerRegression.Native]::Bounds($row)
        $point=[FileSpacerRegression.AltDragInput]::BackgroundPoint($body,$false)
        [FileSpacerRegression.AltDragInput]::BeginRight($window,$point,$true,$false)
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Captured($window) } 'Alt-right click captured without movement'
        if($ending -eq 'Escape') { [FileSpacerRegression.AltDragInput]::Escape() }
        elseif($ending -eq 'Cancel') { [void][FileSpacerRegression.Native]::Message($window,0x1f,0,0) }
        elseif($ending -eq 'Deactivate') { Activate-Ui $otherWindow }
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'unmoved gesture ended'
        $afterClick=[FileSpacerRegression.Native]::Bounds($window)
        Assert-Ui ($afterClick.Width -eq $bounds.Width -and $afterClick.Height -eq $bounds.Height) 'An unmoved gesture resized the window.'
        # SetWindowPos has no native sizing loop to clear a stale deferred flag.
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,0,0,$bounds.Width+64,$bounds.Height+32,0x16)) 'Cannot resize after the unmoved gesture.'
        Wait-Ui {
            $rowAfter=[FileSpacerRegression.Native]::Bounds($row)
            $viewAfter=[FileSpacerRegression.Native]::ClientBounds($body)
            $rowAfter.Width -eq $rowBefore.Width+64 -and $viewAfter.Width -eq $before.Width+64
        } ('toolbar and native view resize after '+$ending) 200 5
        [void](Wait-Columns $window)
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,0,0,$bounds.Width,$bounds.Height,0x16)) 'Cannot restore the test geometry.'
        [void](Wait-Columns $window)
        Resize-ByBorder $window
        Write-Host ('PASS: unmoved Alt-right gesture ending with '+$ending+' leaves toolbar, viewport and columns responsive.')
    }
} catch { $failure=$_;throw } finally {
    try { [FileSpacerRegression.AltDragInput]::End() } finally { Stop-Ui -Failure $failure }
}
