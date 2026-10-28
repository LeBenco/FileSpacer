# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
. (Join-Path $PSScriptRoot '..\..\common\NativeShellOutput.ps1')
$output=New-NativeShellOutput
try {
    $test = Build-NativeTest -Name NativeShellStateTests -CaseDirectory $PSScriptRoot -ProductionSources @('MixedFolder.cpp','ShellViewHost.cpp','ShellViewFilter.cpp','ShellViewSettings.cpp','ShellViewSelection.cpp','ShellViewIcons.cpp','FolderStateStore.cpp','DPI.cpp') -Sqlite -Storage -CommonControls -Libraries @('Gdi32.lib','Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib') -OutputDirectory $output
    Invoke-TestProgram $test
} finally { Save-NativeShellOutput $output }
