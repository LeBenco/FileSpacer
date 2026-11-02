# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name StorageRetryTests -CaseDirectory $PSScriptRoot -Extract StorageRetry -Standard 'c++14'
Invoke-TestProgram $test @()
