# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name LanguageTests -CaseDirectory $PSScriptRoot
Invoke-TestProgram $test @((Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'))
