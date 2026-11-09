# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name PeekTests -CaseDirectory $PSScriptRoot -ProductionSources @('Peek.cpp','WinUtils.cpp') -Resources -Standard 'c++14' -Libraries @('Ole32.lib','Dwmapi.lib')
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'PeekInput.cs')
$failure=$null; $protocolChanged=$false; $settingsCreated=$false; $directoryCreated=$false
$oldCapture=$env:FILESPACER_PEEK_CAPTURE
$protocol='HKCU:\Software\Classes\powertoys'
$settingsDirectory=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Microsoft\PowerToys'
$settingsFile=Join-Path $settingsDirectory 'settings.json'
function Captures { return @(Get-ChildItem -LiteralPath $script:CaptureDirectory -Filter '*.args' -File) }
function Expect-Capture([int]$Count,[string[]]$Paths) {
    Wait-Ui { @(Captures).Count -eq $Count } 'Peek stand-in received the command'
    $latest=Captures | Sort-Object LastWriteTimeUtc | Select-Object -Last 1
    $actual=@([IO.File]::ReadAllText($latest.FullName,[Text.Encoding]::Unicode).Split([char]10) | Where-Object {$_})
    Assert-Ui ((@($actual|Sort-Object) -join "`n") -ceq (@($Paths|Sort-Object) -join "`n")) 'Peek received different paths or settings arguments.'
}
function Focus-Item([IntPtr]$Window,[string]$Name,[bool]$Selected=$true) {
    Use-UiView $Window {param($view) $view.Select($Name); if(!$Selected){$view.Deselect($Name)}; $view.Focus()}
}
try {
    Initialize-Ui 'peek' -FixtureParent (Join-Path $ProjectRoot 'build')
    # A dedicated client with no real PowerToys configuration is required. Never
    # overwrite installed settings, even temporarily, for this stand-in test.
    if(Test-Path -LiteralPath $settingsFile) {
        @{Kind='PeekSettingsIsolation';Reason='An existing PowerToys settings.json must remain untouched.'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'blocked.json')
        throw 'BLOCKED: an existing PowerToys settings.json prevents this isolated fixture.'
    }
    foreach($base in @([Environment]::GetFolderPath('LocalApplicationData'),[Environment]::GetFolderPath('ProgramFiles'))) {
        if(Test-Path -LiteralPath (Join-Path $base 'PowerToys\WinUI3Apps\PowerToys.Peek.UI.exe')) {
            @{Kind='PeekInstallationIsolation';Reason='A real default Peek installation must remain untouched.'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'blocked.json')
            throw 'BLOCKED: a real default Peek installation prevents this isolated fixture.'
        }
    }
    $script:CaptureDirectory=Join-Path $script:UiRoot 'capture'
    $apps=Join-Path $script:UiRoot 'PowerToys fixture\WinUI3Apps'
    [void](New-Item -ItemType Directory -Path $script:CaptureDirectory,$apps)
    $runner=Join-Path (Split-Path $apps -Parent) 'PowerToys.exe'
    $peek=Join-Path $apps 'PowerToys.Peek.UI.exe'
    Copy-Item -LiteralPath $test -Destination $runner
    Copy-Item -LiteralPath $test -Destination $peek
    # Stop-Ui removes UiRoot before associations are restored. Keep both exports
    # in the persistent case reports, outside the application fixture directory.
    $backup=[IO.Path]::GetFullPath((Join-Path $env:FS_REGRESSION_RUN 'powertoys-peek-before.reg'))
    $hadProtocol=Test-Path -LiteralPath $protocol
    if($hadProtocol) {
        & reg.exe export 'HKCU\Software\Classes\powertoys' $backup /y | Out-Host
        if($LASTEXITCODE -ne 0){throw 'Cannot back up the PowerToys protocol association.'}
    }
    $protocolChanged=$true
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\Classes\powertoys')
    try {$key.SetValue('URL Protocol','');$command=$key.CreateSubKey('shell\open\command');try{$command.SetValue('',('"'+$runner+'" "%1"'))}finally{$command.Dispose()}}finally{$key.Dispose()}
    if(!(Test-Path -LiteralPath $settingsDirectory)) {[void](New-Item -ItemType Directory -Path $settingsDirectory);$directoryCreated=$true}
    [IO.File]::WriteAllText($settingsFile,'{"enabled":{"Peek":true}}',(New-Object Text.UTF8Encoding($false)));$settingsCreated=$true
    $env:FILESPACER_PEEK_CAPTURE=$script:CaptureDirectory
    $preference=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($script:UiSettingsKey)
    try{$preference.DeleteValue('PeekEnabled',$false)}finally{$preference.Dispose()}
    $folder=Join-Path $script:UiRoot 'Files'
    [void](New-Item -ItemType Directory -Path $folder,(Join-Path $folder 'Subfolder'))
    $first='caf'+[char]0xe9+' & %.txt';$second='second file.txt'
    [IO.File]::WriteAllText((Join-Path $folder $first),'fixture')
    [IO.File]::WriteAllText((Join-Path $folder $second),'fixture')
    $window=Open-UiFolder $folder 3
    Activate-Ui $window

    Write-Host 'STEP: default disabled, real Space remains a native selection key'
    Focus-Item $window $first $false
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Assert-UiSelected $window $first
    Assert-Ui (@(Captures).Count -eq 0) 'Peek ran while the FileSpacer option was disabled.'

    Write-Host 'STEP: native settings link and Cancel preserve pending activation'
    $sheet=Show-UiSettings $window;$page=Set-UiPage 3
    $check=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_PEEK_ENABLED'))
    $link=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_PEEK_SETTINGS'))
    Assert-Ui ([FileSpacerRegression.Native]::Message($check,0xf0,0,0) -eq 0) 'Peek should be disabled by default.'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowVisible($link)) 'PowerToys link is not visible.'
    Assert-Ui ([FileSpacerRegression.Native]::Text($link).Contains('PowerToys')) 'Prerequisite/link label is missing.'
    Set-UiCheck $page 'IDC_PEEK_ENABLED' $true
    Activate-Ui $sheet
    [void][FileSpacerRegression.Native]::Message($page,0x28,$link.ToInt64(),1) # WM_NEXTDLGCTL
    [FileSpacerRegression.Native]::Keys($sheet,0x0d,$false,$false)
    Expect-Capture 1 @('--open-settings=Peek')
    Press-UiSettings 5
    Assert-Ui ($null -eq (Get-UiPreference 'PeekEnabled')) 'Cancel persisted the pending Peek option.'
    [void](Show-UiSettings $window);$page=Set-UiPage 3
    Set-UiCheck $page 'IDC_PEEK_ENABLED' $true
    Press-UiSettings 3
    Assert-Ui ((Get-UiPreference 'PeekEnabled') -eq 1) 'Peek option was not persisted.'
    Activate-Ui $window

    Write-Host 'STEP: single selection, exact Unicode path and preserved view state'
    # Icon positions are meaningful in a spatial view. The modern Details
    # renderer legitimately returns E_NOTIMPL for GetItemPosition.
    Use-UiView $window {param($v) $v.SetViewModeAndIconSize(1,48)}
    Wait-UiViewMode $window 1 48
    Use-UiView $window {param($v) $v.AutoArrange=$false}
    Focus-Item $window $first
    $before=Use-UiView $window {param($v) [ordered]@{Selected=($v.Selected()-join ';');Focus=$v.Focused();Positions=($v.Positions()-join ';')}}
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Expect-Capture 2 @((Join-Path $folder $first))
    $after=Use-UiView $window {param($v) [ordered]@{Selected=($v.Selected()-join ';');Focus=$v.Focused();Positions=($v.Positions()-join ';')}}
    Assert-Ui (($before|ConvertTo-Json -Compress) -ceq ($after|ConvertTo-Json -Compress)) 'Preview changed selection, focused item or icon positions.'

    Write-Host 'STEP: repeated keydown opens one preview; multiple selection stays complete'
    [FileSpacerRegression.PeekInput]::Space($window,$false,$true)
    Expect-Capture 3 @((Join-Path $folder $first))
    Use-UiView $window {param($v) $v.Select($first);$v.Select($second,$true);$v.Select('Subfolder',$true);$v.Focus()}
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Expect-Capture 4 @((Join-Path $folder $first),(Join-Path $folder $second),(Join-Path $folder 'Subfolder'))

    Write-Host 'STEP: Ctrl+Space and Shift+Space retain native selection behavior'
    Focus-Item $window $first
    [FileSpacerRegression.Native]::Keys($window,0x20,$true,$false)
    Wait-Ui { @(Use-UiView $window {param($v) $v.Selected()}).Count -eq 0 } 'native Ctrl+Space selection toggle'
    [FileSpacerRegression.PeekInput]::Space($window,$true,$false)
    Assert-UiSelected $window $first
    Assert-Ui (@(Captures).Count -eq 4) 'A modified Space launched Peek.'

    Write-Host 'STEP: search and rename editors receive real spaces'
    [FileSpacerRegression.Native]::Keys($window,0x46,$true,$false)
    $search=Get-UiSearchEdit $window
    Wait-Ui { [FileSpacerRegression.Native]::FocusedWindowInOwningThread($window) -eq $search } 'Ctrl+F focused the search edit'
    [FileSpacerRegression.Native]::SetText($search,'abc')
    # EM_SETSEL(-1,-1) deselects at the existing caret; it does not move to
    # the end after WM_SETTEXT. Establish and verify the intended insertion point.
    [void][FileSpacerRegression.Native]::Message($search,0xb1,3,3)
    Assert-Ui ([FileSpacerRegression.Native]::Message($search,0xb0,0,0) -eq 0x00030003) 'Search insertion point is not at the end of abc.'
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Wait-Ui { [FileSpacerRegression.Native]::Text($search) -ceq 'abc ' } 'search received Space'
    [FileSpacerRegression.Native]::SetText($search,'')
    Wait-Ui { (Use-UiView $window {param($v) $v.Count}) -eq 3 } 'search cleared'
    Focus-Item $window $first
    [FileSpacerRegression.Native]::Keys($window,0x71,$false,$false) # F2
    $renameState=@{Window=[IntPtr]::Zero}
    Wait-Ui {$renameState.Window=[FileSpacerRegression.Native]::FocusedWindowInOwningThread($window);return ([FileSpacerRegression.Native]::Message($renameState.Window,0x87,0,0) -band 8) -ne 0} 'native rename editor'
    $rename=$renameState.Window
    [FileSpacerRegression.Native]::SetText($rename,'rename')
    [void][FileSpacerRegression.Native]::Message($rename,0xb1,6,6)
    Assert-Ui ([FileSpacerRegression.Native]::Message($rename,0xb0,0,0) -eq 0x00060006) 'Rename insertion point is not at the end of rename.'
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Wait-Ui { [FileSpacerRegression.Native]::Text($rename) -ceq 'rename ' } 'rename editor received Space'
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($rename) -or ![FileSpacerRegression.Native]::IsWindowVisible($rename) } 'rename cancelled'
    Assert-Ui (@(Captures).Count -eq 4) 'Typing in an editor launched Peek.'

    foreach($json in @('{"enabled":{"Peek":false}}','{bad json','{"enabled":{"Peek":"true"}}')) {
        Write-Host ('STEP: native fallback for PowerToys configuration: '+$json)
        [IO.File]::WriteAllText($settingsFile,$json,(New-Object Text.UTF8Encoding($false)))
        Focus-Item $window $first $false
        [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
        Assert-UiSelected $window $first
        Assert-Ui (@(Captures).Count -eq 4) 'An inactive/invalid PowerToys configuration launched Peek.'
    }
    [IO.File]::WriteAllText($settingsFile,'{"enabled":{"Peek":true}}',(New-Object Text.UTF8Encoding($false)))
    Write-Host 'STEP: missing PowerToys settings and absent Peek retain native Space'
    Remove-Item -LiteralPath $settingsFile
    Focus-Item $window $first $false
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Assert-UiSelected $window $first
    [IO.File]::WriteAllText($settingsFile,'{"enabled":{"Peek":true}}',(New-Object Text.UTF8Encoding($false)))
    Move-Item -LiteralPath $peek -Destination ($peek+'.disabled')
    Focus-Item $window $first $false
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    Assert-UiSelected $window $first
    Assert-Ui (@(Captures).Count -eq 4) 'Missing settings or missing Peek launched a preview.'
    Move-Item -LiteralPath ($peek+'.disabled') -Destination $peek
    Write-Host 'STEP: an incompatible executable produces an explicit launch error'
    $image=[IO.File]::ReadAllBytes($peek);$offset=[BitConverter]::ToInt32($image,0x3c)+4
    $image[$offset]=0x64;$image[$offset+1]=0xaa # ARM64 PE machine, on the required x64 client.
    [IO.File]::WriteAllBytes($peek,$image)
    Focus-Item $window $first
    [FileSpacerRegression.Native]::Keys($window,0x20,$false,$false)
    $processId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$processId)
    $errorState=@{Window=[IntPtr]::Zero}
    Wait-Ui {$dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId));if($dialogs.Count -eq 1){$errorState.Window=$dialogs[0];return $true};return $false} 'explicit Peek launch error dialog'
    Assert-Ui (@(Captures).Count -eq 4) 'The invalid executable was launched.'
    [void][FileSpacerRegression.Native]::Message($errorState.Window,0x111,1,0)
    Assert-UiSelected $window $first
    Write-Host 'PASS: native Shell Space, opt-in/Cancel/apply, PowerToys page link, exact selection, repeat/modifiers/editors, disabled/invalid JSON fallback, launch failure and unchanged selection/focus/positions. Real Peek rendering is not exercised by this stand-in.'
} catch {$failure=$_;throw}
finally {
    $cleanupErrors=New-Object Collections.ArrayList
    try {Stop-Ui -Failure $failure} catch {[void]$cleanupErrors.Add($_)}
    $env:FILESPACER_PEEK_CAPTURE=$oldCapture
    try {
        if($settingsCreated -and (Test-Path -LiteralPath $settingsFile)) {Remove-Item -LiteralPath $settingsFile -Force}
        if($directoryCreated -and @(Get-ChildItem -LiteralPath $settingsDirectory -Force).Count -eq 0){Remove-Item -LiteralPath $settingsDirectory}
    } catch {[void]$cleanupErrors.Add($_)}
    try {
        if($protocolChanged) {
            if(Test-Path -LiteralPath $protocol){Remove-Item -LiteralPath $protocol -Recurse -Force}
            if($hadProtocol){& reg.exe import $backup | Out-Host;if($LASTEXITCODE -ne 0){throw 'PowerToys protocol restoration failed.'}}
            Assert-Ui ((Test-Path -LiteralPath $protocol) -eq $hadProtocol) 'PowerToys protocol association was not restored.'
            if($hadProtocol){$afterBackup=[IO.Path]::GetFullPath((Join-Path $env:FS_REGRESSION_RUN 'powertoys-peek-after.reg'));& reg.exe export 'HKCU\Software\Classes\powertoys' $afterBackup /y|Out-Host;if($LASTEXITCODE -ne 0){throw 'Cannot verify restored PowerToys association.'};Assert-Ui ((Get-FileHash -LiteralPath $backup).Hash -eq (Get-FileHash -LiteralPath $afterBackup).Hash) 'Restored PowerToys protocol differs from its original export.'}
        }
    } catch {[void]$cleanupErrors.Add($_)}
    if($cleanupErrors.Count) {
        $details=@($cleanupErrors|ForEach-Object {$_.ToString()}) -join "`r`n"
        [IO.File]::WriteAllText((Join-Path $env:FS_REGRESSION_RUN 'peek-cleanup-errors.log'),$details)
        # Post-Stop-Ui restoration errors must also reach the standard outcome
        # classifier; a foreground prerequisite must never hide failed cleanup.
        $cleanupReport=Join-Path $env:FS_REGRESSION_RUN 'cleanup-errors.json'
        $allCleanup=@($cleanupErrors|ForEach-Object {$_.ToString()})
        if(Test-Path -LiteralPath $cleanupReport) {
            $allCleanup=@((Get-Content -LiteralPath $cleanupReport -Raw|ConvertFrom-Json))+$allCleanup
        }
        ConvertTo-Json -InputObject @($allCleanup)|Set-Content -LiteralPath $cleanupReport -Encoding UTF8
        if(!$failure){throw ('Peek cleanup failed: '+$details)}
        Write-Warning ('Additional Peek cleanup failures: '+$details)
    }
}
