# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name ViewStateTests -CaseDirectory $PSScriptRoot -Extract View
foreach ($scenario in @('success','all-fail','columns-fail','width-fail','sort-fail','group-fail','no-manager','no-group')) { Invoke-TestProgram $test @($scenario) }
