# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
$failure=$null
$observations=New-Object Collections.ArrayList
function Open-ConfigurationFromMenu([IntPtr]$Window,[switch]$CancelMenu) {
    Activate-Ui $Window
    $button=[FileSpacerRegression.Native]::ToolbarButton((Get-UiCommandToolbar $Window),(Get-UiId 'IDM_CONTEXT_MENU'))
    [FileSpacerViewMenuTests.MenuUi]::Click([int](($button.Left+$button.Right)/2),[int](($button.Top+$button.Bottom)/2),$Window)
    $found=@{Popup=$null;Dialog=[IntPtr]::Zero}
    Wait-Ui {
        $found.Popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
        return $null -ne $found.Popup
    } 'actual application popup opened'
    if($CancelMenu) {
        [FileSpacerViewMenuTests.MenuUi]::Escape($Window)
        Wait-Ui { $null -eq [FileSpacerViewMenuTests.MenuUi]::Active($Window,$true) } 'actual application popup cancelled'
        return
    }
    $index=-1
    for($i=0;$i -lt $found.Popup.Entries.Count;++$i) {
        if($found.Popup.Entries[$i].Command -eq (Get-UiId 'IDM_SETTINGS')) { $index=$i }
    }
    Assert-Ui ($index -ge 0) 'Configuration command missing from the actual menu.'
    [FileSpacerViewMenuTests.MenuUi]::Choose($found.Popup,$index,$Window)
    $processId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$processId)
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId) | Where-Object {
            @([FileSpacerRegression.Native]::Children($_,'SysTabControl32')).Count -eq 1
        })
        if($dialogs.Count -eq 1) { $found.Dialog=$dialogs[0];return $true }
        return $false
    } 'Configuration opened from the actual application menu'
    $script:UiSettings=$found.Dialog
}
function Save-Layout([string]$Stage) {
    $row=Get-UiControl $script:window 'ReBarWindow32'
    $hostWindow=Get-UiControl $script:window 'FileSpacer Toolbar Host'
    $value=@{Stage=$Stage;Client=[FileSpacerRegression.Native]::ClientBounds($script:window);
        Row=[FileSpacerRegression.Native]::Bounds($row);Host=[FileSpacerRegression.Native]::ClientBounds($hostWindow);
        Commands=[FileSpacerRegression.Native]::Bounds((Get-UiCommandToolbar $script:window));
        View=[FileSpacerRegression.Native]::ClientBounds((Get-ColumnsViewWindow $script:window))}
    [void]$observations.Add($value)
    $observations|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'layout.json') -Encoding UTF8
    Write-Host ('GEOMETRY '+$Stage+': client='+$value.Client.Width+', row='+$value.Row.Width+', host='+$value.Host.Width+', commands right='+$value.Commands.Right)
    return $value
}
try {
    Initialize-Ui 'column-cold-start' -FixtureParent (Join-Path $ProjectRoot 'build')
    $folder=Join-Path $script:UiRoot 'Documents'
    [void][IO.Directory]::CreateDirectory($folder)
    1..100 | ForEach-Object { [IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns') }
    $script:window=Open-UiFolder $folder 100
    [void](Show-UiSettings $script:window);$page=Set-UiPage 0
    $startingFolder=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_START_FOLDER_PATH'))
    [FileSpacerRegression.Native]::SetText($startingFolder,$folder)
    $page=Set-UiPage 1
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_ENABLED' $true
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_DEFERRED' $true
    Press-UiSettings 3
    Wait-Ui { (Get-ColumnsViewWindow $script:window) -ne [IntPtr]::Zero } 'native view rebuilt after applying column preferences'
    Use-UiView $script:window {param($view) $view.Mode=4};Wait-UiViewMode $script:window 4
    [void](Wait-Columns $script:window)
    Close-UiFolder $script:window
    foreach($process in $script:UiProcesses) { Assert-Ui ($process.WaitForExit(10000)) 'FileSpacer did not exit before the cold restart.' }
    Assert-Ui ((Get-UiPreference 'StartingFolder') -eq $folder) 'The configured starting folder was not saved.'
    $process=Start-TestApplication -Executable $script:UiExecutable -Arguments '/test' -OutputDirectory $env:FS_REGRESSION_RUN
    [void]$script:UiProcesses.Add($process)
    $script:window=Track-UiFolder $folder
    Wait-UiViewMode $script:window 4
    Assert-Ui ((Get-UiPreference 'DetailsColumnsEnabled') -eq 1 -and (Get-UiPreference 'DetailsColumnsDeferred') -eq 1) 'Cold start did not preserve the column preferences.'
    [void](Wait-Columns $script:window)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($script:window,[IntPtr](-1),0,0,0,0,0x13)) 'Cannot expose the cold-start test window without activating its caption.'
    foreach($iteration in @(1,2,3,4)) {
        [void](Save-Layout ('before-configuration-'+$iteration))
        if($iteration -eq 1) {
            Open-ConfigurationFromMenu $script:window -CancelMenu
        } else {
            Open-ConfigurationFromMenu $script:window
            $page=Set-UiPage 1
            if($iteration -eq 4) { Set-UiCheck $page 'IDC_DETAILS_COLUMNS_DEFERRED' $false }
            Press-UiSettings $(if($iteration -eq 2){5}else{3})
            Assert-Ui ((Get-UiPreference 'DetailsColumnsDeferred') -eq [int]($iteration -ne 4)) 'Configuration did not preserve the expected deferred/continuous preference.'
        }
        $bounds=[FileSpacerRegression.Native]::Bounds($script:window)
        $before=Save-Layout ('before-'+$iteration)
        $start=New-Object FileSpacerRegression.MousePoint
        $corner=$false
        foreach($offset in @(3,4,5,6,7)) {
            $start.X=$bounds.Right-$offset;$start.Y=$bounds.Bottom-$offset
            $packed=($start.X -band 65535) -bor (($start.Y -band 65535) -shl 16)
            if([FileSpacerRegression.Native]::Message($script:window,0x84,0,$packed) -eq 17) { $corner=$true;break }
        }
        Assert-Ui $corner 'No native bottom-right sizing border was found.'
        [FileSpacerRegression.AltDragInput]::BeginConfigured($script:window,$start,0,0)
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Moving($script:window) } 'native sizing loop entered'
        $point=New-Object FileSpacerRegression.MousePoint
        $point.X=$start.X+96;$point.Y=$start.Y+48
        [FileSpacerRegression.AltDragInput]::Move($point)
        Wait-Ui { [FileSpacerRegression.Native]::Bounds($script:window).Width -eq $bounds.Width+96 } 'cold-start window follows the border'
        [void](Save-Layout ('held-'+$iteration))
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($script:window) } 'native sizing loop exited'
        [void](Save-Layout ('released-immediate-'+$iteration))
        Wait-Ui {
            $client=[FileSpacerRegression.Native]::ClientBounds($script:window)
            $row=[FileSpacerRegression.Native]::Bounds((Get-UiControl $script:window 'ReBarWindow32'))
            $hostBounds=[FileSpacerRegression.Native]::ClientBounds((Get-UiControl $script:window 'FileSpacer Toolbar Host'))
            [Math]::Abs($row.Width-$client.Width) -le 1 -and [Math]::Abs($hostBounds.Width-$row.Width) -le 2
        } 'cold-start toolbar and its band child settle to the new width after release' 250 5
        $after=Save-Layout ('released-settled-'+$iteration)
        $captureBounds=[FileSpacerRegression.Native]::Bounds($script:window)
        $bitmap=New-Object Drawing.Bitmap($captureBounds.Width,$captureBounds.Height)
        $graphics=[Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($captureBounds.Left,$captureBounds.Top,0,0,$bitmap.Size)
            $bitmap.Save((Join-Path $env:FS_REGRESSION_RUN ('released-'+$iteration+'.png')))
        } finally { $graphics.Dispose();$bitmap.Dispose() }
        Assert-Ui ([Math]::Abs($after.Commands.Right-$after.Host.Right) -le 1) 'Cold-start commands did not follow the right edge after release.'
        Assert-Ui ($after.View.Width -eq $before.View.Width+96) 'Cold-start native view retained its old width after release.'
        [void](Wait-Columns $script:window)
    }
    Write-Host 'PASS: cold start in saved Details/deferred mode, real application menu cancellation, Configuration Cancel/OK and continuous-mode switch; toolbar, commands, viewport and columns remain responsive to real border resizes.'
} catch { $failure=$_;try { [void](Save-Layout 'failure') } catch { Write-Warning $_ };throw $failure } finally {
    try { [FileSpacerRegression.AltDragInput]::End() } finally { Stop-Ui -Failure $failure }
}
