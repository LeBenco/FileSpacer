# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name JumpListRetryTests -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Extract JumpLists -Standard c++14 -Libraries @('Ole32.lib','Shell32.lib','Uuid.lib','Advapi32.lib')
$folder=Join-Path $RunDirectory 'pinned'
[void][IO.Directory]::CreateDirectory($folder)
Invoke-TestProgram $test @((Join-Path $RunDirectory 'retry.sqlite3'),$folder)
