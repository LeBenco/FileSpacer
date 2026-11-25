. (Join-Path $PSScriptRoot 'GridDrag.ps1')

function Open-SpacingSlider([IntPtr]$Window,[int]$Axis) {
    $bar = [FileSpacerRegression.Native]::Children($Window,'msctls_statusbar32')[0]
    $buttons = @([FileSpacerRegression.Native]::Children($bar,'Button') |
        Sort-Object { [FileSpacerRegression.Native]::Bounds($_).Left })
    Assert-Ui ($buttons.Count -eq 2) 'Expected both spacing buttons'
    $button = $buttons[$Axis]
    Wait-Ui { [FileSpacerRegression.Native]::IsWindowVisible($button) -and
        [FileSpacerRegression.Native]::IsWindowEnabled($button) } "available spacing axis $Axis ($([FileSpacerRegression.Native]::Text($button)), mode=$((Read-PreviewMode $Window).Mode), pitch=$((Read-Spacing $Window).X)x$((Read-Spacing $Window).Y))"
    $bounds = [FileSpacerRegression.Native]::Bounds($button)
    $popupName = [FileSpacerRegression.Native]::Text($button)
    [FileSpacerViewMenuTests.MenuUi]::Click(($bounds.Left+$bounds.Right)/2,($bounds.Top+$bounds.Bottom)/2,$Window)
    $pidValue = [uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$pidValue)
    $found = @{}
    Wait-Ui {
        foreach ($popup in [FileSpacerRegression.Native]::Top('Static',$popupName,$pidValue)) {
            $tracks = [FileSpacerRegression.Native]::Children($popup,'msctls_trackbar32')
            if ($tracks.Length -eq 1 -and [FileSpacerRegression.Native]::IsWindowVisible($popup)) {
                $found.Popup = $popup
                $found.Track = $tracks[0]
                return $true
            }
        }
        return $false
    } 'native trackbar popup'
    return $found
}

function Read-Spacing([IntPtr]$Window) {
    return Use-UiView $Window {param($v) $v.Spacing}
}

function Close-SpacingOutside([IntPtr]$Window,$Slider) {
    $bar = [FileSpacerRegression.Native]::Children($Window,'msctls_statusbar32')[0]
    $bounds = [FileSpacerRegression.Native]::Bounds($bar)
    [FileSpacerViewMenuTests.MenuUi]::Click($bounds.Left+6,($bounds.Top+$bounds.Bottom)/2,$Window)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($Slider.Popup) } 'outside click dismisses the popup'
}
