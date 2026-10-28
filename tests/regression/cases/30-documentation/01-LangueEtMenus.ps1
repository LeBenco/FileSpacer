# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name DocumentationTests -CaseDirectory $PSScriptRoot -Standard c++14 -Extract Documentation -Resources -ProductionSources @('UIStrings.cpp','DPI.cpp') -Libraries @('Shell32.lib','Gdi32.lib','Comctl32.lib')
Invoke-TestProgram $test @($RunDirectory)
