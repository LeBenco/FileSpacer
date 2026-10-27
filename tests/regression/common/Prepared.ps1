# Guest execution never invokes a compiler. Unknown or changed inputs fail closed.
$env:FS_REGRESSION_PREPARED=Join-Path $ProjectRoot 'prepared'
$global:FileSpacerPreparedManifest=Get-Content -LiteralPath (Join-Path $env:FS_REGRESSION_PREPARED 'index.json') -Raw | ConvertFrom-Json

function Get-PreparedHash([string]$Text) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','') }
    finally { $sha.Dispose() }
}
function Add-Type {
    param([string[]]$Path,[string]$TypeDefinition,[string[]]$AssemblyName,
        [string[]]$ReferencedAssemblies,[switch]$PassThru)
    if ($AssemblyName) { Microsoft.PowerShell.Utility\Add-Type -AssemblyName $AssemblyName -PassThru:$PassThru; return }
    $case=if($env:FS_REGRESSION_CASE){$env:FS_REGRESSION_CASE}else{'runner'}
    $sources=@()
    if ($Path) { foreach($file in $Path){$sources+=Get-PreparedHash ([IO.File]::ReadAllText($file))} }
    elseif ($TypeDefinition) { $sources+=Get-PreparedHash $TypeDefinition }
    else { throw 'Unsupported Add-Type invocation in a prepared run.' }
    foreach($hash in $sources) {
        $entry=@($global:FileSpacerPreparedManifest.Managed | Where-Object { $_.Case -eq $case -and $_.Hashes -contains $hash })
        if ($entry.Count -ne 1) { throw ('No host-built helper for '+$case+' / '+$hash) }
        $assembly=Join-Path $env:FS_REGRESSION_PREPARED $entry[0].Assembly
        if ((Get-FileHash -LiteralPath $assembly -Algorithm SHA256).Hash -ne $entry[0].Sha256) { throw 'Prepared helper hash mismatch.' }
        $loaded=[Reflection.Assembly]::LoadFrom($assembly)
        if ($PassThru) { $loaded.GetExportedTypes() }
    }
}
function Copy-PreparedNativeTest($Parameters,[string]$OutputDirectory) {
    $caseDirectory=[IO.Path]::GetFullPath($Parameters.CaseDirectory)
    $relative=$caseDirectory.Substring($ProjectRoot.TrimEnd('\').Length+1)
    $entries=@($global:FileSpacerPreparedManifest.Native | Where-Object { $_.Name -eq $Parameters.Name -and $_.CaseDirectory -eq $relative })
    if ($entries.Count -ne 1) { throw ('No unique prepared native test: '+$Parameters.Name) }
    $entry=$entries[0]
    foreach($name in @('ProductionSources','Sqlite','Storage','Extract','Resources','CommonControls','Libraries','Standard','Defines')) {
        $actual=if($Parameters.ContainsKey($name)){@($Parameters[$name]) -join '|'}else{''}
        if ($actual -cne $entry.Parameters.$name) { throw ('Prepared native arguments differ: '+$Parameters.Name+' / '+$name) }
    }
    $source=Join-Path $env:FS_REGRESSION_PREPARED $entry.Executable
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.Sha256) { throw 'Prepared executable hash mismatch.' }
    $target=Join-Path $OutputDirectory ($Parameters.Name+'.exe')
    Copy-Item -LiteralPath $source -Destination $target
    return $target
}
