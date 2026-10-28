# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
. (Join-Path $PSScriptRoot '..\..\common\NativeShellOutput.ps1')
$output=New-NativeShellOutput
try {
    $test=Build-NativeTest -Name NativeSelectedLabelTests -CaseDirectory $PSScriptRoot -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp') -CommonControls -Libraries @('Gdi32.lib','Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib') -OutputDirectory $output
    Invoke-TestProgram $test
} finally { Save-NativeShellOutput $output }
