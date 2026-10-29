# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'SearchIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'everything-path' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot 'Folder'
    [void](New-Item -ItemType Directory -Path $folder)
    # A stand-in executable in a directory with spaces, which "Copy as path" quotes.
    $tools=Join-Path $script:UiRoot 'Outils de recherche'
    [void](New-Item -ItemType Directory -Path $tools)
    $executable=Join-Path $tools 'Everything.exe'
    [IO.File]::WriteAllText($executable,'fixture')
    $window=Open-UiFolder $folder 0

    Write-Host 'STEP: quoted path with surrounding spaces, as pasted from Copy as path'
    Set-SearchOptions $window $false 5 ('  "'+$executable+'"  ')
    $stored=[string](Get-UiPreference 'EverythingPath')
    Assert-Ui ($stored -ceq $executable) ('Stored Everything path is not normalized: ['+$stored+']')
    Write-Host 'PASS: the quotes and spaces around the pasted path are removed when applying.'

    Write-Host 'STEP: an already clean path is stored unchanged'
    Set-SearchOptions $window $false 5 $executable
    $stored=[string](Get-UiPreference 'EverythingPath')
    Assert-Ui ($stored -ceq $executable) ('Clean Everything path was altered: ['+$stored+']')
    Write-Host 'PASS: a clean path is stored as typed.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
