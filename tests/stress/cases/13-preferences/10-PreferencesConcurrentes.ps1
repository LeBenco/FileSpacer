# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot '..\..\..\regression\cases\13-preferences\10-PreferencesConcurrentes.ps1') -ProjectRoot $ProjectRoot -Rounds 100
