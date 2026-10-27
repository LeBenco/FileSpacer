# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$sourceRoot=$ProjectRoot
$savedPrepared=$env:FS_REGRESSION_PREPARED;$savedCase=$env:FS_REGRESSION_CASE
$fixture=Join-Path $env:FS_REGRESSION_RUN 'prepared-contract'
function Expect-PreparationError([scriptblock]$Action,[string]$Message) {
    $failure=$null
    try { & $Action } catch { $failure=$_.Exception.Message }
    if (!$failure -or !$failure.Contains($Message)) { throw ('Expected preparation error: '+$Message+'; actual='+$failure) }
}
try {
    $ProjectRoot=$fixture
    $prepared=Join-Path $fixture 'prepared'
    [void][IO.Directory]::CreateDirectory($prepared)
    $artifact=Join-Path $prepared 'fixture.exe'
    # This is a copy/identity contract, never an executable integration test.
    [IO.File]::WriteAllText($artifact,'fixture bytes')
    $description=@{}
    foreach($name in @('ProductionSources','Sqlite','Storage','Extract','Resources','CommonControls','Libraries','Standard','Defines')){$description[$name]=''}
    $description.Standard='c++14'
    $hash=(Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash
    @{Native=@(@{Name='Fixture';CaseDirectory='cases';Parameters=$description;Executable='fixture.exe';Sha256=$hash});Managed=@()} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $prepared 'index.json') -Encoding UTF8
    . (Join-Path $sourceRoot 'tests\regression\common\Prepared.ps1')
    $parameters=@{Name='Fixture';CaseDirectory=(Join-Path $fixture 'cases');Standard='c++14'}
    $output=Join-Path $fixture 'output';[void][IO.Directory]::CreateDirectory($output)
    $result=Copy-PreparedNativeTest -Parameters $parameters -OutputDirectory $output
    if ([IO.File]::ReadAllText($result) -cne 'fixture bytes') { throw 'Prepared native artifact was not copied faithfully.' }
    $parameters.Standard='c++17'
    Expect-PreparationError { Copy-PreparedNativeTest -Parameters $parameters -OutputDirectory $output } 'Prepared native arguments differ'
    $parameters.Standard='c++14'
    [IO.File]::WriteAllText($artifact,'changed bytes')
    Expect-PreparationError { Copy-PreparedNativeTest -Parameters $parameters -OutputDirectory $output } 'Prepared executable hash mismatch'
    $parameters.Name='Missing'
    Expect-PreparationError { Copy-PreparedNativeTest -Parameters $parameters -OutputDirectory $output } 'No unique prepared native test'
    Expect-PreparationError { Add-Type -TypeDefinition 'public class Unprepared {}' } 'No host-built helper'
    Write-Host 'PASS: prepared artifact identity, build arguments and missing-helper failures.'
} finally {
    $env:FS_REGRESSION_PREPARED=$savedPrepared;$env:FS_REGRESSION_CASE=$savedCase
    $ProjectRoot=$sourceRoot
}
