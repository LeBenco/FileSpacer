# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
Add-Type -AssemblyName System.Drawing
$testFailure=$null
try {
    Initialize-Ui 'shell-thumbnails'
    $folder=Join-Path $script:UiRoot 'Images';[void][IO.Directory]::CreateDirectory($folder)
    $expected=@(foreach ($index in 1..64) {
        $name=('image-{0:D2}.png' -f $index);$bitmap=New-Object Drawing.Bitmap -ArgumentList 160,120
        $graphics=[Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([Drawing.Color]::FromArgb(($index*31%256),($index*67%256),($index*97%256)))
            $graphics.FillRectangle([Drawing.Brushes]::White,($index%80),20,40,60)
            $bitmap.Save((Join-Path $folder $name),[Drawing.Imaging.ImageFormat]::Png)
        } finally { $graphics.Dispose();$bitmap.Dispose() };$name
    })
    $window=Open-UiFolder $folder 64
    foreach ($presentation in @(@{Mode=4;Size=16},@{Mode=3;Size=16},@{Mode=1;Size=96})) {
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize($presentation.Mode,$presentation.Size)}
        Wait-Ui { (Use-UiView $window {param($view) $view.Mode}) -eq $presentation.Mode } 'requested native mode is visible'
        Assert-SearchNames $window $expected
        Focus-ShellFixture $window 'image-01.png';Assert-UiSelected $window 'image-01.png'
        [FileSpacerRegression.Native]::Keys($window,0x74,$false,$false)
        Assert-SearchNames $window $expected
    }
    Save-ShellScreenshot $window 'large-images.png'
    Save-ShellCoverage 'coverage.json' @{Automatic='PASS: identities, modes and selection';ThumbnailRendering='REVIEW: inspect the screenshot and live images using manual case V01';ProviderExtractionNotAssumed=$true}
    Write-Host 'PASS: 64 real images survive mode changes, selection and refresh. Thumbnail pixels require visual validation; large-images.png is evidence only.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
