# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\PrivateDiagnostics.ps1')
$failure=$null
try { & (Join-Path $PSScriptRoot '..\..\..\regression\cases\18-vue-shell-native\05-SelectionAll.ps1') -ProjectRoot $ProjectRoot -ItemCount 5000 -Presented }
catch {$failure=$_;throw}
finally {
    try { Write-PrivateDiagnostics $ProjectRoot $env:FS_REGRESSION_RUN }
    catch { if(!$failure){throw};Write-Warning ('Additional private diagnostic failure: '+$_.Exception.Message) }
}
