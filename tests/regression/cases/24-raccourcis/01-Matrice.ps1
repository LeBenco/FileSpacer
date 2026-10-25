# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
if (!$env:FS_REGRESSION_RUN) { throw 'Run through tests.cmd to isolate and restore the test registry.' }
$test=Build-NativeTest -Name ShortcutTests -CaseDirectory $PSScriptRoot -Standard c++14 -Defines @('FILESPACER_DEBUG') -ProductionSources @('Settings.cpp','FolderStateStore.cpp','ShortcutSettingsPage.cpp','UIStrings.cpp','DPI.cpp','WindowResizeGesture.cpp') -Sqlite -Resources -Libraries @('Gdi32.lib','Comctl32.lib','UxTheme.lib','Ole32.lib','OleAut32.lib','Oleacc.lib','Shell32.lib','Shlwapi.lib','Advapi32.lib','Uuid.lib')
Invoke-TestProgram $test @('en')
Invoke-TestProgram $test @('fr')
