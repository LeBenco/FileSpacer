# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $ProjectRoot 'tests\client\HyperVSmoke.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors|Out-String)}
$function=$ast.Find({param($node)$node-is[Management.Automation.Language.FunctionDefinitionAst] -and $node.Name-eq'Remove-DocumentationCaptureInput'},$true)
if(!$function){throw 'The guest capture cleanup function is missing.'}
. ([scriptblock]::Create($function.Extent.Text))
$run='vm-0123456789abcdef0123456789abcdef'
$fixture=Join-Path $env:FS_REGRESSION_RUN 'capture-cleanup\FileSpacerTests'
function New-CleanupFixture([string]$Name,[string]$Case='99-documentation\01-Capture.ps1') {
    $root=Join-Path $fixture $Name
    $inputPath=Join-Path $root 'input';$output=Join-Path $root 'output'
    [void][IO.Directory]::CreateDirectory($inputPath);[void][IO.Directory]::CreateDirectory($output)
    @{RunId=$run}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $inputPath 'manifest.json') -Encoding UTF8
    @{Cases=@($Case)}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $inputPath 'selection.json') -Encoding UTF8
    [IO.File]::WriteAllText((Join-Path $inputPath 'temporary.zip'),'Disposable source package')
    [IO.File]::WriteAllText((Join-Path $output 'capture.gif'),'Final capture')
    [IO.File]::WriteAllText((Join-Path $output 'result.json'),'Preserved diagnostic report')
    return $inputPath
}
function Expect-CleanupRejected([string]$Root,[string]$Id) {
    $failed=$false
    try{[void](Remove-DocumentationCaptureInput $Root $Id)}catch{$failed=$true}
    if(!$failed -or !(Test-Path -LiteralPath (Join-Path $Root 'temporary.zip'))){throw 'An unrelated or mismatched input was removed.'}
}
$inputPath=New-CleanupFixture '1234abcd'
if(!(Remove-DocumentationCaptureInput $inputPath $run) -or (Test-Path -LiteralPath $inputPath)){throw 'The completed capture input remains.'}
$output=Join-Path (Split-Path $inputPath -Parent) 'output'
if(!(Test-Path -LiteralPath (Join-Path $output 'capture.gif')) -or !(Test-Path -LiteralPath (Join-Path $output 'result.json'))){throw 'Final captures or reports were removed.'}
Expect-CleanupRejected (New-CleanupFixture '2345bcde' '37-colonnes\01-Options.ps1') $run
Expect-CleanupRejected (New-CleanupFixture '3456cdef') 'vm-fedcba9876543210fedcba9876543210'
Expect-CleanupRejected (New-CleanupFixture 'unexpected-name') $run
$cleanupInput=New-CleanupFixture '4567def0' '99-documentation\03-ReclaimInputs.ps1'
if(!(Remove-DocumentationCaptureInput $cleanupInput $run)){throw 'The cleanup request did not remove its own disposable input.'}
$outsideInput=New-CleanupFixture '5678abcd'
$junction=Join-Path $fixture '6789abcd'
try{
    [void](New-Item -ItemType Junction -Path $junction -Target (Split-Path $outsideInput -Parent))
    Expect-CleanupRejected (Join-Path $junction 'input') $run
}finally{if(Test-Path -LiteralPath $junction){[IO.Directory]::Delete($junction)}}
if(!(Test-Path -LiteralPath (Join-Path $outsideInput 'temporary.zip'))){throw 'Junction cleanup changed its target.'}
Write-Output 'PASS: remove only the matching documentation input, preserving captures and reports; reject other tests, run identities and paths.'
