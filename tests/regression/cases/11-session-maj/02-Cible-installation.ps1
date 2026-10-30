# Test-Category: Internal
# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name UpdateInstallTargetTests -CaseDirectory $PSScriptRoot -ProductionSources @('UpdateInstallTarget.cpp') -Standard 'c++14' -Libraries @('Advapi32.lib')
Invoke-TestProgram $test @($RunDirectory)
