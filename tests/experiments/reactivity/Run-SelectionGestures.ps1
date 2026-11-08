# Host only, explicitly invoked. Keep registry/database isolation in tests.cmd.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path,
    [ValidateRange(30,900)][int]$TimeoutSeconds=300)
$ErrorActionPreference='Stop'
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
if(Get-Process FileSpacer -ErrorAction SilentlyContinue){throw 'Close FileSpacer first; no process was stopped.'}
$leaf='56-selection-gestures-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$stage=Join-Path $ProjectRoot ('tests\regression\cases\'+$leaf)
if(Test-Path -LiteralPath $stage){throw 'The unique staging directory already exists.'}
[void][IO.Directory]::CreateDirectory($stage)
$case=Join-Path $stage '01-Gestures.ps1'
$exitCode=1
try {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '55-SelectionGestures.ps1') -Destination $case
    Push-Location -LiteralPath $ProjectRoot
    try {
        & $env:ComSpec /d /c tests.cmd -Category UI -Pattern ($leaf+'\01-Gestures.ps1') -TimeoutSeconds $TimeoutSeconds
        $exitCode=$LASTEXITCODE
    }finally{Pop-Location}
}finally{
    if(Test-Path -LiteralPath $case){Remove-Item -LiteralPath $case}
    # Nonrecursive: unexpected contents are preserved and cleanup reports an error.
    Remove-Item -LiteralPath $stage
}
exit $exitCode
