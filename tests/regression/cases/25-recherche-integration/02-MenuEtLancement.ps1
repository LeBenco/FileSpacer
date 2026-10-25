# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name EverythingTests -CaseDirectory $PSScriptRoot -ProductionSources @('Everything.cpp','SearchBox.cpp','WinUtils.cpp','DPI.cpp','UIStrings.cpp') -Resources -CommonControls -Standard 'c++14' -Libraries @('Ole32.lib','Gdi32.lib','Comctl32.lib','UxTheme.lib','Dwmapi.lib')
Invoke-TestProgram $test
if ($env:FILESPACER_EXPECT_EVERYTHING_ABSENT -eq '1') { Invoke-TestProgram $test @('--expect-absent') }
