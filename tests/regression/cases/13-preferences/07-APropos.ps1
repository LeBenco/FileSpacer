# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'about-icon' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot 'Folder'
    [void][IO.Directory]::CreateDirectory($folder)
    $window=Open-UiFolder $folder 0
    [void](Show-UiSettings $window)
    $about=Set-UiPage 4
    $icon=[FileSpacerRegression.Native]::GetDlgItem($about,(Get-UiId 'IDC_ABOUT_ICON'))
    Assert-Ui ($icon -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindowVisible($icon)) 'About has no visible application icon.'
    Assert-Ui ([FileSpacerRegression.Native]::Message($icon,0x171,0,0) -ne 0) 'About application icon was not loaded.'
    $bounds=[FileSpacerRegression.Native]::Bounds($icon)
    $dpi=[FileSpacerRegression.Native]::GetDpiForWindow($about)
    $size=[int][Math]::Round(48*$dpi/96.0)
    Assert-Ui ($bounds.Width -eq $size -and $bounds.Height -eq $size) 'About icon does not use the 48-pixel size at the current DPI.'
    foreach ($label in [FileSpacerRegression.Native]::Children($about,'Static')) {
        if ($label -eq $icon -or ![FileSpacerRegression.Native]::IsWindowVisible($label)) { continue }
        $textBounds=[FileSpacerRegression.Native]::Bounds($label)
        $overlap=$bounds.Left -lt $textBounds.Right -and $textBounds.Left -lt $bounds.Right -and
            $bounds.Top -lt $textBounds.Bottom -and $textBounds.Top -lt $bounds.Bottom
        Assert-Ui (!$overlap) 'About icon overlaps a text label.'
    }
    Press-UiSettings 5
    Write-Host ('PASS: loaded 48-pixel About icon; actual size='+$size+' pixels at '+$dpi+' DPI, without overlapping labels.')
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
