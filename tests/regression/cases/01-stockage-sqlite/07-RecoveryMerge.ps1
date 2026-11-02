# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name StorageRecoveryMergeTests -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp','ShellViewIcons.cpp','DPI.cpp') -Storage -Sqlite -CommonControls -Standard c++14 -Libraries @('Ole32.lib','Shell32.lib','Shlwapi.lib','Gdi32.lib','Uuid.lib')
$fixture=Join-Path $RunDirectory 'canonical-fixture'
Invoke-TestProgram $test @($fixture)
$verifier=Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
$before=(Get-FileHash -LiteralPath ($fixture+'.sqlite3')).Hash
Invoke-TestProgram $verifier @('--verify-product',($fixture+'.sqlite3'),'canonical',$fixture,($fixture+'.positions'))
if((Get-FileHash -LiteralPath ($fixture+'.sqlite3')).Hash-ne$before){throw 'Read-only verifier changed database pages.'}
