# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'LockUi.cs')
Add-Type -AssemblyName System.Drawing
function Save-Caption([IntPtr]$Window,[string]$Name) {
    $rect=[FileSpacerRegression.Native]::Bounds($Window)
    $bitmap=New-Object Drawing.Bitmap(($rect.Right-$rect.Left),50)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rect.Left,$rect.Top,0,0,$bitmap.Size)
        $bitmap.Save((Join-Path $env:FS_REGRESSION_RUN ($Name+'.png')))
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
}
function Caption-Text([IntPtr]$Window) {
    return (@([FileSpacerRegression.Native]::Children($Window,'Static') | ForEach-Object { [FileSpacerRegression.Native]::Text($_) }) -join '|')
}
function Assert-Locked([IntPtr]$Window,[bool]$Value) {
    Wait-Ui { ((Caption-Text $Window) -match 'Window state locked|État de la fenêtre verrouillé') -eq $Value } "accessible lock indicator = $Value" 200
}
function Caption-Menu([IntPtr]$Window,[bool]$Expected,[bool]$Invoke=$false,[string]$Label='') {
    Activate-Ui $Window
    $id=Get-UiId 'IDM_PROXY_BUTTON'
    $bars=@([FileSpacerRegression.Native]::Children($Window,'ToolbarWindow32') | Where-Object { [FileSpacerRegression.Native]::Message($_,0x419,$id,0) -ge 0 })
    Assert-Ui ($bars.Count -eq 1) 'One title menu toolbar required.'
    $rect=[FileSpacerRegression.Native]::ToolbarButton($bars[0],$id)
    [LockUi]::Click($bars[0],(($rect.Left+$rect.Right)/2),(($rect.Top+$rect.Bottom)/2))
    $found=@{Menu=[IntPtr]::Zero; Popup=[IntPtr]::Zero}
    Wait-Ui {
        $popups=@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,0))
        if ($popups.Count -eq 1) { $found.Popup=$popups[0]; $found.Menu=[LockUi]::SendMessage($popups[0],0x1e1,[IntPtr]::Zero,[IntPtr]::Zero); return $found.Menu -ne [IntPtr]::Zero }; return $false
    } 'native caption popup'
    $index=-1
    for ($i=0;$i -lt [LockUi]::GetMenuItemCount($found.Menu);++$i) {
        if ([LockUi]::GetMenuItemID($found.Menu,$i) -eq (Get-UiId 'IDM_TOGGLE_STATE_LOCK')) { $index=$i }
    }
    Assert-Ui (($index -ge 0) -eq $Expected) 'Unexpected lock command visibility.'
    if ($Label) { Assert-Ui ([LockUi]::MenuText($found.Menu,$index) -match $Label) 'Incorrect translated lock command.' }
    if ($Invoke) {
        $item=[LockUi]::ItemRect($found.Menu,$index)
        Write-Host "STEP: invoke lock menu index=$index, label=$([LockUi]::MenuText($found.Menu,$index))"
        [LockUi]::Click($found.Popup,(($item.Left+$item.Right)/2),(($item.Top+$item.Bottom)/2))
    } else { [FileSpacerRegression.Native]::Keys($Window,0x1b,$false,$false) }
    Wait-Ui { @([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,0)).Count -eq 0 } 'caption menu closed'
}
function Place([IntPtr]$Window,[int]$X,[int]$Y,[int]$Width,[int]$Height) {
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$X,$Y,$Width,$Height,0x14)) 'Cannot change geometry.'
}
function Geometry([IntPtr]$Window) {
    $r=[FileSpacerRegression.Native]::Bounds($Window); return "$($r.Left),$($r.Top),$($r.Right),$($r.Bottom)"
}
function Enable-Locks([IntPtr]$Window) {
    [void](Show-UiSettings $Window); $page=Set-UiPage 0
    Set-UiCheck $page 'IDC_WINDOW_STATE_LOCK_ENABLED' $true
    Press-UiSettings 3
    Assert-Ui ((Get-UiPreference 'WindowStateLockEnabled') -eq 1) 'Feature activation not persisted.'
}
function Disable-Locks([IntPtr]$Window,[bool]$Confirm) {
    [void](Show-UiSettings $Window); $page=Set-UiPage 0
    Set-UiCheck $page 'IDC_WINDOW_STATE_LOCK_ENABLED' $false
    Press-UiSettings 4
    $dialog=@{Handle=[IntPtr]::Zero}
    $windowProcessId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$windowProcessId)
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$windowProcessId) | Where-Object { $_ -ne $script:UiSettings -and [FileSpacerRegression.Native]::IsWindowEnabled($_) -and [FileSpacerRegression.Native]::Text($_) -match '^Confirm' })
        if ($dialogs.Count -eq 1) { $dialog.Handle=$dialogs[0]; return $true }; return $false
    } 'native destructive confirmation'
    $script:UiConfirmation=$dialog.Handle
    $button=if ($Confirm) {1} else {2}
    [void][FileSpacerRegression.Native]::Message($dialog.Handle,0x466,$button,0)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($dialog.Handle) } 'confirmation closed'
    if ($Confirm) { Wait-Ui { (Get-UiPreference 'WindowStateLockEnabled') -eq 0 } 'feature disabled' }
    else { Assert-Ui ((Get-UiPreference 'WindowStateLockEnabled') -eq 1) 'Cancel altered global setting.' }
    Press-UiSettings 5
}
$testFailure=$null
$script:UiConfirmation=[IntPtr]::Zero
try {
    $fixtureParent=Join-Path $ProjectRoot 'build/lock-fixtures'
    [void][IO.Directory]::CreateDirectory($fixtureParent)
    Initialize-Ui 'window-state-lock' -FixtureParent $fixtureParent
    $a=Join-Path $script:UiRoot 'LockedA - a long folder title to exercise ellipsis and lock spacing'; $b=Join-Path $script:UiRoot 'LockedB'
    foreach ($f in @($a,$b)) { [void][IO.Directory]::CreateDirectory($f); [IO.File]::WriteAllText((Join-Path $f 'First.txt'),'a'); [IO.File]::WriteAllText((Join-Path $f 'Second.txt'),'b') }
    $first=Open-UiFolder $a 2; $second=Open-UiFolder $b 2
    Assert-Ui ((Get-UiPreference 'WindowStateLockEnabled') -ne 1) 'Initial feature must be disabled.'
    Assert-Locked $first $false; Caption-Menu $first $false
    Enable-Locks $first
    Assert-Locked $first $false; Assert-Locked $second $false
    Caption-Menu $first $true $false 'Verrouiller|Lock'
    $work=[FileSpacerRegression.Native]::WorkArea($first)
    Place $first ($work.Left+35) ($work.Top+35) 620 440
    Use-UiView $first {param($v) $v.SetViewModeAndIconSize(1,48); $v.AutoArrange=$false }
    Wait-Ui { (Use-UiView $first {param($v) $v.Mode}) -eq 1 } 'icon mode visible' 400
    $reference=Geometry $first
    Caption-Menu $first $true $true
    Assert-Locked $first $true; Caption-Menu $first $true $false 'Déverrouiller|Unlock'
    Save-Caption $first 'locked-title'
    Place $first ($work.Left+35) ($work.Top+35) 260 440
    Save-Caption $first 'locked-narrow-title'
    Assert-Locked $first $true
    Place $first ($work.Left+35) ($work.Top+35) 620 440
    Use-UiView $first {param($v) $v.Position('First.txt',170,130); $v.Position('Second.txt',310,210)}
    $positions=Use-UiView $first {param($v) $v.Positions()}
    # Close in the spatial mode to capture the current manual placement,
    # independently of the later presentation-change scenario.
    Close-UiFolder $first; $first=Open-UiFolder $a 2
    Assert-Locked $first $true
    Wait-Ui { (Use-UiView $first {param($v) $v.Positions()}) -eq $positions } 'manual icon coordinates restored while locked' 400
    [void][FileSpacerRegression.Native]::PostMessage($first,0x112,[IntPtr]0xf030,[IntPtr]::Zero)
    Wait-Ui { [LockUi]::IsZoomed($first) } 'locked window can be temporarily maximized'
    Close-UiFolder $first; $first=Open-UiFolder $a 2
    Assert-Ui (![LockUi]::IsZoomed($first) -and (Geometry $first) -eq $reference) 'Temporary maximization replaced the normal reference.'
    Place $first ($work.Left+65) ($work.Top+70) 700 470
    Use-UiView $first {param($v) $v.SetViewModeAndIconSize(4,32)}
    Wait-Ui { (Use-UiView $first {param($v) $v.Mode}) -eq 4 } 'temporary Details mode visible' 500
    Assert-Ui ((Geometry $first) -ne $reference) 'Temporary geometry did not change.'
    Close-UiFolder $first; $first=Open-UiFolder $a 2
    Assert-Locked $first $true
    Assert-Ui ((Geometry $first) -eq $reference) 'Locked geometry was overwritten.'
    Assert-Ui ((Use-UiView $first {param($v) $v.Mode}) -eq 1) 'Locked icon mode was overwritten.'
    Assert-Ui ((Use-UiView $first {param($v) $v.IconSize}) -eq 48) 'Locked icon size was overwritten.'
    Place $first ($work.Left+75) ($work.Top+60) 650 450
    Use-UiView $first {param($v) $v.SetViewModeAndIconSize(4,32)}
    Wait-Ui { (Use-UiView $first {param($v) $v.Mode}) -eq 4 } 'temporary view before unlock' 400
    $unlocked=Geometry $first
    Caption-Menu $first $true $true; Assert-Locked $first $false
    Close-UiFolder $first; $first=Open-UiFolder $a 2
    Assert-Ui ((Geometry $first) -eq $unlocked -and (Use-UiView $first {param($v) $v.Mode}) -eq 4) 'Unlock failed to capture current visible state.'
    Caption-Menu $first $true $true; Assert-Locked $first $true
    Close-UiFolder $first; Close-UiFolder $second
    Wait-Ui { @($script:UiProcesses | Where-Object { !$_.HasExited }).Count -eq 0 } 'all fixture folder processes exit before filesystem move'
    foreach ($com in @($script:UiShellWindows,$script:UiShell)) {
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($com)
    }
    $script:UiShellWindows=$null; $script:UiShell=$null
    [GC]::Collect(); [GC]::WaitForPendingFinalizers()
    $movedParent=Join-Path $script:UiRoot 'Moved'; [void][IO.Directory]::CreateDirectory($movedParent)
    $renamed=Join-Path $movedParent 'RenamedLockedA'
    Move-Item -LiteralPath $a -Destination $renamed
    $script:UiShell=New-Object -ComObject Shell.Application
    $script:UiShellWindows=$script:UiShell.Windows()
    $a=$renamed; $first=Open-UiFolder $a 2
    $second=Open-UiFolder $b 2
    Assert-Locked $first $true
    Assert-Ui ((Geometry $first) -eq $unlocked) 'NTFS move/rename lost the locked geometry reference.'
    [void][FileSpacerRegression.Native]::PostMessage($second,0x112,[IntPtr]0xf030,[IntPtr]::Zero)
    Wait-Ui { [LockUi]::IsZoomed($second) } 'second folder maximized before capture'
    Caption-Menu $second $true $true; Assert-Locked $second $true
    [void][FileSpacerRegression.Native]::PostMessage($second,0x112,[IntPtr]0xf120,[IntPtr]::Zero)
    Wait-Ui { ![LockUi]::IsZoomed($second) } 'temporary restoration of maximized reference'
    Close-UiFolder $second; $second=Open-UiFolder $b 2
    Assert-Ui ([LockUi]::IsZoomed($second)) 'Locked maximized reference was overwritten.'
    [void][FileSpacerRegression.Native]::PostMessage($second,0x112,[IntPtr]0xf120,[IntPtr]::Zero)
    Wait-Ui { ![LockUi]::IsZoomed($second) } 'second window remains temporarily restored'
    Activate-Ui $first
    Disable-Locks $first $false
    Assert-Locked $first $true; Assert-Locked $second $true
    $c=Join-Path $script:UiRoot 'ClosedLocked'; [void][IO.Directory]::CreateDirectory($c)
    $closed=Open-UiFolder $c 0
    Caption-Menu $closed $true $true; Assert-Locked $closed $true
    Close-UiFolder $closed
    Place $first ($work.Left+100) ($work.Top+85) 680 465
    $disabled=Geometry $first
    Disable-Locks $first $true
    Assert-Locked $first $false; Assert-Locked $second $false
    Assert-Ui ((Geometry $first) -eq $disabled) 'Disabling moved an open window.'
    Caption-Menu $first $false
    Enable-Locks $first; Assert-Locked $first $false
    $closed=Open-UiFolder $c 0; Assert-Locked $closed $false
    Close-UiFolder $closed
    Close-UiFolder $first; Close-UiFolder $second
    $first=Open-UiFolder $a 2; $second=Open-UiFolder $b 2
    Assert-Locked $first $false; Assert-Locked $second $false
    Assert-Ui ((Geometry $first) -eq $disabled) 'Saving did not resume after disabling.'
    Write-Host 'PASS: default, menu, capture, temporary view/geometry, reopen, icon persistence, unlock, cancel, global clearing of open/closed folders and restart.'
} catch { $testFailure=$_; throw }
finally {
    if ($script:UiConfirmation -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:UiConfirmation)) {
        [void][FileSpacerRegression.Native]::PostMessage($script:UiConfirmation,0x466,[IntPtr]2,[IntPtr]::Zero)
    }
    Stop-Ui -Failure $testFailure
}
