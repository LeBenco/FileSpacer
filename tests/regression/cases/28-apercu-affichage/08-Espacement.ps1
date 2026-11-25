# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'SpacingSlider.ps1')

$failure = $null
try {
    Initialize-Ui 'spacing-slider' -FixtureParent $RunDirectory
    Set-UiPreference 'StatusTextEnabled' 1
    Set-UiPreference 'LastOpenedVersion' 1
    foreach ($defer in @(0,1)) {
        Set-UiPreference 'IconSpacingDeferred' $defer
        $folder = Join-Path $script:UiRoot "folder-$defer"
        [void][IO.Directory]::CreateDirectory($folder)
        foreach ($i in 0..5) { [void][IO.Directory]::CreateDirectory((Join-Path $folder ('item-{0:D4}' -f $i))) }
        $window = Open-UiFolder $folder 6
        $dpi=[FileSpacerRegression.Native]::GetDpiForWindow($window)
        $rounding=if ($dpi -eq 96) {0} else {[int][Math]::Ceiling($dpi/192.0)}
        $menu=Open-PreviewMenu $window
        [FileSpacerViewMenuTests.MenuUi]::Choose($menu,2,$window)
        Wait-PreviewMenuClosed $window
        Wait-UiViewMode $window 1 48
        Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
        Use-UiView $window {
            param($v)
            foreach ($i in 0..5) { $v.Position(('item-{0:D4}' -f $i),97+($i%3)*130,83+([int][Math]::Floor($i/3))*110) }
        }
        $seed='item-0000=97,83;item-0001=227,83;item-0002=357,83;item-0003=97,193;item-0004=227,193;item-0005=357,193'
        Wait-Ui {(Use-UiView $window {param($v) $v.Positions()}) -eq $seed} 'six free coordinates settled' 150
        foreach ($axis in @(0,1)) {
            $slider = Open-SpacingSlider $window $axis
            $track = $slider.Track
            $before = Read-Spacing $window
            $max = [FileSpacerRegression.Native]::Message($track,0x402,0,0)
            $start = [FileSpacerRegression.Native]::Message($track,0x400,0,0)
            Assert-Ui ($max -gt 0 -and $max -eq 3*$start) "standard gap and upper bound for axis $axis"
            $bounds = [FileSpacerRegression.Native]::Bounds($track)
            Assert-Ui (($bounds.Width -gt $bounds.Height) -eq ($axis -eq 0)) 'track orientation'
            [FileSpacerViewMenuTests.MenuUi]::Key($slider.Popup,0x24,1)
            $zero = Read-Spacing $window
            $old = if ($axis) {$before.Y} else {$before.X}
            $min = if ($axis) {$zero.Y} else {$zero.X}
            Assert-Ui ([Math]::Abs($old-$min-$start) -le $rounding) "zero removes only the external gap on axis $axis ($old, $min, $start)"
            $point = New-Object FileSpacerRegression.MousePoint
            $thumbOffset=[int][Math]::Round(10*$dpi/96.0)
            $point.X = $bounds.Left+$thumbOffset
            $point.Y = $bounds.Top+$thumbOffset
            try {
                [FileSpacerRegression.AltDragInput]::Begin($slider.Popup,$point,$false,$false)
                [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
                if ($axis) {$point.Y = $bounds.Bottom-2} else {$point.X = $bounds.Right-2}
                [FileSpacerRegression.AltDragInput]::Move($point)
                Wait-Ui { [FileSpacerRegression.Native]::Message($track,0x400,0,0) -eq $max } 'thumb reached maximum while held'
                Wait-Ui {
                    $held = Read-Spacing $window
                    $actual = if ($axis) {$held.Y} else {$held.X}
                    return [Math]::Abs($actual - ($min + (1-$defer)*$max)) -le $rounding
                } 'live versus deferred geometry during actual mouse drag' 100
            } finally { [FileSpacerRegression.AltDragInput]::End() }
            Wait-Ui {
                $now = Read-Spacing $window
                if ($axis) {return [Math]::Abs($now.Y-$min-$max) -le $rounding -and $now.X -eq $before.X}
                return [Math]::Abs($now.X-$min-$max) -le $rounding -and $now.Y -eq $before.Y
            } 'released thumb applies just the chosen axis'
            Assert-Ui ([FileSpacerRegression.Native]::IsWindowVisible($slider.Popup)) 'release keeps the slider open'
            Close-SpacingOutside $window $slider
        }
        $saved = Read-Spacing $window
        $actualPositions=Use-UiView $window {param($v) $v.Positions()}
        Assert-Ui ($actualPositions -eq $seed) "spacing preserves free coordinates: $actualPositions"
        Close-UiFolder $window
        $window = Open-UiFolder $folder 6
        Wait-Ui { $s=Read-Spacing $window; $s.X -eq $saved.X -and $s.Y -eq $saved.Y } 'per-folder spacing after reopen'
        $restored=Get-PreviewRestoredPositions $window $seed
        Assert-Ui ((Use-UiView $window {param($v) $v.Positions()}) -eq $restored) 'free coordinates after reopen'
        Use-UiView $window {
            param($v)
            $pitch=$v.Spacing
            for ($i=0;$i -lt 6;++$i) {
                $v.Position(('item-{0:D4}' -f $i),($i%3)*2*$pitch.X,([int][Math]::Floor($i/3))*3*$pitch.Y)
            }
        }
        Choose-Placement 'IDM_KEEP_ICONS_ALIGNED'
        Use-UiView $window {param($v) $v.Select('item-0001')}
        function Read-OccupiedCells {
            Use-UiView $window {
                param($v)
                $pitch=$v.Spacing
                $entries=@([regex]::Matches($v.Positions(),'([^;=]+)=(-?\d+),(-?\d+)') | Sort-Object {$_.Groups[1].Value})
                $origin=$entries[0].Groups
                foreach ($e in $entries) {
                    $x=[Math]::Round(([int]$e.Groups[2].Value-[int]$origin[2].Value)/$pitch.X)
                    $y=[Math]::Round(([int]$e.Groups[3].Value-[int]$origin[3].Value)/$pitch.Y)
                    "$($e.Groups[1].Value)=$x,$y"
                }
            }
        }
        $cells=(Read-OccupiedCells) -join ';'
        Assert-Ui (($cells -split ';').Count -eq 6 -and $cells -match '=4,3') 'sparse fixture contains two skipped columns and rows'
        foreach ($axis in @(0,1)) {
            $slider=Open-SpacingSlider $window $axis
            [FileSpacerViewMenuTests.MenuUi]::Key($slider.Popup,0x24,1)
            Close-SpacingOutside $window $slider
        }
        Assert-Ui (((Read-OccupiedCells) -join ';') -eq $cells) 'snap preserves occupied cells and holes'
        Assert-Ui (Use-UiView $window {param($v) $v.SnapToGrid -and !$v.AutoArrange -and (@($v.Selected()) -join ';') -eq 'item-0001'}) 'snap and selection preserved'
        Close-UiFolder $window
    }
} catch { $failure=$_; throw } finally { Stop-Ui $failure }
