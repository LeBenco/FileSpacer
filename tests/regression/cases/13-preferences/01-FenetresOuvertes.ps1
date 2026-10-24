# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
function Check-Windows([bool]$Status,[bool]$Toolbar,[bool]$Path) {
    foreach ($window in @($first,$second)) {
        Assert-UiVisible $window 'msctls_statusbar32' $Status
        Assert-UiVisible $window 'ReBarWindow32' $Toolbar
        Assert-UiVisible $window 'FileSpacer Path Bar' ($Toolbar -and $Path)
        Assert-UiVisible $window 'FileSpacer Search Box' $Toolbar
    }
}
function Check-Counts([int]$Count) {
    foreach ($window in @($first,$second)) {
        $observed=@{Count=$null}
        try {
            Wait-Ui {
                $observed.Count=Use-UiView $window {param($view) $view.Count}
                return $observed.Count -eq $Count
            } "filtered view count = $Count" 400
        } catch {
            $failure=$_
            try {
                $text=[FileSpacerRegression.Native]::Text((Get-UiSearchEdit $window))
                Write-Host "DIAG: HWND=$window, expected=$Count, observed=$($observed.Count), search='$text', saved LiveNameSearch=$(Get-UiPreference 'LiveNameSearch')"
                $windowProcessId=[uint32]0
                [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$windowProcessId)
                foreach ($dialog in [FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$windowProcessId)) {
                    $messages=@([FileSpacerRegression.Native]::Children($dialog,'Static') | ForEach-Object { [FileSpacerRegression.Native]::Text($_) })
                    Write-Host "DIAG: dialog '$([FileSpacerRegression.Native]::Text($dialog))': $($messages -join '; ')"
                }
            } catch { Write-Warning ('Cannot collect filter diagnostics: '+$_.Exception.Message) }
            throw $failure
        }
    }
}
try {
    Initialize-Ui 'preferences'
    $a=Join-Path $script:UiRoot 'A'; $b=Join-Path $script:UiRoot 'B'
    foreach ($folder in @($a,$b)) {
        [void](New-Item -ItemType Directory -Path $folder)
        [IO.File]::WriteAllText((Join-Path $folder 'keep.txt'),'fixture')
        [IO.File]::WriteAllText((Join-Path $folder 'other.txt'),'fixture')
    }
    $first=Open-UiFolder $a 2; $second=Open-UiFolder $b 2
    $firstPid=[uint32]0; $secondPid=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($first,[ref]$firstPid)
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($second,[ref]$secondPid)
    Assert-Ui ($firstPid -ne $secondPid) 'Cross-process propagation requires two distinct application processes.'
    Check-Windows $true $true $true

    [void](Show-UiSettings $first); $page=Set-UiPage 1
    Assert-Ui (![FileSpacerRegression.Native]::IsWindowEnabled([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_FULL_NAMES_ON_SELECTION')))) 'Unavailable selected-name rendering must be disabled in Settings.'
    Set-UiCheck $page 'IDC_STATUS_TEXT_ENABLED' $false
    Set-UiCheck $page 'IDC_PATH_BAR_ENABLED' $false
    Check-Windows $true $true $true
    Assert-Ui ((Get-UiPreference 'StatusTextEnabled') -eq 1 -and (Get-UiPreference 'PathBarEnabled') -eq 1) 'Pending edits were saved before Apply.'
    Press-UiSettings 5
    Check-Windows $true $true $true
    Assert-Ui ((Get-UiPreference 'StatusTextEnabled') -eq 1 -and (Get-UiPreference 'PathBarEnabled') -eq 1) 'Cancel saved pending changes.'
    Write-Host 'PASS: pending preferences and Cancel affect neither saved values nor open windows.'

    [void](Show-UiSettings $first); $page=Set-UiPage 1
    Set-UiCheck $page 'IDC_STATUS_TEXT_ENABLED' $false
    Set-UiCheck $page 'IDC_PATH_BAR_ENABLED' $false
    Press-UiSettings 4; Check-Windows $false $true $false
    Assert-Ui ((Get-UiPreference 'StatusTextEnabled') -eq 0 -and (Get-UiPreference 'PathBarEnabled') -eq 0) 'Apply did not save the selected preferences.'
    Set-UiCheck $page 'IDC_TOOLBAR_ENABLED' $false
    Press-UiSettings 4; Check-Windows $false $false $false
    Set-UiCheck $page 'IDC_TOOLBAR_ENABLED' $true
    Press-UiSettings 4; Check-Windows $false $true $false
    Assert-Ui ([FileSpacerRegression.Native]::Message([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_PATH_BAR_ENABLED')),0xf0,0,0) -eq 0) 'Hiding/showing the toolbar lost the path-bar choice.'
    # Cancel only the edit made after the last Apply.
    Set-UiCheck $page 'IDC_STATUS_TEXT_ENABLED' $true
    Press-UiSettings 5; Check-Windows $false $true $false
    Assert-Ui ((Get-UiPreference 'StatusTextEnabled') -eq 0) 'Cancel reverted a previous Apply or saved a pending edit.'
    Write-Host 'PASS: Apply updates both processes; toolbar hide/show retains choices; Cancel preserves prior Apply.'

    Close-UiFolder $second
    $second=Open-UiFolder $b 2
    Check-Windows $false $true $false
    [void](Show-UiSettings $first); $page=Set-UiPage 1
    Assert-Ui ([FileSpacerRegression.Native]::Message([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_STATUS_TEXT_ENABLED')),0xf0,0,0) -eq 0) 'Reopened preferences did not reload the applied value.'
    Set-UiCheck $page 'IDC_STATUS_TEXT_ENABLED' $true
    Set-UiCheck $page 'IDC_PATH_BAR_ENABLED' $true
    Press-UiSettings 4; Check-Windows $true $true $true
    Write-Host 'PASS: applied values survive reopening the preferences and creating a new folder window.'

    foreach ($window in @($first,$second)) { [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'keep') }
    Check-Counts 2 # Live search is initially disabled.
    $page=Set-UiPage 0
    Assert-Ui (![FileSpacerRegression.Native]::IsWindowEnabled([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_KEEP_SELECTION_ON_ACTIVATE')))) 'Unavailable activation-selection behavior must be disabled in Settings.'
    Set-UiCheck $page 'IDC_LIVE_NAME_SEARCH' $true
    Press-UiSettings 4
    Write-Host "STEP: saved LiveNameSearch=$(Get-UiPreference 'LiveNameSearch'); waiting for filtering in both processes."
    Check-Counts 1
    Press-UiSettings 3
    foreach ($window in @($first,$second)) { [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'') }
    Check-Counts 2
    [void](Show-UiSettings $first); $page=Set-UiPage 0
    Set-UiCheck $page 'IDC_LIVE_NAME_SEARCH' $false
    Press-UiSettings 3
    foreach ($window in @($first,$second)) { [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'keep') }
    Check-Counts 2
    foreach ($window in @($first,$second)) {
        $button=Get-UiControl (Get-UiControl $window 'FileSpacer Search Box') 'Button'
        [void][FileSpacerRegression.Native]::Message($button,0xf5,0,0)
    }
    Check-Counts 1
    Write-Host 'PASS: live-search enable/disable propagates to both processes; explicit search remains functional.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
