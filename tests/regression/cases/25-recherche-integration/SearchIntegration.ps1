. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\SearchUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'SearchMenus.cs') -ReferencedAssemblies UIAutomationClient,UIAutomationTypes

function Set-SearchOptions([IntPtr]$Window,[bool]$Everything,[int]$Limit,[string]$Path='',[int]$Scope=0) {
    [void](Show-UiSettings $Window); $page=Set-UiPage 0
    $limitEdit=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_SEARCH_HISTORY_LIMIT'))
    Assert-Ui ($limitEdit -ne [IntPtr]::Zero) 'History limit is missing from General settings.'
    [FileSpacerRegression.Native]::SetText($limitEdit,[string]$Limit)
    $page=Set-UiPage 3
    Set-UiCheck $page 'IDC_EVERYTHING_ENABLED' $Everything
    [FileSpacerRegression.Native]::SetText([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_EVERYTHING_PATH')),$Path)
    [void][FileSpacerRegression.Native]::Message([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_EVERYTHING_SCOPE')),0x14e,$Scope,0)
    Press-UiSettings 3
    Wait-Ui { (Get-UiPreference 'SearchHistoryLimit') -eq $Limit -and (Get-UiPreference 'EverythingEnabled') -eq [int]$Everything } 'search preferences applied'
}
# Apply refuses a typed Everything path that is relative, not an .exe or names no
# file: an explicit error naming the path, the sheet left open with the focus on
# the field, and the stored preference unchanged.
function Expect-EverythingPathRefused([IntPtr]$Window,[string]$Path) {
    $before=[string](Get-UiPreference 'EverythingPath')
    $sheet=Show-UiSettings $Window; $page=Set-UiPage 3
    $edit=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_EVERYTHING_PATH'))
    Assert-Ui ($edit -ne [IntPtr]::Zero) 'Everything path field is missing from Integration settings.'
    [FileSpacerRegression.Native]::SetText($edit,$Path)
    Press-UiSettings 4
    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$ownerId)
    $found=@{Dialog=[IntPtr]::Zero}
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$ownerId) | Where-Object { $_ -ne $sheet })
        if ($dialogs.Count -ne 1) { return $false };$found.Dialog=$dialogs[0];return $true
    } 'Everything path refusal dialog'
    $text=(@([FileSpacerRegression.Native]::Children($found.Dialog,'Static') | ForEach-Object { [FileSpacerRegression.Native]::Text($_) }) -join ' ')
    Write-Host ('DIAG: path refusal dialog text=['+$text+'] refused path=['+$Path+']')
    Assert-Ui ($text.Contains('Everything') -and $text.Contains($Path)) 'Path refusal is not explicit.'
    $ok=Get-UiControl $found.Dialog 'Button'
    [void][FileSpacerRegression.Native]::PostMessage($ok,0xf5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($found.Dialog) } 'path refusal dismissed'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($sheet)) 'The refused Apply closed the preferences.'
    Wait-Ui { [FileSpacerRegression.Native]::FocusedWindowInOwningThread($sheet) -eq $edit } 'focus returned to the Everything path field'
    $stored=[string](Get-UiPreference 'EverythingPath')
    Assert-Ui ($stored -ceq $before) ('A refused Everything path was stored: ['+$stored+']')
    Press-UiSettings 5
}
function Show-SearchMenu([IntPtr]$Window) {
    Activate-Ui $Window
    [FileSpacerRegression.Native]::Keys($Window,0x46,$true,$false) # Ctrl+F
    return Get-SearchPopup $Window
}
function Get-SearchPopup([IntPtr]$Window) {
    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$ownerId)
    $found=@{Window=[IntPtr]::Zero}
    Wait-Ui {
        $windows=@([FileSpacerRegression.Native]::Top('FileSpacer Search Suggestions',[NullString]::Value,$ownerId))
        if ($windows.Count -ne 1) { return $false }
        $found.Window=$windows[0]; return $true
    } 'search dropdown popup'
    return $found.Window
}
function Assert-SearchMenu([IntPtr]$Window,[string[]]$Expected) {
    $popup=Show-SearchMenu $Window
    try {
        $items=[FileSpacerRegression.SearchMenus]::Items($popup)
        Assert-Ui (($items -join '|') -ceq ($Expected -join '|')) ('Search menu: expected=['+($Expected -join ';')+'], actual=['+($items -join ';')+']')
    } finally {
        [FileSpacerRegression.Native]::Keys($Window,0x1b,$false,$false)
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'search menu dismissed'
    }
}
function Select-SearchMenu([IntPtr]$Window,[int]$Index) {
    $popup=Show-SearchMenu $Window
    Write-Host ('STEP: select recent/menu index='+$Index+', items='+([FileSpacerRegression.SearchMenus]::Items($popup) -join ';'))
    [FileSpacerRegression.Native]::Keys($Window,0x28,$false,$false) # Down: first item from an unselected popup
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -ge 0 } 'keyboard menu navigation started' 0 5
    Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq 0 } 'first suggestion selected' 0 5
    for ($step=0;$step -lt $Index;++$step) {
        [FileSpacerRegression.Native]::Keys($Window,0x28,$false,$false)
        Wait-Ui { [FileSpacerRegression.SearchMenus]::Selected($popup) -eq ($step+1) } 'Down selects requested menu entry'
    }
    [FileSpacerRegression.Native]::Keys($Window,0x0d,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($popup) } 'search menu selection completed'
}
function Submit-Search([IntPtr]$Window,[string]$Query,[string[]]$Names) {
    [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $Window),$Query)
    [void][FileSpacerRegression.Native]::Message((Get-UiSearchSubmit $Window),0xf5,0,0)
    Assert-SearchNames $Window $Names
}
