# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
& (Join-Path $PSScriptRoot 'Remaining.ps1') -ProjectRoot $ProjectRoot -Case 'PlainDouble'
