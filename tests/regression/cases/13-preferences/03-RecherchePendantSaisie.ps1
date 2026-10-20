# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\SearchAssertions.ps1')
$testFailure=$null
try {
    Initialize-Ui 'live-search'
    # Isolate normal typing from the separate Apply-to-existing-query scenario.
    Set-UiPreference 'LiveNameSearch' 1
    $folder=Join-Path $script:UiRoot 'LiveSearch'
    [void](New-Item -ItemType Directory -Path $folder)
    foreach ($name in @('keep.txt','other.txt')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $window=Open-UiFolder $folder 2
    $edit=Get-UiSearchEdit $window
    [FileSpacerRegression.Native]::SetText($edit,'keep')
    Assert-SearchCount $window 1
    [FileSpacerRegression.Native]::SetText($edit,'')
    Assert-SearchCount $window 2
    [FileSpacerRegression.Native]::SetText($edit,'no-match-unique')
    Assert-SearchCount $window 0
    [FileSpacerRegression.Native]::SetText($edit,'')
    Assert-SearchCount $window 2
    Write-Host 'PASS: enabled live search filters changed text and restores the full view when cleared.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
