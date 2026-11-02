# Copy only a quiescent test database. Hold all source handles until the copy
# finishes, so a new SQLite writer cannot change a page or a WAL mid-export.
function Copy-TestDatabaseSnapshot([string]$Database,[string]$Destination) {
    $records=New-Object Collections.Generic.List[object]
    $sources=New-Object Collections.Generic.List[object]
    [void][IO.Directory]::CreateDirectory($Destination)
    $proof=[ordered]@{DatabasePresent=(Test-Path -LiteralPath $Database);Consistent=$false;Files=@();Error=$null}
    try {
        foreach($suffix in @('','-wal','-shm')) {
            $path=$Database+$suffix
            if(Test-Path -LiteralPath $path) {
                $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
                $sources.Add([pscustomobject]@{Path=$path;Stream=$stream})
            }
        }
        foreach($source in $sources) {
            $target=Join-Path $Destination ([IO.Path]::GetFileName($source.Path))
            $output=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try {$source.Stream.CopyTo($output)} finally {$output.Dispose()}
            $records.Add([pscustomobject]@{Name=[IO.Path]::GetFileName($target);Bytes=(Get-Item -LiteralPath $target).Length;
                Sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash})
        }
        $proof.Consistent=$true
    } catch {$proof.Error=$_.Exception.Message;throw}
    finally {
        foreach($source in $sources){$source.Stream.Dispose()}
        $proof.Files=@($records.ToArray())
        $proof|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $Destination 'snapshot.json') -Encoding UTF8
    }
}
