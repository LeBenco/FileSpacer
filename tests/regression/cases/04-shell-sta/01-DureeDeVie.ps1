# Test-Category: Internal
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test = Build-NativeTest -Name ShellLifetimeTests -CaseDirectory $PSScriptRoot -Extract Shell -ProductionSources @('COMUtils.cpp') -Libraries @('Ole32.lib','Shlwapi.lib','User32.lib','Uuid.lib')
Invoke-TestProgram $test @()
