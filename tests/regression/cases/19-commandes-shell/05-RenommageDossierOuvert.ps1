# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'shell-rename-open-folder'
    $parent=Join-Path $script:UiRoot 'Rename-parent';$before=Join-Path $parent 'Before';$after=Join-Path $parent 'After'
    [void][IO.Directory]::CreateDirectory($before);[IO.File]::WriteAllText((Join-Path $before 'Payload'),'kept-after-directory-rename')
    $first=Open-UiFolder $parent 1;$child=Open-UiFolder $before 1
    Focus-ShellFixture $child 'Payload';$geometry=[FileSpacerRegression.Native]::Bounds($child)
    Assert-UiSelected $child 'Payload'
    Rename-ShellFixture $first $parent 'Before' 'After'
    $renamed=Track-UiFolder $after
    Assert-Ui ($renamed -eq $child) 'Renaming an open folder replaced or duplicated its spatial window.'
    Assert-SearchNames $renamed @('Payload');Assert-UiSelected $renamed 'Payload'
    $current=[FileSpacerRegression.Native]::Bounds($renamed)
    Assert-Ui ($current.Left -eq $geometry.Left -and $current.Top -eq $geometry.Top -and $current.Width -eq $geometry.Width -and $current.Height -eq $geometry.Height) 'Folder rename changed window geometry.'
    Focus-ShellFixture $renamed
    [FileSpacerRegression.Native]::Keys($renamed,0x74,$false,$false)
    Assert-SearchNames $renamed @('Payload')
    Assert-Ui ([IO.File]::ReadAllText((Join-Path $after 'Payload')) -ceq 'kept-after-directory-rename') 'Renamed folder contents changed.'
    Close-UiFolder $renamed
    $reopened=Open-UiFolder $after 1
    Assert-SearchNames $reopened @('Payload')
    Assert-Ui (@(Find-UiFolder $before).Count -eq 0) 'The obsolete folder identity was reopened.'
    Write-Host 'PASS: UI rename retargets the existing open folder, preserves its selection/geometry and reopens the new identity.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
