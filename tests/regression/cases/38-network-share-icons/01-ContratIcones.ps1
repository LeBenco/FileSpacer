# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name NetworkShareIconTests -CaseDirectory $PSScriptRoot -Standard c++14 -Defines @('STRICT_TYPED_ITEMIDS') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Propsys.lib','Uuid.lib','Version.lib')
Invoke-TestProgram $test @($RunDirectory,(Get-TestBuildPath $ProjectRoot 'FileSpacer.exe'))
