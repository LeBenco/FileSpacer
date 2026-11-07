# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '04-DesktopPlacement.ps1') -ProjectRoot $ProjectRoot -Operation profile
