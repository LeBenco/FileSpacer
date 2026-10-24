# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'shell-clipboard'
    $source=Join-Path $script:UiRoot 'Copy-source';$destination=Join-Path $script:UiRoot 'Copy-target'
    foreach ($folder in @($source,$destination)) { [void][IO.Directory]::CreateDirectory($folder) }
    [IO.File]::WriteAllText((Join-Path $source 'Copy-me'),'copy-payload-123')
    [IO.File]::WriteAllText((Join-Path $source 'Move-me'),'move-payload-456')
    $first=Open-UiFolder $source 2;$second=Open-UiFolder $destination 0
    foreach ($operation in @(@{Name='Copy-me';Key=0x43;Move=$false;Contents='copy-payload-123'},@{Name='Move-me';Key=0x58;Move=$true;Contents='move-payload-456'})) {
        Focus-ShellFixture $first $operation.Name
        [FileSpacerRegression.Native]::Keys($first,$operation.Key,$true,$false)
        Focus-ShellFixture $second
        [FileSpacerRegression.Native]::Keys($second,0x56,$true,$false)
        $target=Join-Path $destination $operation.Name
        Wait-Ui { Test-Path -LiteralPath $target } 'native Ctrl+V produces the target file'
        Wait-Ui { try { [IO.File]::ReadAllText($target) -ceq $operation.Contents } catch [IO.IOException] { $false } } 'clipboard operation produces the exact bytes'
        if ($operation.Move) { Wait-Ui { !(Test-Path -LiteralPath (Join-Path $source $operation.Name)) } 'Ctrl+X/Ctrl+V removes the source' }
        else { Assert-Ui (Test-Path -LiteralPath (Join-Path $source $operation.Name)) 'Ctrl+C/Ctrl+V removed the source.' }
    }
    Assert-SearchNames $first @('Copy-me');Assert-SearchNames $second @('Copy-me','Move-me')
    Write-Host 'PASS: real Ctrl+C/X/V copy and move bytes, preserve/remove sources and update both native views.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
