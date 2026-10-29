# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
function Get-ApplyButton([IntPtr]$Sheet) {
    $buttons=@([FileSpacerRegression.Native]::Children($Sheet,'Button') | Where-Object { ([FileSpacerRegression.Native]::Text($_) -replace '&','') -in @('Apply','Appliquer') })
    Assert-Ui ($buttons.Count -eq 1) 'The property sheet has no single Apply button.'
    return $buttons[0]
}
function Get-ShortcutEditor([IntPtr]$Page) {
    $editors=@([FileSpacerRegression.Native]::Children($Page,'#32770'))
    Assert-Ui ($editors.Count -eq 1) 'The shortcut page has no single editor dialog.'
    return $editors[0]
}
function Click-Control([IntPtr]$Parent,[string]$Name) {
    $control=[FileSpacerRegression.Native]::GetDlgItem($Parent,(Get-UiId $Name))
    Assert-Ui ($control -ne [IntPtr]::Zero) "Missing control: $Name"
    [void][FileSpacerRegression.Native]::Message($control,0xf5,0,0) # BM_CLICK
}
# A task dialog draws its texts itself: read them through UI Automation.
function Get-DialogTexts([IntPtr]$Dialog) {
    $root=[System.Windows.Automation.AutomationElement]::FromHandle($Dialog)
    $condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::Text)
    return @($root.FindAll([System.Windows.Automation.TreeScope]::Descendants,$condition) | ForEach-Object { $_.Current.Name })
}
function Get-RefusalDialog([uint32]$ProcessId,[IntPtr]$Sheet) {
    foreach ($dialog in @([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$ProcessId) | Where-Object { $_ -ne $Sheet })) {
        $texts=Get-DialogTexts $dialog
        if (($texts -join "`n") -match 'cannot be applied|ne peut pas être appliqué') { return @{Window=$dialog;Texts=$texts} }
    }
    return $null
}
$failure=$null
try {
    Initialize-Ui 'shortcut-apply'
    $folder=Join-Path $script:UiRoot 'Shortcuts'
    [void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'item.txt'),'fixture')
    Assert-Ui ($null -eq (Get-UiPreference 'WindowShortcuts') -and $null -eq (Get-UiPreference 'EverythingEnabled')) 'The test needs absent stored shortcuts and Everything preferences.'
    $window=Open-UiFolder $folder 1
    $ownerId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$ownerId)
    $sheet=Show-UiSettings $window
    $apply=Get-ApplyButton $sheet
    Assert-Ui (-not [FileSpacerRegression.Native]::IsWindowEnabled($apply)) 'Apply must start disabled.'
    # Another page registers a change: Apply becomes enabled for the whole sheet.
    $browser=Set-UiPage 3
    Set-UiCheck $browser 'IDC_EVERYTHING_ENABLED' $true
    Wait-Ui { [FileSpacerRegression.Native]::IsWindowEnabled($apply) } 'Apply enabled by the Integration page'
    # Shortcuts page: give Move the Resize gesture (Alt + right button) to create a conflict.
    $page=Set-UiPage 2
    Click-Control $page 'IDC_SHORTCUT_MOVE'
    $editor=Get-ShortcutEditor $page
    Wait-Ui { [FileSpacerRegression.Native]::IsWindowVisible($editor) } 'shortcut editor opened for Move'
    Click-Control $editor 'IDC_SHORTCUT_RIGHT'
    $feedback=[FileSpacerRegression.Native]::GetDlgItem($editor,(Get-UiId 'IDC_SHORTCUT_FEEDBACK'))
    Wait-Ui { [FileSpacerRegression.Native]::Text($feedback) -match 'Already assigned to|Déjà utilisé pour' } 'conflict feedback shown'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($apply)) 'Apply stays enabled by the other page while the shortcuts are invalid.'
    Press-UiSettings 4
    $refusal=$null
    Wait-Ui { $script:refusal=Get-RefusalDialog $ownerId $sheet; $null -ne $script:refusal } 'refusal dialog naming the invalid entry'
    $refusal=$script:refusal
    Write-Host ('STEP: refusal dialog: '+($refusal.Texts -join ' | '))
    Assert-Ui (($refusal.Texts -join "`n") -match 'Resize the window|Redimensionner la fenêtre') 'The refusal does not name the conflicting entry.'
    Assert-Ui (($refusal.Texts -join "`n") -match 'Move the window|Déplacer la fenêtre') 'The refusal does not name the other action of the conflict.'
    [void][FileSpacerRegression.Native]::Message($refusal.Window,0x466,1,0) # TDM_CLICK_BUTTON IDOK
    Wait-Ui { -not [FileSpacerRegression.Native]::IsWindow($refusal.Window) } 'refusal dialog closed'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($sheet)) 'The sheet closed on a refused Apply.'
    Assert-Ui ([IntPtr][FileSpacerRegression.Native]::Message($sheet,0x476,0,0) -eq $page) 'The shortcuts page did not keep the activation.'
    Start-Sleep -Milliseconds 300
    Assert-Ui ($null -eq (Get-UiPreference 'EverythingEnabled')) 'A refused Apply still stored the Integration change.'
    Assert-Ui ($null -eq (Get-UiPreference 'WindowShortcuts')) 'A refused Apply stored invalid shortcuts.'
    Write-Host 'PASS: a conflicting shortcut is refused with a dialog naming the entry, and nothing is applied.'
    # Resolve the conflict: Apply is accepted and every page is applied.
    Click-Control $editor 'IDC_SHORTCUT_LEFT'
    Wait-Ui { [FileSpacerRegression.Native]::Text($feedback) -notmatch 'Already assigned to|Déjà utilisé pour' } 'conflict feedback cleared'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($apply)) 'Apply must be enabled once the shortcuts are valid again.'
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'EverythingEnabled') -eq 1 } 'Integration change applied'
    Assert-Ui ($null -eq (Get-RefusalDialog $ownerId $sheet)) 'A valid Apply showed a refusal dialog.'
    Wait-Ui { -not [FileSpacerRegression.Native]::IsWindowEnabled($apply) } 'Apply disabled after applying'
    Press-UiSettings 5
    Write-Host 'PASS: once the conflict is resolved, Apply is accepted and the other page is applied.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
