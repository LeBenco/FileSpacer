# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'SearchIntegration.ps1')
$testFailure=$null;$external=$null;$externalWindow=[IntPtr]::Zero
$executable=if($env:FILESPACER_EVERYTHING_EXE){$env:FILESPACER_EVERYTHING_EXE}else{Join-Path $env:LOCALAPPDATA 'Programs\Everything\Everything.exe'}
function Expect-LaunchError([IntPtr]$Window,[string]$Path) {
    Set-SearchOptions $Window $true 5 $Path
    Submit-Search $Window 'keep' @('keep.txt')
    Select-SearchMenu $Window 0
    $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$ownerId)
    $found=@{Dialog=[IntPtr]::Zero}
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$ownerId))
        if ($dialogs.Count -ne 1) { return $false };$found.Dialog=$dialogs[0];return $true
    } 'explicit Everything launch error dialog'
    $text=(@([FileSpacerRegression.Native]::Children($found.Dialog,'Static') | ForEach-Object { [FileSpacerRegression.Native]::Text($_) }) -join ' ')
    Assert-Ui ($text.Contains('Everything') -and $text.Contains($Path) -and $text -match '([Ee]rreur|[Ee]rror)') 'Launch error is not explicit.'
    $ok=Get-UiControl $found.Dialog 'Button'
    [void][FileSpacerRegression.Native]::PostMessage($ok,0xf5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($found.Dialog) } 'launch error dismissed'
    Assert-Ui ([FileSpacerRegression.Native]::Text((Get-UiSearchEdit $Window)) -eq 'keep') 'Failed launch cleared the query.'
    Assert-SearchNames $Window @('keep.txt')
}
function Assert-EverythingQuery([string]$Expected) {
    Wait-Ui {
        $edits=@([FileSpacerRegression.Native]::Children($externalWindow,'Edit'))
        return @($edits | Where-Object { [FileSpacerRegression.Native]::Text($_) -ceq $Expected }).Count -eq 1
    } ('actual Everything search text: '+$Expected)
}
function Assert-EverythingNames([string[]]$Expected) {
    $wanted=@($Expected | Sort-Object) -join ';'
    $observed=@{Names=''}
    try {
        Wait-Ui {
            $observed.Names=([FileSpacerRegression.SearchMenus]::ResultNames($externalWindow) | Sort-Object) -join ';'
            return $observed.Names -ceq $wanted
        } ('actual Everything results: '+$wanted) 300
    } catch { Write-Host ('DIAG: actual Everything names='+$observed.Names);throw }
}
try {
    if (!(Test-Path -LiteralPath $executable)) {
        $reason='Install Everything or set FILESPACER_EVERYTHING_EXE for the real integration test.'
        @{Kind='EverythingPrerequisite';Reason=$reason} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'blocked.json') -Encoding UTF8
        throw ('BLOCKED: '+$reason)
    }
    Initialize-Ui 'everything-real' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot ('Dossier espace '+[char]0xe9)
    [void](New-Item -ItemType Directory -Path $folder)
    foreach($name in @('keep.txt','other.txt')) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $sub=Join-Path $folder 'sub';[void](New-Item -ItemType Directory -Path $sub)
    [IO.File]::WriteAllText((Join-Path $sub 'subkeep.txt'),'fixture')
    [IO.File]::WriteAllText((Join-Path $script:UiRoot 'outside-keep.txt'),'fixture')
    $window=Open-UiFolder $folder 3
    Expect-LaunchError $window (Join-Path $script:UiRoot 'absent\Everything.exe')
    [void](Show-UiSettings $window);$page=Set-UiPage 3
    [void][FileSpacerRegression.Native]::Message([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_EVERYTHING_DETECT')),0xf5,0,0)
    $detected=[FileSpacerRegression.Native]::Text([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_EVERYTHING_PATH')))
    Assert-Ui ([IO.Path]::GetFullPath($detected) -eq [IO.Path]::GetFullPath($executable)) 'Automatic detection did not find installed Everything.'
    Press-UiSettings 5
    Assert-Ui (@(Get-Process Everything -ErrorAction SilentlyContinue).Count -eq 0) 'Close existing Everything before isolated integration test.'
    $config=Join-Path $script:UiRoot 'Everything.ini'
    $ini=@('[Everything]','app_data=0','run_as_admin=0','run_in_background=0','show_tray_icon=0',
        'check_for_updates_on_startup=0','auto_include_fixed_volumes=0','auto_include_removable_volumes=0',
        'auto_include_fixed_refs_volumes=0','auto_include_removable_refs_volumes=0',('folders='+$script:UiRoot),'folder_monitor_changes=1','language=1033') -join "`r`n"
    [IO.File]::WriteAllText($config,$ini,(New-Object Text.UTF8Encoding($false)))
    $external=Start-TestApplication -Executable $executable -Arguments ('-config "'+$config+'" -db "'+(Join-Path $script:UiRoot 'Everything.db')+'"') -OutputDirectory $env:FS_REGRESSION_RUN
    Wait-Ui { $external.Refresh();return $external.MainWindowHandle -ne [IntPtr]::Zero } 'real Everything window'
    $externalWindow=$external.MainWindowHandle
    Write-Host ('STEP: real Everything class='+[FileSpacerRegression.Native]::Class($externalWindow)+', version='+(Get-Item -LiteralPath $executable).VersionInfo.FileVersion)
    Set-SearchOptions $window $true 5 '' 0
    Submit-Search $window 'keep' @('keep.txt')
    Select-SearchMenu $window 0
    Assert-EverythingQuery ('"'+$folder+'\" <keep>')
    Assert-EverythingNames @('keep.txt','subkeep.txt')
    Assert-SearchNames $window @('keep.txt','other.txt','sub')
    Assert-Ui ([FileSpacerRegression.Native]::Text((Get-UiSearchEdit $window)) -eq '') 'Successful launch did not clear the local edit.'
    [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'keep|other')
    Select-SearchMenu $window 0
    Assert-EverythingQuery ('"'+$folder+'\" <keep|other>')
    Assert-EverythingNames @('keep.txt','other.txt','subkeep.txt')
    # Manual executable path containing spaces and non-ASCII characters.
    $manualFolder=Join-Path $script:UiRoot ('Outils espace '+[char]0xe9)
    [void](New-Item -ItemType Directory -Path $manualFolder)
    $manual=Join-Path $manualFolder 'Everything.exe';Copy-Item -LiteralPath $executable -Destination $manual
    Set-SearchOptions $window $true 0 $manual 1
    [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),'keep')
    Select-SearchMenu $window 0
    Assert-EverythingQuery 'keep'
    Assert-EverythingNames @('keep.txt','subkeep.txt','outside-keep.txt')
    foreach($query in @(('caf'+[char]0xe9+' & | ^ % "a b" C:\test\ -exit'),'""','trail\','-exit','a|b')) {
        [FileSpacerRegression.Native]::SetText((Get-UiSearchEdit $window),$query)
        Select-SearchMenu $window 0
        Assert-EverythingQuery $query
        Assert-SearchNames $window @('keep.txt','other.txt','sub')
    }
    Write-Host 'PASS: real Everything 1.4, automatic detection, manual Unicode path, both scopes, exact visible queries, quote/metacharacter transfer, success reset and explicit failure preserving local search.'
} catch { $testFailure=$_; throw }
finally {
    try {
        if ($externalWindow -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($externalWindow)) {
            [void][FileSpacerRegression.Native]::PostMessage($externalWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        }
        if ($external) {
            if (!$external.WaitForExit(10000)) { throw 'Test Everything did not exit after WM_CLOSE.' }
            Assert-Ui ($external.ExitCode -eq 0) 'Everything failed on exit.'
            $external.Dispose()
        }
    } finally { Stop-Ui -Failure $testFailure }
}
