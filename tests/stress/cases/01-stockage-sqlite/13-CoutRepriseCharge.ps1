# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot '..\..\..\regression\cases\01-stockage-sqlite\09-RepriseProvisoire.ps1') -ProjectRoot $ProjectRoot -ItemCount 5000 -MeasureRecovery
