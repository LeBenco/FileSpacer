# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
$failure=$null
try {
    Initialize-Ui 'view-preview-keyboard' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 300
    $window=Open-UiFolder $folder 300
    Reset-PreviewMode $window
    $modes=Get-PreviewModes $window
    $initial=Read-PreviewMode $window
    $state=Set-PreviewState $window
    $popup=Open-PreviewMenu $window
    $modeCount=$popup.Entries.Count
    $all=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    Assert-Ui ($null -ne $all -and $all.Entries.Count -gt $modeCount) 'The native View menu has no entry after the display modes.'
    $all.Entries|ForEach-Object {[pscustomobject]@{Command=$_.Command;Label=$_.Label;State=$_.State}}|ConvertTo-Json -Depth 3|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'menu-entries.json') -Encoding UTF8
    $toggle=$all.Entries[$modeCount]
    # Down highlights the entries in order; the menu skips its separators.
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x28,4);Wait-PreviewMode $window $modes[3]
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x28,$modeCount-3);Wait-PreviewMode $window $initial
    Assert-PreviewState $window $state
    Write-Host 'PASS: keyboard highlight previews a mode and leaves it on an entry outside the modes.'
    [FileSpacerViewMenuTests.MenuUi]::Key($window,0x0D,1)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window $initial
    # The chosen entry (auto arrange, align to grid...) may legitimately move the icons.
    Wait-Ui {$now=Read-PreviewState $window; $now.Selected -eq $state.Selected -and $now.Focus -eq $state.Focus} 'original selection and focus after the chosen entry' 250
    $popup=Open-PreviewMenu $window
    $after=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
    $entry=@($after.Entries|Where-Object {$_.Command -eq $toggle.Command})
    Assert-Ui ($entry.Count -eq 1) 'The chosen entry disappeared from the native View menu.'
    Assert-Ui ((($entry[0].State -band 8) -ne 0) -ne (($toggle.State -band 8) -ne 0)) ('Enter did not run the chosen entry "{0}".' -f $toggle.Label)
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window $initial
    Write-Host 'PASS: Enter on an entry outside the modes keeps the original mode and runs that entry.'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $initial
    Write-Host 'PASS: the original mode, not the keyboard preview, survives reopening.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
