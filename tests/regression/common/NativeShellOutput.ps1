# The native fixtures are relative to the executable. The complete runner's
# timestamp/case/harness nesting exceeds MAX_PATH before Shell work even starts.
function New-NativeShellOutput {
    $output=Join-Path $ProjectRoot ('build\ns-'+[Guid]::NewGuid().ToString('N'))
    # Reserve room for the longest fixture path, including its long Unicode leaf.
    if ($output.Length+130 -ge 260) { throw 'The native fixture requires a shorter project path (MAX_PATH).' }
    [void][IO.Directory]::CreateDirectory($output)
    @{Directory=$output; Archive='native-artifacts.zip'} | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $RunDirectory 'native-output.json') -Encoding UTF8
    return $output
}
function Save-NativeShellOutput([string]$OutputDirectory) {
    # Retain the short-path originals and put all binaries, logs and fixtures in
    # the run archive too, without copying long fixture names to the deep tree.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($OutputDirectory,(Join-Path $RunDirectory 'native-artifacts.zip'))
}
