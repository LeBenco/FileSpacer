# Test-Category: UI
# Same identities/fallback assertions with real visible 800x400 Shell views.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot '05-SelectionAll.ps1') -ProjectRoot $ProjectRoot -Presented
