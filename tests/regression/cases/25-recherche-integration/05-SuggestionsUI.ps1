# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'SearchIntegration.ps1')
$testFailure=$null
function Click-SearchEdit {
    $rect=[FileSpacerRegression.Native]::Bounds($edit)
    [FileSpacerRegression.Native]::ClickControl($window,$edit,[int](($rect.Left+$rect.Right)/2),[int](($rect.Top+$rect.Bottom)/2))
}
function Assert-EditFocus {
    Assert-Ui ([FileSpacerRegression.Native]::FocusedWindowInOwningThread($window) -eq $edit) 'Suggestions stole the editing focus.'
}
function Type-Search([string]$Text) {
    [FileSpacerRegression.Native]::Keys($window,0x41,$true,$false)
    foreach($character in $Text.ToUpperInvariant().ToCharArray()) {
        [FileSpacerRegression.Native]::Keys($window,[uint16]$character,$false,$false)
    }
    Wait-Ui { [FileSpacerRegression.Native]::Text($edit) -ieq $Text } 'actual keyboard text'
}
try {
    Initialize-Ui 'search-suggestions' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'LiveNameSearch' 1
    $folder=Join-Path $script:UiRoot 'Folder';[void](New-Item -ItemType Directory -Path $folder)
    foreach($name in @('keep.txt','other.txt','a&b.txt')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $window=Open-UiFolder $folder 3;$edit=Get-UiSearchEdit $window
    Set-SearchOptions $window $true 5 (Join-Path $script:UiRoot 'missing\Everything.exe')
    Submit-Search $window 'a&b' @('a&b.txt')
    Submit-Search $window 'keep' @('keep.txt');Submit-Search $window 'other' @('other.txt')
    $search=Get-UiControl $window 'FileSpacer Search Box'
    Assert-Ui (@([FileSpacerRegression.Native]::Children($search,'Button')).Count -eq 1) 'A chevron or second search button remains.'
    Activate-Ui $window;Use-UiView $window {param($view) $view.Focus()}
    Click-SearchEdit
    $popup=Get-SearchPopup $window
    Assert-EditFocus
    Assert-Ui ([FileSpacerRegression.SearchMenus]::Selected($popup) -eq -1) 'Focus preselected a suggestion.'
    $hoverList=[FileSpacerRegression.SearchMenus]::List($popup)
    $hoverBounds=[FileSpacerRegression.Native]::ClientBounds($hoverList)
    Assert-Ui ([FileSpacerRegression.Native]::SetCursorPos($hoverBounds.Left+12,$hoverBounds.Top+10)) 'Cannot move the test pointer.'
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq -1 } 'Pointer movement does not select Everything' 200
    $items=[FileSpacerRegression.SearchMenus]::Items($popup)
    Assert-Ui (($items[1..4] -join '|') -ceq '<separator>|other|keep|a&b') 'Suggestions changed order or literal ampersands.'
    Type-Search 'keep';Assert-SearchNames $window @('keep.txt')
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($popup)) 'Typing closed suggestions.'
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'Escape closes suggestions'
    Assert-Ui ([FileSpacerRegression.Native]::Text($edit) -ieq 'keep') 'Escape erased the input.'
    Assert-SearchNames $window @('keep.txt');Assert-EditFocus
    Click-SearchEdit;$popup=Get-SearchPopup $window # Also works when the edit already owns focus.
    [FileSpacerRegression.Native]::Keys($window,0x28,$false,$false)
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq 0 } 'Down selects Everything explicitly'
    [FileSpacerRegression.Native]::Keys($window,0x28,$false,$false)
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq 1 } 'Down skips the separator'
    [FileSpacerRegression.Native]::Keys($window,0x26,$false,$false)
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq 0 } 'Up skips the separator'
    Type-Search 'other'
    Assert-Ui ([FileSpacerRegression.SearchMenus]::Selected($popup) -eq -1) 'Editing kept a stale selected action.'
    [FileSpacerRegression.Native]::Keys($window,0x0d,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'Enter submits local search'
    Assert-SearchNames $window @('other.txt')
    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$ownerId)
    Assert-Ui (@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$ownerId)).Count -eq 0) 'Enter implicitly launched Everything.'
    $popup=Show-SearchMenu $window
    [FileSpacerRegression.Native]::Keys($window,0x09,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'Tab dismisses suggestions'
    Assert-Ui ([FileSpacerRegression.Native]::FocusedWindowInOwningThread($window) -eq (Get-UiSearchSubmit $window)) 'Tab did not reach the original magnifier.'
    [FileSpacerRegression.Native]::Keys($window,0x09,$false,$false)
    Assert-Ui ([FileSpacerRegression.Native]::FocusedWindowInOwningThread($window) -ne $edit) 'Tab trapped focus in search.'
    $popup=Show-SearchMenu $window;Assert-EditFocus
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'second dismissal'
    [FileSpacerRegression.Native]::Keys($window,0x28,$false,$false)
    $popup=Get-SearchPopup $window
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq 0 } 'Down reopens without a button'
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    [FileSpacerRegression.Native]::SetText($edit,'keep');Assert-SearchNames $window @('keep.txt')
    $popup=Show-SearchMenu $window
    $list=[FileSpacerRegression.SearchMenus]::List($popup);$rect=[FileSpacerRegression.Native]::ClientBounds($list)
    $firstHeight=[FileSpacerRegression.Native]::Message($list,0x1a1,0,0)
    $separatorHeight=[FileSpacerRegression.Native]::Message($list,0x1a1,1,0)
    $rowHeight=[FileSpacerRegression.Native]::Message($list,0x1a1,2,0)
    [FileSpacerRegression.Native]::ClickControl($window,$list,$rect.Left+12,[int]($rect.Top+$firstHeight+$separatorHeight+$rowHeight/2))
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'mouse choice accepted without activation'
    Assert-SearchNames $window @('other.txt');Assert-EditFocus
    Assert-Ui ([FileSpacerRegression.Native]::Text($edit) -ieq 'other') 'Mouse choice did not reuse the recent query.'
    $popup=Show-SearchMenu $window
    Close-UiFolder $window
    Assert-Ui (![FileSpacerRegression.Native]::IsWindow($popup)) 'Closing the owner left an orphan popup.'
    Write-Host 'PASS: no chevron, click/Ctrl+F/Down, nonmodal typing and instant search, literal history, explicit selection, native Enter, Escape, Tab, mouse reuse and owner cleanup.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
