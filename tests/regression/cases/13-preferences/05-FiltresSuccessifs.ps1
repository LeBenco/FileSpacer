# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\SearchUi.ps1')
$testFailure=$null
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
    Move-Item -LiteralPath (Join-Path $folder 'other-new.txt') -Destination (Join-Path $folder 'keep-renamed.txt')
    Assert-SearchNames $window @('keep-renamed.txt','keep.txt')
    Remove-Item -LiteralPath (Join-Path $folder 'keep-renamed.txt')
    Assert-SearchNames $window @('keep.txt')
    Remove-Item -LiteralPath (Join-Path $folder 'keep.txt')
    Assert-SearchNames $window @()
    [IO.File]::WriteAllText((Join-Path $folder 'keep-reborn.txt'),'fixture')
    Assert-SearchNames $window @('keep-reborn.txt')
    Move-Item -LiteralPath (Join-Path $folder 'ignored.tmp') -Destination (Join-Path $folder 'ignored-renamed.tmp')
    Assert-SearchNames $window @('keep-reborn.txt')
    [FileSpacerRegression.Native]::SetText($edit,'')
    Assert-SearchNames $window @('ignored-renamed.tmp','keep-reborn.txt','notes.md','other.txt')
    Write-Host 'PASS: successive queries, native create/rename/delete notifications, empty-to-nonempty results and clearing expose exact identities without an explicit refresh.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
