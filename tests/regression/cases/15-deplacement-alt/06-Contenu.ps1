# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot 'Scenario.ps1') -ProjectRoot $ProjectRoot -ViewMode 8 -ViewName 'Content' -IconSize -1
