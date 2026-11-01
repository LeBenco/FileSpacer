# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
if (!$env:FS_REGRESSION_RUN) { throw 'Run through tests.cmd to isolate and restore the test registry.' }
$test=Build-NativeTest -Name ShortcutTests -CaseDirectory $PSScriptRoot -Standard c++14 -Defines @('FILESPACER_DEBUG') -ProductionSources @('FolderStateStore.cpp','UIStrings.cpp','DPI.cpp','WindowResizeGesture.cpp') -Sqlite -Resources -Libraries @('Gdi32.lib','Comctl32.lib','UxTheme.lib','Ole32.lib','OleAut32.lib','Oleacc.lib','Shell32.lib','Shlwapi.lib','Advapi32.lib','Uuid.lib')
$fixture=Join-Path $RunDirectory ('powertoys-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'Microsoft\PowerToys\GrabAndMove'))
Invoke-TestProgram $test @('en',$fixture)
Invoke-TestProgram $test @('fr',$fixture)
foreach ($language in @('en','fr','de','el','es','it','pt')) {
    Invoke-TestProgram $test @($language,$fixture,'notice-only')
}
