param(
    [string]$ProjectRoot,
    [Parameter(Mandatory=$true)][uint32]$ViewMode,
    [Parameter(Mandatory=$true)][string]$ViewName,
    [int]$IconSize=-1,
    [bool]$FullNamesOnSelection=$false
)
# Mode variants use the validated empty-folder gesture, with no UIA locator.
& (Join-Path $PSScriptRoot 'Remaining.ps1') -ProjectRoot $ProjectRoot -Case Mode -ViewMode $ViewMode -ViewName $ViewName -IconSize $IconSize -FullNamesOnSelection $FullNamesOnSelection
