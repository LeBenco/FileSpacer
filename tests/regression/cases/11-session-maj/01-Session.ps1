# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name UpdateSessionTests -CaseDirectory $PSScriptRoot -ProductionSources @('UpdateSessionStore.cpp') -Sqlite
Invoke-TestProgram $test @((Join-Path $RunDirectory "state.sqlite3"))
