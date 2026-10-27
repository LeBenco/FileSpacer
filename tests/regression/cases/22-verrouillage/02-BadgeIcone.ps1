# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name NativeLockBadgeTests -CaseDirectory $PSScriptRoot -Extract LockBadge -Resources -Libraries @('Gdi32.lib')
Invoke-TestProgram $test
