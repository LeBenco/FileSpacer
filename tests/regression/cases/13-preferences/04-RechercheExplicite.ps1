# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\SearchAssertions.ps1')
$testFailure=$null
try {
    Initialize-Ui 'explicit-search'
    $folder=Join-Path $script:UiRoot 'ExplicitSearch'
    [void](New-Item -ItemType Directory -Path $folder)
    foreach ($name in @('keep.txt','other.txt')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $window=Open-UiFolder $folder 2
    $edit=Get-UiSearchEdit $window
    $button=Get-UiSearchSubmit $window
    [FileSpacerRegression.Native]::SetText($edit,'keep')
    Assert-SearchCount $window 2
    [void][FileSpacerRegression.Native]::Message($button,0xf5,0,0) # BM_CLICK
    Assert-SearchCount $window 1
    [FileSpacerRegression.Native]::SetText($edit,'')
    Assert-SearchCount $window 1
    [void][FileSpacerRegression.Native]::Message($button,0xf5,0,0)
    Assert-SearchCount $window 2
    Write-Host 'PASS: with live search disabled, only an explicit submit changes the filter; clearing also requires submit.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
