# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'SearchIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'search-history' -FixtureParent (Join-Path $ProjectRoot 'build')
    $a=Join-Path $script:UiRoot 'A';$b=Join-Path $script:UiRoot 'B'
    foreach($folder in @($a,$b)) {
        [void](New-Item -ItemType Directory -Path $folder)
        foreach($name in @('keep.txt','other.txt')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    }
    $first=Open-UiFolder $a 2;$second=Open-UiFolder $b 2
    [void](Show-UiSettings $first);$page=Set-UiPage 3
    Assert-Ui ([FileSpacerRegression.Native]::Message([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_EVERYTHING_SCOPE')),0x147,0,0) -eq 0) 'Default scope is not current folder.'
    $page=Set-UiPage 0
    $limitEdit=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_SEARCH_HISTORY_LIMIT'))
    Assert-Ui ($limitEdit -ne [IntPtr]::Zero) 'History limit is missing from General settings.'
    Assert-Ui ([FileSpacerRegression.Native]::Text($limitEdit) -eq '5') 'Default history limit is not 5.'
    [FileSpacerRegression.Native]::SetText($limitEdit,'21');Press-UiSettings 4
    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($first,[ref]$ownerId)
    $invalid=@{Dialog=[IntPtr]::Zero}
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$ownerId) | Where-Object {$_ -ne $script:UiSettings})
        if($dialogs.Count -ne 1){return $false};$invalid.Dialog=$dialogs[0];return $true
    } 'invalid limit error dialog'
    [void][FileSpacerRegression.Native]::PostMessage((Get-UiControl $invalid.Dialog 'Button'),0xf5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($invalid.Dialog) -and [FileSpacerRegression.Native]::Text($limitEdit) -eq '5' } 'invalid limit rejected and restored'
    Press-UiSettings 5
    Submit-Search $first 'keep' @('keep.txt');Submit-Search $first 'other' @('other.txt');Submit-Search $first 'keep' @('keep.txt')
    Assert-SearchMenu $first @('keep','other')
    Submit-Search $second 'other' @('other.txt');Assert-SearchMenu $second @('other')
    Select-SearchMenu $first 1;Assert-SearchNames $first @('other.txt')
    Assert-Ui ([FileSpacerRegression.Native]::Text((Get-UiSearchEdit $first)) -eq 'other') 'Recent query did not populate edit.'
    Close-UiFolder $first;$first=Open-UiFolder $a 2
    Assert-SearchMenu $first @('other','keep')
    # Real instant search keeps functioning without recording every changed text.
    [void](Show-UiSettings $first);$page=Set-UiPage 0;Set-UiCheck $page 'IDC_LIVE_NAME_SEARCH' $true;Press-UiSettings 3
    [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $first),'keep');Assert-SearchNames $first @('keep.txt')
    [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $first),'other');Assert-SearchNames $first @('other.txt')
    Assert-SearchMenu $first @('other','keep')
    Set-SearchOptions $first $true 5
    $preview=Show-SearchMenu $first
    $withEverything=@([FileSpacerRegression.SearchMenus]::Items($preview))
    [FileSpacerRegression.Native]::Keys($first,0x1b,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($preview) } 'combined menu preview dismissed'
    $everythingLabel=$withEverything[0]
    Assert-Ui ($withEverything.Count -eq 4 -and $withEverything[1] -eq '<separator>' -and $withEverything[2] -eq 'other') 'Combined menu order differs.'
    # A recent entry continues to use the native local filter with Everything enabled.
    Select-SearchMenu $first 2;Assert-SearchNames $first @('keep.txt')
    Set-SearchOptions $first $false 5
    for($number=0;$number -lt 7;++$number) { Submit-Search $first ('default-'+$number) @() }
    Assert-SearchMenu $first @(6..2 | ForEach-Object {'default-'+$_})
    Set-SearchOptions $first $false 20
    for($number=0;$number -lt 22;++$number) { Submit-Search $first ('term-'+$number) @() }
    $expected=@(21..2 | ForEach-Object {'term-'+$_});Assert-SearchMenu $first $expected
    Close-UiFolder $first
    Set-SearchOptions $second $false 2
    $first=Open-UiFolder $a 2;Assert-SearchMenu $first @('term-21','term-20')
    Set-SearchOptions $first $true 0
    Assert-SearchMenu $first @($everythingLabel)
    Submit-Search $first 'keep' @('keep.txt');Assert-SearchMenu $first @($everythingLabel)
    Set-SearchOptions $first $false 0
    Assert-Ui (@([FileSpacerRegression.Native]::Children((Get-UiControl $first 'FileSpacer Search Box'),'Button')).Count -eq 1) 'Search must contain only its original magnifier.'
    Submit-Search $first 'other' @('other.txt')
    Set-SearchOptions $first $false 5
    # Re-enabling must not let the other process save its pre-clear cache again.
    Submit-Search $second 'keep' @('keep.txt')
    Set-SearchOptions $first $true 5
    Assert-SearchMenu $second @($everythingLabel,'<separator>','keep')
    # Reproduce delayed delivery with the registry already back at five. A normal
    # namespace's snapshot must not clear /test; a /test snapshot must still clear.
    $settingsMessage=[FileSpacerRegression.SearchMenus]::SettingsMessage()
    [void][FileSpacerRegression.Native]::PostMessage($second,$settingsMessage,[IntPtr]1,[IntPtr]::Zero)
    Assert-SearchMenu $second @($everythingLabel,'<separator>','keep')
    [void][FileSpacerRegression.Native]::PostMessage($second,$settingsMessage,[IntPtr]1,[IntPtr]1)
    Assert-SearchMenu $second @($everythingLabel)
    Set-SearchOptions $first $false 0
    Set-SearchOptions $first $false 5
    Submit-Search $second 'keep' @('keep.txt')
    Close-UiFolder $second;$second=Open-UiFolder $b 2
    Assert-SearchMenu $second @('keep')
    Close-UiFolder $first;$first=Open-UiFolder $a 2
    # No menu is opened for an empty history with Everything disabled.
    Activate-Ui $first;[FileSpacerRegression.Native]::Keys($first,0x46,$true,$false)
    Assert-SearchNames $first @('keep.txt','other.txt')
    Assert-Ui (@([FileSpacerRegression.Native]::Top('FileSpacer Search Suggestions',[NullString]::Value,0)).Count -eq 0) 'Empty sections opened a popup.'
    Write-Host 'PASS: real Shell search/recent reuse, instant search, independent folders, reopen, 0/5/20, limit changes and all section combinations.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
