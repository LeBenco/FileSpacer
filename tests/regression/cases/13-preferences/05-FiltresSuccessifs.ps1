# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
function Assert-SearchNames([IntPtr]$Window,[string[]]$Names) {
    [string[]]$orderedNames=@($Names)
    [Array]::Sort($orderedNames,[StringComparer]::OrdinalIgnoreCase)
    $expected=$orderedNames -join ';'
    $observed=@{Names=''}
    try {
        Wait-Ui {
            $observed.Names=(Use-UiView $Window {param($view) $view.Names()}) -join ';'
            return $observed.Names -ceq $expected
        } ('visible search items: '+$expected) 400
    } catch {
        Write-Host ('DIAG: expected=['+$expected+'], observed=['+$observed.Names+']')
        throw
    }
}
try {
    Initialize-Ui 'successive-filters'
    Set-UiPreference 'LiveNameSearch' 1
    $folder=Join-Path $script:UiRoot 'Queries'
    [void][IO.Directory]::CreateDirectory($folder)
    foreach ($name in @('keep.txt','other.txt','notes.md')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $window=Open-UiFolder $folder 3
    $edit=Get-UiSearchEdit $window
    foreach ($step in @(
        @{Query='keep'; Names=@('keep.txt')},
        @{Query='other'; Names=@('other.txt')},
        @{Query='*.md'; Names=@('notes.md')},
        @{Query='no-match-unique'; Names=@()},
        @{Query=''; Names=@('keep.txt','notes.md','other.txt')}
    )) {
        [FileSpacerRegression.Native]::SetText($edit,$step.Query)
        Assert-SearchNames $window $step.Names
    }
    [FileSpacerRegression.Native]::SetText($edit,'keep')
    Assert-SearchNames $window @('keep.txt')
    [IO.File]::WriteAllText((Join-Path $folder 'keep-new.txt'),'fixture')
    [IO.File]::WriteAllText((Join-Path $folder 'ignored.tmp'),'fixture')
    Assert-SearchNames $window @('keep-new.txt','keep.txt')
    Move-Item -LiteralPath (Join-Path $folder 'keep-new.txt') -Destination (Join-Path $folder 'other-new.txt')
    Assert-SearchNames $window @('keep.txt')
    Write-Host 'PASS: successive queries replace visible items, including equal-count queries, wildcards, clearing and filesystem changes.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
