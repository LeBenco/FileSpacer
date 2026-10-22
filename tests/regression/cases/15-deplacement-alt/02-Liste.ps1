# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot 'Scenario.ps1') -ProjectRoot $ProjectRoot -ViewMode 3 -ViewName 'List' -IconSize -1
