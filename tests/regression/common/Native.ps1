$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
if ($env:OS -ne 'Windows_NT' -or ![Environment]::Is64BitProcess) { throw 'Windows x64 is required.' }
if (!$env:FS_REGRESSION_PREPARED -and !(Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Run tests.cmd, or use an x64 MSVC developer prompt.' }
if ($env:FS_REGRESSION_RUN) { $RunDirectory=$env:FS_REGRESSION_RUN }
else { $RunDirectory=Join-Path $ProjectRoot ('build\regression\individual-'+[Guid]::NewGuid().ToString('N')) }
[void][IO.Directory]::CreateDirectory($RunDirectory)

function Invoke-Compiler([string[]]$Arguments) {
    & cl.exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "MSVC failed with exit code $LASTEXITCODE" }
}
function Build-NativeTest {
    param([Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$CaseDirectory,
        [string[]]$ProductionSources=@(), [switch]$Sqlite, [switch]$Storage,
        [ValidateSet('','Menu','Shell','Settings','View','Adapters','Geometry','LockBadge','Documentation','JumpLists')][string]$Extract='',
        [switch]$Resources, [switch]$CommonControls, [string[]]$Libraries=@(),
        [ValidateSet('c++14','c++17')][string]$Standard='c++17', [string[]]$Defines=@(),
        [string]$OutputDirectory='')
    $output=Join-Path $RunDirectory $Name
    if ($OutputDirectory) { $output=[IO.Path]::GetFullPath($OutputDirectory) }
    [void][IO.Directory]::CreateDirectory($output)
    if ($env:FS_REGRESSION_PREPARED) {
        return Copy-PreparedNativeTest -Parameters $PSBoundParameters -OutputDirectory $output
    }
    if ($Extract) {
        & (Join-Path $PSScriptRoot 'Prepare-Sources.ps1') -Mode $Extract -ProjectRoot $ProjectRoot -CaseDirectory $CaseDirectory -OutputDirectory $output
    }
    $source=Join-Path $CaseDirectory ($Name+'.cpp')
    if ($Extract -in @('View','Adapters','Geometry')) { $source=Join-Path $output ($Name+'.cpp') }
    $exe=Join-Path $output ($Name+'.exe')
    $arguments=@('/nologo',('/std:'+$Standard),'/EHsc','/W4','/MTd','/Z7','/UNDEBUG','/utf-8','/D_CRT_SECURE_NO_WARNINGS',
        ('/I'+(Join-Path $ProjectRoot 'src')), ('/I'+$CaseDirectory), ('/I'+$output),
        ('/Fo'+$output+'\\'), ('/Fe'+$exe))
    if ($Storage) { $arguments+='/DFILESPACER_STORAGE_TEST' }
    foreach ($define in $Defines) { $arguments+=('/D'+$define) }
    if ($Sqlite) {
        $sqliteDirectory=Join-Path $ProjectRoot 'third_party\sqlite'
        $obj=Join-Path $output 'sqlite3.obj'
        Invoke-Compiler @('/nologo','/TC','/W3','/MTd','/Z7','/D_CRT_SECURE_NO_WARNINGS','/DSQLITE_OMIT_LOAD_EXTENSION','/DSQLITE_DQS=0',('/Fo'+$obj),'/c',(Join-Path $sqliteDirectory 'sqlite3.c')) | Out-Host
        $arguments+=@(('/I'+$sqliteDirectory),$obj)
    }
    $arguments+=$source
    foreach ($production in $ProductionSources) { $arguments+=Join-Path $ProjectRoot ('src\'+$production) }
    if ($Resources) {
        $res=Join-Path $output ($Name+'.res')
        & rc.exe /n /c65001 /D_UNICODE /DUNICODE ('/I'+(Join-Path $ProjectRoot 'src')) /fo $res (Join-Path $CaseDirectory ($Name+'.rc')) | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
        $arguments+=$res
    }
    $arguments+=@('/link','/subsystem:console','/incremental:no','/debug',('/pdb:'+(Join-Path $output ($Name+'.pdb'))),'User32.lib',
        '/manifest:embed',('/manifestinput:'+(Join-Path $PSScriptRoot 'TestExecutable.manifest')))
    if ($Resources -or $CommonControls) { $arguments+="/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'" }
    $arguments+=$Libraries
    # Suppress success-stream compiler output: only the executable path is returned.
    Invoke-Compiler $arguments | Out-Host
    return $exe
}
function Invoke-TestProgram([string]$Executable,[string[]]$Arguments=@()) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $Executable (exit $LASTEXITCODE)" }
}
