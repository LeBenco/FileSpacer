# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'ViewMenu.ps1')
$failure=$null
try {
    Initialize-Ui 'view-preview-external' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=New-PreviewFolder 300
    $window=Open-UiFolder $folder 300
    Reset-PreviewMode $window
    $modes=Get-PreviewModes $window
    $initial=Read-PreviewMode $window
    $state=Set-PreviewState $window
    $popup=Open-PreviewMenu $window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,3);Wait-PreviewMode $window $modes[3]
    # Another process changes the folder while a mode is previewed. The Shell
    # notification is handled inside the menu loop; no enumeration follows it.
    $external=Join-Path $folder 'external.txt'
    [IO.File]::WriteAllText($external,'external')
    Wait-Ui {(Use-UiView $window {param($view) $view.Count}) -eq 301} 'external file shown while previewing'
    [IO.File]::Delete($external)
    Wait-Ui {(Use-UiView $window {param($view) $view.Count}) -eq 300} 'external file removed while previewing'
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0);Wait-PreviewMode $window $modes[0]
    [FileSpacerViewMenuTests.MenuUi]::Escape($window)
    Wait-PreviewMenuClosed $window
    Wait-PreviewMode $window $initial
    Assert-PreviewState $window $state
    Write-Host 'PASS: a folder change by another process during the preview still lets Escape restore the original state.'
    # The window must keep saving afterwards: a new sort order, then an icon move.
    $names=Use-UiView $window {param($view) @($view.Names()|Sort-Object)}
    $last=$names[$names.Count-1]
    Use-UiView $window {param($view) $view.SortBy($false,$false)}
    Wait-Ui {(Use-UiView $window {param($view) @($view.OrderedNames())[0]}) -ceq $last} 'name descending order applied' 250
    Use-UiView $window {param($view) $view.Position($names[6],420,360)}
    $moved=$names[6]+'=420,360'
    Wait-Ui {(Read-PreviewState $window).Positions -like ('*'+$moved+'*')} 'moved icon placed after the preview' 250
    $expected=Read-PreviewState $window
    $expected|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'before-close.json') -Encoding UTF8
    Close-UiFolder $window
    $window=Open-UiFolder $folder 300
    Wait-PreviewMode $window $initial
    Wait-Ui {(Use-UiView $window {param($view) @($view.OrderedNames())[0]}) -ceq $last} 'name descending order restored after reopening' 250
    Read-PreviewState $window|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'after-reopen.json') -Encoding UTF8
    Wait-Ui {(Read-PreviewState $window).Positions -eq $expected.Positions} 'icon positions restored, including the move made after the preview' 250
    Write-Host 'PASS: the sort order and icon positions changed after the preview survive closing and reopening.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
