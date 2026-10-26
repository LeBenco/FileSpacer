# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
$script:PlacementConfirmation=[IntPtr]::Zero
function Set-PlacementRect([IntPtr]$Window,[int]$X,[int]$Y) {
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$X,$Y,$script:PlacementWidth,$script:PlacementHeight,0x14)) 'Fixture positioning failed.'
    Wait-Ui { $r=[FileSpacerRegression.Native]::Bounds($Window); $r.Left -eq $X -and $r.Top -eq $Y -and $r.Width -eq $script:PlacementWidth -and $r.Height -eq $script:PlacementHeight } 'fixture geometry'
}
function Overlap-Area($First,$Second) {
    return [Math]::Max(0,[Math]::Min($First.Right,$Second.Right)-[Math]::Max($First.Left,$Second.Left))*[Math]::Max(0,[Math]::Min($First.Bottom,$Second.Bottom)-[Math]::Max($First.Top,$Second.Top))
}
try {
    Initialize-Ui 'smart-placement' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'FolderWindow2Width' 400; Set-UiPreference 'FolderWindow2Height' 300
    Set-UiPreference 'KeepSourceWindowOpen' 0
    $parentPath=Join-Path $script:UiRoot 'Parent'; $childPath=Join-Path $parentPath 'Child'
    $otherPath=Join-Path $script:UiRoot 'Other'; $thirdPath=Join-Path $script:UiRoot 'Third'
    foreach ($path in @($childPath,$otherPath,$thirdPath)) { [void][IO.Directory]::CreateDirectory($path) }
    $parent=Open-UiFolder $parentPath 1
    $initial=[FileSpacerRegression.Native]::Bounds($parent)
    $script:PlacementWidth=$initial.Width; $script:PlacementHeight=$initial.Height
    $work=[FileSpacerRegression.Native]::WorkArea($parent)
    Assert-Ui ($work.Width -ge 2*$script:PlacementWidth+60 -and $work.Height -ge 2*$script:PlacementHeight+60) 'Placement fixture requires room for two default windows on each axis at the current DPI.'
    Set-PlacementRect $parent $work.Left $work.Top
    $other=Open-UiFolder $otherPath 0
    $parentRect=[FileSpacerRegression.Native]::Bounds($parent); $otherRect=[FileSpacerRegression.Native]::Bounds($other)
    Assert-Ui ((Overlap-Area $parentRect $otherRect) -eq 0) 'A new window overlaps another process despite free space.'
    Write-Host 'PASS: first placement avoids an existing window in another process.'

    [void](Show-UiSettings $other); $general=Set-UiPage 0
    $smart=[FileSpacerRegression.Native]::GetDlgItem($general,(Get-UiId 'IDC_SMART_WINDOW_PLACEMENT'))
    Assert-Ui ($smart -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::Message($smart,0xf0,0,0) -eq 1) 'Smart placement is not enabled by default in General.'
    $placementLabel=[FileSpacerRegression.Native]::Text($smart).Replace('&','')
    $frenchLabel='Minimiser la superposition des fen'+[char]0xea+'tres'
    Assert-Ui ($placementLabel -in @('Minimize window overlap',$frenchLabel)) 'The placement option label is incorrect.'
    Assert-Ui ([FileSpacerRegression.Native]::GetDlgItem($general,(Get-UiId 'IDC_RESET_FOLDER_STATE')) -eq [IntPtr]::Zero) 'Maintenance remains in General.'
    Set-UiCheck $general 'IDC_SMART_WINDOW_PLACEMENT' $false
    Press-UiSettings 5
    Assert-Ui ($null -eq (Get-UiPreference 'SmartWindowPlacement')) 'Cancel saved the pending placement option.'
    [void](Show-UiSettings $other); $general=Set-UiPage 0
    Set-UiCheck $general 'IDC_SMART_WINDOW_PLACEMENT' $false
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'SmartWindowPlacement') -eq 0 } 'placement disabled by Apply'
    Set-UiCheck $general 'IDC_SMART_WINDOW_PLACEMENT' $true
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'SmartWindowPlacement') -eq 1 } 'placement reenabled by Apply'
    $about=Set-UiPage 4
    $maintenance=[FileSpacerRegression.Native]::GetDlgItem($about,(Get-UiId 'IDC_RESET_FOLDER_STATE'))
    Assert-Ui ($maintenance -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindowVisible($maintenance)) 'Maintenance is missing from About.'
    $button=[FileSpacerRegression.Native]::Bounds($maintenance); $client=[FileSpacerRegression.Native]::ClientBounds($about)
    Assert-Ui ($button.Left -ge $client.Left -and $button.Right -le $client.Right -and $button.Bottom -le $client.Bottom) 'Maintenance is clipped.'
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($about,0x111,[IntPtr](Get-UiId 'IDC_RESET_FOLDER_STATE'),$maintenance)) 'Cannot invoke maintenance confirmation from About.'
    $dialogProcessId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($other,[ref]$dialogProcessId)
    $confirmation=@{Window=[IntPtr]::Zero}
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$dialogProcessId) | Where-Object { $_ -ne $script:UiSettings -and [FileSpacerRegression.Native]::IsWindowEnabled($_) -and [FileSpacerRegression.Native]::Text($_) -match '^Confirm' })
        if ($dialogs.Count -ne 1) { return $false }; $confirmation.Window=$dialogs[0]; $script:PlacementConfirmation=$dialogs[0]; return $true
    } 'maintenance confirmation'
    [void][FileSpacerRegression.Native]::Message($confirmation.Window,0x466,7,0)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($confirmation.Window) } 'maintenance confirmation declined'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($parent) -and [FileSpacerRegression.Native]::IsWindow($other)) 'Declining maintenance closed folder windows.'
    Press-UiSettings 5
    Write-Host 'PASS: default placement option, Cancel/Apply, maintenance moved to About without clipping.'

    [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf020,0)
    Wait-Ui { ([FileSpacerRegression.Native]::Style($parent,-16) -band 0x20000000) -ne 0 } 'parent minimized'
    $third=Open-UiFolder $thirdPath 0
    $thirdRect=[FileSpacerRegression.Native]::Bounds($third)
    Assert-Ui ($thirdRect.Left -eq $work.Left -and $thirdRect.Top -eq $work.Top) 'Minimized parent still occupies its former position.'
    Close-UiFolder $third
    [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf120,0)
    Wait-Ui { ([FileSpacerRegression.Native]::Style($parent,-16) -band 0x20000000) -eq 0 } 'parent restored'
    Set-PlacementRect $parent ($work.Left+20) ($work.Top+20)
    Set-PlacementRect $other ($work.Left+$script:PlacementWidth+40) ($work.Top+$script:PlacementHeight+40)
    $sourceRect=[FileSpacerRegression.Native]::Bounds($parent)
    Activate-Ui $parent
    Use-UiView $parent { param($view) $view.Select('Child'); $view.Focus() }
    [FileSpacerRegression.Native]::Keys($parent,0x0d,$false,$false)
    $child=Track-UiFolder $childPath
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($parent) } 'automatic source closure'
    $childRect=[FileSpacerRegression.Native]::Bounds($child)
    Assert-Ui ($childRect.Left -eq $sourceRect.Left -and $childRect.Top -eq $sourceRect.Top) 'Replacement placement counted the closing source.'
    Write-Host 'PASS: minimized windows ignored; spatial replacement reuses the withdrawn source position.'

    Close-UiFolder $child
    Set-PlacementRect $other $childRect.Left $childRect.Top
    $child=Open-UiFolder $childPath 0
    $restored=[FileSpacerRegression.Native]::Bounds($child)
    Assert-Ui ($restored.Left -eq $childRect.Left -and $restored.Top -eq $childRect.Top) 'Saved position lost priority over overlap reduction.'
    Assert-Ui ((Overlap-Area $restored ([FileSpacerRegression.Native]::Bounds($other))) -gt 0) 'Saved-position priority was not tested against a conflicting window.'
    Write-Host 'PASS: saved spatial position restored despite occupied area.'
} catch { $testFailure=$_; throw }
finally {
    if ($script:PlacementConfirmation -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:PlacementConfirmation)) {
        [void][FileSpacerRegression.Native]::PostMessage($script:PlacementConfirmation,0x466,[IntPtr]7,[IntPtr]::Zero)
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($script:PlacementConfirmation) } 'maintenance confirmation cleanup'
    }
    Stop-Ui -Failure $testFailure
}
