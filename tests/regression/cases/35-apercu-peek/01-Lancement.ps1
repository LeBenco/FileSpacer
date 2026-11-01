# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name PeekTests -CaseDirectory $PSScriptRoot -ProductionSources @('Peek.cpp','WinUtils.cpp') -Resources -Standard 'c++14' -Libraries @('Ole32.lib','Dwmapi.lib')
Invoke-TestProgram $test
