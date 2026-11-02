# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot '09-RepriseProvisoire.ps1') -ProjectRoot $ProjectRoot -ItemCount 5000 -MeasureRecovery
