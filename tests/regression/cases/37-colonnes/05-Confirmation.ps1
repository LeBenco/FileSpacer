# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
function Find-ColumnWarning([IntPtr]$Window) {
    $windowProcessId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$windowProcessId)
    return @([FileSpacerRegression.Native]::Top('#32770','FileSpacer',$windowProcessId) |
        Where-Object { [FileSpacerRegression.Native]::IsWindowEnabled($_) -and [FileSpacerRegression.Native]::IsWindowVisible($_) })
}
function Request-ColumnFill([IntPtr]$Window,[switch]$Menu) {
    if($Menu) {
        [void](Open-PreviewMenu $Window)
        $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
        $index=-1
        for($i=0;$i -lt $popup.Entries.Count;++$i) { if($popup.Entries[$i].Command -eq (Get-UiId 'IDM_FILL_COLUMNS')) { $index=$i } }
        Assert-Ui ($index -ge 0) 'The column command is absent from the real View menu.'
        [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$index,$Window)
        Wait-PreviewMenuClosed $Window
    } else {
        [void][FileSpacerRegression.Native]::PostMessage($Window,0x111,[IntPtr](Get-UiId 'IDM_FILL_COLUMNS'),[IntPtr]::Zero)
    }
}
$failure=$null
try {
    Initialize-Ui 'column-no-warning' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'DetailsColumnsEnabled' 1
    Set-UiPreference 'DetailsColumnsDeferred' 1
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $folder=Join-Path $script:UiRoot 'Small'
    [void][IO.Directory]::CreateDirectory($folder)
    1..5|ForEach-Object{[IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns')}
    $window=Open-UiFolder $folder 5
    Use-UiView $window {param($view) $view.Mode=4};Wait-UiViewMode $window 4
    [void](Wait-Columns $window)
    Request-ColumnFill $window
    Wait-ModernColumns $window
    Request-ColumnFill $window -Menu
    [void](Wait-Columns $window)
    Assert-Ui (@(Find-ColumnWarning $window).Count -eq 0) 'Deferred activation should not warn.'
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_DEFERRED' $false
    Press-UiSettings 3
    Request-ColumnFill $window
    Wait-ModernColumns $window
    Request-ColumnFill $window -Menu
    [void](Wait-Columns $window)
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($window) -and @(Find-ColumnWarning $window).Count -eq 0) 'Continuous activation must apply without a modal warning.'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 5
    [void](Wait-Columns $window)
    Assert-Ui (@(Find-ColumnWarning $window).Count -eq 0) 'Restoring the accepted choice asked again.'
    Request-ColumnFill $window
    Wait-ModernColumns $window
    Assert-Ui (@(Find-ColumnWarning $window).Count -eq 0) 'Disabling column fill asked for confirmation.'
    Write-Host 'PASS: deferred and continuous activation apply without warning, the enabled choice persists, and restoring or disabling does not prompt.'
} catch {$failure=$_;throw} finally {
    foreach($process in @($script:UiProcesses)){
        if(!$process.HasExited){
            foreach($dialog in @([FileSpacerRegression.Native]::Top('#32770','FileSpacer',[uint32]$process.Id))){
                [void][FileSpacerRegression.Native]::PostMessage($dialog,0x466,[IntPtr]7,[IntPtr]::Zero)
            }
        }
    }
    Stop-Ui -Failure $failure
}
