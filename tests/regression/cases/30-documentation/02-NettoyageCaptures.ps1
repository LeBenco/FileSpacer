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
# Former packages: only directories named like a run, beside the current one; links are never followed.
$packages=$ast.Find({param($node)$node-is[Management.Automation.Language.FunctionDefinitionAst] -and $node.Name-eq'Remove-FormerPackages'},$true)
if(!$packages){throw 'The guest package cleanup function is missing.'}
. ([scriptblock]::Create($packages.Extent.Text))
$packageRoot=Join-Path $env:FS_REGRESSION_RUN 'package-cleanup\FileSpacerTests'
$kept=Join-Path $env:FS_REGRESSION_RUN 'package-cleanup\outside'
[void][IO.Directory]::CreateDirectory($kept);[IO.File]::WriteAllText((Join-Path $kept 'keep.txt'),'Outside the packages')
foreach($name in @('aaaa1111','bbbb2222','cccc3333','unexpected-name')){
    [void][IO.Directory]::CreateDirectory((Join-Path $packageRoot ($name+'\input')))
    [IO.File]::WriteAllText((Join-Path $packageRoot ($name+'\input\file.txt')),'Package content')
}
$inner=Join-Path $packageRoot 'aaaa1111\link';$named=Join-Path $packageRoot 'dddd4444'
try{
    [void](New-Item -ItemType Junction -Path $inner -Target $kept)
    [void](New-Item -ItemType Junction -Path $named -Target $kept)
    $result=Remove-FormerPackages (Join-Path $packageRoot 'bbbb2222')
    if($result.Removed-ne2 -or (@($result.Kept)-join',')-ne'dddd4444'){throw ('Unexpected package cleanup result: removed='+$result.Removed+' kept='+(@($result.Kept)-join','))}
    foreach($name in @('aaaa1111','cccc3333')){if(Test-Path -LiteralPath (Join-Path $packageRoot $name)){throw ('A former package remains: '+$name)}}
    foreach($name in @('bbbb2222','unexpected-name')){if(!(Test-Path -LiteralPath (Join-Path $packageRoot ($name+'\input\file.txt')))){throw ('The cleanup removed what it must keep: '+$name)}}
    if(!(Test-Path -LiteralPath $named) -or !(Test-Path -LiteralPath (Join-Path $kept 'keep.txt'))){throw 'The package cleanup followed or removed a link.'}
}finally{foreach($link in @($inner,$named)){if(Test-Path -LiteralPath $link){[IO.Directory]::Delete($link)}}}
$misplaced=$false
try{[void](Remove-FormerPackages (Join-Path $env:FS_REGRESSION_RUN 'package-cleanup\elsewhere\bbbb2222'))}catch{$misplaced=$true}
if(!$misplaced){throw 'A package location outside FileSpacerTests was accepted.'}
Write-Output 'PASS: remove only the matching documentation input, preserving captures and reports; reject other tests, run identities and paths; remove former packages only, never through a link.'
