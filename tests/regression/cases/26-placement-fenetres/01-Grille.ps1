# Test-Category: Internal
# Functional checks only: no timing threshold or optimality oracle.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name WindowPlacementTests -CaseDirectory $PSScriptRoot -ProductionSources @('WindowPlacement.cpp') -Standard 'c++14' -Libraries @('Dwmapi.lib')
Invoke-TestProgram $test @()
