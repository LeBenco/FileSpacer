# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name SettingsSizeTests -CaseDirectory $PSScriptRoot -Extract Settings -ProductionSources @('UIStrings.cpp','DPI.cpp') -Resources -Libraries @('User32.lib','Comctl32.lib','Gdi32.lib')
Invoke-TestProgram $test @()
