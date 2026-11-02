# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
Invoke-TestProgram $test @((Join-Path $RunDirectory 'mutex.sqlite3'))
