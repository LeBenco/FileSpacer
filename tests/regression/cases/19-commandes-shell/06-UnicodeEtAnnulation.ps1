# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
try {
    Initialize-Ui 'shell-unicode-cancel'
    $folder=Join-Path $script:UiRoot ('Folder with spaces '+[char]0xe9);[void][IO.Directory]::CreateDirectory($folder)
    $before='caf'+[char]0xe9+' original';$after='Renamed '+[char]0x03bb+' '+('long-name-'*6)
    [IO.File]::WriteAllText((Join-Path $folder $before),'unicode-path-payload')
    $window=Open-UiFolder $folder 1
    Focus-ShellFixture $window $before
    [FileSpacerRegression.Native]::Keys($window,0x71,$false,$false)
    Wait-Ui { [FileSpacerRegression.ShellIntegration]::RenameFocused($window) } 'rename editor before cancellation'
    [FileSpacerRegression.Native]::Keys($window,0x41,$true,$false)
    [FileSpacerRegression.ShellIntegration]::TypeText($window,'Must-not-exist')
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Assert-SearchNames $window @($before)
    Assert-Ui (!(Test-Path -LiteralPath (Join-Path $folder 'Must-not-exist'))) 'Escape committed the cancelled rename.'
    Rename-ShellFixture $window $folder $before $after
    Assert-SearchNames $window @($after)
    Assert-Ui ([IO.File]::ReadAllText((Join-Path $folder $after)) -ceq 'unicode-path-payload') 'Unicode rename changed contents.'
    Close-UiFolder $window;$window=Open-UiFolder $folder 1
    Assert-SearchNames $window @($after)
    Write-Host 'PASS: cancelling rename leaves disk unchanged; spaces, Unicode and a long leaf name survive UI rename and reopening.'
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
