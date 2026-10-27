# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name ContextMenuTests -CaseDirectory $PSScriptRoot -Standard c++14 -Defines @('FILESPACER_DEBUG') -ProductionSources @('Settings.cpp','FolderStateStore.cpp','UIStrings.cpp','DPI.cpp') -Sqlite -Resources -Libraries @('Gdi32.lib','Comctl32.lib','Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Advapi32.lib','Uuid.lib')
Invoke-TestProgram $test @('en')
Invoke-TestProgram $test @('fr')
