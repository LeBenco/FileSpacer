# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\ViewLayout.ps1')
. (Join-Path $PSScriptRoot '..\..\common\NavigationScenarios.ps1')
Invoke-ExistingParent
