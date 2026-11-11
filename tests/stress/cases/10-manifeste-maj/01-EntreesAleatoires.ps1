# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\..\regression\common\Native.ps1')
$test=Build-NativeTest -Name UpdateManifestStressTests -CaseDirectory $PSScriptRoot -ProductionSources @('UpdateManifest.cpp')
Invoke-TestProgram $test @()
