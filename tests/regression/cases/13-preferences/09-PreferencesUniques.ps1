# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
function Get-SettingsSheets([uint32[]]$ProcessIds) {
    $sheets=@()
    foreach ($processId in $ProcessIds) {
        $sheets+=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId) | Where-Object {
            @([FileSpacerRegression.Native]::Children($_,'SysTabControl32')).Count -eq 1
        })
    }
    return $sheets
}
$failure=$null
try {
    Initialize-Ui 'settings-unique'
    $a=Join-Path $script:UiRoot 'A'; $b=Join-Path $script:UiRoot 'B'
    foreach ($folder in @($a,$b)) { [void][IO.Directory]::CreateDirectory($folder); [IO.File]::WriteAllText((Join-Path $folder 'item.txt'),'fixture') }
    $first=Open-UiFolder $a 1
    $second=Open-UiFolder $b 1
    $firstId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($first,[ref]$firstId)
    $secondId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($second,[ref]$secondId)
    Assert-Ui ($firstId -ne $secondId) 'The two folders must be served by two FileSpacer processes.'
    $sheet=Show-UiSettings $first
    Assert-Ui (@(Get-SettingsSheets @($firstId,$secondId)).Count -eq 1) 'One Preferences dialog expected after the first request.'
    # The second process is asked for the Preferences: the existing dialog must be activated, not duplicated.
    Activate-Ui $second
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($second,0x111,[IntPtr](Get-UiId 'IDM_SETTINGS'),[IntPtr]::Zero)) 'Settings command failed.'
    Wait-Ui { [FileSpacerRegression.Native]::GetForegroundWindow() -eq $sheet } 'existing Preferences dialog brought to the foreground by the other process' 300
    Assert-Ui (@(Get-SettingsSheets @($firstId,$secondId)).Count -eq 1) 'A second Preferences dialog was opened by the other process.'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($sheet)) 'The original Preferences dialog disappeared.'
    Write-Host 'PASS: a Preferences request from another FileSpacer process activates the existing dialog instead of opening a second one.'
    Press-UiSettings 5
    Wait-Ui { @(Get-SettingsSheets @($firstId,$secondId)).Count -eq 0 } 'no Preferences dialog left after Cancel'
    Write-Host 'PASS: cancelling the single dialog leaves no Preferences dialog in either process.'
}catch{$failure=$_;throw}finally{Stop-Ui -Failure $failure}
