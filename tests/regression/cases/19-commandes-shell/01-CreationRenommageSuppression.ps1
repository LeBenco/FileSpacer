# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'shell-create-rename-delete'
    $folder=Join-Path $script:UiRoot 'Commands';[void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'Rename-me'),'native-rename-payload')
    $window=Open-UiFolder $folder 1
    Rename-ShellFixture $window $folder 'Rename-me' 'Renamed-by-F2'
    Assert-Ui ([IO.File]::ReadAllText((Join-Path $folder 'Renamed-by-F2')) -ceq 'native-rename-payload') 'F2 rename changed file contents.'
    Assert-SearchNames $window @('Renamed-by-F2')
    Focus-ShellFixture $window
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr](Get-UiId 'IDM_NEW_FOLDER'),[IntPtr]::Zero)) 'Cannot dispatch the application New Folder command.'
    Wait-Ui { @([IO.Directory]::GetDirectories($folder)).Count -eq 1 } 'application command creates exactly one directory'
    $createdDirectory=[IO.Path]::GetFileName([IO.Directory]::GetDirectories($folder)[0])
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Assert-SearchNames $window @('Renamed-by-F2',$createdDirectory)
    Rename-ShellFixture $window $folder $createdDirectory 'Created-by-command'
    # Text creation is now exposed only by the native New menu, not by an
    # application command. Keep this case on the remaining application command.
    Assert-SearchNames $window @('Renamed-by-F2','Created-by-command')
    Remove-ShellFixture $window $folder 'Created-by-command'
    Assert-SearchNames $window @('Renamed-by-F2')
    Remove-ShellFixture $window $folder 'Renamed-by-F2'
    Assert-SearchNames $window @()
    Write-Host 'PASS: application New Folder, F2 rename and native Shift+Delete update both disk and visible identities.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
