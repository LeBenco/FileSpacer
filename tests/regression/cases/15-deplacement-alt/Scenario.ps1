param(
    [string]$ProjectRoot,
    [Parameter(Mandatory=$true)][uint32]$ViewMode,
    [Parameter(Mandatory=$true)][string]$ViewName,
    [int]$IconSize=-1,
    [bool]$FullNamesOnSelection=$false
)
# Mode variants exercise both the empty background and a selected item.
& (Join-Path $PSScriptRoot 'Remaining.ps1') -ProjectRoot $ProjectRoot -Case Mode -ViewMode $ViewMode -ViewName $ViewName -IconSize $IconSize -FullNamesOnSelection $FullNamesOnSelection
