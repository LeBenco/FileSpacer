# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name ItemValidationTests -CaseDirectory $PSScriptRoot -Extract ItemValidation -Standard c++14 -Libraries @('Ole32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib')
Invoke-TestProgram $test @($RunDirectory)
