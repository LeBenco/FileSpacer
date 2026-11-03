# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$summaryScript=Join-Path $PSScriptRoot '..\..\common\Write-RegressionSummary.ps1'
$fixture=Join-Path $ProjectRoot ('build\summary-contract-'+[Guid]::NewGuid().ToString('N'))
$savedSummary=$env:GITHUB_STEP_SUMMARY
function Assert-Summary([bool]$Condition,[string]$Message) { if(!$Condition){throw $Message} }
function Write-Fixture($Cases,[int]$Expected,[string]$Status='FAIL',[string]$RunnerError='') {
    $passed=@($Cases|Where-Object Status -eq 'PASS').Count
    $failed=@($Cases|Where-Object Status -eq 'FAIL').Count
    $blocked=@($Cases|Where-Object Status -eq 'BLOCKED').Count
    ConvertTo-Json -InputObject @($Cases) -Depth 5|Set-Content (Join-Path $fixture 'output\results.json') -Encoding UTF8
    @{Status=$Status;Expected=$Expected;Passed=$passed;Failed=$failed;Blocked=$blocked;NotRun=($Expected-@($Cases).Count);Error=$RunnerError}|
        ConvertTo-Json|Set-Content (Join-Path $fixture 'output\summary.json') -Encoding UTF8
}
function Read-Summary([bool]$ExpectedSuccess) {
    $succeeded=$true
    [IO.File]::WriteAllText($env:GITHUB_STEP_SUMMARY,'')
    try { & $summaryScript -RunDirectory $fixture -Commit 'fixture-sha' -Scope 'Application' } catch {
        if($ExpectedSuccess){throw}
        $succeeded=$false
    }
    Assert-Summary ($succeeded -eq $ExpectedSuccess) 'Summary validation returned the wrong outcome.'
    $markdown=Get-Content $env:GITHUB_STEP_SUMMARY -Raw
    Assert-Summary ($markdown -eq (Get-Content (Join-Path $fixture 'regression-summary.md') -Raw)) 'Published and archived summaries differ.'
    return $markdown
}
try {
    [void][IO.Directory]::CreateDirectory((Join-Path $fixture 'output'))
    $env:GITHUB_STEP_SUMMARY=Join-Path $fixture 'published.md'
    $worker=@{Status='PASS';Administrator=$false;SessionId=1;RunnerExitCode=0;Error=''}
    $worker|ConvertTo-Json|Set-Content (Join-Path $fixture 'worker-result.json') -Encoding UTF8
    $cases=@(
        [pscustomobject]@{Test='01-fixture\passing';Category='Internal';Status='PASS';ExitCode=0;Detail=''},
        [pscustomobject]@{Test='02-fixture\passing';Category='UI';Status='PASS';ExitCode=0;Detail=''}
    )
    Write-Fixture $cases 2 'PASS'
    $markdown=Read-Summary $true
    Assert-Summary ($markdown.Contains('| Internal | 1 | 1 | 0 | 0 | 0 |') -and $markdown.Contains('| UI | 1 | 1 | 0 | 0 | 0 |')) 'Category totals are missing.'
    Assert-Summary (!$markdown.Contains('### Failed or blocked tests')) 'Successful suites must not list failures.'

    Write-Fixture @($cases[0]) 1 'PASS'
    $markdown=Read-Summary $true
    Assert-Summary ($markdown.Contains('**PASS** - 1/1 tests passed.')) 'Single-case JSON must validate on PowerShell 5.1.'
    Write-Fixture @() 1 'INCOMPLETE'
    $markdown=Read-Summary $false
    Assert-Summary ($markdown.Contains('| Not assigned (never started) | 1 | 0 | 0 | 0 | 1 |')) 'A report without executed cases must remain incomplete.'

    $cases[0].Status='FAIL';$cases[0].ExitCode=1;$cases[0].Detail="first | <error>`nsecond"
    $cases[1].Status='BLOCKED';$cases[1].ExitCode=1;$cases[1].Detail='Desktop unavailable'
    $caseLog=Join-Path $fixture 'output\01-fixture\passing\output.log'
    [void][IO.Directory]::CreateDirectory((Split-Path $caseLog -Parent))
    'FAIL: native | error'|Set-Content $caseLog -Encoding UTF8
    Write-Fixture $cases 3
    $markdown=Read-Summary $false
    Assert-Summary ($markdown.Contains('| Internal | 1 | 0 | 1 | 0 | 0 |') -and $markdown.Contains('| UI | 1 | 0 | 0 | 1 | 0 |')) 'Failures and blocked cases must stay distinct.'
    Assert-Summary ($markdown.Contains('| Not assigned (never started) | 1 | 0 | 0 | 0 | 1 |')) 'Missing cases must remain visible.'
    Assert-Summary ($markdown.Contains('first &#124; &lt;error&gt; second') -and $markdown.Contains('Desktop unavailable')) 'Failure details must be escaped and preserved.'
    Assert-Summary ($markdown.Contains('<br>FAIL: native &#124; error')) 'Native failure evidence must remain readable in the table.'
    Assert-Summary ($markdown.Contains('| Internal | 01-fixture\passing | FAIL | 1 |') -and $markdown.Contains('| UI | 02-fixture\passing | BLOCKED | 1 |')) 'Failure table must identify each test.'

    $many=@(1..25|ForEach-Object{[pscustomobject]@{Test=('01-fixture\failure-{0:D2}' -f $_);Category='Internal';Status='FAIL';ExitCode=1;Detail='fixture failure'}})
    Write-Fixture $many 25
    $markdown=Read-Summary $false
    Assert-Summary ($markdown.Contains('failure-25')) 'The failure list must not silently truncate cases.'

    foreach($case in $cases){$case.Status='PASS';$case.ExitCode=0;$case.Detail=''}
    Write-Fixture $cases 2 'FAIL' 'Cleanup fixture failure'
    $markdown=Read-Summary $false
    Assert-Summary ($markdown.Contains('Runner/cleanup error: Cleanup fixture failure')) 'Passing cases must not hide cleanup errors.'
    Write-Fixture $cases 2 'PASS'
    $worker.Administrator=$true
    $worker|ConvertTo-Json|Set-Content (Join-Path $fixture 'worker-result.json') -Encoding UTF8
    $markdown=Read-Summary $false
    Assert-Summary ($markdown.Contains('**FAIL - suite not validated**') -and $markdown.Contains('admin=True')) 'Passing cases must not hide an invalid worker.'
    Remove-Item -LiteralPath (Join-Path $fixture 'output\summary.json')
    $markdown=Read-Summary $false
    Assert-Summary ($markdown.Contains('**FAIL - no regression report.**')) 'Missing reports must not pass.'
    Write-Host 'PASS: category tables, complete failure lists, escaping and validation gates.'
} finally {
    $env:GITHUB_STEP_SUMMARY=$savedSummary
    $buildRoot=[IO.Path]::GetFullPath((Join-Path $ProjectRoot 'build'))+[IO.Path]::DirectorySeparatorChar
    if(![IO.Path]::GetFullPath($fixture).StartsWith($buildRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture cleanup escaped the build directory.'}
    if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}
}
