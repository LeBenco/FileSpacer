# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name GestureInputTests -CaseDirectory $PSScriptRoot -Standard c++14 -Defines @('FILESPACER_DEBUG') -ProductionSources @('WindowResizeGesture.cpp') -Libraries @('Gdi32.lib')
Invoke-TestProgram $test @('--secondary')
